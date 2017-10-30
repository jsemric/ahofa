#include <iostream>
#include <sstream>
#include <fstream>
#include <vector>
#include <cassert>
#include <map>

#include "nfa.h"

static int hex_to_int(const std::string &str)
{
    int x = 0;
    int m = 1;
    for (int i = str.length()-1; i > 1; i--) {
        int c = tolower(str[i]);
        x += m * (c - (c > '9' ? 'a' - 10 : '0'));
        m *= 16;
    }

    return x;
}

static std::string int_to_hex(const unsigned num)
{
    assert (num <= 255);
    char buf[16] = "";
    sprintf(buf, "0x%.2x", num);
    return buf;
}

NFA::NFA()
{

}

NFA::~NFA()
{

}

StrVec NFA::read_from_file(const char *input) {
    std::ifstream in{input};
    if (!in.is_open()) {
        throw std::runtime_error("error loading NFA");
    }
    auto res = read_from_file(in);
    in.close();
    return res;
}

StrVec NFA::read_from_file(std::ifstream &input)
{
    bool no_final = true;
    std::string buf, init;
    std::set<std::string> state_set;
    std::map<std::string, unsigned long> state_map;
    std::vector<Triple<std::string, std::string, std::string>> trans;

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
        trans.push_back(Triple<std::string, std::string, std::string>{s1, s2, a});

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
    initial_state = state_map[init];
    state_max = state_count - 1;

    // initializing transitions
    transitions = std::vector<std::vector<unsigned long>>(state_count*alph_size);
    state_freq = std::vector<unsigned long>(state_count);
    visited_states = std::vector<bool>(state_count);

    // setting transitions
    for (auto i : trans) {
        unsigned long idx = (state_map[i.first] << shift) + hex_to_int(i.third);
        assert (idx < transitions.size());
        transitions[idx].push_back(state_map[i.second]);
    }

    if (!no_final) {
        do {
            auto tmp = state_map.find(buf);
            if (tmp != state_map.end()) {
                final_states.insert(state_map[buf]);
            }
        } while (std::getline(input, buf));
    }

    return state_rmap;
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
                    out << fmap(i) << " " << fmap(k) << " " << int_to_hex(j) << "\n";
                }
            }
        }
    }

    for (auto i : final_states) {
        out << fmap(i) << "\n";
    }
}

void NFA::compute_depth()
{
    state_depth = std::vector<unsigned>(state_max + 1);
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
}

void NFA::print_freq(std::ostream &out, bool usemap) const
{
    auto fmap = [this, &usemap](unsigned int x) {
        return usemap ? this->state_rmap[x] : std::to_string(x);
    };

    for (unsigned long i = 0; i <= state_max; i++) {
        out << fmap(i) << " " << state_freq[i] << " " << state_depth[i] << "\n";
    }
}
