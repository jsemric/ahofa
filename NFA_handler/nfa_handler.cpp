#include <iostream>
#include <exception>
#include <vector>
#include <boost/program_options.hpp>
#include <ctype.h>
#include <stdexcept>

#include "nfa.h"
#include "pcap_reader.h"

const char *helpstr =
"Usage: ./nfa_handler [OPTIONS]\n"
"Compute the number of accepted packets by NFA if none of options -a, -f, -b are set.\n";

namespace po = boost::program_options;

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
    // TODO copy and parallel run in loop
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
        // TODO nfa -> steal freqs
    }

    nfa.print(out);
    out << "=====\n";
    nfa.print_freq(out);
}

void compute_accepted(const std::vector<std::string> &pcaps, const std::string &str,
                      std::ostream &out)
{
    NFA nfa = read_nfa(str);
    unsigned long total = 0;
    unsigned long accepted = 0;
    // TODO copy and parallel run in loop
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
    // TODO copy and parallel run in loop
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

    out << str1 << " <+> " << str2 << "\n";
    out << "=====\n";
    out << "Total: " << total << "\n";
    out << "Error: " << acc2 << "/" << acc1 << " " << 1.0*(acc2 - acc1)/total << "\n";
}

void conflict_options(const po::variables_map &vm, const std::string &opt1,
                      const std::string &opt2)
{
    if (vm.count(opt1) && !vm[opt1].defaulted() && vm.count(opt2) && !vm[opt2].defaulted())
    {
        throw std::logic_error(std::string("Conflicting options '") + opt1 + "' and '" + opt2
                               + "'.");
    }
}

int main(int argc, char **argv) {

    std::string nfa_str1, nfa_str2, out;
    std::vector<std::string> pcaps;

    po::options_description desc("Allowed options");
    desc.add_options()
    ("help,h", "show this help and exit")
    ("pcap,p", po::value(&pcaps)->value_name("PCAP")->required(), "pcap file")
    ("target,t", po::value(&nfa_str1)->value_name("NFA")->required(), "file with NFA")
    ("automaton,a", po::value(&nfa_str2)->value_name("NFA"), "file with NFA, which suppose to be "
     "an over-approximation of the first NFA set by -t option")
    ("packet-freq,f", "compute a packet frequency of each state NFA")
    ("byte-freq,b", "compute frequencies of bytes instead of packets NFA")
    ("output,o", po::value(&out)->value_name("FILE"), "output file");

    po::variables_map varmap;
    try {
        po::store(po::parse_command_line(argc, argv, desc), varmap);
        if (argc == 1 || varmap.count("help")) {
            std::cout << helpstr << std::endl;
            std::cout << desc << std::endl;
            return 0;
        }
        po::notify(varmap);
        conflict_options(varmap, "byte-freq", "automaton");
        conflict_options(varmap, "packet-freq", "automaton");
        conflict_options(varmap, "packet-freq", "byte-freq");

        std::ostream *output = &std::cout;
        if (varmap.count("output")) {
            output = new std::ofstream{out};
            if (!static_cast<std::ofstream*>(output)->is_open()) {
                std::cerr << "Error: cannot open output file " << out << "\n";
                return 1;
            }
        }

        if (varmap.count("byte-freq") || varmap.count("packet-freq")) {
            compute_frequencies(pcaps, nfa_str1, varmap.count("packet-freq"), *output);
        }
        else if (varmap.count("automaton")) {
            compute_error_fast(pcaps, nfa_str1, nfa_str2, *output);
        }
        else {
            compute_accepted(pcaps, nfa_str1, *output);
        }

        if (varmap.count("output")) {
            static_cast<std::ofstream*>(output)->close();
            delete output;
        }
    }
    catch (std::exception &e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
