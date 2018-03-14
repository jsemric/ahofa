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
float prune(
    Nfa &nfa, const map<State, unsigned long> &state_freq,
    float pct = 0.2, float eps = -1);

float merge_and_prune(
    Nfa &nfa, const map<State, unsigned long> &state_freq, float pct,
    float threshold = 0.995);

float nfold_merge(
    FastNfa &nfa, const string &capturefile, float pct, float th = 0.995,
    size_t count = 10000, size_t iterations = 100000);

} // end of namespace
