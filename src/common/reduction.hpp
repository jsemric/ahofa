/// @author Jakub Semric
/// 2017

#pragma once

#include <map>
#include <vector>
#include <set>

#include "nfa.hpp"

namespace reduction {

using namespace std;

float prune(
    Nfa &nfa, const map<State, unsigned long> &state_freq, float pct);

map<State, unsigned long> compute_freq(
    const Nfa &nfa, string fname, size_t count = ~0ULL);

map<State, unsigned long> compute_freq(
    const Nfa &nfa, pcap_t *pcap, size_t count = ~0ULL);

pair<float,size_t> reduce(
    Nfa &nfa, const string &samples, float pct = -1, float th = 0.995,
    size_t iterations = 0, bool pre = false, float max_freq = 0.1);

int merge(
    Nfa &nfa, const map<State, unsigned long> &state_freq, float threshold,
    float max_freq);
} // end of namespace
