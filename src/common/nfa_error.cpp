
#include <iostream>
#include <ostream>
#include <exception>
#include <vector>
#include <stdexcept>
#include <ctype.h>

#include <boost/filesystem.hpp>

#include "nfa_error.hpp"
#include "nfa.hpp"
#include "pcap_reader.hpp"

using namespace reduction;
using namespace std;

namespace fs = boost::filesystem;

NfaError::NfaError(
    const FastNfa &a1, const FastNfa &a2, const vector<string> &pcaps,
    bool consistent)
    : target{a1}, reduced{a2}, pcaps{pcaps}, consistent{consistent}
{
	for (auto i : pcaps)
	{
	    char err_buf[4096] = "";
    	pcap_t *p;

    	if (!(p = pcap_open_offline(i.c_str(), err_buf)))
    	{
			throw runtime_error("Not a valid pcap file: \'" + i + "'");
		}
		pcap_close(p);
	}

    fidx_target = target.get_final_state_idx();
    fidx_reduced = reduced.get_final_state_idx();
}

void NfaError::process_pcaps()
{
    for (auto p : pcaps) {
    	ErrorStats stats(reduced.state_count(), target.state_count());
        try {
            // just checking how many packets are accepted
            pcapreader::process_payload(
                p.c_str(),
                [&] (const unsigned char *payload, unsigned len)
                {
                    compute_error(stats, payload, len);
                });
            results.push_back(pair<string,ErrorStats>(p,stats));
        }
        catch (exception &e) {
            // other error
            cerr << "\033[1;31mERROR\033[0m " << e.what() << "\n";
            break;
        }
    }
}

void NfaError::compute_error(
	ErrorStats &stats, const unsigned char *payload, unsigned plen) const
{
    vector<bool> bm(reduced.state_count());
    reduced.parse_word(payload, plen, [&bm](State s){ bm[s] = 1; });
    int match1 = 0;
    stats.total++;
    for (size_t i = 0; i < fidx_reduced.size(); i++)
    {
        size_t idx = fidx_reduced[i];
        if (bm[idx])
        {
            match1++;
            stats.nfa1_data[idx]++;
        }
    }

    stats.accepted_reduced += match1 > 0;

    if (match1 || consistent)
    {    
        int match2 = 0;
        // something was matched, lets find the difference
        vector<bool> bm(target.state_count());
        target.parse_word(payload, plen, [&bm](State s){ bm[s] = 1; });
        for (size_t i = 0; i < fidx_target.size(); i++)
        {
            size_t idx = fidx_target[i];
            if (bm[idx])
            {
                match2++;
                stats.nfa2_data[idx]++;
            }
        }

        if (match1 != match2)
        {
            stats.wrongly_classified++;
            if (consistent && match2 > match1)
            {
                throw runtime_error(
                	"Reduced automaton ain't over-approximation!\n");
            }
        }
        else
        {
            stats.correctly_classified++;
        }

        if (match2)
        {
            stats.accepted_target++;
        }
    }
}