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
#include <ctype.h>
#include <getopt.h>

#include <boost/filesystem.hpp>

#include "nfa.hpp"
#include "pcap_reader.hpp"
#include "reduction.hpp"

#define db(x) std::cerr << x << "\n"

using namespace reduction;

namespace fs = boost::filesystem;

struct Data {
    // long data
    std::vector<size_t> vector_data1;
    std::vector<size_t> vector_data2;
    // packets statistics
    size_t total;
    size_t accepted_reduced;
    size_t accepted_target;
    size_t wrongly_classified;

    Data(size_t data_size1 = 1, size_t data_size2 = 1) :
        vector_data1(data_size1), vector_data2(data_size2), total{0},
        accepted_reduced{0}, accepted_target{0}, wrongly_classified{0} {}
    ~Data() = default;

    void aggregate(const Data &other_data) {
        total += other_data.total;
        accepted_target += other_data.accepted_target;
        accepted_reduced += other_data.accepted_reduced;
        wrongly_classified += other_data.wrongly_classified;
        for (size_t i = 0; i < other_data.vector_data1.size(); i++) {
            vector_data1[i] += other_data.vector_data1[i];
        }
        for (size_t i = 0; i < other_data.vector_data2.size(); i++) {
            vector_data2[i] += other_data.vector_data2[i];
        }
    }
};

const char *helpstr =
"Usage: ./nfa_handler [COMMAND] [OPTIONS]\n"
"General options:\n"
"  -h            : show this help and exit\n"
"  -o <FILE>     : specify output file\n"
"  -n <NWORKERS> : number of workers to run in parallel\n"
"  -f <FILTER>   : define bpf filter, for syntax see man page\n"
"\nCommands:\n\n"
"Error Computing:\n"
"Usage: ./nfa_handlerler error [OPTIONS] TARGET [REDUCED] PCAPS ...\n"
"Compute an error between 2 NFAs, label NFA or reduce the NFA.\n"
"TARGET is supposed to be NFA and REDUCED is supposed to be an\n"
"over-approximation of TARGET. PCAP stands for packet capture file.\n"
"\nAutomaton Reduction:\n"
"Usage: ./nfa_hanler reduce [OPTIONS] NFA [PCAPS | NFA_STATES]\n"
"additional options:\n"
"  -e <N>        : specify error, default value is 0.01\n"
"  -r <N>        : reduce to %, this discards -e option\n"
"  -t <TYPE>     : specify reduction type, possible choices: prune, ga, armc\n"
"\nState Labeling\n"
"Usage: ./nfa_hanler label NFA PCAPS ...\n"
"Some description. Has no additional options\n";


// general program options
std::string cmd = "";
unsigned nworkers = 1;
const char *filter_expr;

// reduction additional options
std::string reduction_type = "prune";
float eps = -1;
float reduce_ratio = 0.01;

// thread communication
bool continue_work = true;
std::mutex mux;

// common data
Data all_data;
FastNfa target, reduced;
FastNfa &nfa = target;
std::string nfa_str1, nfa_str2;

// error computing data
std::vector<size_t> final_state_idx1;
std::vector<size_t> final_state_idx2;
// maps state to state string name
std::map<State,State> state_map1;
std::map<State,State> state_map2;

// time statistics
std::chrono::steady_clock::time_point timepoint;

// gather results
void sum_up(const Data &data)
{
    mux.lock();
    all_data.aggregate(data);
    mux.unlock();
}

void sighandl(int signal)
{
    std::cout << "\n";
    // stop all work
    continue_work = false;
}

std::map<State, unsigned long> read_state_labels(
    const Nfa &nfa, const std::string &fname)
{
    std::map<State, unsigned long> ret;
    std::ifstream in{fname};
    if (!in.is_open()) {
        throw std::runtime_error("error loading NFA");
    }

    std::string buf;
    while (std::getline(in, buf)) {
        // remove '#' comment
        buf = buf.substr(0, buf.find("#"));
        if (buf == "") {
            continue;
        }
        std::istringstream iss(buf);
        State s;
        unsigned long l;
        if (!(iss >> s >> l)) {
            throw std::runtime_error("invalid state labels syntax");
        }
        if (!nfa.is_state(s)) {
            throw std::runtime_error("invalid NFA state: " + std::to_string(s));
        }
        ret[s] = l;
    }
    in.close();
    return ret;
}


void reduce(const std::vector<std::string> &args)
{
    // TODO
    if (reduction_type == "prune") {
        auto labels = read_state_labels(nfa, args[0]);
        prune(nfa, labels, reduce_ratio, eps);
    }
    else {
        ;
    }
}

