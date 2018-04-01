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

#include "nfa_error.hpp"
#include "nfa.hpp"

using namespace reduction;
using namespace std;

namespace fs = boost::filesystem;

const char *helpstr =
"The program computes an error of an incorrect packet classification by an\n"
"over-approximated NFA. Positional arguments are TARGET REDUCED PCAPS ...,\n"
"where TARGET is an input NFA, REDUCED denotes over-approximated reduction\n"
"of the TARGET and PCAPS are packet capture files.\n"
"Usage: ./nfa_handler [OPTIONS] TARGET REDUCED ...\n"
"options:\n"
"  -h            : show this help and exit\n"
"  -o <FILE>     : specify output file\n"
"  -n <NWORKERS> : number of workers to run in parallel\n"
"  -c            : rigorous error computation, consistent but much slower,\n"
"                  use only if not sure about over-approximation\n"
"  -a            : aggregate statistics, otherwise output is in csv format\n"
"  -d            : add csv header\n";

void write_error_stats(
    ostream &out, const vector<pair<string,ErrorStats>> &data,
    string reduced_str, bool aggregate, bool header,
    size_t sc_t, size_t sc_r)
{
    float ratio = sc_r * 1.0 / sc_t;
    if (aggregate)
    {
        ErrorStats aggr(sc_r, sc_t);
        for (auto i : data)
            aggr.aggregate(i.second);

        float pe = aggr.fp_a * 1.0 / aggr.total;
        float ce = aggr.fp_c * 1.0 / aggr.total;
        float cls_ratio = aggr.pp_c * 1.0 / (aggr.pp_c + aggr.fp_c);
        assert(cls_ratio >= 0 && cls_ratio <= 1);
        assert(pe >= 0 && pe <= ce && ce <= 1);
        out << "reduction : " << ratio << endl;
        out << "total     : " << aggr.total << endl;
        out << "pe        : " << pe << endl;
        out << "ce        : " << ce << endl;
        out << "+rate     : " << cls_ratio << endl;
    }
    else
    {
        if (header)
        {
            out << "reduced,pcap,total,fp_a,pp_a,fp_c,pp_c,all_c"
                << endl;
        }

        for (auto i : data)
        {
            auto pcap = i.first;
            auto d = i.second;
            out <<  fs::basename(reduced_str)
                << "," << fs::basename(pcap) + fs::extension(pcap) << ","
                << d.total << "," <<  d.fp_a << "," << d.pp_a << "," << d.fp_c
                << "," << d.pp_c << "," << d.all_c << endl;
        }
    }
}

int main(int argc, char **argv)
{
    chrono::steady_clock::time_point timepoint = chrono::steady_clock::now();
    string outfile;
    vector<string> pcaps;
    unsigned nworkers = 1;
    bool consistent = false, aggregate = false, header = false;

    FastNfa target, reduced;
    string nfa_str1, nfa_str2;

    int opt_cnt = 1;    // program name
    int c;

    try {
        if (argc < 2)
        {
            cerr << helpstr;
            return 1;
        }

        while ((c = getopt(argc, argv, "ho:n:cad")) != -1)
        {
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
                case 'c':
                    consistent = true;
                    break;
                case 'a':
                    aggregate = true;
                    break;
                case 'd':
                    header = true;
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
        {
            pcaps.push_back(argv[i]);
        }

        ostream *output = &cout;
        if (outfile != "")
        {
            output = new ofstream(outfile);
            if (!static_cast<ofstream*>(output)->is_open())
                throw runtime_error("cannot open output file");
        }

        // divide work
        vector<vector<string>> v(nworkers);
        for (unsigned i = 0; i < pcaps.size(); i++)
        {
            v[i % nworkers].push_back(pcaps[i]);
        }

        vector<pair<string,ErrorStats>> stats;
        vector<future<vector<pair<string,ErrorStats>>>> threads;
        for (unsigned i = 0; i < nworkers; i++)
        {
            threads.push_back(
                async(
                    compute_error,ref(target),ref(reduced),ref(v[i]),
                    consistent)
                );
        }

        for (unsigned i = 0; i < nworkers; i++)
        {
            auto r = threads[i].get();
            stats.insert(stats.end(), r.begin(), r.end());
        }

        write_error_stats(
            *output, stats, nfa_str2, aggregate, header,
            target.state_count(), reduced.state_count());

        unsigned msec = chrono::duration_cast<chrono::microseconds>(
            chrono::steady_clock::now() - timepoint).count();
        unsigned sec = msec / 1000 / 1000;
        unsigned min = sec / 60;
        cerr << "duration  : " << min << "m/" << sec % 60 << "s/"
            << msec % 1000 << "ms\"\n";

        if (outfile != "")
        {
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