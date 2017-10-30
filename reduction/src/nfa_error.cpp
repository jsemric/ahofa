#include <iostream>
#include <ostream>
#include <exception>
#include <vector>
#include <ctype.h>
#include <stdexcept>
#include <ctime>
#include <getopt.h>

#include "nfa.h"
#include "pcap_reader.h"

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
"  -x            : slower but check if REDUCED is really over-approximation\n"
"                  of TARGET\n";

int nworkers = 1;

void compute_accepted(
    const NFA &nfa,
    const std::vector<std::string> &pcaps,
    std::ostream &out)
{
    unsigned long total = 0;
    unsigned long accepted = 0;

    out << "Total: " << total << "\n";
    out << "Accepted: " << accepted << "\n";
}

void compute_error(
        const NFA &target,
        const NFA &reduced,
        const std::vector<std::string> &pcaps,
        std::ostream &out,
        bool fast = true)
{
    for (int i = 0; i < pcaps.size(); i++) {
        /* divide work */
        /* lamba for each thread */
        pcapreader::process_payload(
            pcaps[i].c_str(),
            [&target, &reduced] (const unsigned char *payload, unsigned len) {
                ;
            },
            ~0ULL);
        /* wait for process */
        /* sum up error*/
    }
}

int main(int argc, char **argv) {

    std::string nfa_str1, nfa_str2, ofname;
    std::vector<std::string> pcaps;

    const char *outfile;
    int opt_cnt = 1;
    int c;
    bool accepted_only = false;
    bool fast = true;
    while ((c = getopt(argc, argv, "ho:n:ax")) != -1) {
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
            case 'x':
                fast = false;
                break;
            default:
                return 1;
        }
    }

    NFA reduced, target;
    try {
        if (nworkers <= 0  && nworkers >= 4/*XXX*/) {
            throw std::runtime_error(
                "invalid number of cores \"" + std::to_string(nworkers) + "\"");
        }

        // checking a number of arguments
        if (argc - opt_cnt < 3 - accepted_only)
        {
            throw std::runtime_error("invalid arguments");
        }

        // get automata
        target.read_from_file(argv[opt_cnt]);
        if (!accepted_only) {
            reduced.read_from_file(argv[opt_cnt + 1]);
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

        // start computation
        if (accepted_only) {
            compute_accepted(target, pcaps, *output);
        }
        else {
            compute_error(target, reduced, pcaps, *output, fast);
        }

        if (outfile) {
            static_cast<std::ofstream*>(output)->close();
            delete output;
        }
    }
    catch (std::exception &e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
