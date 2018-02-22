/// @author Jakub Semric
/// 2017

#include <iostream>
#include <sstream>
#include <fstream>
#include <vector>
#include <cassert>
#include <map>

#include "aux.hpp"
#include "nfa.hpp"

using namespace reduction;
using namespace std;

//^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
// implementation of Nfa class methods
//^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Nfa::Nfa(const Nfa &nfa)
{
    initial_state = initial_state;
    final_states = final_states;
    transitions = nfa.transitions;
}

void Nfa::read_from_file(const char *input)
{
    ifstream in{input};
    if (!in.is_open()) {
        throw runtime_error("error loading NFA");
    }
    read_from_file(in);
    in.close();
}

void Nfa::read_from_file(ifstream &input)
{
    bool no_final = true;
    string buf, init;
    set<string> state_set;
    map<string, State> state_map;
    vector<TransFormat> trans;

    // reading initial state
    getline(input, init);
    // setting initial state
    set_initial_state(init);

    // reading transitions
    while (getline(input, buf)) {
        istringstream iss(buf);
        string s1, s2, a;
        if (!(iss >> s1 >> s2 >> a )) {
            no_final = false;
            break;
        }
        trans.push_back(TransFormat{s1, s2, a});
    }
    // setting transitions
    set_transitions(trans);

    // set final states
    vector<string> finals;
    if (!no_final) {
        do {
            finals.push_back(buf);
        } while (getline(input, buf));
    }

    set_final_states(finals);
}

void Nfa::set_initial_state(const string &init)
{
    initial_state = stoi(init);
}

void Nfa::set_final_states(const vector<string> &finals)
{
    for (auto i : finals) {
        State s = stoi(i);
        final_states.insert(s);
//        transitions[s];
//        if (transitions.find(s) == transitions.end()) cerr << s << "\n";
    }
}

void Nfa::set_transitions(const vector<TransFormat> &trans)
{
    for (auto i : trans) {
        State pstate = stoi(i.first);
        State qstate = stoi(i.second);
        Symbol symbol = hex_to_int(i.third);
        transitions[pstate][symbol].insert(qstate);
        transitions[qstate];
    }
}

/// Compute predeceasing states of all states.
map<State,set<State>> Nfa::pred() const
{
    map<State,set<State>> ret;
    for (auto i : transitions) {
        for (auto j : i.second) {
            for (auto k : j.second) {
                ret[k].insert(i.first);
            }
        }
    }
    return ret;
}

/// Compute succeeding states of all states.
map<State,set<State>> Nfa::succ() const
{
    map<State,set<State>> ret;
    for (auto i : transitions) {
        for (auto j : i.second) {
            for (auto k : j.second) {
                ret[i.first].insert(k);
            }
        }
    }
    return ret;
}

set<State> Nfa::get_states() const
{
    set<State> ret;
    for (auto i : transitions) {
        ret.insert(i.first);
    }
    return ret;
}

void Nfa::merge_states(const map<State,State> &mapping)
{
    // verify mapping
    for (auto i : mapping) {
        if (!is_state(i.first) || !is_state(i.second))
            
        {
            throw runtime_error("cannot merge non-existing states");
        }
        if (initial_state == i.first) {
            throw runtime_error("cannot merge initial state");
        }
    }

    // redirect transitions from removed states
    // merge i.first to i.second
    for (auto i : mapping) {
        State merged_state = i.first;
        State src_state = i.second;
        if (merged_state == src_state) {
            // senseless merging state to itself
            continue;
        }

        for (auto j : transitions[merged_state]) {
            Symbol symbol = j.first;
            set_union(transitions[src_state][symbol], j.second);
        }

        if (is_final(merged_state)) {
            // cerr << i.first << " " << i.second << "\n";
            final_states.erase(merged_state);
            final_states.insert(src_state);
        }

        // remove state and its transitions
        transitions.erase(merged_state);
    }

    // redirect transitions that lead to removed states
    for (auto &states : transitions) {
        for (auto &rules : states.second) {
            set<State> new_states;
            for (auto i : rules.second) {
                auto a = mapping.find(i);
                State s = a != mapping.end() ? a->second : i;
                new_states.insert(s);
            }
            rules.second = move(new_states);
        }
    }

    clear_final_state_selfloop();
}
/*
map<State,set<State>> Nfa::split() const {
    map<State,set<State>> ret;
    // states which cannot be merged
    auto pr = pred();
    for (auto f : final_states) {
        ret[f].insert(f);
        set<State> actual = pr[f];
        set<State> visited{f};
        while (!actual.empty()) {
            set<State> next;
            set_union(visited, actual);
            for (auto i : actual) {
                ret[i].insert(f);
                for (auto j : pr[i]) {
                    if (visited.find(j) == visited.end()) {
                        next.insert(j);
                    }
                }
            }
            actual = move(next);
        }
    }

    return ret;
}
*/
map<State,State> Nfa::split_to_rules() const
{
    set<State> visited{initial_state};
    map<State,State> ret;
    // states which cannot be merged
    ret[initial_state] = initial_state;
    // find state with self-loop to self
    for (auto i : transitions.at(initial_state)) {
        bool cond  = true;
        for (auto j : i.second) {
            if (has_selfloop_over_alph(j)) {
                visited.insert(j);
                ret[j] = initial_state;
                cond = false;
                break;
            }
        }
        if (cond == false) {
            break;
        }
    }

    auto pr = pred();
    for (auto f : final_states) {
        ret[f] = f;
        set<State> actual = pr[f];
        while (!actual.empty()) {
            set<State> next;
            set_union(visited, actual);
            for (auto i : actual) {
                ret[i] = f;
                for (auto j : pr[i]) {
                    if (visited.find(j) == visited.end()) {
                        next.insert(j);
                    }
                }
            }
            actual = move(next);
        }
    }

    return ret;
}

