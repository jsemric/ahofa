/// @author Jakub Semric
/// 2017

#pragma GCC diagnostic ignored "-Wunused-parameter"

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

#define db(x) cerr << x << "\n"

using namespace reduction;
using namespace std;

namespace fs = boost::filesystem;

class StopWork : public exception {};

struct Data {
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
"Usage: ./nfa_handler [COMMAND] [OPTIONS]\n"
"General options:\n"
"  -h            : show this help and exit\n"
"  -o <FILE>     : specify output file\n"
"  -n <NWORKERS> : number of workers to run in parallel\n"
"  -f <FILTER>   : define BPF filter, for syntax see man page\n"
"\nCommands: {error, reduce, label}\n"
"\nError Computing\n"
"Usage: ./nfa_handlerler error [OPTIONS] TARGET REDUCED PCAPS ...\n"
"Compute an error between 2 NFAs, denoted as TARGET and REDUCED.\n"
"TARGET is supposed to be the input NFA and REDUCED is supposed to be an\n"
"over-approximation of TARGET. PCAP stands for packet capture file.\n"
"\nAutomaton Reduction\n"
"Usage: ./nfa_hanler reduce [OPTIONS] NFA [MORE]\n"
"Reduce the automaton using one of the following approaches: state pruning or\n"
"GA (Genetic Algorithm).\n"
"additional options:\n"
"  -e <N>        : specify error, default value is 0.01\n"
"  -r <N>        : reduce to %, this discards -e option\n"
"  -t <TYPE>     : specify the reduction type {prune, ga, armc}.\n"
"                  In case of prune and ga reduction the MORE argument is \n"
"                  a file with the labeled states of the input automaton.\n"
"                  Otherwise, it stands for pcap capture files.\n"
"\nState Labeling\n"
"Usage: ./nfa_hanler label NFA PCAPS ...\n"
"Argument NFA is the input automaton. PCAPS stands for packets used for state\n"
"labeling. This command supplies no additional options.\n\n";


// general program options
string cmd = "";
unsigned nworkers = 1;
const char *filter_expr;

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
    if (reduction_type == "prune") {
        auto labels = read_state_labels(nfa, args[0]);
        prune(nfa, labels, reduce_ratio, eps);
    }
    else if (reduction_type == "armc") {
        // each state marked with prefix
        vector<set<size_t>> state_labeling(nfa.state_count());
        // we distinguish the prefixes by some integral value
        size_t prefix = 0;
        pcapreader::process_payload(
            args[0].c_str(),
            [&] (const unsigned char *payload, unsigned len)
            {
                nfa.parse_word(payload, len,
                    [&state_labeling, &prefix](State s)
                    {
                        state_labeling[s].insert(prefix);
                    },
                    [&prefix]() {prefix++;});
            }, filter_expr);
        auto res = armc(nfa, state_labeling);
        for (auto i : res) {
            for (auto j : i) {
                cout << state_map[j] << " ";
            }
            cout << endl;
        }
    }
    else if (reduction_type == "a") {
        Data data(nfa.state_count());
        // generate random strings
        srand(time(0));
        for (size_t i = 0; i < 10000; i++) {
            unsigned char word[1000];
            for (size_t j = 0; j < 1000; j++) {
                word[j] = rand() % 256;
            }
            label_states(data, word, 1000);
        }

        map<State, size_t> labels;
        for (size_t i = 0; i < data.nfa1_data.size(); i++) {
            labels[state_map[i]] = data.nfa1_data[i];
        }

        prune(nfa, labels, reduce_ratio, eps);
    }
    else if (reduction_type == "ga") {
        // TODO or not
    }
}