void compute_error(Data &data, const unsigned char *payload, unsigned plen)
{
    std::vector<bool> bm(reduced.state_count());
    reduced.parse_word(payload, plen, [&bm](State s){ bm[s] = 1; });
    int match1 = 0;
    for (size_t i = 0; i < final_state_idx1.size(); i++) {
        size_t idx = final_state_idx1[i];
        if (bm[idx]) {
            match1++;
            data.vector_data1[idx]++;
        }
    }

    if (match1) {
        data.accepted_reduced++;
        int match2 = 0;
        // something was matched, lets find the difference
        std::vector<bool> bm(target.state_count());
        target.parse_word(payload, plen, [&bm](State s){ bm[s] = 1; });
        for (size_t i = 0; i < final_state_idx2.size(); i++) {
            size_t idx = final_state_idx2[i];
            if (bm[idx]) {
                match2++;
                data.vector_data2[idx]++;
            }
        }

        if (match1 != match2) data.wrongly_classified++;
        
        if (match2) data.accepted_target++;
    }
}

void label_states(
     Data &reached_states, const unsigned char *payload, unsigned plen)
{
    std::vector<bool> bm(nfa.state_count());
    nfa.parse_word(payload, plen, [&bm](State s){ bm[s] = 1; });
    for (size_t i = 0; i < reached_states.vector_data1.size(); i++) {
        reached_states.vector_data1[i] += bm[i];
    }
}

template<typename Handler>
void process_pcaps(
    const std::vector<std::string> &pcaps, Data &local_data, Handler handler)
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
                        throw std::exception();
                    }
                    // specific payload handler
                    local_data.total++;
                    handler(local_data, payload, len);

                }, filter_expr);
        }
        catch (std::ios_base::failure &e) {
            std::cerr << "\033[1;31mWarning: " << e.what() << "\033[0m\n";
            // process other capture files, continue for loop
        }
        catch (std::runtime_error &e) {
            std::cerr << "\033[1;31mWarning: " << e.what() << "\033[0m\n";
        }
        catch (std::exception &e) {
            // SIGINT or other error
            break;
        }
    }
    // sum up results
    sum_up(local_data);
}

void write_output(std::ostream &out, const std::vector<std::string> &pcaps)
{
    unsigned msec = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now() - timepoint).count();
    unsigned sec = msec / 1000 / 1000;
    unsigned min = sec / 60;

    if (cmd == "label") {
        out << "# Total packets : " << all_data.total << std::endl;
    
        out << "# Elapsed time  : " << min << "m/" << sec % 60  << "s/"
            << msec % 1000 << "ms\n";

        auto state_map = nfa.get_reversed_state_map();
        //auto state_depth = target.get_states_depth();
        for (unsigned long i = 0; i < target.state_count(); i++) {
            out << state_map[i] << " " << all_data.vector_data1[i] << "\n";
        }
    }
    else if (cmd == "error") {
        size_t matches1 = 0, matches2 = 0;
        for (auto i : all_data.vector_data1) {
            matches1 += i;
        }
        for (auto i : all_data.vector_data2) {
            matches2 += i;
        }
        size_t wrongly_classified =  all_data.accepted_reduced - 
            all_data.accepted_target;

        float err = wrongly_classified * 1.0 / all_data.total;
        float err2 = all_data.wrongly_classified * 1.0 / all_data.total;
        float mme = (matches1 - matches2) * 1.0 / all_data.total;
        unsigned long sc1 = target.state_count();
        unsigned long sc2 = reduced.state_count();

        out << "{\n";
        out << "    \"target file\"         : \"" << nfa_str1 << "\",\n";
        out << "    \"target\"              : \"" << fs::basename(nfa_str1)
            << "\",\n";
        out << "    \"target states\"       : " << sc1 << ",\n";
        out << "    \"reduced file\"        : \"" << nfa_str2 << "\",\n";
        out << "    \"reduced\"             : \"" << fs::basename(nfa_str2)
            << "\",\n";
        out << "    \"reduced states\"      : " << sc2 << ",\n";
        out << "    \"reduction\"           : " << 1.0 * sc2 / sc1
            << ",\n";
        out << "    \"total packets\"       : " << all_data.total << ",\n";
        out << "    \"accepted by target\"  : " << all_data.accepted_target
            << ",\n";
        out << "    \"accepted by reduced\" : " << all_data.accepted_reduced
            << ",\n";
        out << "    \"wrongly classified\"  : " << wrongly_classified << ",\n";
        out << "    \"packet error\"        : " << err << ",\n";
        out << "    \"reduced matches\"     : " << matches1 << ",\n";
        out << "    \"target matches\"      : " << matches2 << ",\n";
        out << "    \"mean match error\"    : " << mme << ",\n";
        out << "    \"wrong packet matches\": " << all_data.wrongly_classified 
            << ",\n";
        out << "    \"packet match error\"  : " << err2 << ",\n";
        // concrete rules results
        out << "    \"reduced rules\"       : {\n";
        for (size_t i = 0; i < final_state_idx1.size(); i++) {
            State s = final_state_idx1[i];
            out << "        \"q" << state_map1[s]  << "\" : "
                << all_data.vector_data1[s];
            if (i == final_state_idx1.size() - 1) {
                out << "}";
            }
            out << ",\n";
        }

        out << "    \"target rules\"       : {\n";
        for (size_t i = 0; i < final_state_idx2.size(); i++) {
            State s = final_state_idx2[i];
            out << "        \"q" << state_map2[s]  << "\" : "
                << all_data.vector_data2[s];
            if (i == final_state_idx2.size() - 1) {
                out << "\n    }";
            }
            out << ",\n";
        }
        
        out << "    \"pcaps\"               : [\n";
        for (size_t i = 0; i < pcaps.size(); i++) {
            out << "        \"" << pcaps[i] << "\"";
            if (i == pcaps.size() - 1) {
                out << "]";
            }
            out << ",\n";
        }
        out << "    \"elapsed time\"        : \"" << min << "m/"
            << sec % 60  << "s/" << msec % 1000 << "ms\"\n";
        out << "}\n";
    }
    else if (cmd == "reduce") {
        std::cerr << "Elapsed time : " << min << "m/" << sec % 60  << "s/"
            << msec % 1000 << "ms\n";
        nfa.print(out);
    }
}

