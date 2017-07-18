/// @author Jakub Semric
///
/// Class PcapReader for reading and processing packets in packet caputure files.
/// @file pcap_reader.h
///
/// Unless otherwise stated, all code is licensed under a
/// GNU General Public Licence v2.0

#ifndef __pcap_reader_h
#define __pcap_reader_h

#include <fstream>
#include <iostream>
#include <stdio.h>

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

#define MIN_PACKET 60

/// Class PcapReader for reading and processing packets in packet caputure files.
class PcapReader
{
private:
    pcap_t *pcap;
    char err_buf[1024];
    struct pcap_pkthdr *header;
    const unsigned char *packet;

private:
    inline const unsigned char *get_payload();
    inline const unsigned char *parse_ip(const unsigned char*) const;

public:
    PcapReader();
    PcapReader(const std::string &fname);
    ~PcapReader();

    void open(const std::string &input);
    inline const struct pcap_pkthdr* get_header() const noexcept;
    void omit_packets(unsigned long count);
    template<typename F>
    void process_packets(F func, unsigned long count);
};

inline const struct pcap_pkthdr* PcapReader::get_header() const noexcept
{
    return header;
}

/// Generic function for processing packet payload.
///
/// @tparam F lambda function which manipulates with packet payload
/// @param count Total number of processed packets, which includes some payload data.
template<typename F>
void PcapReader::process_packets(F func, unsigned long count)
{
    const unsigned char *payload;
    while (pcap_next_ex(pcap, &header, &packet) == 1 && count) {
        payload = get_payload();
        int len = header->caplen - (payload - packet);
        if (len > 0) {
            count--;
            func(payload, len);
        }
    }
}

/// Parses a packet and return a pointer to a first byte of payload.
inline const unsigned char *PcapReader::get_payload()
{

    // vlan + ethernet
    if (packet[12] == 0x81 && packet[13] == 0) {
        return parse_ip(packet + 4 + sizeof(struct ether_header));
    }

    // ethernet
    return parse_ip(packet + sizeof(struct ether_header));
}

/// Parses IPv4/6 packet headder.
inline const unsigned char *PcapReader::parse_ip(const unsigned char *payload) const
{
    uint8_t protocol;
    auto caplen = header->caplen;

    // ip
    if (payload[0] == 0x45) {
        protocol = ((struct ip*)payload)->ip_p;
        payload += 4*((struct ip*)payload)->ip_hl;
    }
    else if (payload[0] == 0x60) {
        protocol = ((struct ip6_hdr*)payload)->ip6_ctlun.ip6_un1.ip6_un1_nxt;
        payload += sizeof(struct ip6_hdr);
    }
    else {
        return packet + caplen;
    }

    // transport protocol
    switch (protocol) {
        case IPPROTO_TCP:
            int ofset;
            #if __FAVOR_BSD
            ofset = 4*((((struct tcphdr*)payload)->th_off));
            #else
            ofset = 4*((((struct tcphdr*)payload)->doff));
            #endif
            // remove padding
            if (ofset) {
                payload += ofset;
            }
            else {
                return packet + caplen;
            }

            if (*payload == 0)
                return packet + caplen;

            break;

        case IPPROTO_UDP:
            payload += sizeof(struct udphdr);
            break;

        case IPPROTO_ICMPV6:
            // TODO
        case IPPROTO_ICMP:
            if (caplen <= MIN_PACKET) {
                return packet + caplen;
            }

            payload += 8;
            if (*payload == 0x45 || *payload == 0x60) {
                return parse_ip(payload);
            }
            break;

        case IPPROTO_IPIP:
            return parse_ip(payload);
            break;

        case IPPROTO_GRE:
        default:
            return packet + caplen;
    }

    return payload;
}

#endif