void compute_error(Data &data, const unsigned char *payload, unsigned plen)
{
    vector<bool> bm(reduced.state_count());
    reduced.parse_word(payload, plen, [&bm](State s){ bm[s] = 1; });
    int match1 = 0;
    for (size_t i = 0; i < final_state_idx1.size(); i++) {
        size_t idx = final_state_idx1[i];
        if (bm[idx]) {
            match1++;
            data.nfa1_data[idx]++;
        }
    }

    if (match1) {
        data.accepted_reduced++;
        int match2 = 0;
        // something was matched, lets find the difference
        vector<bool> bm(target.state_count());
        target.parse_word(payload, plen, [&bm](State s){ bm[s] = 1; });
        for (size_t i = 0; i < final_state_idx2.size(); i++) {
            size_t idx = final_state_idx2[i];
            if (bm[idx]) {
                match2++;
                data.nfa2_data[idx]++;
            }
        }

        if (match1 != match2) {
            data.wrongly_classified++;
        }
        else {
            data.correctly_classified++;
        }

        if (match2) data.accepted_target++;
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

void write_output(ostream &out, const vector<string> &pcaps)
{
    unsigned msec = chrono::duration_cast<chrono::microseconds>(
        chrono::steady_clock::now() - timepoint).count();
    unsigned sec = msec / 1000 / 1000;
    unsigned min = sec / 60;

    if (cmd == "label") {
        out << "# Total packets : " << all_data.total << endl;

        out << "# Elapsed time  : " << min << "m/" << sec % 60  << "s/"
            << msec % 1000 << "ms\n";

        auto state_map = nfa.get_reversed_state_map();
        //auto state_depth = target.get_states_depth();
        for (unsigned long i = 0; i < target.state_count(); i++) {
            out << state_map[i] << " " << all_data.nfa1_data[i] << "\n";
        }
    }
    else if (cmd == "error") {
        size_t cls1 = 0, cls2 = 0;
        for (auto i : all_data.nfa1_data) {
            cls1 += i;
        }
        for (auto i : all_data.nfa2_data) {
            cls2 += i;
        }
        size_t wrong_acceptances = all_data.accepted_reduced -
            all_data.accepted_target;

        float pe = wrong_acceptances * 1.0 / all_data.total;
        float ce = all_data.wrongly_classified * 1.0 / all_data.total;
        float ace = (cls1 - cls2) * 1.0 / all_data.total;
        float cls_ratio = all_data.correctly_classified * 1.0 /
            (all_data.correctly_classified + all_data.wrongly_classified);
        unsigned long sc1 = target.state_count();
        unsigned long sc2 = reduced.state_count();

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
        out << "    \"total packets\": " << all_data.total << ",\n";
        out << "    \"accepted by target\": " << all_data.accepted_target
            << ",\n";
        out << "    \"accepted by reduced\": " << all_data.accepted_reduced
            << ",\n";
        out << "    \"wrongly acceptances\": " << wrong_acceptances << ",\n";
        out << "    \"reduced classifications\":" << cls1 << ",\n";
        out << "    \"target classifications\": " << cls2 << ",\n";
        out << "    \"wrong packet classifications\": "
            << all_data.wrongly_classified << ",\n";
        out << "    \"correct packet classifications\":"
            << all_data.correctly_classified << ",\n";
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
                << all_data.nfa1_data[s];
            if (i == final_state_idx1.size() - 1) {
                out << "}";
            }
            out << ",\n";
        }

        out << "    \"target rules\": {\n";
        for (size_t i = 0; i < final_state_idx2.size(); i++) {
            State s = final_state_idx2[i];
            out << "        \"q" << state_map2[s]  << "\" : "
                << all_data.nfa2_data[s];
            if (i == final_state_idx2.size() - 1) {
                out << "\n    }";
            }
            out << ",\n";
        }

        out << "    \"pcaps\": [\n";
        for (size_t i = 0; i < pcaps.size(); i++) {
            out << "        \"" << pcaps[i] << "\"";
            if (i == pcaps.size() - 1) {
                out << "]";
            }
            out << ",\n";
        }
        out << "    \"elapsed time\": \"" << min << "m/"
            << sec % 60  << "s/" << msec % 1000 << "ms\"\n";
        out << "}\n";
    }
    else if (cmd == "reduce" && reduction_type != "armc") {
        cerr << "Elapsed time: " << min << "m/" << sec % 60  << "s/"
            << msec % 1000 << "ms\n";
        nfa.print(out);
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
    int opt_cnt = 2;    // program name + command
    int c;
    bool reduce_options_set = false;

    try {
        if (argc > 1) {
            cmd = argv[1];
            if (cmd == "--help" || cmd == "-h") {
                cerr << helpstr;
                return 0;
            }
            // check if command is valid
            if (cmd != "error" && cmd != "reduce" && cmd != "label") {
                throw runtime_error("invalid command: \"" + cmd + "\"");
            }
        }
        else {
            cerr << helpstr;
            return 1;
        }

        while ((c = getopt(argc, argv, "ho:n:axf:e:r:t:")) != -1) {
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
                // reduction additional options
                case 'e':
                    opt_cnt++;
                    reduce_options_set = 1;
                    eps = stod(optarg);
                    check_float(eps);
                    break;
                case 'r':
                    opt_cnt++;
                    reduce_options_set = 1;
                    reduce_ratio = stod(optarg);
                    check_float(reduce_ratio);
                    break;
                case 't':
                    opt_cnt++;
                    reduce_options_set = 1;
                    reduction_type = optarg;
                    break;
                default:
                    return 1;
            }
        }
        // resolve conflict options
        if ((cmd == "error" && reduce_options_set) ||
            (cmd == "label" && reduce_options_set))
        {
            throw runtime_error("invalid combinations of arguments");
        }

        if (nworkers <= 0 ||
            nworkers >= thread::hardware_concurrency())
        {
            throw runtime_error(
                "invalid number of cores \"" + to_string(nworkers) + "\"");
        }

        // checking the number of positional arguments
        int min_pos_cnt = cmd == "error" ? 3 : 2;

        if (argc - opt_cnt < min_pos_cnt)
        {
            throw runtime_error("invalid positional arguments");
        }

        /*
        for (int i = opt_cnt; i <= argc; i++) {
            cout << argv[i] << "\n";
        }
        */

        // get automata
        nfa_str1 = argv[opt_cnt];
        target.read_from_file(nfa_str1.c_str());
        size_t size1 = nfa.state_count(), size2 = 1;
        state_map = nfa.get_reversed_state_map();
        // do some preparation for commands `label` and `error`
        if (cmd == "error") {
            nfa_str2 = argv[opt_cnt + 1];
            reduced.read_from_file(nfa_str2.c_str());
            final_state_idx1 = reduced.get_final_state_idx();
            final_state_idx2 = target.get_final_state_idx();
            state_map1 = reduced.get_reversed_state_map();
            state_map2 = target.get_reversed_state_map();
            size2 = target.state_count();
            opt_cnt++;
        }
        else if (cmd == "label") {
            // TODO
        }

        // get capture files
        for (int i = opt_cnt + 1/*2 - accepted_only*/; i < argc; i++) {
            pcaps.push_back(argv[i]);
            //cerr << argv[i] << "\n";
        }

        // check output file
        ostream *output = &cout;
        if (outfile) {
            output = new ofstream{outfile};
            if (!static_cast<ofstream*>(output)->is_open()) {
                throw runtime_error("cannot open output file");
            }
        }

        // divide work
        vector<vector<string>> v(nworkers);
        for (unsigned i = 0; i < pcaps.size(); i++) {
            v[i % nworkers].push_back(pcaps[i]);
        }

        // start computation
        if (cmd == "reduce") {
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

                            if (cmd == "label") {
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

        write_output(*output, pcaps);

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