void check_float(float x, float max_val = 1, float min_val = 0) 
{
    if (x > max_val || x < min_val) {
        throw std::runtime_error(
            "invalid float value: \"" + std::to_string(x) +
            "\", should be in range (" + std::to_string(min_val) + "," +
            std::to_string(max_val) + ")");
    }
}

int main(int argc, char **argv)
{
    timepoint = std::chrono::steady_clock::now();
    std::string ofname;
    std::vector<std::string> pcaps;

    const char *outfile = nullptr;
    int opt_cnt = 2;    // program name + command
    int c;
    bool reduce_options_set = false;

    try {
        if (argc > 1) {
            cmd = argv[1];
            if (cmd == "--help" || cmd == "-h") {
                std::cerr << helpstr;
                return 0;
            }
            // check if command is valid
            if (cmd != "error" && cmd != "reduce" && cmd != "label") {
                throw std::runtime_error("invalid command: \"" + cmd + "\"");
            }
        } 
        else {
            std::cerr << helpstr;
            return 1;
        }

        while ((c = getopt(argc, argv, "ho:n:axf:e:r:t:")) != -1) {
            opt_cnt++;
            switch (c) {
                // general options
                case 'h':
                    std::cerr << helpstr;
                    return 0;
                case 'o':
                    outfile = optarg;
                    opt_cnt++;
                    break;
                case 'n':
                    nworkers = std::stoi(optarg);
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
                    eps = std::stod(optarg);
                    check_float(eps);
                    break;
                case 'r':
                    opt_cnt++;
                    reduce_options_set = 1;
                    reduce_ratio = std::stod(optarg);
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
            throw std::runtime_error("invalid combinations of arguments");
        }

        if (nworkers <= 0 ||
            nworkers >= std::thread::hardware_concurrency())
        {
            throw std::runtime_error(
                "invalid number of cores \"" + std::to_string(nworkers) + "\"");
        }

        // checking the number of positional arguments
        int min_pos_cnt = cmd == "error" ? 3 : 2;

        if (argc - opt_cnt < min_pos_cnt)
        {
            throw std::runtime_error("invalid positional arguments");
        }

        /*
        for (int i = opt_cnt; i <= argc; i++) {
            std::cout << argv[i] << "\n";
        }
        */

        // get automata
        nfa_str1 = argv[opt_cnt];
        target.read_from_file(nfa_str1.c_str());
        size_t size1 = nfa.state_count(), size2 = 1;
        // do some preparation for commands `label` and `error`
        if (cmd == "error") {
            nfa_str2 = argv[opt_cnt + 1];
            reduced.read_from_file(nfa_str2.c_str());
            final_state_idx1 = reduced.get_final_state_idx();
            final_state_idx2 = target.get_final_state_idx();
            state_map1 = reduced.get_reversed_state_map();
            state_map2 = target.get_reversed_state_map();
            size1 = reduced.state_count();
            size2 = target.state_count();
            opt_cnt++;
        }
        else if (cmd == "label") {
            // TODO
        }

        // get capture files
        for (int i = opt_cnt + 1/*2 - accepted_only*/; i < argc; i++) {
            pcaps.push_back(argv[i]);
            //std::cerr << argv[i] << "\n";
        }

        // check output file
        std::ostream *output = &std::cout;
        if (outfile) {
            output = new std::ofstream{outfile};
            if (!static_cast<std::ofstream*>(output)->is_open()) {
                throw std::runtime_error("cannot open output file");
            }
        }

        // divide work
        std::vector<std::vector<std::string>> v(nworkers);
        for (unsigned i = 0; i < pcaps.size(); i++) {
            v[i % nworkers].push_back(pcaps[i]);
        }

        // start computation
        if (cmd == "reduce") {
            reduce(pcaps);
        }
        else {
            // signal handling
            std::signal(SIGINT, sighandl);
            all_data = Data(size1, size2);

            std::vector<std::thread> threads;
            for (unsigned i = 0; i < nworkers; i++) {
                threads.push_back(std::thread{[&v, i, size1, size2]()
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
            std::cerr << "Saved to \"" << outfile << "\"\n";
            static_cast<std::ofstream*>(output)->close();
            delete output;
        }
    }
    catch (std::exception &e) {
        std::cerr << "\033[1;31mError:\033[0m " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
