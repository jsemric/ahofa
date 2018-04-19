/// @author Jakub Semric
/// 2018

#include <iostream>
#include <sstream>
#include <fstream>
#include <vector>
#include <cassert>
#include <map>

#include "nfa.hpp"

using namespace reduction;
using namespace std;

static uint8_t hex_to_int(const string &str)
{
    assert(str.size() > 2);

    int x = 0;
    int m = 1;
    for (int i = str.length()-1; i > 1; i--) {
        int c = tolower(str[i]);
        x += m * (c - (c > '9' ? 'a' - 10 : '0'));
        m *= 16;
    }
    assert(x >= 0 && x < 256);

    return static_cast<uint8_t>(x);
}

static string int_to_hex(const unsigned num)
{
    assert (num <= 255);
    char buf[16] = "";
    sprintf(buf, "0x%.2x", num);
    return buf;
}

template<typename T>
static void set_union(set<T> &s1, const set<T> &s2) {
    for (auto i : s2) {
        s1.insert(i);
    }
}

//^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
// implementation of Nfa class methods
//^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Nfa::Nfa(const Nfa &nfa)
{
    initial_state = nfa.initial_state;
    final_states = nfa.final_states;
    transitions = nfa.transitions;
}

Nfa Nfa::read_from_file(const string input)
{
    ifstream in{input};
    if (!in.is_open()) {
        throw runtime_error("cannot open NFA file");
    }
    auto res = read_from_file(in);
    in.close();
    return res;
}

Nfa Nfa::read_from_file(ifstream &input)
{
    bool no_final = true;
    string buf;
    vector<TransFormat> trans;
    set<State> finals;
    State init;

    try {
        // reading initial state
        getline(input, buf);
        init = stoul(buf);

        // reading transitions
        while (getline(input, buf)) {
            istringstream iss(buf);
            string s1, s2, a;
            if (!(iss >> s1 >> s2 >> a )) {
                no_final = false;
                break;
            }
            trans.push_back(TransFormat{stoul(s1), stoul(s2), hex_to_int(a)});
        }
        // set final states
        if (!no_final) {
            do {
                finals.insert(stoul(buf));
            } while (getline(input, buf));
        }
    }
    catch (...) {
        throw runtime_error("invalid NFA syntax");
    }

    return Nfa(init, trans, finals);
}

Nfa::Nfa(State init, const vector<TransFormat> &t, const set<State> &finals) :
    initial_state{init}, final_states{finals}
{
    transitions[init];
    for (auto i : t) {
        transitions[i.first][i.third].insert(i.second);
        transitions[i.second];
    }

    for (auto i : final_states) {
        transitions[i];
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

/// TODO comment
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

/// TODO comment
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

map<State,unsigned> Nfa::state_depth() const
{
    unsigned depth = 0;
    map<State,unsigned> ret;
    set<State> actual{initial_state};
    set<State> visited{initial_state};

    while (!actual.empty())
    {
        set<State> next;
        for (auto s : actual)
        {
            ret[s] = depth;
            for (auto i : transitions.at(s))
            {
                for (auto j : i.second)
                {
                    if (visited.find(j) == visited.end())
                    {
                        next.insert(j);
                        visited.insert(j);
                    }
                }
            }
        }
        depth++;
        actual = move(next);
    }

    return ret;
}

/// TODO comment
void Nfa::merge_sl_states()
{
    map<State,State> mapping;
    set<State> s;
    auto pre = pred();
    auto suc = succ();
    for (auto state : suc[initial_state])
    {
        if (has_selfloop_over_alph(state) && pre[state].size() == 2)
        {
            s.insert(state);
        }
    }
    s.erase(initial_state);
    State first = initial_state;
    for (auto i : s) {
        if (first == initial_state)
            first = i;
        else
            mapping[i] = first;
    }

    if (!mapping.empty())
        merge_states(mapping);
}

//^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
// implementation of NfaArray class methods
//^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

NfaArray::NfaArray(const Nfa &nfa) : Nfa{nfa}
{
    // map states
    State cnt = 0;
    state_map.clear();
    for (auto i : transitions)
        state_map[i.first] = cnt++;

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

NfaArray::NfaArray(const NfaArray &nfa) : NfaArray{static_cast<Nfa>(nfa)}
{
}

/// TODO comment
map<State,State> NfaArray::get_reversed_state_map() const
{
    map<State,State> ret;
    for (auto i : state_map)
    {
        ret[i.second] = i.first;
    }
    return ret;
}

/// TODO comment
vector<State> NfaArray::get_final_state_idx() const
{
    vector<State> ret;
    for (auto i : final_states)
    {
        ret.push_back(state_map.at(i));
    }
    return ret;
}

/// TODO comment
void NfaArray::label_states(
    vector<size_t> &state_freq, const unsigned char *payload,
    unsigned len) const
{
    vector<bool> bm(state_count());
    parse_word(payload, len, [&bm](State s){ bm[s] = 1; });
    for (size_t i = 0; i < state_freq.size(); i++)
        state_freq[i] += bm[i];

    state_freq[get_initial_state_idx()]++;
}
