#include <iostream>
#include <exception>
#include <vector>
#include <ctype.h>
#include <stdexcept>
#include <ctime>
#include <getopt.h>

#include "nfa.h"
#include "pcap_reader.h"

const char *helpstr =
"Usage: ./nfa_handler [OPTIONS]\n"
"Compute the number of accepted packets by NFA if none of options -a, -f, -b are set.\n";
/*
NFA read_nfa(const std::string &fname) {
    NFA nfa;
    std::ifstream input{fname};

    if (!input.is_open()) {
        throw std::runtime_error("cannot open NFA file '" + fname + "'");
    }

    nfa.read_from_file(input);
    input.close();

    return nfa;
}

void compute_frequencies(const std::vector<std::string> &pcaps, const std::string &str,
                         bool one_per_packet, std::ostream &out)
{
    NFA nfa = read_nfa(str);
    nfa.compute_depth();
    for (auto &p : pcaps) {
        PcapReader pcap_reader;
        pcap_reader.open(p);
        if (one_per_packet) {
            pcap_reader.process_packets(
            [&nfa](const unsigned char* payload, unsigned length)
            {
                nfa.compute_packet_frequency(payload, length);
            }, ~0LL);
        }
        else {
            pcap_reader.process_packets(
            [&nfa](const unsigned char* payload, unsigned length)
            {
                nfa.compute_frequency(payload, length);
            }, ~0LL);
        }
    }

    nfa.print_freq(out);
}

void compute_accepted(const std::vector<std::string> &pcaps, const std::string &str,
                      std::ostream &out)
{
    NFA nfa = read_nfa(str);
    unsigned long total = 0;
    unsigned long accepted = 0;

    for (auto &p : pcaps) {
        PcapReader pcap_reader;
        pcap_reader.open(p);
        pcap_reader.process_packets(
        [&total, &accepted, &nfa](const unsigned char* payload, unsigned length)
        {
            total++;
            if (nfa.accept(payload, length)) {
                accepted++;
            }
        }, ~0LL);
    }

    out << "Total: " << total << "\n";
    out << "Accepted: " << accepted << "\n";
}

void compute_error(const std::vector<std::string> &pcaps, const std::string &str1,
                   const std::string &str2, std::ostream &out)
{
    NFA nfa1 = read_nfa(str1);
    NFA nfa2 = read_nfa(str2);
    unsigned long total = 0;
    unsigned long acc1 = 0;
    unsigned long acc2 = 0;
    for (auto &p : pcaps) {
        PcapReader pcap_reader;
        pcap_reader.open(p);
        pcap_reader.process_packets(
        [&total, &acc1, &acc2, &nfa1, &nfa2](const unsigned char* payload, unsigned length)
        {
            total++;
            bool b1 = nfa1.accept(payload, length);
            bool b2 = nfa2.accept(payload, length);
            acc1 += b1; acc2 += b2;
            if (b1 && !b2) {
                std::cerr << "NFA is not over-approximation of target NFA.\n";
                exit(1);
            }
        }, ~0LL);
    }

    out << "Total: " << total << "\n";
    out << str1 << ": " << acc1 << "\n";
    out << str2 << ": " << acc2 << "\n";
}

void compute_error_fast(const std::vector<std::string> &pcaps, const std::string &str1,
                        const std::string &str2, std::ostream &out)
{
    NFA nfa1 = read_nfa(str1);
    NFA nfa2 = read_nfa(str2);
    unsigned long total = 0;
    unsigned long acc1 = 0;
    unsigned long acc2 = 0;

    for (auto &p : pcaps) {
        PcapReader pcap_reader;
        pcap_reader.open(p);
        pcap_reader.process_packets(
        [&total, &acc1, &acc2, &nfa1, &nfa2](const unsigned char* payload, unsigned length)
        {
            total++;
            if (nfa2.accept(payload, length)) {
                acc2++;
                acc1 += nfa1.accept(payload, length);
            }
        }, ~0LL);
    }

    out << total << " " << acc1 <<  " " << acc2 << std::endl;
}
*/

void compute_error(
        const NFA &aut1,
        const NFA &aut2,
        const std::vector<std::string> &pcaps)
{
    for (int i = 0; i < pcaps.size(); i++) {
        /* divide work */
        /* lamba for each thread */
        pcapreader::process_payload(
            pcaps[i].c_str(),
            [&] (const unsigned char *payload, unsigned len){
                ;
            },
            ~0ULL);
        /* wait for process */
        /* sum up error*/
    }
}

int main(int argc, char **argv) {

    std::string nfa_str1, nfa_str2, ofname;
    std::vector<std::string> pcaps;

    const char *output;
    int nworkers;
    int opt_cnt = 1;
    int c;
    while ((c = getopt(argc, argv, "ho:n:")) != -1) {
        opt_cnt += 2;
        switch (c) {
            case 'h':
                fprintf(stderr, "%s", helpstr);
                return 0;
            case 'o':
                output = optarg;
                break;
            case 'n':
                nworkers = std::stoi(optarg);
                break;
            default:
                return 1;
        }
    }

    try {
        if (nworkers < 1 || nworkers >= 4/*XXX*/) {
            throw std::runtime_error("invalid number of cores");
        }

        /* check nfa files */

        /* check output file */

        /* start computation */

        /* signal handling */

        /* output */
    }
    catch (std::exception &e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
