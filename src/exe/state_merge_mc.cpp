#include <iostream>
#include <fstream>
#include <algorithm>

#include "pcap_reader.hpp"
#include "nfa.hpp"

using namespace reduction;
using namespace std;

vector<vector<size_t>> label_with_prefix(const NfaArray &nfa, string pcap)
{
    // each state marked with prefix
    vector<vector<size_t>> state_labels(nfa.state_count());
    // we distinguish the prefixes by some integral value
    size_t prefix = 0;
    pcapreader::process_payload(
        pcap.c_str(),
        [&] (const unsigned char *payload, unsigned len)
        {
            if (nfa.accept(payload, len) == false)
            {
                nfa.parse_word(payload, len,
                    [&state_labels, &prefix](State s)
                    {
                        // insert to a vector only once at max
                        if (state_labels[s].empty() ||
                            state_labels[s].back() != prefix)
                        {
                            state_labels[s].push_back(prefix);
                        }
                    },
                    [&prefix]() {prefix++;});
            }
        });

    return state_labels;
}


int main(int argc, char **argv)
{
    if (argc < 3) {
        cerr << "Error: 2 arguments required NFA and PCAP\n";
        return 1;
    }

    float th = 0.75;
    if (argc > 3)
        th = stod(argv[3]);
    assert(th > 0 && th <= 1);

    NfaArray nfa(Nfa::read_from_file(argv[1]));
    auto state_labels = label_with_prefix(nfa, argv[2]);
    auto state_map = nfa.get_reversed_state_map();

    // empty sets eq. group
    for (size_t i = 0; i < state_labels.size(); i++) {
        if (state_labels[i].empty()) {
            cout << state_map.at(i) << " ";
        }
    }
    cout << endl;

    // eq. pairs wrt the threshold th
    for (size_t i = 0; i < state_labels.size(); i++) {
        if (state_labels[i].empty()) {
            continue;
        }

        for (size_t j = i + 1; j < state_labels.size(); j++) {
            // compute the intersection
            // vectors are supposed to be sorted
            vector<size_t> res(state_labels[i].size());
            auto it = set_intersection(
                state_labels[i].begin(), state_labels[i].end(),
                state_labels[j].begin(), state_labels[j].end(), res.begin()
            );
            if (it != res.begin()) {
                int denom = max(state_labels[i].size(), state_labels[j].size());
                int cnt = it - res.begin();
                float sim_rate = cnt * 100.0 / denom;
                if (sim_rate > th) {
                    cout << state_map.at(i) << " " << state_map.at(j) << endl;
                }
            }
        }
    }

    return 0;
}
