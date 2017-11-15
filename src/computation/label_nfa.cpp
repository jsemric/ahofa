#pragma GCC diagnostic ignored "-Wunused-parameter"

#include <iostream>
#include <algorithm>
#include <ostream>
#include <exception>
#include <vector>
#include <csignal>
#include <stdexcept>
#include <ctime>
#include <mutex>
#include <thread>
#include <chrono>
#include <ctype.h>
#include <getopt.h>

#include <boost/filesystem.hpp>

#include "nfa.h"
#include "pcap_reader.h"

#define db(x) std::cerr << x << "\n"

namespace fs = boost::filesystem;

const char *helpstr =
"Usage: ./label_nfa [OPTIONS] NFA PCAP ...\n"
"Label NFA states with samples from PCAP files.\n"
"options:\n"
"  -h            : show this help and exit\n"
"  -o <FILE>     : specify output file\n"
"  -n <NWORKERS> : number of workers to run in parallel\n"
"  -f <FILTER>   : define bpf filter, for syntax see man page\n";

// program options
unsigned nworkers = 1;
std::string nfa_str;
const char *filter_expr;
// thread communication
bool continue_work = true;
std::mutex mux;
std::vector<unsigned long> state_labels;
// time statistics
std::chrono::steady_clock::time_point timepoint;

// gather results
void sum_up(const std::vector<unsigned long> &data)
{{{
    static bool first = true;
    mux.lock();
    if (first) {
        state_labels = std::vector<unsigned long>(data);
        first = false;
    }
    else {
        for (size_t i = 0; i < state_labels.size(); i++) {
            state_labels[i] += data[i];
        }
    }
    mux.unlock();
}}}

void sighandl(int signal)
{{{
    std::cout << "\n";
    // stop all work
    continue_work = false;
}}}

void label_nfa(const NFA &nfa, const std::vector<std::string> &pcaps)
{{{
    std::vector<unsigned long> data(nfa.state_count());
    std::vector<bool> labeled;
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
                    nfa.label_states(payload, len, labeled);
                    for (size_t i = 0; i < data.size(); i++) {
                        data[i] += labeled[i];
                    }
                }, filter_expr);
        }
        catch (std::ios_base::failure &e) {
            std::cerr << "\033[1;31mWarning: " << e.what() << "\033[0m\n";
        }
        catch (std::runtime_error &e) {
            std::cerr << "\033[1;31mWarning:\033[0m " << e.what() << "\n";
            // process other capture files
        }
        catch (std::exception &e) {
            break;
        }
    }
    // sum up results
    sum_up(data);
}}}

void write_output(std::ostream &out, const NFA &nfa)
{{{
    auto total = std::max_element(state_labels.begin(), state_labels.end());
    out << "# Total packets : " << *total << std::endl;
    unsigned msec = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now() - timepoint).count();
    unsigned sec = msec / 1000 / 1000;
    unsigned min = sec / 60;
    out << "# Elapsed time  : " << min << "m/" << sec % 60  << "s/"
        << msec % 1000 << "ms\n";

    auto state_map = nfa.get_rmap();
    auto state_depth = nfa.get_states_depth();
    for (unsigned long i = 0; i < nfa.state_count(); i++) {
        out << state_map[i] << " " << state_labels[i] << " "
            << state_depth[i] << "\n";
    }
}}}

int main(int argc, char **argv)
{{{

    timepoint = std::chrono::steady_clock::now();
    std::string ofname;
    std::vector<std::string> pcaps;

    const char *outfile = nullptr;
    int opt_cnt = 1;
    int c;
    while ((c = getopt(argc, argv, "ho:n:axf:")) != -1) {
        opt_cnt++;
        switch (c) {
            case 'h':
                fprintf(stderr, "%s", helpstr);
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
            default:
                return 1;
        }
    }

    NFA nfa;
    try {
        if (nworkers <= 0 ||
            nworkers >= std::thread::hardware_concurrency())
        {
            throw std::runtime_error(
                "invalid number of cores \"" + std::to_string(nworkers) + "\"");
        }

        // checking a number of arguments (min NFA and PCAP)
        if (argc - opt_cnt < 2)
        {
            throw std::runtime_error("invalid arguments");
        }

        // get automata
        nfa_str = argv[opt_cnt];
        nfa.read_from_file(nfa_str.c_str());

        // get capture files
        for (int i = opt_cnt + 1; i < argc; i++) {
            pcaps.push_back(argv[i]);
        }

        // check output file
        std::ostream *output = &std::cout;
        if (outfile) {
            output = new std::ofstream{outfile};
            if (!static_cast<std::ofstream*>(output)->is_open()) {
                throw std::runtime_error("cannot open output file");
            }
        }

        // signal handling
        std::signal(SIGINT, sighandl);

        // divide work
        std::vector<std::vector<std::string>> v(nworkers);
        for (unsigned i = 0; i < pcaps.size(); i++) {
            v[i % nworkers].push_back(pcaps[i]);
        }

        std::vector<std::thread> threads;
        // start computation
        for (unsigned i = 0; i < nworkers; i++) {
            threads.push_back(std::thread{[&nfa, &v, i]()
                    {
                        label_nfa(nfa, v[i]);
                    }
                });
        }

        for (unsigned i = 0; i < nworkers; i++) {
            if (threads[i].joinable()) {
                threads[i].join();
            }
        }

        write_output(*output, nfa);

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
}}}
