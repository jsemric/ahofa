/// @author Jakub Semric
/// 2018

#include <iostream>
#include <ostream>
#include <vector>
#include <ctype.h>
#include <stdlib.h>

#include "nfa_stats.hpp"
#include "nfa.hpp"
#include "pcap_reader.hpp"

namespace reduction
{

/// Computes statistics of the reduced automaton.
/// @param target original automaton
/// @param reduced reduced automaton (has to be over-approximation of target!)
/// @param pcaps filenames of PCAP files
/// @param consistent if set, check whether reduced is over-approximation
/// @return  vector of pairs, where the first item is the PCAP file and the
/// second item is statistic of reduced automaton over this file
vector<pair<string,NfaStats>> compute_nfa_stats(
    const NfaArray &target, const NfaArray &reduced, const vector<string> &pcaps,
    bool consistent)
{
    // for (auto i : pcaps) {
    //     char err_buf[4096] = "";
    //     pcap_t *p;
    // 
    //     if (!(p = pcap_open_offline(i.c_str(), err_buf)))
    //         throw runtime_error("Not a valid pcap file: \'" + i + "'");
    // 
    //     pcap_close(p);
    // }

    auto fidx_target = target.get_final_state_idx();
    auto fidx_reduced = reduced.get_final_state_idx();
    vector<pair<string,NfaStats>> results;

    for (auto p : pcaps) {
        NfaStats stats(reduced.state_count(), target.state_count());
        try {
            //pcapreader::process_payload(
            pcapreader::process_strings(
                p.c_str(),
                [&] (const unsigned char *payload, unsigned len)
                {
                    // bit vector of reached states
                    // 0 - not reached, 1 - reached
                    vector<bool> bm(reduced.state_count());
                    //cerr << "8\n";
                    reduced.parse_word(
                        payload, len, [&bm](State s){ bm[s] = 1; });
                    //cerr << "10\n";
                    int match1 = 0;
                    stats.total++;
                    for (size_t i = 0; i < fidx_reduced.size(); i++) {
                        size_t idx = fidx_reduced[i];
                        if (bm[idx]) {
                            match1++;
                            assert(idx < stats.reduced_states_arr.size());
                            stats.reduced_states_arr[idx]++;
                        }
                    }

                    int match2 = 0;
                    if (match1 || consistent) {
                        // something was matched, lets find the difference
                        vector<bool> bm(target.state_count());
                        target.parse_word(
                            payload, len, [&bm](State s){ bm[s] = 1; });
                        for (size_t i = 0; i < fidx_target.size(); i++) {
                            size_t idx = fidx_target[i];
                            if (bm[idx]) {
                                match2++;
                                assert(idx < stats.target_states_arr.size());
                                stats.target_states_arr[idx]++;
                            }
                        }

                        if (match1 != match2) {
                            stats.fp_c++;
                            stats.all_c += match1 - match2;
                            if (consistent && match2 > match1)
                                throw runtime_error(
                                    "Reduced automaton ain't "
                                    "over-approximation!\n");
                        }
                        else if (match1) {
                            stats.pp_c++;
                        }
                        // accepted packet false/positive positive
                        if (consistent) {
                            if (match1 && match2)
                                stats.pp_a++;
                            else if (match1 && !match2)
                                stats.fp_a++;
                        }
                        else {
                            if (match2) stats.pp_a++; else stats.fp_a++;
                        }
                    }
                });
            results.push_back(pair<string,NfaStats>(p,stats));
        }
        catch (exception &e) {
            // other error
            cerr << "Error while computing NFA stats: " << e.what() << "\n";
            break;
        }
    }
    return results;
}
}
