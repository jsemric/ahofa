/// @author Jakub Semric
/// 2018

#include <iostream>
#include <fstream>
#include <vector>
#include <map>
#include <stdexcept>
#include <ctime>
#include <chrono>
#include <thread>
#include <future>
#include <ctype.h>
#include <getopt.h>

#include <boost/filesystem.hpp>

#include "nfa_stats.hpp"
#include "nfa.hpp"

using namespace reduction;
using namespace std;

namespace fs = boost::filesystem;

const char *helpstr =
"Usage: ./nfa_eval [OPTIONS] TARGET REDUCED PCAP...\n"
"Compute statistics about for REDUCED automaton of the TARGET on PCAP files.\n"
"TARGET and REDUCED are nfa of FA format\n"
"PCAP are packet capture files\n"
"\noptions:\n"
"  -h            : show this help and exit\n"
"  -o <FILE>     : specify output file\n"
"  -n <NWORKERS> : number of workers to run in parallel\n"
"  -r            : rigorous stats computation, consistent but much slower,\n"
"                  use only if not sure about over-approximation\n"
"  -c            : output in CSV format\n";

void write_nfa_stats(
    ostream &out, const vector<pair<string,NfaStats>> &data,
    string reduced_str, bool csv, size_t sc_t, size_t sc_r)
{
    float ratio = sc_r * 1.0 / sc_t;
    if (csv) {
        for (auto i : data) {
            auto pcap = i.first;
            auto d = i.second;
            out <<  fs::basename(reduced_str)
                << "," << fs::basename(pcap) + fs::extension(pcap) << ","
                << d.total << "," <<  d.fp_a << "," << d.pp_a << "," << d.fp_c
                << "," << d.pp_c << "," << d.all_c << endl;
        }
    }
    else {
        NfaStats aggr(sc_r, sc_t);
        for (auto i : data)
            aggr.aggregate(i.second);

        float accuracy = 1.0 - aggr.fp_c * 1.0 / aggr.total;
        float precision = aggr.pp_c * 1.0 / (aggr.pp_c + aggr.fp_c);

        assert(precision >= 0 && precision <= 1);
        assert(0 <= accuracy && accuracy <= 1);
        out << "reduction : " << ratio << endl;
        out << "total     : " << aggr.total << endl;
        out << "accuracy  : " << accuracy << endl;
        out << "precision : " << precision << endl;
    }
}

int main(int argc, char **argv)
{
    chrono::steady_clock::time_point timepoint = chrono::steady_clock::now();
    string outfile;
    vector<string> pcaps;
    unsigned nworkers = 1;
    bool consistent = false, csv = false;

    FastNfa target, reduced;
    string nfa_str1, nfa_str2;

    int opt_cnt = 1;    // program name
    int c;

    try {
        if (argc < 2) {
            cerr << helpstr;
            return 1;
        }

        while ((c = getopt(argc, argv, "ho:n:rc")) != -1) {
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
                case 'r':
                    consistent = true;
                    break;
                case 'c':
                    csv = true;
                    break;
                default:
                    return 1;
            }
        }

        if (argc - opt_cnt < 3)
        {
            throw runtime_error("invalid positional arguments");
        }

        nworkers = min(nworkers, thread::hardware_concurrency());
        assert(nworkers > 0);

        // get automata
        nfa_str1 = argv[opt_cnt];
        target.read_from_file(nfa_str1.c_str());
        nfa_str2 = argv[opt_cnt + 1];
        reduced.read_from_file(nfa_str2.c_str());

        // get capture files
        for (int i = opt_cnt + 2; i < argc; i++)
            pcaps.push_back(argv[i]);

        ostream *output = &cout;
        if (outfile != "") {
            output = new ofstream(outfile);
            if (!static_cast<ofstream*>(output)->is_open())
                throw runtime_error("cannot open output file");
        }

        // divide work
        vector<vector<string>> v(nworkers);
        for (unsigned i = 0; i < pcaps.size(); i++)
            v[i % nworkers].push_back(pcaps[i]);

        vector<pair<string,NfaStats>> stats;
        vector<future<vector<pair<string,NfaStats>>>> threads;
        for (unsigned i = 0; i < nworkers; i++)
            threads.push_back(
                async(
                    compute_nfa_stats, ref(target),ref(reduced),ref(v[i]),
                    consistent)
                );

        for (unsigned i = 0; i < nworkers; i++) {
            auto r = threads[i].get();
            stats.insert(stats.end(), r.begin(), r.end());
        }

        write_nfa_stats(*output, stats, nfa_str2, csv, target.state_count(),
            reduced.state_count());

        unsigned msec = chrono::duration_cast<chrono::microseconds>(
            chrono::steady_clock::now() - timepoint).count();
        unsigned sec = msec / 1000 / 1000;
        unsigned min = sec / 60;
        cerr << "duration  : " << min << "m/" << sec % 60 << "s/"
            << msec % 1000 << "ms\"\n";

        if (outfile != "") {
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