void Nfa::print(ostream &out) const
{
    out << initial_state << "\n";

    for (auto i : transitions) {
        for (auto j : i.second) {
            for (auto k : j.second) {
                out << i.first << " " << k << " " << int_to_hex(j.first)
                    << "\n";
            }
        }
    }

    for (auto i : final_states) {
        out << i << "\n";
    }
}

bool Nfa::has_selfloop_over_alph(State s) const
{
    int cnt = 0;
    for (auto i : transitions.at(s)) {
        cnt++;
        if (i.second.find(s) == i.second.end()) {
           return false;
        }
    }

    return cnt == 256;
}

void Nfa::clear_final_state_selfloop()
{
    for (auto f : final_states) {
        bool remove = true;
        for (auto &i : transitions[f]) {
            auto &states = i.second;
            // remove states only with self-loop
            if (states.size() > 1 ||
                (states.size() == 1 && states.find(f) == states.end()))
            {
                remove = false;
                break;
            }
        }
        if (remove) {
            transitions[f].clear();
        }
    }
}

void Nfa::collapse_final_states()
{
    map<State,State> mapping;
    State first = initial_state;
    for (auto i : final_states) {
        if (first == initial_state) {
            first = i;
        }
        else {
            mapping[i] = first;
        }
    }
    merge_states(mapping);
}
/*
template<typename FuncType>
void Nfa::breadth_first_search(FuncType handler) const
{
    set<State> actual{initial_state};
    set<State> visited{initial_state};

    while (!actual.empty()) {
        set<State> next;
        for (auto s : actual) {
            handler(s);
            for (auto i : transitions.at(s)) {
                for (auto j : i.second) {
                    if (visited.find(j) == visited.end()) {
                        next.insert(j);
                    }
                }
            }
        }
        set_union(visited, actual);
        actual = move(next);
    }
}*/

map<State,unsigned> Nfa::state_depth() const
{
    set<State> actual{initial_state};
    set<State> visited{initial_state};
    unsigned depth = 0;
    map<State,unsigned> ret;

    while (!actual.empty()) {
        set<State> next;
        for (auto s : actual) {
            ret[s] = depth;
            for (auto i : transitions.at(s)) {
                for (auto j : i.second) {
                    if (visited.find(j) == visited.end()) {
                        next.insert(j);
                    }
                }
            }
        }
        depth++;
        set_union(visited, actual);
        actual = move(next);
    }

    return ret;
}

void Nfa::reduce(map<State,size_t> state_freq)
{
    auto suc = succ();
    auto depth = state_depth();
    map<State,State> mapping;
    auto rules = split_to_rules();
    int cnt=0;
    int cnt2=0;
    set<State> to_merge;

    set<State> actual{initial_state};
    set<State> visited{initial_state};

    while (!actual.empty())
    {
        set<State> next;
        for (auto state : actual)
        {
            auto freq = state_freq[state];
            for (auto next_state : suc[state])
            {
                if (visited.find(next_state) == visited.end())
                {
                    if (state_freq[next_state] > 0)
                    {
                        if (false && depth[state] > 2 &&
                            1.0 * state_freq[next_state] / freq > 0.999)
                        {
                            cnt2++;
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
                    else
                    {
                        cnt++;
                        mapping[next_state] = rules[next_state];
                    }
                    next.insert(next_state);
                    visited.insert(next_state);
                }
            }
        }
        //set_union(visited, actual);
        actual = move(next);
    }

/*
    for (auto i : state_freq) {
        auto state = i.first;
        auto freq = i.second;
        if (is_final(state)) continue;
        if (freq == 0) {
            cnt++;
            mapping[state] = rules[state];
            to_merge.insert(rules[state]);
        }
        else if (1 && depth[state] > 2) {
            for (auto j : suc[state]) {
                if (1.0 * state_freq[j] / freq > 0.999) {
                    cnt2++;
                    if (mapping.find(state) != mapping.end()) {
                        mapping[j] = mapping[state];
                    }
                    else {
                        mapping[j] =  state;   
                    }
                    to_merge.insert(state);
                }
            }
        }
    }
*/
    for (auto i : to_merge) {
        if (mapping.find(i) != mapping.end()) {
            cerr << i << "\n";
            throw runtime_error("FATAL!");
        }

    }
    merge_states(mapping);
    cerr << cnt2 << " " << cnt + cnt2 << endl;
}

//^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
// implementation of FastNfa class methods
//^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

FastNfa::FastNfa(const Nfa &nfa) : Nfa{nfa}
{
    build();
}

void FastNfa::build()
{
    // map states
    State cnt = 0;
    for (auto i : transitions)
    {
        state_map[i.first] = cnt++;
    }

    trans_vector = vector<vector<State>>(state_count() * alph_size);
    for (auto i : transitions)
    {
        size_t idx_state = state_map[i.first] << shift;
        for (auto j : i.second)
        {
            size_t symbol = j.first;
            for (auto state : j.second)
            {
                assert(idx_state + symbol < trans_vector.size());
                trans_vector[idx_state + symbol].push_back(state_map[state]);
            }
        }
    }
}

void FastNfa::read_from_file(ifstream &input)
{
    Nfa::read_from_file(input);
    build();
}

map<State,State> FastNfa::get_reversed_state_map() const
{
    map<State,State> ret;
    for (auto i : state_map)
    {
        ret[i.second] = i.first;
    }
    return ret;
}

vector<State> FastNfa::get_final_state_idx() const
{
    vector<State> ret;
    for (auto i : final_states)
    {
        ret.push_back(state_map.at(i));
    }
    return ret;
}
