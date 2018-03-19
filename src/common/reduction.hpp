/// @author Jakub Semric
/// 2017

#pragma once

#include <map>
#include <vector>
#include <set>

#include "nfa.hpp"

namespace reduction {

using namespace std;

map<State, unsigned long> compute_freq(
    const FastNfa &nfa, string fname, size_t count = ~0ULL);

map<State, unsigned long> compute_freq(
    const FastNfa &nfa, pcap_t *pcap, size_t count = ~0ULL);

pair<float,size_t> reduce(
    FastNfa &nfa, const string &samples, float pct = -1, float th = 0.995,
    size_t iterations = 0, bool pre = false);

} // end of namespace
