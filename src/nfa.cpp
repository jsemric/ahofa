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

void Nfa::read_from_file(
    const char *input, map<State,set<State>> *final_states_map)
{
    ifstream in{input};
    if (!in.is_open()) {
        throw runtime_error("error loading NFA");
    }
    read_from_file(in, final_states_map);
    in.close();
}

void Nfa::read_from_file(ifstream &input,
    map<State,set<State>> *final_states_map)
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
        // XXX unsafe
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
        if (!is_state(i.first) || !is_state(i.second) ||
            initial_state == i.first)
        {
            throw runtime_error("cannot merge states");
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
            cerr << i.first << " " << i.second << "\n";
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

map<State,State> Nfa::get_paths() const
{
    set<State> visited{initial_state};
    map<State,State> ret;
    // states which cannot be merged
    ret[initial_state] = initial_state;
    // find state with self-loop to self
    for (auto i : transitions.at(initial_state)) {
        bool cond  = true;
        for (auto j : i.second) {
            if (has_selfloop_to_self(j)) {
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

bool Nfa::has_selfloop_to_self(State s) const
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
            if (states.size() > 1 ||
                (states.size() == 1 && states.find(f) == states.end()))
            {
                remove = false;
                break;
            }
        }
        if (remove) {
            transitions[f].clear();
            //cerr << "now!\n";
        }
    }
}

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
    for (auto i : transitions) {
        state_map[i.first] = cnt++;
    }

    trans_vector = vector<vector<State>>(state_count() * alph_size);
    for (auto i : transitions) {
        size_t idx_state = state_map[i.first] << shift;
        for (auto j : i.second) {
            size_t symbol = j.first;
            for (auto state : j.second) {
                assert(idx_state + symbol < trans_vector.size());
                trans_vector[idx_state + symbol].push_back(state_map[state]);
            }
        }
    }
}

void FastNfa::read_from_file(
    ifstream &input, map<State,set<State>> *final_states_map)
{
    Nfa::read_from_file(input, final_states_map);
    build();
}

map<State,State> FastNfa::get_reversed_state_map() const
{
    map<State,State> ret;
    for (auto i : state_map) {
        ret[i.second] = i.first;
    }
    return ret;
}

vector<State> FastNfa::get_final_state_idx() const
{
    vector<State> ret;
    for (auto i : final_states) {
        ret.push_back(state_map.at(i));
    }
    return ret;
}
