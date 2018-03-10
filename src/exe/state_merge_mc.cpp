#include <iostream>
#include <fstream>
#include <algorithm>

#include "pcap_reader.hpp"
#include "nfa.hpp"

using namespace reduction;
using namespace std;
/*
std::vector<std::set<State>> (
    Nfa &nfa, const std::vector<std::set<size_t>> &state_labels)
{
    std::vector<std::set<State>> eq_states;
    std::set<State> empty;
    int cnt = 0;
    for (size_t i = 0; i < state_labels.size(); i++) {

        if (state_labels[i].empty()) {
            empty.insert(i);
            continue;
        }

        std::set<State> eq;
        for (size_t j = i + 1; j < state_labels.size(); j++) {
            if (state_labels[i] == state_labels[j]) {
                eq.insert(j);cnt++;
            }
        }
        if (!eq.empty()) {
            eq.insert(i);
            eq_states.push_back(eq);
        }
    }
    eq_states.push_back(empty);

    std::cerr << cnt << "\n";
    
    return eq_states;
}*/

vector<vector<size_t>> label_with_prefix(const FastNfa &nfa, string pcap)
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
                        // insert to a vector only once
                        if (state_labels[s].empty() ||
                            state_labels[s].back() != prefix)
                        {
                            state_labels[s].push_back(prefix);
                        }
                    },
                    [&prefix]() {prefix++;});
            }
        });

    /*
    auto res = armc(nfa, state_labeling);
    for (auto i : res) {
        for (auto j : i) {
            cout << state_map[j] << " ";
        }
        cout << endl;
    }
    */
    return state_labels;
}


int main(int argc, char **argv)
{
    if (argc < 3) {
        cerr << "Error: 2 arguments required\n";
        return 1;
    }

    FastNfa nfa;
    nfa.read_from_file(argv[1]);
    auto state_labels = label_with_prefix(nfa, argv[2]);

    int total = 0;
    // let us see which states are `similar`
    map<pair<size_t,size_t>,pair<int,float>> sim_states;
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
                int same = it - res.begin();
                float pct = same * 1.0 / denom;
                sim_states[pair<size_t,size_t>(i,j)] =
                    pair<size_t,float>(same, pct);
                if (pct > 0.1) total++;
                //if (state_labels[i] == state_labels[j]) total++;
            }
        }
    }
    cerr << total << endl;

    return 0;
}
