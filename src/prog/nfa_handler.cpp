/// @author Jakub Semric
/// 2018

#include <iostream>
#include <ostream>
#include <sstream>
#include <exception>
#include <vector>
#include <map>
#include <csignal>
#include <stdexcept>
#include <ctime>
#include <mutex>
#include <thread>
#include <chrono>
#include <array>
#include <ctype.h>
#include <getopt.h>

#include <boost/filesystem.hpp>

#include "nfa.hpp"
#include "pcap_reader.hpp"
#include "reduction.hpp"

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

    Data(size_t data_size1 = 1, size_t data_size2 = 1) :
        nfa1_data(data_size1), nfa2_data(data_size2), total{0},
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

const char *helpstr =
"The program provides several operations with automata. Error computing,\n"
"state labeling and reduction. The error computing is set by default, \n"
"positional arguments are TARGET REDUCED PCAPS ..., where TARGET is an input\n"
"NFA, REDUCED denotes over-approximated reduction of the TARGET and\n"
"PCAPS are packet capture files.\n"
"Usage: ./nfa_handler [OPTIONS] FILE1 FILE2 ...\n"
"options:\n"
"  -h            : show this help and exit\n"
"  -o <FILE>     : specify output file or directory for -s option\n"
"  -n <NWORKERS> : number of workers to run in parallel\n"
"  -f <FILTER>   : define BPF filter, for syntax see man page\n"
"  -j            : verbose output in JSON\n"
"  -c            : Rigorous error computation. Consistent but much slower.\n"
"                  Use only if not sure about over-approximation\n"
"  -s            : store results to a separate file per each packet capture\n"
"                  file\n"
"  -l            : label NFA states with traffic, positional arguments are \n"
"                  NFA PCAPS ...\n"
"  -r            : NFA reduction, positional arguments are NFA FREQFILE\n"
"  -t <TYPE>     : reduction types {prune,merge}\n"
"  -e <N>        : specify reduction max. error, default value is 0.01\n"
"  -p <N>        : reduce to %, this discards -e option, N i must be within\n"
"                  interval (0,1)\n";

// general program options
unsigned nworkers = 1;
const char *filter_expr;
bool error_opt = false;
bool label_opt = false;
bool reduce_opt = false;
bool store_sep = false;
bool to_json = false;
bool consistent = false;
string outdir = "data/prune-error";

// reduction additional options
string reduction_type = "prune";
float eps = -1;
float reduce_ratio = 0.01;

// thread communication
bool continue_work = true;
mutex mux;

// common data
Data all_data;
FastNfa target, reduced;
FastNfa &nfa = target;
string nfa_str1, nfa_str2;

// error computing data
vector<size_t> final_state_idx1;
vector<size_t> final_state_idx2;
// maps state to state string name
map<State,State> state_map1;
map<State,State> state_map2;
auto &state_map = state_map2;

// time statistics
chrono::steady_clock::time_point timepoint;

// function declarations
void label_states(
     Data &reached_states, const unsigned char *payload, unsigned plen);

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

map<State, unsigned long> read_state_labels(
    const Nfa &nfa, const string &fname)
{
    map<State, unsigned long> ret;
    ifstream in{fname};
    if (!in.is_open()) {
        throw runtime_error("error loading NFA");
    }

    string buf;
    while (getline(in, buf)) {
        // remove '#' comment
        buf = buf.substr(0, buf.find("#"));
        if (buf == "") {
            continue;
        }
        istringstream iss(buf);
        State s;
        unsigned long l;
        if (!(iss >> s >> l)) {
            throw runtime_error("invalid state labels syntax");
        }
        if (!nfa.is_state(s)) {
            throw runtime_error("invalid NFA state: " + to_string(s));
        }
        ret[s] = l;
    }
    in.close();
    return ret;
}

void reduce(const vector<string> &args)
{
    auto labels = read_state_labels(nfa, args[0]);
    auto old_sc = nfa.state_count();
    float error;

    if (reduction_type == "prune")
    {
        error = prune(nfa, labels, reduce_ratio, eps);
    }
    else if (reduction_type == "merge")
    {
        error = merge_and_prune(nfa, labels, reduce_ratio);
    }


    auto new_sc = nfa.state_count();

    cerr << "Reduction: " << new_sc << "/" << old_sc
        << " " << 100 * new_sc / old_sc << "%\n";
    cerr << "Predicted error: " << error << endl;
}

