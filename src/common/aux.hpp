/// @author Jakub Semric
/// 2017

#pragma once

#include <iostream>
#include <set>


template <typename T1, typename T2, typename T3>
struct Triple
{
    T1 first;
    T2 second;
    T3 third;

    Triple() = default;
    Triple(T1 t1, T2 t2, T3 t3) : first{t1}, second{t2}, third{t3} {};
};


bool is_number(const std::string& s);

int hex_to_int(const std::string &str);

std::string int_to_hex(const unsigned num);

void print_readable(const unsigned char *payload, unsigned length);

template<typename T>
void set_union(std::set<T> &s1, const std::set<T> &s2);

// implementation

template<typename T>
void set_union(std::set<T> &s1, const std::set<T> &s2) {
    for (auto i : s2) {
        s1.insert(i);
    }
}
