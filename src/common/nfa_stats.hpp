/// @author Jakub Semric
/// 2018

#pragma once

#include <iostream>
#include <ostream>
#include <vector>

#include "nfa.hpp"

namespace reduction
{

using namespace std;

struct NfaStats
{
    // array data
    vector<size_t> reduced_states_arr;
    vector<size_t> target_states_arr;

    size_t total;   // total packets
    size_t fp_a;    // accepted false positive
    size_t pp_a;    // accepted positive positive
    size_t fp_c;    // classification false positive
    size_t pp_c;    // classification positive positive
    size_t all_c;   // all additional classifications

    NfaStats(size_t data_reduced_size = 1, size_t data_target_size = 1) :
        reduced_states_arr(data_reduced_size),
        target_states_arr(data_target_size), total{0},
        fp_a{0}, pp_a{0}, fp_c{0}, pp_c{0}, all_c{0} {}

    ~NfaStats() = default;

    void aggregate(const NfaStats &d)
    {
        if (d.reduced_states_arr.size() != reduced_states_arr.size() ||
            d.target_states_arr.size() != target_states_arr.size())
        {
            throw runtime_error(
                "NfaStats.aggregate: different array size");
        }

        total += d.total;
        fp_a += d.fp_a;
        pp_a += d.pp_a;
        fp_c += d.fp_c;
        pp_c += d.pp_c;
        all_c += d.all_c;

        for (size_t i = 0; i < d.reduced_states_arr.size(); i++) {
            reduced_states_arr[i] += d.reduced_states_arr[i];
        }
        for (size_t i = 0; i < d.target_states_arr.size(); i++) {
            target_states_arr[i] += d.target_states_arr[i];
        }
    }
};

vector<pair<string,NfaStats>> compute_nfa_stats(
    const NfaArray &target, const NfaArray &reduced,
    const vector<string> &pcaps, bool consistent = false);

}
