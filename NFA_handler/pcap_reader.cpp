/// @author Jakub Semric
///
/// Class PcapReader for reading and processing packets in packet caputure files.
/// @file pcap_reader.h
///
/// Unless otherwise stated, all code is licensed under a
/// GNU General Public Licence v2.0

// C++ headers
#include <fstream>
#include <iostream>
#include <stdio.h>
#include <system_error>
// C and pcap headers
#include <pcap.h>
#include <pcap/pcap.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <netinet/ip.h>
#include <netinet/ip6.h>
#include <netinet/tcp.h>
#include <netinet/udp.h>
#include <net/ethernet.h>
#include <string.h>
// own headers
#include "pcap_reader.h"

PcapReader::PcapReader() : pcap{0}, err_buf{""}, header{0}, packet{0}
{

}

PcapReader::PcapReader(const std::string &fname) : err_buf{""}, header{0}, packet{0}
{
    if (!(pcap = pcap_open_offline(fname.c_str(), err_buf))) {
        throw std::system_error{};
    }
}

PcapReader::~PcapReader()
{
    if (pcap) {
        pcap_close(pcap);
    }
}

void PcapReader::open(const std::string &input)
{
    if (pcap) {
        pcap_close(pcap);
        memset(err_buf, 0, 1024);
    }

    if (!(pcap = pcap_open_offline(input.c_str(), err_buf))) {
        throw std::system_error{};
    }
}

/// Ignoring packets. Moving to specific position.
///
/// @param count Number of packets to be omitted.
void PcapReader::omit_packets(unsigned long count)
{
    while (pcap_next_ex(pcap, &header, &packet) == 1 && count--) {
        // to nothing
    }
}
