/// @author Jakub Semric
/// 2017

#pragma once

#include <fstream>
#include <iostream>
#include <stdio.h>
#include <cassert>
#include <mutex>
#include <string>
#include <sstream>

#include <pcap.h>
#include <pcap/pcap.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <netinet/ip.h>
#include <netinet/ip6.h>
#include <netinet/tcp.h>
#include <netinet/udp.h>
#include <netinet/ip_icmp.h>
#include <netinet/icmp6.h>
#include <net/ethernet.h>

/// reading and processing packets in packet capture files.
namespace pcapreader
{

struct vlan_ethhdr {
    u_int8_t  ether_dhost[ETH_ALEN];  /* destination eth addr */
    u_int8_t  ether_shost[ETH_ALEN];  /* source ether addr    */
    u_int16_t h_vlan_proto;
    u_int16_t h_vlan_TCI;
    u_int16_t ether_type;
} __attribute__ ((__packed__));

static inline const unsigned char *get_payload(
    const unsigned char *packet,
    const struct pcap_pkthdr *header);


template<typename F>
pcap_t* process_payload(const char* capturefile, F func, size_t count = ~0UL);

pcap_t* init_pcap(const char* capturefile);

template<typename F>
pcap_t* process_payload(pcap_t *pcap, F func, unsigned long count = ~0UL);

template<typename F>
void process_strings(std::string filename, F func, unsigned long count = ~0UL);

/// Generic function for processing packet payload.
///
/// @param capturefile filename of PCAP file
/// @param func lambda function which manipulates with packet payload
/// @param count Total number of processed packets, which includes some 
/// payload data.
template<typename F>
pcap_t* process_payload(const char* capturefile, F func, unsigned long count)
{
    char err_buf[4096] = "";
    pcap_t *pcap;

    if (!(pcap = pcap_open_offline(capturefile, err_buf)))
    {
        throw std::ios_base::failure(
            "cannot open pcap file '" + std::string(capturefile) + "'");
    }

    return process_payload(pcap, func, count);
}

/// Generic function for processing packet payload.
///
/// @param pcap PCAP file pointer
/// @param func lambda function which manipulates with packet payload
/// @param count Total number of processed packets, which includes some 
/// payload data.
template<typename F>
pcap_t* process_payload(pcap_t *pcap, F func, unsigned long count)
{
    struct pcap_pkthdr *header;
    const unsigned char *packet, *payload;

    while (pcap_next_ex(pcap, &header, &packet) == 1 && count)
    {
        payload = get_payload(packet, header);
        int len = header->caplen - (payload - packet);
        if (len > 0) {
            count--;
            func(payload, len);
        }
    }

    if (count)
    {
        pcap_close(pcap);
        return 0;
    }
    else
    {
        return pcap;
    }
}

template<typename F>
void process_strings(std::string filename, F func, unsigned long count)
{
  std::ifstream infile(filename);
  
  std::string line;
  bool first_line_encountered = false; 
  while (std::getline(infile, line))
  {
    if(!first_line_encountered)
    {
      first_line_encountered = true;
      continue;
    }
    func((const unsigned char *)line.c_str(), line.length());
  }
}

/// Extract payload from packet
inline const unsigned char *get_payload(
    const unsigned char *packet,
    const struct pcap_pkthdr *header)
{
    const unsigned char *packet_end = packet + header->caplen;
    size_t offset = sizeof(ether_header);
    const ether_header* eth_hdr = reinterpret_cast<const ether_header*>(packet);
    uint16_t ether_type = ntohs(eth_hdr->ether_type);
    if (ETHERTYPE_VLAN == ether_type)
    {
        offset = sizeof(vlan_ethhdr);
        const vlan_ethhdr* vlan_hdr =
        reinterpret_cast<const vlan_ethhdr*>(packet);

        ether_type = ntohs(vlan_hdr->ether_type);
    }

    unsigned l4_proto;

    if (ETHERTYPE_IP == ether_type)
    {
        const ip* ip_hdr = reinterpret_cast<const ip*>(packet + offset);
        offset += sizeof(ip);
        l4_proto = ip_hdr->ip_p;
    }
    else if (ETHERTYPE_IPV6 == ether_type)
    {
        const ip6_hdr* ip_hdr =
        reinterpret_cast<const ip6_hdr*>(packet + offset);

        offset += sizeof(ip6_hdr);
        l4_proto = ip_hdr->ip6_nxt;
    }
    else
    {
        return packet_end;
    }

    bool cond;
    do {
        cond = false;
        if (IPPROTO_TCP == l4_proto)
        {
            const tcphdr* tcp_hdr =
            reinterpret_cast<const tcphdr*>(packet + offset);

            size_t tcp_hdr_size = tcp_hdr->th_off * 4;
            offset += tcp_hdr_size;
        }
        else if (IPPROTO_UDP == l4_proto)
        {
            offset += sizeof(udphdr);
        }
        else if (IPPROTO_IPIP == l4_proto)
        {
            const ip* ip_hdr = reinterpret_cast<const ip*>(packet + offset);
            offset += sizeof(ip);
            l4_proto = ip_hdr->ip_p;
            cond = true;
        }
        else if (IPPROTO_ESP == l4_proto)
        {
            offset += 8;
        }
        else if (IPPROTO_ICMP == l4_proto)
        {
            offset += sizeof(icmphdr);
        }
        else if (IPPROTO_ICMPV6 == l4_proto)
        {
            offset += sizeof(icmp6_hdr);
        }
        else if (IPPROTO_FRAGMENT == l4_proto)
        {
            const ip6_frag* ip_hdr =
            reinterpret_cast<const ip6_frag*>(packet + offset);
            offset += sizeof(ip6_frag);
            l4_proto = ip_hdr->ip6f_nxt;
            cond = true;
        }
        else if (IPPROTO_IPV6 == l4_proto)
        {
            const ip6_hdr* ip_hdr =
            reinterpret_cast<const ip6_hdr*>(packet + offset);
            offset += sizeof(ip6_hdr);
            l4_proto = ip_hdr->ip6_nxt;
        }
        else
        {
            return packet + header->caplen;
        }
    } while (cond);

    if (offset > header->caplen) {
        return packet + header->caplen;
    }

    return packet + offset;
}

}  // end of namespace
