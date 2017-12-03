/// @author Jakub Semric
/// 2017

#pragma once

#include <iostream>
#include <vector>
#include <map>
#include <unordered_map>
#include <set>
#include <exception>
#include <cassert>
#include <stdio.h>
#include <ctype.h>

#include "aux.h"

namespace reduction {

// transitions serialization format
using TransFormat = Triple<std::string, std::string, std::string>;
// string vector
using StrVec = std::vector<std::string>;
using Symbol = uint8_t;
using State = unsigned long;
// since we use only packets words can only consist of bytes
using Word = const unsigned char*;

class GeneralNFA
{
protected:
    std::set<State> final_states;
    State initial_state;
    StrVec state_rmap;

    virtual unsigned long state_count() const = 0;
    virtual void set_transitions(
        const std::vector<TransFormat> &trans,
        const std::map<std::string, State> &state_map) = 0;

    virtual void set_final_states(
        const std::vector<std::string> &finals,
        const std::map<std::string, State> &state_map) = 0;

    virtual void set_initial_state(
        const std::string &init,
        const std::map<std::string, State> &state_map) = 0;

public:
    GeneralNFA() : initial_state{0} {}
    virtual ~GeneralNFA() {}

    virtual StrVec read_from_file(std::ifstream &input);
    virtual StrVec read_from_file(const char *input);
    virtual bool accept(const Word word, unsigned length) const = 0;
};


/// Faster manipulation with transitions as in NFA2 class.
/// This class should be used only for computing state frequencies or computing
/// the number of accepted words. No modification of states and rules after 
/// initialization is recommended.
class NFA : public GeneralNFA
{
private:
    /// state + symbol = set of states
    /// faster then map, however harder to modify
    std::vector<std::vector<State>> transitions;
    State state_max;

    static const unsigned shift = 8;
    static const unsigned alph_size = 256;

    virtual void set_transitions(
        const std::vector<TransFormat> &trans,
        const std::map<std::string, State> &state_map) override;
    virtual void set_final_states(
        const std::vector<std::string> &finals,
        const std::map<std::string, State> &state_map) override;
    virtual void set_initial_state(
        const std::string &init,
        const std::map<std::string, State> &state_map) override
    {
        initial_state = state_map.at(init);
    }

public:
    NFA() : GeneralNFA{} {}
    ~NFA() {}

    inline bool accept(Word word, unsigned length) const;
    inline void label_states(
        Word word, unsigned length, std::vector<bool> &labeled) const;

    void print(std::ostream &out = std::cout, bool usemap = true) const;
    void compute_depth();

    virtual unsigned long state_count() const override {return state_max + 1;}
    std::vector<unsigned> get_states_depth() const;
    StrVec get_rmap() const { return state_rmap; }
};

class NFA2 : public GeneralNFA
{
private:
    //                 state          rules:     symbol  -> states 
    std::unordered_map<State, std::unordered_map<Symbol, std::set<State>>>
    transitions;

    virtual void set_transitions(
        const std::vector<TransFormat> &trans,
        const std::map<std::string, State> &state_map) override;
    virtual void set_final_states(
        const std::vector<std::string> &finals,
        const std::map<std::string, State> &state_map) override;
    virtual void set_initial_state(
        const std::string &init,
        const std::map<std::string, State> &state_map) override
    {
        (void)state_map;
        initial_state = std::stoi(init);
    }

public:
    NFA2() : GeneralNFA{} {}
    ~NFA2() {}

    virtual unsigned long state_count() const override {
        return transitions.size();
    }

    inline bool is_state(State state) const {
        return transitions.find(state) != transitions.end();
    }
    inline bool is_final(State state) const {
        return final_states.find(state) != final_states.end();
    }

    std::map<State,std::set<State>> pred() const;
    std::map<State,std::set<State>> succ() const;
    void merge_states(std::map<State,State> &mapping);

    inline bool accept(Word word, unsigned length) const;
};

//^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
// inline methods implementation of NFA class
//^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

inline bool NFA::accept(Word word, unsigned length) const
{
    std::set<State> actual{initial_state};

    for (unsigned i = 0; i < length && !actual.empty(); i++) {
        std::set<State> next;
        for (auto j : actual) {
            assert ((j << shift) + word[j] < transitions.size());
            auto trans = transitions[(j << shift) + word[i]];
            if (!trans.empty()) {
                for (auto k : trans) {
                    if (final_states.find(k) != final_states.end()) {
                        return true;
                    }
                    next.insert(k);
                }
            }
        }
        actual = std::move(next);
    }

    return false;
}

inline void NFA::label_states(
    const Word word, unsigned length, std::vector<bool> &labeled) const
{
    std::set<State> actual{initial_state};
    labeled = std::vector<bool>(state_max);
    labeled[initial_state] = true;

    for (unsigned i = 0; i < length; i++) {
        std::set<State> next;
        for (auto j : actual) {
            assert ((j << shift) + word[j] < transitions.size());
            auto trans = transitions[(j << shift) + word[i]];
            if (!trans.empty()) {
                for (auto k : trans) {
                    labeled[k] = true;
                    next.insert(k);
                }
            }
        }
        actual = std::move(next);
    }
}

//^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
// inline methods implementation of NFA2 class
//^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

inline bool NFA2::accept(const Word word, unsigned length) const
{
    std::set<State> actual{initial_state};

    for (unsigned i = 0; i < length && !actual.empty(); i++) {
        std::set<State> next;
        for (auto j : actual) {
            auto trans = transitions.at(j).at(word[i]);
            if (!trans.empty()) {
                for (auto k : trans) {
                    if (final_states.find(k) != final_states.end()) {
                        return true;
                    }
                    next.insert(k);
                }
            }
        }
        actual = std::move(next);
    }

    return false;
}

}   // end of namespace reduction