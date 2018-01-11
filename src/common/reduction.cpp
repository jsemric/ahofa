/// @author Jakub Semric
/// 2017

#include <iostream>
#include <algorithm>
#include <cassert>

#include "aux.hpp"
#include "reduction.hpp"

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
    // total packets
    size_t total = 0;

    for (auto i : state_labels) {
        if (!nfa.is_final(i.first) && i.first != init) {
            sorted_states.push_back(i.first);
        }
        total = total < i.second ? i.second : total;
    }

    // sort states in ascending order according to state packet frequencies
    std::sort(
        sorted_states.begin(), sorted_states.end(),
        [&state_labels](State x, State y) {
            return state_labels.at(x) < state_labels.at(y);
        });

    float error = 0;
    size_t state_count = nfa.state_count();
    size_t removed = 0;
    
    if (eps != -1) {
        // use error rate
        while (error < eps && removed < sorted_states.size())
        {
            State state = sorted_states[removed];
            merge_map[state] = rule_map[state];
            removed++;
            error += (1.0 * state_labels.at(state)) / total;
        }
    }
    else {
        // use pct rate
        size_t to_remove = (1 - pct) * state_count;
        while (removed < to_remove && removed < sorted_states.size())
        {
            State state = sorted_states[removed];
            merge_map[state] = rule_map[state];
            removed++;
            error += (1.0 * state_labels.at(state)) / total;
        }
    }

    nfa.merge_states(merge_map);
    size_t new_sc = state_count - removed;
    size_t reduced_to = new_sc * 100 / state_count;
    std::cerr << "Reduction: " << new_sc << "/" << state_count
        << " " << reduced_to << "%\n";
    std::cerr << "Predicted error: " << error << std::endl;

}

std::vector<std::set<State>> armc(
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
}

}
