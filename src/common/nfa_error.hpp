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

struct ErrorStats
{
    // long data
    vector<size_t> reduced_states_arr;
    vector<size_t> target_states_arr;
    // packets statistics
    size_t total;
    size_t accepted_reduced;
    size_t accepted_target;
    size_t wrongly_classified;
    size_t correctly_classified;

    ErrorStats(size_t data_reduced_size = 1, size_t data_target_size = 1) :
        reduced_states_arr(data_reduced_size), target_states_arr(data_target_size), total{0},
        accepted_reduced{0}, accepted_target{0}, wrongly_classified{0},
        correctly_classified{0} {}

    ~ErrorStats() = default;

    void aggregate(const ErrorStats &other_data)
    {
        if (other_data.reduced_states_arr.size() != reduced_states_arr.size()
            ||
            other_data.target_states_arr.size() != target_states_arr.size())
        {
            throw runtime_error("different array size, cannot aggregate");
        }

        total += other_data.total;
        accepted_target += other_data.accepted_target;
        accepted_reduced += other_data.accepted_reduced;
        wrongly_classified += other_data.wrongly_classified;
        correctly_classified += other_data.correctly_classified;
        for (size_t i = 0; i < other_data.reduced_states_arr.size(); i++) {
            reduced_states_arr[i] += other_data.reduced_states_arr[i];
        }
        for (size_t i = 0; i < other_data.target_states_arr.size(); i++) {
            target_states_arr[i] += other_data.target_states_arr[i];
        }
    }
};

vector<pair<string,ErrorStats>> compute_error(
    const FastNfa &target, const FastNfa &reduced, const vector<string> &pcaps,
    bool consistent = false);

}