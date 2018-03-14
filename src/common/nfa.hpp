/// @author Jakub Semric
/// 2018

#pragma once
#pragma GCC diagnostic ignored "-Wtautological-compare"

#include <iostream>
#include <vector>
#include <map>
#include <unordered_map>
#include <set>
#include <exception>
#include <cassert>
#include <stdio.h>
#include <ctype.h>

#include "aux.hpp"

namespace reduction {

using namespace std;

// transitions serialization format
using TransFormat = Triple<string, string, string>;
// string vector
using StrVec = vector<string>;
using Symbol = uint8_t;
using State = unsigned long;
// since we use only packets words can only consist of bytes
using Word = const unsigned char*;

auto default_lambda = [](){;};

class Nfa
{
protected:
    set<State> final_states;
    State initial_state;
    unordered_map<State, unordered_map<Symbol, set<State>>> transitions;

    void set_transitions(const vector<TransFormat> &trans);
    void set_final_states(const vector<string> &finals);
    void set_initial_state(const string &init);

public:
    Nfa() : initial_state{0} {}
    Nfa(const Nfa &nfa);
    ~Nfa() {}

    // IO
    virtual void read_from_file(ifstream &input);
    void read_from_file(const char *input);
    void print(ostream &out = cout) const;

    // setters, getters
    set<State> get_final_states() const { return final_states;}
    State get_initial_state() const { return initial_state;}
    set<State> get_states() const;
    unsigned long state_count() const { return transitions.size();}

    bool is_state(State state) const
    {
        return transitions.find(state) != transitions.end();
    }

    bool is_final(State state) const
    {
        return final_states.find(state) != final_states.end();
    }

    // helpful functions
    bool has_selfloop_over_alph(State s) const;
    map<State,State> split_to_rules() const;
    map<State,set<State>> pred() const;
    map<State,set<State>> succ() const;
    map<State,unsigned> state_depth() const;
    void clear_final_state_selfloop();
    void merge_final_states(bool merge_all_states = false);
    void remove_unreachable();
    std::set<State> breadth_first_search(
        set<State> actual = set<State>{}) const;

    // essential
    void merge_states(const map<State,State> &mapping);

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
    vector<vector<State>> trans_vector;
    /// state label -> index
    map<State,State> state_map;

    static const unsigned shift = 8;
    static const unsigned alph_size = 256;

public:
    FastNfa() : Nfa{} {}
    FastNfa(const Nfa &nfa);
    FastNfa(const FastNfa &nfa);

    ~FastNfa() {}

    map<State,State> get_state_map() const {return state_map;}
    map<State,State> get_reversed_state_map() const;
    vector<State> get_final_state_idx() const;
    size_t get_initial_state_idx() const { return state_map.at(initial_state);}

    using Nfa::read_from_file;

    virtual void read_from_file(ifstream &input) override;

    void build();

    void label_states(
        vector<size_t> &state_freq, const unsigned char *payload,
        unsigned len) const;

    template<typename FuncType1, typename FuncType2 = decltype(default_lambda)>
    void parse_word(
        const Word word, unsigned length, FuncType1 visited_state_handler,
        FuncType2 loop_handler = default_lambda) const;

    virtual bool accept(const Word word, unsigned length) const override;
};


//^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
// inline methods implementation of FastNfa class
//^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

template<typename FuncType1, typename FuncType2>
void FastNfa::parse_word(
    const Word word, unsigned length, FuncType1 visited_state_handler,
    FuncType2 loop_handler) const
{
    set<State> actual{state_map.at(initial_state)};

    for (unsigned i = 0; i < length && !actual.empty(); i++)
    {
        set<State> next;
        for (auto j : actual)
        {
            assert ((j << shift) + word[i] < trans_vector.size());
            auto trans = trans_vector[(j << shift) + word[i]];

            if (!trans.empty())
            {
                for (auto k : trans)
                {
                    // do something with visited state, use this information
                    visited_state_handler(k);
                    next.insert(k);
                }
            }
        }
        // call function to do something at the end of current iteration
        loop_handler();
        actual = move(next);
    }
}

inline bool FastNfa::accept(const Word word, unsigned length) const
{
    set<State> actual{state_map.at(initial_state)};

    for (unsigned i = 0; i < length && !actual.empty(); i++) {
        set<State> next;
        for (auto j : actual) {
            assert ((j << shift) + word[j] < trans_vector.size());
            auto trans = trans_vector[(j << shift) + word[i]];
            if (!trans.empty()) {
                for (auto k : trans) {
                    // do something with visited state, use this information
                    if (final_states.find(k) != final_states.end()) {
                        return true;
                    }
                    next.insert(k);
                }
            }
        }
        actual = move(next);
    }

    return false;
}

//^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
// inline methods implementation of Nfa class
//^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

inline bool Nfa::accept(const Word word, unsigned length) const
{
    set<State> actual{initial_state};

    for (unsigned i = 0; i < length && !actual.empty(); i++) {
        set<State> next;
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
        actual = move(next);
    }

    return false;
}

}   // end of namespace reduction
