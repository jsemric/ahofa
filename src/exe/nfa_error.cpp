/// @author Jakub Semric
/// 2018

#include <iostream>
#include <ostream>
#include <exception>
#include <vector>
#include <map>
#include <csignal>
#include <stdexcept>
#include <ctime>
#include <mutex>
#include <thread>
#include <chrono>
#include <ctype.h>
#include <getopt.h>

#include <boost/filesystem.hpp>

#include "nfa.hpp"
#include "pcap_reader.hpp"

using namespace reduction;
using namespace std;

namespace fs = boost::filesystem;

class StopWork : public exception {};

struct Data
{
    // long data
    vector<size_t> nfa1_data;
    vector<size_t> nfa2_data;
    // packets statistics
    size_t total;
    size_t accepted_reduced;
    size_t accepted_target;
    size_t wrongly_classified;
    size_t correctly_classified;

    Data(size_t data_reduced_size = 1, size_t data_target_size = 1) :
        nfa1_data(data_reduced_size), nfa2_data(data_target_size), total{0},
        accepted_reduced{0}, accepted_target{0}, wrongly_classified{0},
        correctly_classified{0} {}

    ~Data() = default;

    void aggregate(const Data &other_data) {
        total += other_data.total;
        accepted_target += other_data.accepted_target;
        accepted_reduced += other_data.accepted_reduced;
        wrongly_classified += other_data.wrongly_classified;
        correctly_classified += other_data.correctly_classified;
        for (size_t i = 0; i < other_data.nfa1_data.size(); i++) {
            nfa1_data[i] += other_data.nfa1_data[i];
        }
        for (size_t i = 0; i < other_data.nfa2_data.size(); i++) {
            nfa2_data[i] += other_data.nfa2_data[i];
        }
    }
};

//extern mutex pcapreader::bpf_compile_mux;

const char *helpstr =
"The program computes an error of an incorrect packet classification by an\n"
"over-approximated NFA. Positional arguments are TARGET REDUCED PCAPS ...,\n"
"where TARGET is an input NFA, REDUCED denotes over-approximated reduction of\n"
"the TARGET and PCAPS are packet capture files.\n"
"Usage: ./nfa_handler [OPTIONS] TARGET REDUCED ...\n"
"options:\n"
"  -h            : show this help and exit\n"
"  -o <FILE>     : specify output file or directory for -s option\n"
"  -n <NWORKERS> : number of workers to run in parallel\n"
"  -f <FILTER>   : define BPF filter, for syntax see man page\n"
"  -j            : verbose output in JSON\n"
"  -c            : Rigorous error computation. Consistent but much slower.\n"
"                  Use only if not sure about over-approximation\n"
"  -s            : store results to a separate file per each packet capture\n"
"                  file\n";

// general program options
unsigned nworkers = 1;
const char *filter_expr;
bool store_sep = false;
bool to_json = false;
bool consistent = false;
string outdir = "data/prune-error";

// thread communication
bool continue_work = true;
mutex mux;

// common data
Data all_data;
FastNfa target, reduced;
string nfa_str1, nfa_str2;

// error computing data
vector<size_t> final_state_idx_reduced;
vector<size_t> final_state_idx_target;
// maps state to state string name
map<State,State> state_map_reduced;
map<State,State> state_map_target;

// time statistics
chrono::steady_clock::time_point timepoint;

// gather results
void sum_up(const Data &data)
{
    mux.lock();
    all_data.aggregate(data);
    mux.unlock();
}

void sighandl(int signal)
{
    (void)signal;
    cout << "\n";
    // stop all work
    continue_work = false;
}

void compute_error(Data &data, const unsigned char *payload, unsigned plen)
{
    vector<bool> bm(reduced.state_count());
    reduced.parse_word(payload, plen, [&bm](State s){ bm[s] = 1; });
    int match1 = 0;

    for (size_t i = 0; i < final_state_idx_reduced.size(); i++)
    {
        size_t idx = final_state_idx_reduced[i];
        if (bm[idx])
        {
            match1++;
            data.nfa1_data[idx]++;
        }
    }

    data.accepted_reduced += match1 > 0;

    if (match1 || consistent)
    {    
        int match2 = 0;
        // something was matched, lets find the difference
        vector<bool> bm(target.state_count());
        target.parse_word(payload, plen, [&bm](State s){ bm[s] = 1; });
        for (size_t i = 0; i < final_state_idx_target.size(); i++)
        {
            size_t idx = final_state_idx_target[i];
            if (bm[idx])
            {
                match2++;
                data.nfa2_data[idx]++;
            }
        }

        if (match1 != match2)
        {
            data.wrongly_classified++;
            if (consistent && match2 > match1)
            {
                cerr << "Reduced automaton ain't over-approximation!\n";
                exit(1);
            }
        }
        else
        {
            data.correctly_classified++;
        }

        if (match2)
        {
            data.accepted_target++;
        }
    }
}

