/// @author Jakub Semric
/// 2017

#include <iostream>
#include <sstream>
#include <fstream>
#include <vector>
#include <cassert>
#include <map>

#include "aux.h"
#include "nfa.h"

using namespace reduction;

//^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
// implementation of GeneralNfa class methods
//^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

StrVec GeneralNfa::read_from_file(const char *input)
{
    std::ifstream in{input};
    if (!in.is_open()) {
        throw std::runtime_error("error loading NFA");
    }
    auto res = read_from_file(in);
    in.close();
    return res;
}

StrVec GeneralNfa::read_from_file(std::ifstream &input)
{
    bool no_final = true;
    std::string buf, init;
    std::set<std::string> state_set;
    std::map<std::string, State> state_map;
    std::vector<TransFormat> trans;

    // reading initial state
    std::getline(input, init);
    state_set.insert(init);

    // reading transitions
    while (std::getline(input, buf)) {
        std::istringstream iss(buf);
        std::string s1, s2, a;
        if (!(iss >> s1 >> s2 >> a )) {
            no_final = false;
            break;
        }
        trans.push_back(TransFormat{s1, s2, a});

        state_set.insert(s1);
        state_set.insert(s2);
    }

    state_rmap = StrVec(state_set.size());
    // mapping states
    unsigned long state_count = 0;
    for (auto i : state_set) {
        state_rmap[state_count] = i;
        state_map[i] = state_count++;
    }

    // setting initial state
    set_initial_state(init, state_map);
    //initial_state = state_map[init];

    // setting transitions
    set_transitions(trans, state_map);

    // set final states
    std::vector<std::string> finals;
    if (!no_final) {
        do {
            if (state_map.find(buf) != state_map.end()) {
                finals.push_back(buf);
            }
        } while (std::getline(input, buf));
    }
    set_final_states(finals, state_map);

    // return reversed mapping of the states
    return state_rmap;
}

void GeneralNfa::set_initial_state(
    const std::string &init,
    const std::map<std::string, State> &state_map)
{
    (void)state_map;
    initial_state = std::stoi(init);
}

void GeneralNfa::set_final_states(
    const std::vector<std::string> &finals,
    const std::map<std::string, State> &state_map)
{
    (void)state_map;
    for (auto i : finals) {
        final_states.insert(std::stoi(i));
    }
}

//^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
// implementation of FastNfa class methods
//^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

void FastNfa::set_transitions(
    const std::vector<TransFormat> &trans,
    const std::map<std::string, State> &state_map)
{
    // initializing transitions
    state_max = state_map.size() - 1;
    transitions = std::vector<std::vector<unsigned long>>(
        state_map.size()*alph_size);

    for (auto i : trans) {
        unsigned long idx = (state_map.at(i.first) << shift) +
            hex_to_int(i.third);
        assert (idx < transitions.size());
        transitions[idx].push_back(state_map.at(i.second));
    }
}

void FastNfa::set_final_states(
    const std::vector<std::string> &finals,
    const std::map<std::string, State> &state_map)
{
    for (auto i : finals) {
        final_states.insert(state_map.at(i));
    }
}


void FastNfa::print(std::ostream &out, bool usemap) const
{
    auto fmap = [this, &usemap](unsigned int x) {
        return usemap ? this->state_rmap[x] : std::to_string(x);
    };

    out << this->initial_state << "\n";

    for (unsigned long i = 0; i <= state_max; i++) {
        size_t idx = i << shift;
        for (unsigned j = 0; j < alph_size; j++) {
            if (!transitions[idx + j].empty()) {
                for (auto k : transitions[idx + j]) {
                    out << fmap(i) << " " << fmap(k) << " " << int_to_hex(j)
                        << "\n";
                }
            }
        }
    }

    for (auto i : final_states) {
        out << fmap(i) << "\n";
    }
}

std::vector<unsigned> FastNfa::get_states_depth() const
{
    std::vector<unsigned> state_depth(state_max + 1);
    std::set<unsigned long> actual{initial_state};
    std::set<unsigned long> all{initial_state};
    unsigned cnt = 1;

    while (not actual.empty()) {
        std::set<unsigned long> new_gen;
        for (auto i : actual) {
            size_t seg = i << shift;
            for (size_t j = seg; j < seg + 256; j++) {
                for (auto k : transitions[j]) {
                    if (all.find(k) == all.end()) {
                        all.insert(k);
                        new_gen.insert(k);
                        state_depth[k] = cnt;
                    }
                }
            }
        }
        actual = std::move(new_gen);
        cnt++;
    }

    return state_depth;
}

std::map<State,std::string> FastNfa::get_rules() const
{
    std::map<State, std::string> ret;
    return ret;
}

//^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
// implementation of Nfa class methods
//^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

void Nfa::set_transitions(
    const std::vector<TransFormat> &trans,
    const std::map<std::string, State> &state_map)
{
    (void)state_map;
    for (auto i : trans) {
        // XXX unsafe
        State pstate = std::stoi(i.first);
        State qstate = std::stoi(i.second);
        Symbol symbol = hex_to_int(i.third);
        transitions[pstate][symbol].insert(qstate);
        transitions[qstate];
        //std::cerr << i.first << " " << i.second << " " << qstate << "\n";
    }
}

/// Compute predeceasing states of all states.
std::map<State,std::set<State>> Nfa::pred() const
{
    std::map<State,std::set<State>> ret;
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
std::map<State,std::set<State>> Nfa::succ() const
{
    std::map<State,std::set<State>> ret;
    for (auto i : transitions) {
        for (auto j : i.second) {
            for (auto k : j.second) {
                ret[i.first].insert(k);
            }
        }
    }
    return ret;
}

void Nfa::merge_states(const std::map<State,State> &mapping)
{
    // verify mapping
    for (auto i : mapping) {
        if (!is_state(i.first) || !is_state(i.second) ||
            initial_state == i.first)
        {
            throw std::runtime_error("cannot merge states");
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

        if (!is_final(src_state)) {
            // final states have implicit self-loop over the input alphabet
            // no reason to redirect transitions
            for (auto j : transitions[merged_state]) {
                Symbol symbol = j.first;
                set_union(transitions[src_state][symbol], j.second);
            }

            if (is_final(merged_state)) {
                std::cerr << i.first << " " << i.second << "\n";
                final_states.erase(merged_state);
                final_states.insert(src_state);
            }
        }
        // remove state and its transitions
        transitions.erase(merged_state);
    }

    // redirect transitions to removed states
    for (auto &states : transitions) {
        for (auto &rules : states.second) {
            std::set<State> new_states;
            for (auto i : rules.second) {
                auto a = mapping.find(i);
                State s = a != mapping.end() ? a->second : i;
                new_states.insert(s);
            }
            rules.second = std::move(new_states);
        }
    }

    clear_final_state_transitions();
}

std::map<State,State> Nfa::get_paths() const
{
    std::set<State> visited{initial_state};
    std::map<State,State> ret;
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
        std::set<State> actual = pr[f];
        while (!actual.empty()) {
            std::set<State> next;
            set_union(visited, actual);
            for (auto i : actual) {
                ret[i] = f;
                for (auto j : pr[i]) {
                    if (visited.find(j) == visited.end()) {
                        next.insert(j);
                    }
                }
            }
            actual = std::move(next);
        }
    }

    return ret;
}

void Nfa::print(std::ostream &out) const
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

void Nfa::clear_final_state_transitions()
{
    for (auto i : final_states) {
        transitions[i].clear();
    }
}