#include <iostream>
#include <fstream>
#include <map>
#include <vector>
#include <getopt.h>

#include "pcap_reader.hpp"
#include "nfa.hpp"
#include "nfa_stats.hpp"
#include "reduction.hpp"

using namespace std;
using namespace reduction;

vector<map<State, unsigned long>> divide(const Nfa &nfa, string pcap)
{
    vector<map<State, unsigned long>> res;
    NfaArray m(nfa);
    auto state_map = m.get_reversed_state_map();
    vector<size_t> state_freq(nfa.state_count());
    int cnt = 0;
    int prev = 50;

    pcapreader::process_payload(
        pcap.c_str(),
        [&] (const unsigned char *payload, unsigned len)
        {
            m.label_states(state_freq, payload, len);
            cnt++;
            if (cnt >= prev) {
                map<State, unsigned long> freq;
                for (unsigned long i = 0; i < nfa.state_count(); i++)
                    freq[state_map[i]] = state_freq[i];
                res.push_back(freq);
                prev *= 2;
            }
        });

    if (cnt != prev) {
        map<State, unsigned long> freq;
        for (unsigned long i = 0; i < nfa.state_count(); i++)
            freq[state_map[i]] = state_freq[i];
        res.push_back(freq);
    }

    return res;
}

int main(int argc, char **argv) {

    if (argc < 4) {
        cerr << "3 arguments required: NFA and 2xPCAP file \n";
        return 1;
    }
    try {
        Nfa target = Nfa::read_from_file(string(argv[1]));
        NfaArray target_a(target);
        auto state_freqs = divide(target, argv[2]);
        for (auto i : state_freqs) {
            Nfa reduced(target);
            prune(reduced, i, 0.18);
            NfaArray reduced_a(reduced);
            auto stats = compute_nfa_stats(target_a, reduced_a,
                vector<string>{argv[3]})[0].second;
            float accuracy = 1.0 - stats.fp_c * 1.0 / stats.total;
            float precision = stats.pp_c * 1.0 / (stats.pp_c + stats.fp_c);
            size_t samples = i[target.get_initial_state()];
            cout << samples << " " << accuracy << " " << precision << endl;
        }
    }
    catch (exception &e) {
        cerr << "\033[1;31mERROR\033[0m " << e.what() << endl;
        return 1;
    }

    return 0;
}
