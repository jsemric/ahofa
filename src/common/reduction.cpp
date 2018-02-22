/// @author Jakub Semric
/// 2018

#include <iostream>
#include <algorithm>
#include <cassert>

#include "aux.hpp"
#include "reduction.hpp"

namespace reduction {

using namespace std;

void prune(
    Nfa &nfa, const map<State, unsigned long> &state_freq,
    float pct, float eps)
{
    assert(eps == -1 || (eps > 0 && eps <= 1));
    assert(pct > 0 && pct <= 1);

    map<State,State> merge_map;
    // merge only states with corresponding rule, which is defined by final
    // state
    auto rule_map = nfa.split_to_rules();
    auto depth = nfa.state_depth();

    // sort state_freq
    // mark which states to prune
    vector<State> sorted_states;
    State init = nfa.get_initial_state();
    // total packets
    size_t total = 0;

    for (auto i : state_freq)
    {
        if (!nfa.is_final(i.first) && i.first != init)
        {
            sorted_states.push_back(i.first);
        }
        total = total < i.second ? i.second : total;
    }

    try {
        // sort states in ascending order according to state packet frequencies
        // and state depth
        sort(
            sorted_states.begin(), sorted_states.end(),
            [&state_freq, &depth](State x, State y)
            {
                auto _x = state_freq.at(x);
                auto _y = state_freq.at(y);
                if (_x == _y)
                {
                    return depth.at(x) > depth.at(y);
                }   
                else
                {
                    return _x < _y;
                }
            });

        float error = 0;
        size_t state_count = nfa.state_count();
        size_t removed = 0;
        
        if (eps != -1)
        {
            // use error rate
            while (error < eps && removed < sorted_states.size())
            {
                State state = sorted_states[removed];
                merge_map[state] = rule_map[state];
                removed++;
                error += (1.0 * state_freq.at(state)) / total;
            }
        }
        else 
        {
            // use pct rate
            size_t to_remove = (1 - pct) * state_count;
            while (removed < to_remove && removed < sorted_states.size())
            {
                State state = sorted_states[removed];
                merge_map[state] = rule_map[state];
                removed++;
                error += (1.0 * state_freq.at(state)) / total;
            }
        }

        nfa.merge_states(merge_map);
        size_t new_sc = state_count - removed;
        size_t reduced_to = new_sc * 100 / state_count;
        cerr << "Reduction: " << new_sc << "/" << state_count
            << " " << reduced_to << "%\n";
        cerr << "Predicted error: " << error << endl;
    }
    catch (out_of_range &e)
    {
        string errmsg =
            "invalid index in state frequencies in 'prune' function ";
        errmsg += e.what();
        throw out_of_range(errmsg);
    }
}

void merge_and_prune(
    Nfa &nfa, const map<State, unsigned long> &state_freq, float pct)
{
    auto suc = nfa.succ();
    auto depth = nfa.state_depth();
    map<State,State> mapping;
    auto rules = nfa.split_to_rules();
    int cnt_merged = 0;
    set<State> to_merge;

    set<State> actual{nfa.get_initial_state()};
    set<State> visited{nfa.get_initial_state()};

    while (!actual.empty())
    {
        set<State> next;
        for (auto state : actual)
        {
            auto freq = state_freq.at(state);
            for (auto next_state : suc[state])
            {
                if (visited.find(next_state) == visited.end())
                {
                    if (state_freq.at(next_state) > 0)
                    {
                        if (depth[state] > 2 &&
                            1.0 * state_freq.at(next_state) / freq > 0.999)
                        {
                            cnt_merged++;
                            if (mapping.find(state) != mapping.end())
                            {
                                mapping[next_state] = mapping[state];
                            }
                            else
                            {
                                mapping[next_state] = state;   
                            }
                        }
                        
                    }
                    next.insert(next_state);
                    visited.insert(next_state);
                }
            }
        }
        actual = move(next);
    }

    // just for verification
    for (auto i : to_merge) {
        if (mapping.find(i) != mapping.end()) {
            throw runtime_error("FAILURE");
        }
    }

    // change the reduction ratio in order to adjust pruning
    double scnt = nfa.state_count();
    pct -= scnt * pct / (scnt - cnt_merged);

    // prune the rest
    nfa.merge_states(mapping);
    auto freq = state_freq;
    for (auto i : mapping)
    {
        freq.erase(i.first);
    }

    prune(nfa, freq, pct);
}

}