string gen_output_name(string pcap)
{
    string base = outdir + "/" +
                  fs::basename(nfa_str1) + "." +
                  fs::path(pcap).filename().string() + ".";
    string res;
    string hash = "00000";
    int i = 0;
    do {
        hash[0] = '0' + (rand()%10);
        hash[1] = '0' + (rand()%10);
        hash[2] = '0' + (rand()%10);
        hash[3] = '0' + (rand()%10);
        hash[4] = '0' + (rand()%10);
        res = base + hash + ".json";
    } while (fs::exists(res) || i++ > 100000);

    return res;
}

void write_error_data(ostream &out, const Data &data, const string pcapname);

void process_pcaps(
    const vector<string> &pcaps, Data &local_data)
{
    for (unsigned i = 0; i < pcaps.size(); i++) {
        try {
            // just checking how many packets are accepted
            pcapreader::process_payload(
                pcaps[i].c_str(),
                [&] (const unsigned char *payload, unsigned len)
                {
                    if (continue_work == false) {
                        // SIGINT caught in parent, sum up and exit
                        throw StopWork();
                    }
                    // specific payload handler
                    local_data.total++;
                    compute_error(local_data, payload, len);

                }, filter_expr);

            if (store_sep) {
                auto fname = gen_output_name(pcaps[i]);
                ofstream out(fname);
                if (out.is_open()) {
                    write_error_data(out, local_data, pcaps[i]);
                    out.close();
                }
                else {
                    throw ofstream::failure(
                        "cannot open file: " + fname);
                }
            }
        }
        catch (ios_base::failure &e) {
            cerr << "\033[1;31mWARNING\033[0m " << e.what() << "\n";
            // process other capture files, continue for loop
        }
        catch (runtime_error &e) {
            cerr << "\033[1;31mWARNING\033[0m " << e.what() << "\n";
        }
        catch (StopWork &e) {
            // SIGINT - stop the thread
            break;
        }
        catch (exception &e) {
            // SIGINT or other error
            cerr << "\033[1;31mWARNING\033[0m " << e.what() << "\n";
            break;
        }
    }
    // sum up results
    sum_up(local_data);
}

void write_error_data(ostream &out, const Data &data, const string pcapname)
{
    unsigned msec = chrono::duration_cast<chrono::microseconds>(
        chrono::steady_clock::now() - timepoint).count();
    unsigned sec = msec / 1000 / 1000;
    unsigned min = sec / 60;

    size_t cls1 = 0, cls2 = 0;
    for (auto i : data.nfa1_data) {
        cls1 += i;
    }
    for (auto i : data.nfa2_data) {
        cls2 += i;
    }
    size_t wrong_acceptances = data.accepted_reduced -
        data.accepted_target;

    float pe = wrong_acceptances * 1.0 / data.total;
    float ce = data.wrongly_classified * 1.0 / data.total;
    float cls_ratio = data.correctly_classified * 1.0 /
        (data.correctly_classified + data.wrongly_classified);
    unsigned long sc1 = target.state_count();
    unsigned long sc2 = reduced.state_count();

    if (!to_json) {
        out << "reduction : " << 1.0 * sc2 / sc1 << endl;
        out << "total     : " << data.total << endl;
        out << "pe        : " << pe << endl;
        out << "ce        : " << ce << endl;
        out << "+rate     : " << cls_ratio << endl;
        out << "duration  : " << min << "m/" << sec % 60 << "s/"
            << msec % 1000 << "ms\"\n";
        return;
    }

    out << "{\n";
    out << "    \"target file\": \"" << nfa_str1 << "\",\n";
    out << "    \"target\": \"" << fs::basename(nfa_str1)
        << "\",\n";
    out << "    \"target states\": " << sc1 << ",\n";
    out << "    \"reduced file\": \"" << nfa_str2 << "\",\n";
    out << "    \"reduced\": \"" << fs::basename(nfa_str2)
        << "\",\n";
    out << "    \"reduced states\": " << sc2 << ",\n";
    out << "    \"reduction\": " << 1.0 * sc2 / sc1 << ",\n";
    out << "    \"total packets\": " << data.total << ",\n";
    out << "    \"accepted target\": " << data.accepted_target
        << ",\n";
    out << "    \"accepted reduced\": " << data.accepted_reduced
        << ",\n";
    out << "    \"reduced classifications\": " << cls1 << ",\n";
    out << "    \"target classifications\": " << cls2 << ",\n";
    out << "    \"wrong detections\": "
        << data.wrongly_classified << ",\n";
    out << "    \"correct detections\":"
        << data.correctly_classified << ",\n";
 
    // concrete rules results
    out << "    \"reduced rules\": {\n";
    for (size_t i = 0; i < final_state_idx_reduced.size(); i++) {
        State s = final_state_idx_reduced[i];
        out << "        \"q" << state_map_reduced[s]  << "\" : "
            << data.nfa1_data[s];
        if (i == final_state_idx_reduced.size() - 1) {
            out << "}";
        }
        out << ",\n";
    }

    out << "    \"target rules\": {\n";
    for (size_t i = 0; i < final_state_idx_target.size(); i++) {
        State s = final_state_idx_target[i];
        out << "        \"q" << state_map_target[s]  << "\" : "
            << data.nfa2_data[s];
        if (i == final_state_idx_target.size() - 1) {
            out << "\n    }";
        }
        out << ",\n";
    }

    if (pcapname != "")
    {
        out << "    \"pcap\" : \"" << pcapname << "\"\n";
    }
    out << "}\n";
}

