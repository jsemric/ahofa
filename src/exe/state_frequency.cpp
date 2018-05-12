/// @author Jakub Semric
/// 2018

#include <iostream>
#include <ostream>
#include <vector>
#include <map>

#include "nfa.hpp"
#include "pcap_reader.hpp"

using namespace reduction;
using namespace std;

map<State, unsigned long> compute_freq(
    const Nfa &nfa, pcap_t *pcap, size_t count=~0UL)
{
    map<State, unsigned long> freq;
    NfaArray m(nfa);

    vector<size_t> state_freq(nfa.state_count());

    pcapreader::process_payload(
        pcap,
        [&] (const unsigned char *payload, unsigned len)
        {
            m.label_states(state_freq, payload, len);
        }, count);

    // remap frequencies
    auto state_map = m.get_reversed_state_map();
    for (unsigned long i = 0; i < nfa.state_count(); i++)
    {
        freq[state_map[i]] = state_freq[i];
    }

    return freq;
}

map<State, unsigned long> compute_freq(
    const Nfa &nfa, string fname, size_t count=~0UL)
{
    char err_buf[4096] = "";
    pcap_t *pcap;
    if (!(pcap = pcap_open_offline(fname.c_str(), err_buf)))
        throw std::ios_base::failure("cannot open pcap file '" + fname + "'");

    return compute_freq(nfa, pcap, count);
}

int main(int argc, char **argv)
{
    try{
        if (argc > 3) {
            cerr << "computing packet frequency\n";
            string nfa_str = argv[1];
            string pcap = argv[2];
            Nfa nfa = Nfa::read_from_file(nfa_str);
            auto freq = compute_freq(nfa, pcap);

            ofstream out{argv[3]};
            if (!out.is_open())
                throw runtime_error("cannot open output file");

            for (auto i : freq)
                out << i.first << " " << i.second << endl;
            out.close();
        }
        else {
            cerr << "3 arguments required: NFA PCAP OUTPUT\n";
        }
    }
    catch (exception &e) {
        cerr << "\033[1;31mERROR\033[0m " << e.what() << endl;
        return 1;
    }
    return 0;
}
