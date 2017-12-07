/// @author Jakub Semric
/// 2017

#pragma once

#include <map>

#include "nfa.hpp"

namespace reduction {

// propagate final states to theirs predecessors
void prune(
    Nfa &nfa, const std::map<State, unsigned long> &state_labels,
    float pct = 0.18, float eps = -1);

} // end of namespace
