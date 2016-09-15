/*-
 * Copyright (c) 2016 Universitetet i Oslo
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote
 *    products derived from this software without specific prior written
 *    permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef FLYCATCHER_ETHERNET_H_INCLUDED
#define FLYCATCHER_ETHERNET_H_INCLUDED

struct iface;
struct packet;

typedef union { uint8_t o[6]; } __attribute__((__packed__)) ether_addr;
typedef union { uint8_t o[4]; uint32_t q; } __attribute__((__packed__)) ipv4_addr;
typedef union { uint8_t o[16]; uint16_t w[8]; } __attribute__((__packed__)) ipv6_addr;

#define FLYCATCHER_ETHER_ADDR { 0x02, 0x00, 0x18, 0x11, 0x09, 0x02 }
extern ether_addr flycatcher_ether_addr;

#define FLYCATCHER_TCP4_SEQ 0x18110902U

typedef enum ether_type {
	ether_type_ip	 = 0x0800,
	ether_type_arp	 = 0x0806,
	ether_type_vlan	 = 0x8100,
	ether_type_ipv6	 = 0x86dd,
} ether_type;

typedef struct ether_hdr {
	ether_addr	 dst;
	ether_addr	 src;
	uint16_t	 type;
} __attribute__((__packed__)) ether_hdr;

typedef struct ether_ftr {
	uint32_t	 fcs;
} __attribute__((__packed__)) ether_ftr;

typedef struct ether_flow {
	struct packet	*p;
	ether_addr	 src;
	ether_addr	 dst;
	uint16_t	 type;
	uint16_t	 len;
} ether_flow;

typedef enum arp_oper {
	arp_oper_who_has = 1,
	arp_oper_is_at	 = 2,
} arp_oper;

typedef enum arp_type {
	arp_type_ether	 = 1,
	arp_type_ipv4	 = 0x0800,
} arp_type;

typedef struct arp_pkt {
	uint16_t	 htype;
	uint16_t	 ptype;
	uint8_t		 hlen;
	uint8_t		 plen;
	uint16_t	 oper;
	ether_addr	 sha;
	ipv4_addr	 spa;
	ether_addr	 tha;
	ipv4_addr	 tpa;
} __attribute__((__packed__)) arp_pkt;

typedef enum ip_proto {
	ip_proto_icmp	 = 0x01,
	ip_proto_tcp	 = 0x06,
	ip_proto_udp	 = 0x11,
} ip_proto;

typedef struct ipv4_hdr {
#define ipv4_hdr_ver(ih) ((ih)->ver_ihl >> 4)
#define ipv4_hdr_ihl(ih) ((ih)->ver_ihl & 0xf)
	uint8_t		 ver_ihl;
#define ipv4_hdr_dscp(ih) ((ih)->dscp_ecn >> 2)
#define ipv4_hdr_ecn(ih) ((ih)->dscp_ecn & 0x3)
	uint8_t		 dscp_ecn;
	uint16_t	 len;
	uint16_t	 id;
#define ipv4_hdr_fl(ih) (be16toh((ih)->fl_off) >> 2)
#define ipv4_hdr_off(ih) (be16toh((ih)->fl_off) & 0x3)
	uint16_t	 fl_off;
	uint8_t		 ttl;
	uint8_t		 proto;
	uint16_t	 sum;
	ipv4_addr	 srcip;
	ipv4_addr	 dstip;
	uint8_t		 opt[];
} __attribute__((__packed__)) ipv4_hdr;

typedef enum icmp_type {
	icmp_type_echo_reply	 = 0x00,
	icmp_type_echo_request	 = 0x08,
} icmp_type;

typedef struct icmp_hdr {
	uint8_t		 type;
	uint8_t		 code;
	uint16_t	 sum;
	uint32_t	 hdata;
	uint8_t		 data[];
} __attribute__((__packed__)) icmp_hdr;

typedef struct tcp4_hdr {
	uint16_t	 sp;
	uint16_t	 dp;
	uint32_t	 seq;
	uint32_t	 ack;
#define tcp4_hdr_off(th) ((th)->off_ns >> 4)
#define tcp4_hdr_ns(th)  ((th)->off_ns & 0x01)
	uint8_t		 off_ns;
#define TCP4_CWR	 0x80
#define TCP4_ECE	 0x40
#define TCP4_URG	 0x20
#define TCP4_ACK	 0x10
#define TCP4_PSH	 0x08
#define TCP4_RST	 0x04
#define TCP4_SYN	 0x02
#define TCP4_FIN	 0x01
#define tcp4_hdr_cwr(th) ((th)->fl & TCP4_CWR)
#define tcp4_hdr_ece(th) ((th)->fl & TCP4_ECE)
#define tcp4_hdr_urg(th) ((th)->fl & TCP4_URG)
#define tcp4_hdr_ack(th) ((th)->fl & TCP4_ACK)
#define tcp4_hdr_psh(th) ((th)->fl & TCP4_PSH)
#define tcp4_hdr_rst(th) ((th)->fl & TCP4_RST)
#define tcp4_hdr_syn(th) ((th)->fl & TCP4_SYN)
#define tcp4_hdr_fin(th) ((th)->fl & TCP4_FIN)
	uint8_t		 fl;
	uint16_t	 win;
	uint16_t	 sum;
	uint16_t	 urg;
#define TCP_OPT_END	 0
#define TCP_OPT_NOP	 1
#define TCP_OPT_MSS	 2
#define TCP_OPT_WS	 3
#define TCP_OPT_SACKOK	 4
#define TCP_OPT_SACK	 5
#define TCP_OPT_TIME	 8
	uint8_t		 opt[];
} __attribute__((__packed__)) tcp4_hdr;

typedef struct udp4_hdr {
	uint16_t	 sp;
	uint16_t	 dp;
	uint16_t	 len;
	uint16_t	 sum;
	uint8_t		 data[];
} __attribute__((__packed__)) udp4_hdr;

typedef struct ipv4_flow {
	struct ether_flow	*eth;
	/* pseudo-header */
	union {
		uint8_t		 pseudo[12];
		struct {
			ipv4_addr	 src;
			ipv4_addr	 dst;
			uint16_t	 proto;
			uint16_t	 len;
		} __attribute__((__packed__));
	};
	uint16_t	 sum;
} ipv4_flow;

int	 arp_reserve(const ipv4_addr *);
int	 arp_register(const ipv4_addr *, const ether_addr *, uint64_t);
int	 arp_lookup(const ipv4_addr *, ether_addr *);

uint32_t ether_crc32(const uint8_t *, size_t);

int	 ethernet_send(struct iface *, ether_type, ether_addr *,
    const void *, size_t);
int	 ethernet_reply(struct ether_flow *, const void *, size_t);

char	*ipv4_fromstr(const char *, ipv4_addr *);
uint16_t ip_cksum(uint16_t, const void *, size_t);
int	 ipv4_reply(ipv4_flow *, ip_proto, const void *, size_t);


int	 packet_analyze_ethernet(struct packet *, const void *, size_t);
int	 packet_analyze_arp(struct ether_flow *, const void *, size_t);
int	 packet_analyze_ip4(struct ether_flow *, const void *, size_t);
int	 packet_analyze_icmp4(struct ipv4_flow *, const void *, size_t);
int	 packet_analyze_udp4(struct ipv4_flow *, const void *, size_t);
int	 packet_analyze_tcp4(struct ipv4_flow *, const void *, size_t);

int	 log_packet4(const struct timeval *,
    const ipv4_addr *, int, const ipv4_addr *, int,
    const char *, size_t, const char *, ...);

#endif