void write_output(ostream &out)
{
    if (!store_sep)
    {
        write_error_data(out, all_data, "");
    }
}

int main(int argc, char **argv)
{
    timepoint = chrono::steady_clock::now();
    string ofname;
    vector<string> pcaps;

    const char *outfile = nullptr;
    int opt_cnt = 1;    // program name
    int c;

    try {
        if (argc < 2) {
            cerr << helpstr;
            return 1;
        }

        while ((c = getopt(argc, argv, "ho:n:f:jcs")) != -1) {
            opt_cnt++;
            switch (c) {
                // general options
                case 'h':
                    cerr << helpstr;
                    return 0;
                case 'o':
                    outfile = optarg;
                    opt_cnt++;
                    break;
                case 'n':
                    nworkers = stoi(optarg);
                    opt_cnt++;
                    break;
                case 'f':
                    filter_expr = optarg;
                    opt_cnt++;
                    break;
                case 'j':
                    to_json = true;
                    break;
                case 'c':
                    consistent = true;
                    break;
                case 's':
                    store_sep = true;
                    break;
                default:
                    return 1;
            }
        }

        if (nworkers <= 0 ||
            nworkers >= thread::hardware_concurrency())
        {
            throw runtime_error(
                "invalid number of cores \"" + to_string(nworkers) + "\"");
        }

        if (argc - opt_cnt < 3)
        {
            throw runtime_error("invalid positional arguments");
        }

        // get automata
        nfa_str1 = argv[opt_cnt];
        target.read_from_file(nfa_str1.c_str());
        size_t target_size = target.state_count();
        state_map_target = target.get_reversed_state_map();
        final_state_idx_target = target.get_final_state_idx();
        // do some preparation for commands `label` and `error`
        nfa_str2 = argv[opt_cnt + 1];
        reduced.read_from_file(nfa_str2.c_str());
        size_t reduced_size = reduced.state_count();
        state_map_reduced = reduced.get_reversed_state_map();
        final_state_idx_reduced = reduced.get_final_state_idx();

        // get capture files
        for (int i = opt_cnt + 2/*2 - accepted_only*/; i < argc; i++) {
            pcaps.push_back(argv[i]);
            //cerr << argv[i] << "\n";
        }

        // check output file
        ostream *output = &cout;
        if (outfile) {
            if (store_sep) {
                outdir = outfile;
                if (!fs::is_directory(outdir)) {
                    throw runtime_error("invalid directory");
                }
                outfile = nullptr;
            }
            else {
                output = new ofstream{outfile};
                if (!static_cast<ofstream*>(output)->is_open()) {
                    throw runtime_error("cannot open output file");
                }
            }
        }

        // divide work
        vector<vector<string>> v(nworkers);
        for (unsigned i = 0; i < pcaps.size(); i++) {
            v[i % nworkers].push_back(pcaps[i]);
        }

        // start computation
        // signal handling
        signal(SIGINT, sighandl);
        all_data = Data(reduced_size, target_size);

        vector<thread> threads;
        for (unsigned i = 0; i < nworkers; i++)
        {
            threads.push_back(thread{[&v, i, reduced_size, target_size]()
                    {
                        Data local_data(reduced_size, target_size);
                        process_pcaps(v[i], local_data);
                    }
                });
        }

        for (unsigned i = 0; i < nworkers; i++) {
            if (threads[i].joinable()) {
                threads[i].join();
            }
        }

        if (!store_sep)
        {
            write_output(*output);
        }

        if (outfile) {
            cerr << "Saved to \"" << outfile << "\"\n";
            static_cast<ofstream*>(output)->close();
            delete output;
        }
    }
    catch (exception &e) {
        cerr << "\033[1;31mERROR\033[0m " << e.what() << endl;
        return 1;
    }

    return 0;
}
