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
        cerr << "Error: 2 arguments required\n";
        return 1;
    }

    NfaArray nfa;
    nfa.read_from_file(argv[1]);
    auto state_labels = label_with_prefix(nfa, argv[2]);

    // which states are `similar`
    map<pair<size_t,size_t>,pair<int,float>> sim_states;
    for (size_t i = 0; i < state_labels.size(); i++) {
        float max_pct = 0;
        if (state_labels[i].empty()) {
            //cout << max_pct << endl;
            continue;
        }

        //for (size_t j = i + 1; j < state_labels.size(); j++) {
        for (size_t j = 0; j < state_labels.size(); j++) {
            if (i == j) continue;
            // compute the intersection
            // vectors are supposed to be sorted
            vector<size_t> res(state_labels[i].size());
            auto it = set_intersection(
                state_labels[i].begin(), state_labels[i].end(),
                state_labels[j].begin(), state_labels[j].end(), res.begin()
            );
            if (it != res.begin()) {
                int denom = max(state_labels[i].size(), state_labels[j].size());
                int same = it - res.begin();
                float pct = same * 100.0 / denom;
                max_pct = pct > max_pct ? pct : max_pct;
                //cout << pct << endl;
                sim_states[pair<size_t,size_t>(i,j)] =
                    pair<size_t,float>(same, pct);
            }
        }
        cout << max_pct << endl;
    }

    return 0;
}
