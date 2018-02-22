/// @author Jakub Semric
/// 2017

#pragma once

#include <map>
#include <vector>
#include <set>

#include "nfa.hpp"

namespace reduction {

using namespace std;
// propagate final states to theirs predecessors
void prune(
    Nfa &nfa, const map<State, unsigned long> &state_freq,
    float pct = 0.2, float eps = -1);

void merge_and_prune(
    Nfa &nfa, const map<State, unsigned long> &state_freq, float pct);

vector<set<State>> armc(
    Nfa &nfa, const vector<set<size_t>> &state_freq);

} // end of namespace
