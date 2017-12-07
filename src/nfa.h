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

class Nfa
{
protected:
    std::set<State> final_states;
    State initial_state;
    std::unordered_map<State, std::unordered_map<Symbol, std::set<State>>>
    transitions;

    void set_transitions(const std::vector<TransFormat> &trans);
    void set_final_states(const std::vector<std::string> &finals);
    void set_initial_state(const std::string &init);

public:
    Nfa() : initial_state{0} {}
    ~Nfa() {}

    virtual void read_from_file(std::ifstream &input);
    void read_from_file(const char *input);

    std::set<State> get_final_states() const {return final_states;}
    State get_initial_state() const {return initial_state;}
    unsigned long state_count() const {return transitions.size();}

    bool is_state(State state) const {
        return transitions.find(state) != transitions.end();
    }

    bool is_final(State state) const {
        return final_states.find(state) != final_states.end();
    }

    bool has_selfloop_to_self(State s) const;
    std::map<State,State> get_paths() const;
    std::map<State,std::set<State>> pred() const;
    std::map<State,std::set<State>> succ() const;
    void merge_states(const std::map<State,State> &mapping);
    void print(std::ostream &out = std::cout) const;
    void clear_final_state_transitions();

    virtual bool accept(const Word word, unsigned length) const;
};


/// Faster manipulation with transitions as in NFA class.
/// This class should be used only for computing state frequencies or computing
/// the number of accepted words. No modification of states and rules after
/// initialization is recommended.
class FastNfa : public Nfa
{
private:
    /// state + symbol = set of states
    /// faster then map, however harder to modify
    std::vector<std::vector<State>> trans_vector;
    std::map<State,State> state_map;

    static const unsigned shift = 8;
    static const unsigned alph_size = 256;

public:
    FastNfa() : Nfa{} {}
    ~FastNfa() {}

    std::map<State,State> get_state_map() const {return state_map;}
    std::map<State,State> get_reversed_state_map() const;
    std::vector<State> get_final_state_idx() const;
    virtual void read_from_file(std::ifstream &input) override;
    using Nfa::read_from_file;
    void build();

    template<typename Handler>
    void parse_word(const Word word, unsigned length, Handler handler) const;
};


//^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
// inline methods implementation of FastNfa class
//^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

template<typename Handler>
void FastNfa::parse_word(
    const Word word, unsigned length, Handler handler) const
{
    std::set<State> actual{state_map.at(initial_state)};

    for (unsigned i = 0; i < length && !actual.empty(); i++) {
        std::set<State> next;
        for (auto j : actual) {
            assert ((j << shift) + word[j] < trans_vector.size());
            auto trans = trans_vector[(j << shift) + word[i]];
            if (!trans.empty()) {
                for (auto k : trans) {
                    handler(k);
                    next.insert(k);
                }
            }
        }
        actual = std::move(next);
    }   
}

//^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
// inline methods implementation of Nfa class
//^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

inline bool Nfa::accept(const Word word, unsigned length) const
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
