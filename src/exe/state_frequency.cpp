/// @author Jakub Semric
/// 2018

#include <iostream>
#include <ostream>
#include <vector>
#include <map>
#include <getopt.h>

#include "nfa.hpp"
#include "pcap_reader.hpp"

using namespace reduction;
using namespace std;

#define AFLAG_BOTH 2
#define AFLAG_IN_LANG 1
#define AFLAG_NIN_LANG 0

const char *helpstr =
"Usage: ./state_frequency [OPTIONS] NFA PCAP OUTPUT\n"
"Compute packet frequency for each state.\n"
"\noptions:\n"
"  -h            : show this help and exit\n"
"  -c <N>        : packet max count\n"
"  -a <N>        : 1 - only accepted, 0 - not accepted, default both\n";

map<State, unsigned long> compute_freq(
    const Nfa &nfa, pcap_t *pcap, int aflag = AFLAG_BOTH, size_t count=~0UL)
{
    map<State, unsigned long> freq;
    NfaArray m(nfa);
    vector<size_t> state_freq(nfa.state_count());

    pcapreader::process_payload(
        pcap,
        [&] (const unsigned char *payload, unsigned len)
        {
            if (aflag >= AFLAG_BOTH) {
                m.label_states(state_freq, payload, len);                
            }
            else {
                // only accepted or ~accepted
                if (m.accept(payload, len) == 1) {
                    m.label_states(state_freq, payload, len);
                }
            }
        }, count);

    // remap frequencies
    auto state_map = m.get_reversed_state_map();
    for (unsigned long i = 0; i < nfa.state_count(); i++)
    {
        freq[state_map[i]] = state_freq[i];
    }

    return freq;
}

map<State, unsigned long> compute_freq(
    const Nfa &nfa, string fname, int aflag = AFLAG_BOTH, size_t count=~0UL)
{
  
    
    // char err_buf[4096] = "";
    // pcap_t *pcap;
    // if (!(pcap = pcap_open_offline(fname.c_str(), err_buf)))
    //     throw std::ios_base::failure("cannot open pcap file '" + fname + "'");
    // 
    // return compute_freq(nfa, pcap, aflag, count);
    map<State, unsigned long> freq;
    NfaArray m(nfa);
    vector<size_t> state_freq(nfa.state_count());

    pcapreader::process_strings(
        fname,
        [&] (const unsigned char *payload, unsigned len)
        {
            if (aflag >= AFLAG_BOTH) {
                m.label_states(state_freq, payload, len);                
            }
            else {
                // only accepted or ~accepted
                if (m.accept(payload, len) == 1) {
                    m.label_states(state_freq, payload, len);
                }
            }
        }, count);

    // remap frequencies
    auto state_map = m.get_reversed_state_map();
    for (unsigned long i = 0; i < nfa.state_count(); i++)
    {
        freq[state_map[i]] = state_freq[i];
    }

    return freq;
}

int main(int argc, char **argv)
{
    try{
        size_t cnt = ~0UL;
        int aflag = 2;
        int opt_cnt = 1;
        int c;
        while ((c = getopt(argc, argv, "hc:a:")) != -1) {
            opt_cnt++;
            switch (c) {
                // general options
                case 'h':
                    cerr << helpstr;
                    return 0;
                case 'a':
                    aflag = stoi(optarg);
                    opt_cnt++;
                    break;
                case 'c':
                    cnt = stoul(optarg);
                    opt_cnt++;
                    break;
                default:
                    return 1;
            }
        }

        if (argc > 3) {
            cerr << "computing packet frequency\n";
            string nfa_str = argv[opt_cnt];
            string pcap = argv[opt_cnt + 1];
            Nfa nfa = Nfa::read_from_file(nfa_str);
            ofstream out{argv[opt_cnt + 2]};
            if (!out.is_open())
                throw runtime_error("cannot open output file");

            auto freq = compute_freq(nfa, pcap, aflag, cnt);

            for (auto i : freq)
                out << i.first << " " << i.second << endl;
            out.close();
        }
        else {
            cerr << "3 arguments required: NFA PCAP OUTPUT\n";
        }
    }
    catch (exception &e) {
        cerr << "\033[1;31mERROR\033[0m " << e.what() << endl;
        return 1;
    }
    return 0;
}
