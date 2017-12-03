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
// implementation of NFA class methods
//^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

StrVec GeneralNFA::read_from_file(const char *input) {
    std::ifstream in{input};
    if (!in.is_open()) {
        throw std::runtime_error("error loading NFA");
    }
    auto res = read_from_file(in);
    in.close();
    return res;
}

StrVec GeneralNFA::read_from_file(std::ifstream &input)
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

void NFA::set_transitions(
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

void NFA::set_final_states(
    const std::vector<std::string> &finals,
    const std::map<std::string, State> &state_map)
{
    for (auto i : finals) {
        final_states.insert(state_map.at(i));
    }
}


void NFA::print(std::ostream &out, bool usemap) const
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

std::vector<unsigned> NFA::get_states_depth() const
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

//^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
// implementation of NFA2 class methods
//^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

void NFA2::set_final_states(
    const std::vector<std::string> &finals,
    const std::map<std::string, State> &state_map)
{
    (void)state_map;
    for (auto i : finals) {
        final_states.insert(std::stoi(i));
    }
}

void NFA2::set_transitions(
    const std::vector<TransFormat> &trans,
    const std::map<std::string, State> &state_map)
{   
    (void)state_map;
    for (auto i : trans) {
        // XXX unsafe
        State pstate = std::stoi(i.first);
        Symbol symbol = hex_to_int(i.second);
        State qstate = std::stoi(i.third);
        transitions[pstate][symbol].insert(qstate);
    }
}

/// Compute predeceasing states of all states.
std::map<State,std::set<State>> NFA2::pred() const
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
std::map<State,std::set<State>> NFA2::succ() const
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

void NFA2::merge_states(std::map<State,State> &mapping)
{
    // verify mapping
    for (auto i : mapping) {
        if (is_state(i.first) || is_state(i.second) || initial_state == i.first)
        {
            throw std::runtime_error("cannot merge states");
        }
    }

    // complete the mapping with self-mapping states
    for (auto i : transitions) {
        if (mapping.find(i.first) == mapping.end()) {
            mapping[i.first] = i.first;
        }
    }

    std::set<State> new_final_states;

    // redirect transitions from removed states
    for (auto i : mapping) {
        if (i.second == i.first) {
            continue;
        }
        State src_state = i.second;
        for (auto j : transitions[i.first]) {
            Symbol symbol = j.first;
            for (auto state : j.second) {
                transitions[src_state][symbol].insert(state);
            }
        }
        transitions.erase(i.first);
        if (is_final(i.first)) {
            new_final_states.insert(src_state);
        }
    }

    // redirect transitions to removed states
    for (auto &states : transitions) {
        for (auto &rules : states.second) {
            std::set<State> new_states;
            for (auto i : rules.second) {
                new_states.insert(mapping[i]);
            }
            rules.second = std::move(new_states);
        }
    }

    final_states = new_final_states;
}