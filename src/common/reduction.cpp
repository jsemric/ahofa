/// @author Jakub Semric
/// 2018

#include <iostream>
#include <sstream>
#include <algorithm>
#include <fstream>
#include <cassert>

#include "pcap_reader.hpp"
#include "aux.hpp"
#include "nfa.hpp"
#include "reduction.hpp"

#define SHOW false

// extern std::mutex pcapreader::bpf_compile_mux;

namespace reduction {

using namespace std;

map<State, unsigned long> read_state_labels(
    const Nfa &nfa, const string &fname)
{
    map<State, unsigned long> ret;
    ifstream in{fname};
    if (!in.is_open()) {
        throw runtime_error("error loading NFA");
    }

    string buf;
    while (getline(in, buf)) {
        // remove '#' comment
        buf = buf.substr(0, buf.find("#"));
        if (buf == "") {
            continue;
        }
        istringstream iss(buf);
        State s;
        unsigned long l;
        if (!(iss >> s >> l)) {
            throw runtime_error("invalid state labels syntax");
        }
        if (!nfa.is_state(s)) {
            throw runtime_error("invalid NFA state: " + to_string(s));
        }
        ret[s] = l;
    }
    in.close();
    return ret;
}

float prepare_and_prune(
    Nfa &nfa, const map<State, unsigned long> &state_freq, size_t old_sc,
    float pct)
{
    // change the reduction ratio in order to adjust pruning
    double new_sc = nfa.state_count();
    cerr << old_sc << " "<< new_sc << endl;
    assert(old_sc - new_sc > 0);
    pct -= pct - old_sc * pct / new_sc;

    // adjust state frequencies, remove merged states
    map<State, size_t> freq;
    for (auto i : state_freq)
    {
        if (nfa.is_state(i.first))
        {
            freq[i.first] = i.second;
        }
    }

    return prune(nfa, freq, pct);
}

float gradient_merge(
    Nfa &nfa, const map<State, unsigned long> &state_freq,
    float pct, float threshold)
{
    auto suc = nfa.succ();
    auto depth = nfa.state_depth();
    map<State,State> mapping;
    auto rules = nfa.split_to_rules();
    int cnt_merged = 0;
    set<State> to_merge;
    float error = 0;

    set<State> actual{nfa.get_initial_state()};
    set<State> visited{nfa.get_initial_state()};

    while (!actual.empty())
    {
        set<State> next;
        for (auto state : actual)
        {
            auto freq = state_freq.at(state);
            if (freq == 0)
            {
                continue;
            }
            for (auto next_state : suc[state])
            {
                if (visited.find(next_state) == visited.end())
                {
                    if (state_freq.at(next_state) > 0)
                    {

                        float diff = 1.0 * state_freq.at(next_state) / freq;

                        if (depth[state] > 2 && diff >= threshold)
                        {
                            error += 1 - diff;
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
    pct -= pct - scnt * pct / (scnt - cnt_merged);

    // prune the rest
    nfa.merge_states(mapping);
    auto freq = state_freq;
    for (auto i : mapping)
    {
        freq.erase(i.first);
    }

    return error;
}

void display_heatmap(const FastNfa &nfa, map<State,size_t> &freq)
{
    ofstream out1{"freq.txt"};
    for (auto i : freq)
    {
        out1 << i.first << " " << i.second << endl;
    }
    out1.close();
    ofstream out2{"automaton.fa"};
    nfa.print(out2);
    out2.close();
    system("python3 draw_nfa.py automaton.fa -f freq.txt");
}

float nfold_merge(
    FastNfa &nfa, const string &capturefile, float pct, float th,
    size_t count, size_t iterations)
{
    FastNfa &nfa2 = nfa;
    size_t old_sc = nfa.state_count();
    pcap_t *pcap;
    char err_buf[4096] = "";

    if (!(pcap = pcap_open_offline(capturefile.c_str(), err_buf))) 
    {
        throw std::ios_base::failure(
            "cannot open pcap file '" + capturefile + "'");
    }

    pcap_t *ret = pcap;
    float error = 0;
    map<State,size_t> all_freq;
    while (ret && iterations--)
    {
        // compute freq
        size_t total = 0;
        vector<size_t> state_freq(nfa2.state_count());
        ret = pcapreader::process_payload(
            pcap,
            [&] (const unsigned char *payload, unsigned len)
            {
                total++;
                nfa2.label_states(state_freq, payload, len);
            }, count);
        // remap frequencies
        map<State,size_t> freq;
        auto state_map = nfa2.get_reversed_state_map();
        size_t total_freq = 0;
        size_t zero_freq = 0;
        for (unsigned long i = 0; i < nfa2.state_count(); i++)
        {
            freq[state_map[i]] = state_freq[i];
            all_freq[state_map[i]] = state_freq[i];
            total_freq += state_freq[i];
            zero_freq += state_freq[i] == 0;
        }
        // compute % reduction in each turn
        auto old_sc = nfa2.state_count();
        error = gradient_merge(nfa2, freq, 0.05, th);
        nfa2.build();
        float new_sc = nfa2.state_count();

        
        cerr << "Total freq: " << total_freq << endl;
        cerr << "Zero freq: " << zero_freq << endl;
        cerr << "Error: " << error << endl;
        cerr << "Reduced: " << new_sc / old_sc << endl;
        cerr << "State cnt: " << new_sc << endl;
        cerr << "-----\n";
        
        if (SHOW)
        {
            display_heatmap(nfa2, freq);
            getchar();
        }
    }

    return error + prepare_and_prune(nfa2, all_freq, old_sc, pct);
}

float prune(
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

        return error;
    }
    catch (out_of_range &e)
    {
        string errmsg =
            "invalid index in state frequencies in 'prune' function ";
        errmsg += e.what();
        throw out_of_range(errmsg);
    }
}

float merge_and_prune(
    Nfa &nfa, const map<State, unsigned long> &state_freq, float pct,
    float threshold)
{
    auto old_sc = nfa.state_count();
    float error = gradient_merge(nfa, state_freq, pct, threshold);
    return error + prepare_and_prune(nfa, state_freq, old_sc, pct);
}

}
