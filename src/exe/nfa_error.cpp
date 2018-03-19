/// @author Jakub Semric
/// 2018

#include <iostream>
#include <ostream>
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
"where TARGET is an input NFA, REDUCED denotes over-approximated reduction of\n"
"the TARGET and PCAPS are packet capture files.\n"
"Usage: ./nfa_handler [OPTIONS] TARGET REDUCED ...\n"
"options:\n"
"  -h            : show this help and exit\n"
"  -o <FILE>     : specify output file or directory for -s option\n"
"  -n <NWORKERS> : number of workers to run in parallel\n"
"  -j            : verbose output in JSON\n"
"  -c            : Rigorous error computation. Consistent but much slower.\n"
"                  Use only if not sure about over-approximation\n"
"  -s            : store results to a separate file per each packet capture\n"
"                  file\n";

void write_error_stats(
    ostream &out, const ErrorStats &data, string pcap, const FastNfa &target, 
    string target_str, const FastNfa &reduced, string reduced_str,
    bool to_json, chrono::steady_clock::time_point timepoint)
{
    unsigned msec = chrono::duration_cast<chrono::microseconds>(
        chrono::steady_clock::now() - timepoint).count();
    unsigned sec = msec / 1000 / 1000;
    unsigned min = sec / 60;

    size_t cls1 = 0, cls2 = 0;
    for (auto i : data.nfa1_data)
    {
        cls1 += i;
    }
    for (auto i : data.nfa2_data)
    {
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
    out << "    \"target file\": \"" << target_str << "\",\n";
    out << "    \"target\": \"" << fs::basename(target_str)
        << "\",\n";
    out << "    \"target states\": " << sc1 << ",\n";
    out << "    \"reduced file\": \"" << reduced_str << "\",\n";
    out << "    \"reduced\": \"" << fs::basename(reduced_str)
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

    auto fidx = reduced.get_final_state_idx();
    auto state_map = reduced.get_reversed_state_map();
    for (size_t i = 0; i < fidx.size(); i++) {
        State s = fidx[i];
        out << "        \"q" << state_map[s]  << "\" : "
            << data.nfa1_data[s];
        if (i == fidx.size() - 1) {
            out << "}";
        }
        out << ",\n";
    }

    fidx = target.get_final_state_idx();
    state_map = target.get_reversed_state_map();
    
    out << "    \"target rules\": {\n";
    for (size_t i = 0; i < fidx.size(); i++) {
        State s = fidx[i];
        out << "        \"q" << state_map[s]  << "\" : "
            << data.nfa1_data[s];
        if (i == fidx.size() - 1) {
            out << "}";
        }
        out << ",\n";
    }

    out << "    \"pcap\" : \"" << pcap << "\"\n";
    out << "}\n";
}

string generate_fname(string nfa_str, string outdir, string pcap)
{
    string base = outdir + "/" +
       fs::basename(nfa_str) + "." +
       fs::path(pcap).filename().string() + ".";
    string res;
    string hash = "00000";
    int j = 0;
    do {
        hash[0] = '0' + (rand()%10);
        hash[1] = '0' + (rand()%10);
        hash[2] = '0' + (rand()%10);
        hash[3] = '0' + (rand()%10);
        hash[4] = '0' + (rand()%10);
        res = base + hash + ".json";
    } while (fs::exists(res) || j++ > 100000);
    return res;
}

int main(int argc, char **argv)
{
    chrono::steady_clock::time_point timepoint = chrono::steady_clock::now();
    string outfile;
    vector<string> pcaps;
    // general program options
    unsigned nworkers = 1;
    bool store_sep = false;
    bool to_json = false;
    bool consistent = false;
    string outdir = "data/prune-error";

    FastNfa target, reduced;
    string nfa_str1, nfa_str2;

    int opt_cnt = 1;    // program name
    int c;

    try {
        if (argc < 2) {
            cerr << helpstr;
            return 1;
        }

        while ((c = getopt(argc, argv, "ho:n:jcs")) != -1) {
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

        if (argc - opt_cnt < 3)
        {
            throw runtime_error("invalid positional arguments");
        }

        if (nworkers <= 0 || nworkers >= thread::hardware_concurrency())
        {
            throw runtime_error(
                "invalid number of cores \"" + to_string(nworkers) + "\"");
        }

        ostream *output = &cout;
        if (outfile != "")
        {
            if (store_sep)
            {
                outdir = outfile;
                if (!fs::is_directory(outdir))
                {
                    throw runtime_error("invalid directory");
                }
                outfile = "";
            }
            else
            {
                output = new ofstream{outfile};
                if (!static_cast<ofstream*>(output)->is_open()) {
                    throw runtime_error("cannot open output file");
                }
            }
        }

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
            threads.push_back(async(
                    [&target, &reduced, &v, i, consistent]()
                    {
                        NfaError err{target, reduced, v[i], consistent};
                        err.process_pcaps();
                        return err.get_result();
                    }
                ));
        }

        for (unsigned i = 0; i < nworkers; i++)
        {
            auto r = threads[i].get();
            stats.insert(stats.end(), r.begin(), r.end());
        }
        
        if (store_sep)
        {
            to_json = 1;
            for (auto i : stats)
            {
                auto pcap = i.first;
                // generate output name for each result
                string fname = generate_fname(nfa_str1, outdir, pcap);
                ofstream out{fname};
                if (out.is_open())
                {
                    write_error_stats(
                        out, i.second, i.first, target, nfa_str1, reduced,
                        nfa_str2, to_json, timepoint);
                    out.close();
                }
                else
                {
                    throw runtime_error("cannot save result");
                }
            }
        }
        else
        {
            // aggregate results
            ErrorStats aggr(target.state_count(), reduced.state_count());
            for (auto i : stats)
                aggr.aggregate(i.second);

            write_error_stats(
                *output, aggr, "", target, nfa_str1, reduced,
                nfa_str2, to_json, timepoint);

            if (outfile != "")
            {
                static_cast<ofstream*>(output)->close();
                delete output;
            }
        }
    }
    catch (exception &e) {
        cerr << "\033[1;31mERROR\033[0m " << e.what() << endl;
        return 1;
    }

    return 0;
}