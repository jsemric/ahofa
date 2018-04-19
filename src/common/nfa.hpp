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

namespace reduction {

using namespace std;

template <typename T1, typename T2, typename T3>
struct Triple
{
    T1 first;
    T2 second;
    T3 third;

    Triple() = default;
    Triple(T1 t1, T2 t2, T3 t3) : first{t1}, second{t2}, third{t3} {};
};


// string vector
using StrVec = vector<string>;
using Symbol = uint8_t;
using State = unsigned long;
// since we use only packets words can only consist bytes
using Word = const unsigned char*;
// transitions serialization format
using TransFormat = Triple<State, State, uint8_t>;

auto default_lambda = [](){;};

class Nfa
{
protected:
    State initial_state;
    set<State> final_states;
    unordered_map<State, unordered_map<Symbol, set<State>>> transitions;

public:
    Nfa(State init, const vector<TransFormat> &t, const set<State> &finals);
    Nfa(const Nfa &nfa);
    ~Nfa() {}

    // IO
    static Nfa read_from_file(ifstream &input);
    static Nfa read_from_file(const string input);
    void print(ostream &out = cout) const;

    // getters
    set<State> get_final_states() const { return final_states;}
    State get_initial_state() const { return initial_state;}
    set<State> get_states() const;
    unsigned long state_count() const { return transitions.size();}
    bool is_state(State state) const {
        return transitions.find(state) != transitions.end();
    }
    bool is_final(State state) const {
        return final_states.find(state) != final_states.end();
    }

    // helpful functions
    bool has_selfloop_over_alph(State s) const;
    map<State,State> split_to_rules() const;
    map<State,set<State>> pred() const;
    map<State,set<State>> succ() const;
    map<State,unsigned> state_depth() const;
    void merge_sl_states();
    void clear_final_state_selfloop();

    // essential
    virtual void merge_states(const map<State,State> &mapping);
};

/// Faster manipulation with transitions as in NFA class.
/// This class should be used only for computing state frequencies or computing
/// the number of accepted words. No modification of states and rules after
/// initialization is recommended.
class NfaArray : public Nfa
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
    NfaArray(const Nfa &nfa);
    NfaArray(const NfaArray &nfa);

    ~NfaArray() {}

    map<State,State> get_state_map() const {return state_map;}
    map<State,State> get_reversed_state_map() const;
    vector<State> get_final_state_idx() const;
    size_t get_initial_state_idx() const { return state_map.at(initial_state);}

    void label_states(
        vector<size_t> &state_freq, const unsigned char *payload,
        unsigned len) const;

    template<typename FuncType1, typename FuncType2 = decltype(default_lambda)>
    void parse_word(
        const Word word, unsigned length, FuncType1 visited_state_handler,
        FuncType2 loop_handler = default_lambda) const;

    bool accept(const Word word, unsigned length) const;

private:
    // do not use
    using Nfa::merge_states;
    using Nfa::clear_final_state_selfloop;
    using Nfa::merge_sl_states;
};


//^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
// inline methods implementation of NfaArray class
//^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

/// Parses a word through NfaArray
/// @param word packet payload or string
/// @param length number of bytes in string
/// @param visited_state_handler function which is called after a state has
/// been visited by a packet
/// @param loop_handler function which is called after one byte has been read
template<typename FuncType1, typename FuncType2>
void NfaArray::parse_word(
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

/// Parses a word through NfaArray and decides whether it is accepted.
/// @param word packet payload or string
/// @param length number of bytes in string
/// @return True if a string is accepter, false otherwise
inline bool NfaArray::accept(const Word word, unsigned length) const
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

}   // end of namespace reduction