void compute_error(Data &data, const unsigned char *payload, unsigned plen)
{
    vector<bool> bm(reduced.state_count());
    reduced.parse_word(payload, plen, [&bm](State s){ bm[s] = 1; });
    int match1 = 0;

    for (size_t i = 0; i < final_state_idx1.size(); i++)
    {
        size_t idx = final_state_idx1[i];
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
        for (size_t i = 0; i < final_state_idx2.size(); i++)
        {
            size_t idx = final_state_idx2[i];
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

void label_states(
     Data &reached_states, const unsigned char *payload, unsigned plen)
{
    vector<bool> bm(nfa.state_count());
    nfa.parse_word(payload, plen, [&bm](State s){ bm[s] = 1; });
    for (size_t i = 0; i < reached_states.nfa1_data.size(); i++) {
        reached_states.nfa1_data[i] += bm[i];
    }
    reached_states.nfa1_data[nfa.get_initial_state_idx()]++;
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

template<typename Handler>
void process_pcaps(
    const vector<string> &pcaps, Data &local_data, Handler handler)
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
                    handler(local_data, payload, len);

                }, filter_expr);

            if (store_sep && error_opt) {
                auto fname = gen_output_name(nfa_str1);
                ofstream out(fname);
                if (out.is_open()) {
                    write_error_data(out, local_data, pcaps[i]);
                    out.close();
                }
                else {
                    throw ofstream::failure(
                        "cannot open result file: " + fname);
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
    float ace = (cls1 - cls2) * 1.0 / data.total;
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
    out << "    \"reduction\": " << 1.0 * sc2 / sc1
        << ",\n";
    out << "    \"total packets\": " << data.total << ",\n";
    out << "    \"accepted by target\": " << data.accepted_target
        << ",\n";
    out << "    \"accepted by reduced\": " << data.accepted_reduced
        << ",\n";
    out << "    \"wrong acceptances\": " << wrong_acceptances << ",\n";
    out << "    \"reduced classifications\":" << cls1 << ",\n";
    out << "    \"target classifications\": " << cls2 << ",\n";
    out << "    \"wrong packet classifications\": "
        << data.wrongly_classified << ",\n";
    out << "    \"correct packet classifications\":"
        << data.correctly_classified << ",\n";
    out << "    \"correct packet classifications rate\":"
        << cls_ratio << ",\n";
    out << "    \"ace\": " << ace << ",\n";
    out << "    \"ce\": " << ce << ",\n";
    out << "    \"pe\": " << pe << ",\n";
    // concrete rules results
    out << "    \"reduced rules\": {\n";
    for (size_t i = 0; i < final_state_idx1.size(); i++) {
        State s = final_state_idx1[i];
        out << "        \"q" << state_map1[s]  << "\" : "
            << data.nfa1_data[s];
        if (i == final_state_idx1.size() - 1) {
            out << "}";
        }
        out << ",\n";
    }

    out << "    \"target rules\": {\n";
    for (size_t i = 0; i < final_state_idx2.size(); i++) {
        State s = final_state_idx2[i];
        out << "        \"q" << state_map2[s]  << "\" : "
            << data.nfa2_data[s];
        if (i == final_state_idx2.size() - 1) {
            out << "\n    }";
        }
        out << ",\n";
    }

    if (pcapname != "")
    {
        out << "    \"pcap\" : \"" << pcapname << "\",\n";
    }
    out << "    \"elapsed time\": \"" << min << "m/"
        << sec % 60  << "s/" << msec % 1000 << "ms\"\n";
    out << "}\n";
}

void write_output(ostream &out)
{
    unsigned msec = chrono::duration_cast<chrono::microseconds>(
        chrono::steady_clock::now() - timepoint).count();
    unsigned sec = msec / 1000 / 1000;
    unsigned min = sec / 60;

    if (label_opt)
    {
        out << "# Total packets : " << all_data.total << endl;

        out << "# Elapsed time  : " << min << "m/" << sec % 60  << "s/"
            << msec % 1000 << "ms\n";

        auto state_map = nfa.get_reversed_state_map();
        auto state_depth = target.state_depth();
        for (unsigned long i = 0; i < target.state_count(); i++)
        {
            out << state_map[i] << " " << all_data.nfa1_data[i] << " "
                << state_depth[state_map[i]] << "\n";
        }
    }
    else if (reduce_opt)
    {
        cerr << "Elapsed time: " << min << "m/" << sec % 60  << "s/"
            << msec % 1000 << "ms\n";
        nfa.print(out);
    }
    else {
        if (!store_sep)
            write_error_data(out, all_data, "");
    }
}

void check_float(float x, float max_val = 1, float min_val = 0)
{
    if (x > max_val || x < min_val) {
        throw runtime_error(
            "invalid float value: \"" + to_string(x) +
            "\", should be in range (" + to_string(min_val) + "," +
            to_string(max_val) + ")");
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

        while ((c = getopt(argc, argv, "ho:n:f:rle:p:t:sjc")) != -1) {
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
                case 'r':
                    reduce_opt = true;
                    break;
                case 'l':
                    label_opt = true;
                    break;
                case 'j':
                    to_json = true;
                    break;
                case 'c':
                    consistent = true;
                    break;
                // reduction additional options
                case 'e':
                    opt_cnt++;
                    eps = stod(optarg);
                    check_float(eps);
                    break;
                case 'p':
                    opt_cnt++;
                    reduce_ratio = stod(optarg);
                    check_float(reduce_ratio);
                    break;
                case 't':
                    opt_cnt++;
                    reduction_type = optarg;
                    break;
                case 's':
                    store_sep = true;
                    break;
                default:
                    return 1;
            }
        }

        // resolve conflict options
        if (reduce_opt && label_opt)
        {
            throw runtime_error("invalid combinations of arguments");
        }

        error_opt = !reduce_opt && !label_opt;

        if (nworkers <= 0 ||
            nworkers >= thread::hardware_concurrency())
        {
            throw runtime_error(
                "invalid number of cores \"" + to_string(nworkers) + "\"");
        }

        // checking the min. number of positional arguments
        int min_pos_cnt = error_opt ? 3 : 2;

        if (argc - opt_cnt < min_pos_cnt)
        {
            throw runtime_error("invalid positional arguments");
        }

        // get automata
        nfa_str1 = argv[opt_cnt];
        target.read_from_file(nfa_str1.c_str());
        size_t size1 = nfa.state_count(), size2 = 1;
        state_map = nfa.get_reversed_state_map();
        // do some preparation for commands `label` and `error`
        if (error_opt) {
            nfa_str2 = argv[opt_cnt + 1];
            reduced.read_from_file(nfa_str2.c_str());
            final_state_idx1 = reduced.get_final_state_idx();
            final_state_idx2 = target.get_final_state_idx();
            state_map1 = reduced.get_reversed_state_map();
            state_map2 = target.get_reversed_state_map();
            size2 = target.state_count();
            opt_cnt++;
        }

        // get capture files
        for (int i = opt_cnt + 1/*2 - accepted_only*/; i < argc; i++) {
            pcaps.push_back(argv[i]);
            //cerr << argv[i] << "\n";
        }

        // check output file
        ostream *output = &cout;
        if (outfile) {
            if (store_sep) {
                outdir = outfile;
                if (!fs::is_directory(outfile)) {
                    throw runtime_error("invalid directory");
                }
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
        if (reduce_opt) {
            reduce(pcaps);
        }
        else {
            // signal handling
            signal(SIGINT, sighandl);
            all_data = Data(size1, size2);

            vector<thread> threads;
            for (unsigned i = 0; i < nworkers; i++) {
                threads.push_back(thread{[&v, i, size1, size2]()
                        {
                            Data local_data(size1, size2);

                            if (label_opt) {
                                process_pcaps(v[i], local_data, label_states);
                            }
                            else {
                                process_pcaps(v[i], local_data, compute_error);
                            }
                        }
                    });
            }

            for (unsigned i = 0; i < nworkers; i++) {
                if (threads[i].joinable()) {
                    threads[i].join();
                }
            }
        }

        write_output(*output);

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
