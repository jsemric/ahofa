/// @author Jakub Semric
/// 2018

#include <iostream>
#include <ostream>
#include <sstream>
#include <exception>
#include <vector>
#include <map>
#include <stdexcept>
#include <ctype.h>
#include <getopt.h>

#include "nfa.hpp"
#include "pcap_reader.hpp"
#include "reduction.hpp"
#include "nfa_error.hpp"

using namespace reduction;
using namespace std;

const unsigned NW = 2;

template<typename T>
ErrorStats reduce(const FastNfa &target, vector<string> test_data, T func)
{
    FastNfa reduced = target;
    func(reduced);
    NfaError err{target, reduced, test_data, NW};
    err.start();
    ErrorStats aggr(target.state_count(), reduced.state_count());

    for (auto i : err.get_result())
        aggr.aggregate(i.second);

    return aggr;
}

map<State,size_t> compute_freq(const FastNfa &nfa, string pcap)
{
    vector<size_t> state_freq(nfa.state_count());

    pcapreader::process_payload(
        pcap.c_str(),
        [&] (const unsigned char *payload, unsigned len)
        {
            nfa.label_states(state_freq, payload, len);
        });
    
    auto state_map = nfa.get_reversed_state_map();
    map<State,size_t> freq;

    for (unsigned long i = 0; i < nfa.state_count(); i++)
        freq[state_map[i]] = state_freq[i];

    return freq;
}

int main()
{
    FastNfa target;
    target.read_from_file("min-snort/backdoor.rules.fa");

    string train_data = "pcaps/geant.pcap";
    vector<string> test_data{
        "pcaps/geant2.pcap2","pcaps/week2.pcap","pcaps/meter4-1.pcap8"};
    float pct = 0.16;

    auto freq = compute_freq(target, train_data);
    // use merge and prune
    auto res_merge = reduce(
        target, test_data,
        [&freq, pct](FastNfa &nfa)
        {
            merge_and_prune(nfa, freq, pct);
        });

    auto res_prune = reduce(
        target, test_data,
        [&freq, pct](FastNfa &nfa)
        {
            prune(nfa, freq, pct);
        });

    for (float threshold = 0.9; threshold < 1; threshold += 0.02)
    {
        FastNfa reduced = target;
        // 4 thresholds
        for (int count = 10000; count < 50000; count += 10000)
        {
            // 4 counts
            for (int iter = 4; iter < 20; iter += 4)
            {
                // 4 iterations
                auto res = reduce(
                    target, test_data,
                    [&](FastNfa &nfa)
                    {
                        nfold_merge(
                            nfa, train_data, pct, threshold, count, iter);
                    });
            }
        }
    }

    return 0;    
}