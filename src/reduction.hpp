/// @author Jakub Semric
/// 2017

#pragma once

#include <map>
#include <vector>
#include <set>

#include "nfa.hpp"

namespace reduction {

// propagate final states to theirs predecessors
void prune(
    Nfa &nfa, const std::map<State, unsigned long> &state_labels,
    float pct = 0.18, float eps = -1);

std::vector<std::set<State>> armc(
    Nfa &nfa, const std::vector<std::set<size_t>> &state_labels);

} // end of namespace
