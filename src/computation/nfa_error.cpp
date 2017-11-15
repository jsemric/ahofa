#pragma GCC diagnostic ignored "-Wunused-parameter"

#include <iostream>
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
"Usage: ./nfa_error [OPTIONS] TARGET [REDUCED] PCAP ...\n"
"Compute an error between 2 NFAs.\n"
"TARGET is supposed to be NFA and REDUCED is supposed to be an\n"
"over-approximation of TARGET. PCAP stands for packet capture file.\n\n"
"options:\n"
"  -h            : show this help and exit\n"
"  -o <FILE>     : specify output file\n"
"  -a            : compute only accepted packets by TARGET\n"
"  -n <NWORKERS> : number of workers to run in parallel\n"
"  -x            : slower but checks if REDUCED is really over-approximation\n"
"  -j            : stores the result in JSON format\n"
"                  of TARGET\n"
"  -f <FILTER>   : define bpf filter, for syntax see man page\n";

// program options
unsigned nworkers = 1;
bool accepted_only = false;
bool tojson = false;
const char *filter_expr;
// thread communication
bool continue_work = true;
std::mutex mux;
// data
NFA reduced, target;
std::string nfa_str1, nfa_str2;
unsigned total_packets = 0;
unsigned accepted_target = 0;
unsigned accepted_reduced = 0;
unsigned wrongly_classified = 0;
// time statistics
std::chrono::steady_clock::time_point timepoint;

// gather results
void sum_up(
    unsigned total, unsigned acc_target,
    unsigned acc_reduced = 0, unsigned different = 0)
{{{
    mux.lock();
    total_packets += total;
    accepted_target += acc_target;
    accepted_reduced += acc_reduced;
    wrongly_classified += different;
    mux.unlock();
}}}

void sighandl(int signal)
{{{
    std::cout << "\n";
    // stop all work
    continue_work = false;
}}}

void compute_accepted(const NFA &nfa, const std::vector<std::string> &pcaps)
{{{
    unsigned total = 0, accepted = 0;
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
                    total++;
                    accepted += nfa.accept(payload, len);
                }, filter_expr);
        }
        catch (std::ios_base::failure &e) {
            std::cerr << "\033[1;31mWarning: " << e.what() << "\033[0m\n";
        }
        catch (std::runtime_error &e) {
            std::cerr << "\033[1;31mWarning:\033[0m " << e.what() << "\n";
            // process other capture files, contunue for loop
        }
        catch (std::exception &e) {
            // SIGINT or other error
            break;
        }
    }
    // sum up results
    sum_up(total, accepted);
}}}

void compute_error(
    const NFA &target,
    const NFA &reduced,
    const std::vector<std::string> &pcaps,
    bool fast = true)
{{{
    unsigned total = 0, acc_target = 0, acc_reduced = 0, different = 0;
    for (unsigned i = 0; i < pcaps.size(); i++) {
        try {
            if (fast) {
                // check only for acceptance of reduced
                // if is accepted check for acceptance of target
                // reduced supposes to be an over-approximation
                pcapreader::process_payload(
                    pcaps[i].c_str(),
                    [&] (const unsigned char *payload, unsigned len)
                    {
                        if (continue_work == false) {
                            // SIGINT caught in parent, sum up and exit
                            throw std::exception();
                        }
                        total++;
                        if (reduced.accept(payload, len)) {
                            acc_reduced++;
                            if (target.accept(payload, len)) {
                                acc_target++;
                            }
                            else {
                                different++;
                            }
                        }
                    });
            }
            else {
                pcapreader::process_payload(
                    pcaps[i].c_str(),
                    [&] (const unsigned char *payload, unsigned len)
                    {
                        if (continue_work == false) {
                            // SIGINT caught in parent, sum up and exit
                            throw std::exception();
                        }
                        total++;
                        bool b1 = reduced.accept(payload, len);
                        bool b2 = target.accept(payload, len);
                        acc_reduced += b1;
                        acc_target += b2;
                        different += b1 != b2;
                    });
            }
        }
        catch (std::ios_base::failure &e) {
            std::cerr << "\033[1;31mWarning: " << e.what() << "\033[0m\n";
        }
        catch (std::runtime_error &e) {
            std::cerr << "\033[1;31mWarning: " << e.what() << "\033[0m\n";
            // process other capture files
        }
        catch (std::exception &e) {
            ;
        }
    }
    // sum up results
    sum_up(total, acc_target, acc_reduced, different);
}}}

