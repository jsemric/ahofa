/// @author Jakub Semric
///
/// Probabilistic automaton template class definition and methods.
/// @file pdfa.h
///
/// Unless otherwise stated, all code is licensed under a
/// GNU General Public Licence v2.0

#ifndef __nfa_h
#define __nfa_h

#include <iostream>
#include <vector>
#include <map>
#include <set>
#include <exception>
#include <stdio.h>
#include <ctype.h>

template <typename T1, typename T2, typename T3>
struct Triple
{
    T1 first;
    T2 second;
    T3 third;

    Triple() = default;
    Triple(T1 t1, T2 t2, T3 t3) : first{t1}, second{t2}, third{t3} {};
};

class FiniteAutomatonException : public std::exception
{
private:
    std::string errorMessage;

public:
    FiniteAutomatonException(std::string str="invalid syntax of NFA") :
        errorMessage{str} {};

    virtual const char* what() const throw()
    {
        return errorMessage.c_str();
    }
};

/// Template class for NFA.
class NFA
{
private:
    /// state + symbol = set of states
    std::vector<std::vector<unsigned long>> transitions;
    std::set<unsigned long> final_states;
    unsigned long initial_state;
    unsigned long state_max;
    std::vector<unsigned long> state_freq;
    std::vector<unsigned> state_depth;
    std::vector<bool> visited_states;

    static const unsigned shift = 8;
    static const unsigned alph_size = 256;

public:
    NFA();
    ~NFA();

    template<typename U>
    inline bool accept(const U word, unsigned length) const;

    template<typename U>
    inline void compute_frequency(const U word, unsigned length);

    template<typename U>
    inline void compute_packet_frequency(const U word, unsigned length);

    void print(std::ostream &out = std::cout) const;
    void print_freq(std::ostream &out = std::cout) const;
    void read_from_file(std::ifstream &input);
    void compute_depth();

private:
    void update_freq() noexcept;
};

/*
static void print_readable(const unsigned char *payload, unsigned length) {
    for (unsigned i = 0; i < length; i++) {
        if (isprint(payload[i])) {
            printf("%c", payload[i]);
        }
        else {
            printf("\\x%.2x", payload[i]);
        }
    }
    printf("\n");
}
*/
template<typename U>
inline bool NFA::accept(const U word, unsigned length) const
{
    std::set<unsigned long> actual{initial_state};

    for (unsigned i = 0; i < length && !actual.empty(); i++) {
        std::set<unsigned long> tmp;
        for (auto j : actual) {
            assert ((j << shift) + word[j] < transitions.size());
            auto trans = transitions[(j << shift) + word[i]];
            if (!trans.empty()) {
                for (auto k : trans) {
                    if (final_states.find(k) != final_states.end()) {
//                        print_readable(word, i+1);
                        return true;
                    }
                    tmp.insert(k);
                }
            }
        }
        actual = std::move(tmp);
    }

    return false;
}

template<typename U>
inline void NFA::compute_packet_frequency(const U word, unsigned length)
{
    std::set<unsigned long> actual{initial_state};

    for (unsigned i = 0; i < length && !actual.empty(); i++) {
        std::set<unsigned long> tmp;
        for (auto j : actual) {
            assert ((j << shift) + word[j] < transitions.size());
            auto trans = transitions[(j << shift) + word[i]];
            if (!trans.empty()) {
                for (auto k : trans) {
                    visited_states[k] = true;
                    /*  assuming that packet cannot leave a final state
                    if (final_states.find(k) != final_states.end()) {
                        update_freq();
                        return;
                    }
                    */
                    tmp.insert(k);
                }
            }
        }
        actual = std::move(tmp);
    }

    update_freq();
}

template<typename U>
inline void NFA::compute_frequency(const U word, unsigned length)
{
    std::set<unsigned long> actual{initial_state};

    for (unsigned i = 0; i < length && !actual.empty(); i++) {
        std::set<unsigned long> tmp;
        for (auto j : actual) {
            assert ((j << shift) + word[j] < transitions.size());
            auto trans = transitions[(j << shift) + word[i]];
            if (!trans.empty()) {
                for (auto k : trans) {
                    state_freq[k]++;
                    tmp.insert(k);
                }
            }
        }
        actual = std::move(tmp);
    }
}

inline void NFA::update_freq() noexcept
{
    for (size_t i = 0; i <= state_max; i++) {
        state_freq[i] += visited_states[i];
        visited_states[i] = false;
    }
}

#endif