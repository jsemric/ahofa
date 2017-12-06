/// @author Jakub Semric
/// 2017

#include <iostream>
#include <algorithm>
#include <cassert>

#include "aux.h"
#include "reduction.h"

namespace reduction {

void prune(
    Nfa &nfa, const std::map<State, unsigned long> &state_labels,
    float pct, float eps)
{
    assert(eps == -1 || (eps > 0 && eps <= 1));
    assert(pct > 0 && pct <= 1);

    std::map<State,State> merge_map;
    // merge only states with corresponding rule, which is defined by final
    // state
    std::map<State,State> rule_map = nfa.get_paths();
    
    // sort state_labels
    // mark which states to prune
    std::vector<State> sorted_states;
    State init = nfa.get_initial_state();
    for (auto i : state_labels) {
        if (!nfa.is_final(i.first) && i.first != init) {
            sorted_states.push_back(i.first);
        }
    }

    std::sort(
        sorted_states.begin(), sorted_states.end(),
        [&state_labels](State x, State y){
            return state_labels.at(x) < state_labels.at(y);});

    if (eps != -1) {
        // TODO
    }
    else {
        pct = 0.7;
        int state_count = nfa.state_count();
        int removed = 0;
        int to_remove = (1 - pct) * state_count;
        std::cerr << state_count - to_remove << "/" << state_count << " " 
            << 100*pct << "%\n";
        while (removed < to_remove)
        {
            State state = sorted_states[removed];
            merge_map[state] = rule_map[state];
            removed++;
        }
    }

    /*
    for (auto i : merge_map) {
        std::cerr << i.first << "->" << i.second << "\n";
    }*/
    

    nfa.merge_states(merge_map);
}

}