void write_output(std::ostream &out)
{{{

    unsigned msec = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now() - timepoint).count();
    unsigned sec = msec / 1000 / 1000;
    unsigned min = sec / 60;
    if (accepted_only) {
        out << "*************************************************************"
            << "**\n";
        out << "NFA                 : " << fs::basename(nfa_str1) << "\n";
        out << "Total packets       : " << total_packets << "\n";
        out << "Accepted            : " << accepted_target << "\n";
    }
    else {
        float err = wrongly_classified * 1.0 / total_packets;
        unsigned long sc1 = target.state_count();
        unsigned long sc2 = reduced.state_count();

        if (tojson) {
            out << "{\n";
            out << "    \"target\"              : \"" << fs::basename(nfa_str1)
                << "\",\n";
            out << "    \"target states\"       : " << sc1 << ",\n";
            out << "    \"reduced\"             : \"" << fs::basename(nfa_str2)
                << "\",\n";
            out << "    \"reduced states\"       : " << sc2 << ",\n";
            out << "    \"reduction\"           : " << 1.0 * sc2 / sc1
                << ",\n";
            out << "    \"Total packets\"       : " << total_packets << ",\n";
            out << "    \"Accepted by target\"  : " << accepted_target
                << ",\n";
            out << "    \"Accepted by reduced\" : " << accepted_reduced
                << ",\n";
            out << "    \"Wrongly classified\"  : " << wrongly_classified
                << ",\n";
            out << "    \"Error\"               : " << err << "\n";
            out << "}\n";
            return;
        }
        out << "*************************************************************"
            << "**\n";
        out << "Target              : " << fs::basename(nfa_str1) << "\n";
        out << "State count         : " << sc1 << "\n";
        out << "Reduced             : " << fs::basename(nfa_str2) << "\n";
        out << "State count         : " << sc2 << "\n";
        out << "Reduction           : " << 1.0 * sc2 / sc1 << "\n";
        out << "Total packets       : " << total_packets << "\n";
        out << "Accepted by target  : " << accepted_target << "\n";
        out << "Accepted by reduced : " << accepted_reduced << "\n";
        out << "Wrongly classified  : " << wrongly_classified << "\n";
        out << "Error               : " << err << "\n";
    }
    out << "Elapsed time        : " << min << "m/" << sec % 60  << "s/"
        << msec % 1000 << "ms\n";
    out << "***************************************************************\n";
}}}

int main(int argc, char **argv)
{{{

    timepoint = std::chrono::steady_clock::now();
    std::string ofname;
    std::vector<std::string> pcaps;

    const char *outfile = nullptr;
    int opt_cnt = 1;
    int c;
    bool fast = true;
    while ((c = getopt(argc, argv, "ho:n:axjf:")) != -1) {
        opt_cnt++;
        switch (c) {
            case 'h':
                fprintf(stderr, "%s", helpstr);
                return 0;
            case 'a':
                accepted_only = true;
                break;
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
            case 'x':
                fast = false;
                break;
            case 'j':
                tojson = true;
                break;
            default:
                return 1;
        }
    }

    try {
        if (nworkers <= 0 ||
            nworkers >= std::thread::hardware_concurrency())
        {
            throw std::runtime_error(
                "invalid number of cores \"" + std::to_string(nworkers) + "\"");
        }

        // checking a number of arguments
        if (argc - opt_cnt < 3 - accepted_only)
        {
            throw std::runtime_error("invalid arguments");
        }

        // get automata
        nfa_str1 = argv[opt_cnt];
        target.read_from_file(nfa_str1.c_str());
        if (!accepted_only) {
            nfa_str2 = argv[opt_cnt + 1];
            reduced.read_from_file(nfa_str2.c_str());
        }

        // get capture files
        for (int i = opt_cnt + 2 - accepted_only; i < argc; i++) {
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
            threads.push_back(std::thread{[&v, i, &fast]()
                {
                    if (accepted_only) {
                        compute_accepted(target, v[i]);
                    }
                    else {
                        compute_error(target, reduced, v[i], fast);
                    }
                }
                });
        }

        for (unsigned i = 0; i < nworkers; i++) {
            if (threads[i].joinable()) {
                threads[i].join();
            }
        }

        write_output(*output);

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
