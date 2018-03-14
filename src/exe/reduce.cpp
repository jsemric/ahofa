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
#include <ctype.h>
#include <getopt.h>

#include "nfa.hpp"
#include "pcap_reader.hpp"
#include "reduction.hpp"

using namespace reduction;
using namespace std;

const char *helpstr =
"NFA reduction\n"
"Usage: ./reduce [OPTIONS] NFA FILE\n"
"options:\n"
"  -h            : show this help and exit\n"
"  -o <FILE>     : specify output file or directory for -s option\n"
"  -f            : compute packet frequency of NFA states\n"
"  -t <TYPE>     : reduction type {prune,merge,nfmerge}\n"
"  -s <N>        : frequency threshold for merging, default 0.99\n"
"  -c <N>        : number of packets in one iteration, default 10000\n"
"  -i <N>        : number of iterations, default 10000\n"
"  -p <N>        : reduction rate\n"
"  -e <N>        : error rate\n";

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
    // options
    bool freq_opt = false;
    float eps = -1;
    float pct = 0.05;//-1;
    float threshold = 0.99;
    size_t count = 10000;
    size_t iter = 10000;
    string outfile = "reduced-nfa.fa", pcap, red_type = "prune";

    int opt_cnt = 1;    // program name
    int c;

    try {
        if (argc < 2) {
            cerr << helpstr;
            return 1;
        }

        while ((c = getopt(argc, argv, "ho:fe:p:t:i:c:s:")) != -1) {
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
                case 'f':
                    freq_opt = true;
                    break;
                case 'e':
                    opt_cnt++;
                    eps = stod(optarg);
                    check_float(eps);
                    break;
                case 'p':
                    opt_cnt++;
                    pct = stod(optarg);
                    check_float(pct);
                    break;
                case 's':
                    opt_cnt++;
                    threshold = stod(optarg);
                    check_float(threshold);
                    break;
                case 'c':
                    opt_cnt++;
                    count = stoi(optarg);
                    break;
                case 'i':
                    opt_cnt++;
                    iter = stoi(optarg);
                    break;
                case 't':
                    opt_cnt++;
                    red_type = optarg;
                    break;
                default:
                    return 1;
            }
        }

        // checking the min. number of positional arguments, which is 2
        if (argc - opt_cnt < 2)
        {
            throw runtime_error("invalid positional arguments");
        }

        // get automata
        string nfa_str = argv[opt_cnt];
        string pcap = argv[opt_cnt + 1];

        FastNfa nfa;
        nfa.read_from_file(nfa_str.c_str());
        auto state_map = nfa.get_reversed_state_map();

        ofstream out{outfile};
        if (!out.is_open())
        {
            throw runtime_error("cannot open output file");
        }

        if (freq_opt)
        {
            size_t total = 0;
            vector<size_t> state_freq(nfa.state_count());

            pcapreader::process_payload(
                pcap.c_str(),
                [&] (const unsigned char *payload, unsigned len)
                {
                    total++;
                    nfa.label_states(state_freq, payload, len);
                });
            
            out << "# Total packets : " << total << endl;

            auto state_map = nfa.get_reversed_state_map();
            auto state_depth = nfa.state_depth();
            for (unsigned long i = 0; i < nfa.state_count(); i++)
            {
                out << state_map[i] << " " << state_freq[i] << " "
                    << state_depth[state_map[i]] << "\n";
            }
        }
        else
        {
            auto old_sc = nfa.state_count();
            float error;
            if (red_type == "nfmerge")
            {
                error = nfold_merge(nfa, pcap, pct, threshold, count, iter);
            }
            else
            {
                auto labels = read_state_labels(nfa, pcap);
            
                if (red_type == "prune")
                {
                    error = prune(nfa, labels, pct, eps);
                }
                else if (red_type == "merge")
                {
                    error = merge_and_prune(nfa, labels, pct);
                }
                else
                {
                    throw runtime_error(
                        "invalid reduction type: '" + red_type + "'");
                }    
            }

            auto new_sc = nfa.state_count();

            cerr << "Reduction: " << new_sc << "/" << old_sc
                << " " << 100 * new_sc / old_sc << "%\n";
            cerr << "Predicted error: " << error << endl;
            nfa.print(out);
        }

        out.close();
    }
    catch (exception &e) {
        cerr << "\033[1;31mERROR\033[0m " << e.what() << endl;
        return 1;
    }

    return 0;
}
