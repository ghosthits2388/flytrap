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

#if HAVE_CONFIG_H
#include "config.h"
#endif

#include <sys/types.h>
#include <sys/time.h>

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <fc/assert.h>
#include <fc/endian.h>
#include <fc/log.h>

#include "flycatcher.h"
#include "ethernet.h"
#include "iface.h"
#include "packet.h"

/*
 * A node in the tree.
 */
struct arpn {
	uint32_t	 addr;		/* network address */
	uint8_t		 plen;		/* prefix length */
	uint8_t		 claimed:1;	/* claimed by us */
	uint8_t		 reserved:1;	/* reserved address */
	uint64_t	 first;		/* first seen */
	uint64_t	 last;		/* last seen */
	union {
		struct arpn *sub[16];	/* children */
		struct {
			ether_addr	 ether;
			unsigned int	 nreq;
		};
	};
};

struct arpn arp_root;

/*
 * Print the leaf nodes of a tree in order.
 */
void
arp_print_tree(FILE *f, struct arpn *n)
{
	unsigned int i;

	if (n->plen == 32) {
		fprintf(f, "%u.%u.%u.%u",
		    (n->addr >> 24) & 0xff,
		    (n->addr >> 16) & 0xff,
		    (n->addr >> 8) & 0xff,
		    n->addr & 0xff);
		if (n->plen < 32)
			fprintf(f, "/%u", n->plen);
		fprintf(f, "\n");
	} else {
		for (i = 0; i < 16; ++i)
			if (n->sub[i] != NULL)
				arp_print_tree(f, n->sub[i]);
	}
}

/*
 * Delete all children of a given node in a tree.
 */
void
arp_delete(struct arpn *n)
{
	unsigned int i;

	for (i = 0; i < 16; ++i) {
		if (n->sub[i] != NULL) {
			arp_delete(n->sub[i]);
			free(n->sub[i]);
			n->sub[i] = NULL;
		}
	}
}

#if 0
/*
 * Expire
 */
void
arp_expire(struct arpn *n, uint64_t cutoff)
{
	unsigned int i;

	if (n->last < cutoff) {
		fc_verbose("expiring node %08x/%d", n->addr, n->plen);
		arp_delete(n);
	} else if (n->plen < 32) {
		for (i = 0; i < 16; ++i)
			if (n->sub[i] != NULL)
				arp_expire(n->sub[i], cutoff);
	}
}
#endif

/*
 * Insert an address into a tree.
 */
static struct arpn *
arp_insert(struct arpn *n, uint32_t addr, uint64_t when)
{
	struct arpn *sn, *rn;
	uint32_t sub;
	uint8_t splen;

	if (n == NULL)
		n = &arp_root;
	if (n->plen == 32) {
		fc_assert(n->addr == addr);
		return (n);
	}
	splen = n->plen + 4;
	sub = (addr >> (32 - splen)) % 16;
	if ((sn = n->sub[sub]) == NULL) {
		if ((sn = calloc(1, sizeof *sn)) == NULL)
			return (NULL);
		sn->addr = n->addr | (sub << (32 - splen));
		sn->plen = splen;
		sn->first = sn->last = when;
		fc_debug("added node %08x/%d", sn->addr, sn->plen);
		if (sn->plen == 32) {
			fc_verbose("arp: inserted %d.%d.%d.%d",
			    (addr >> 24) & 0xff, (addr >> 16) & 0xff,
			    (addr >> 8) & 0xff, addr & 0xff);
		}
		n->sub[sub] = sn;
	}
	if ((rn = arp_insert(sn, addr, when)) == NULL)
		return (NULL);
#if 0
	if (sn->last > n->last)
		n->last = sn->last;
#endif
	return (rn);
}

/*
 * ARP registration
 */
int
arp_register(const ipv4_addr *ipv4, const ether_addr *ether, uint64_t when)
{
	struct arpn *an;

	if ((an = arp_insert(NULL, be32toh(ipv4->q), when)) == NULL)
		return (-1);
	if (memcmp(&an->ether, ether, sizeof an->ether) != 0) {
		/* warn if the ipv4_addr moved from one ether_addr to another */
		if (an->ether.o[0] || an->ether.o[1] || an->ether.o[2] ||
		    an->ether.o[3] || an->ether.o[4] || an->ether.o[5]) {
			fc_verbose("%d.%d.%d.%d moved"
			    " from %02x:%02x:%02x:%02x:%02x:%02x"
			    " to %02x:%02x:%02x:%02x:%02x:%02x",
			    ipv4->o[0], ipv4->o[1], ipv4->o[2], ipv4->o[3],
			    an->ether.o[0], an->ether.o[1], an->ether.o[2],
			    an->ether.o[3], an->ether.o[4], an->ether.o[5],
			    ether->o[0], ether->o[1], ether->o[2],
			    ether->o[3], ether->o[4], ether->o[5]);
		} else {
			fc_verbose("%d.%d.%d.%d registered"
			    " at %02x:%02x:%02x:%02x:%02x:%02x",
			    ipv4->o[0], ipv4->o[1], ipv4->o[2], ipv4->o[3],
			    ether->o[0], ether->o[1], ether->o[2],
			    ether->o[3], ether->o[4], ether->o[5]);
		}
		memcpy(&an->ether, ether, sizeof an->ether);
	}
	an->nreq = 0;
	return (0);
}

/*
 * ARP lookup
 */
int
arp_lookup(const ipv4_addr *ipv4, ether_addr *ether)
{
	struct arpn *an;

	fc_debug("ARP lookup %d.%d.%d.%d",
	    ipv4->o[0], ipv4->o[1], ipv4->o[2], ipv4->o[3]);
	an = &arp_root;
	if ((an = an->sub[ipv4->o[0] / 16]) == NULL ||
	    (an = an->sub[ipv4->o[0] % 16]) == NULL ||
	    (an = an->sub[ipv4->o[1] / 16]) == NULL ||
	    (an = an->sub[ipv4->o[1] % 16]) == NULL ||
	    (an = an->sub[ipv4->o[2] / 16]) == NULL ||
	    (an = an->sub[ipv4->o[2] % 16]) == NULL ||
	    (an = an->sub[ipv4->o[3] / 16]) == NULL ||
	    (an = an->sub[ipv4->o[3] % 16]) == NULL)
		return (-1);
	memcpy(ether, &an->ether, sizeof(ether_addr));
	fc_debug("%d.%d.%d.%d is"
	    " at %02x:%02x:%02x:%02x:%02x:%02x",
	    ipv4->o[0], ipv4->o[1], ipv4->o[2], ipv4->o[3],
	    ether->o[0], ether->o[1], ether->o[2],
	    ether->o[3], ether->o[4], ether->o[5]);
	return (0);
}

/*
 * Claim an IP address
 */
static int
arp_reply(iface *i, const arp_pkt *iap, struct arpn *an)
{
	arp_pkt ap;

	(void)an;

	ap.htype = htobe16(arp_type_ether);
	ap.ptype = htobe16(arp_type_ipv4);
	ap.hlen = 6;
	ap.plen = 4;
	ap.oper = htobe16(arp_oper_is_at);
	memcpy(&ap.sha, &i->ether, sizeof(ether_addr));
	memcpy(&ap.spa, &iap->tpa, sizeof(ipv4_addr));
	memcpy(&ap.tha, &iap->sha, sizeof(ether_addr));
	memcpy(&ap.tpa, &iap->spa, sizeof(ipv4_addr));
	if (ethernet_send(i, ether_type_arp, &ap.tha, &ap, sizeof ap) != 0)
		return (-1);
	return (0);
}

/*
 * Register a reserved address
 */
int
arp_reserve(const ipv4_addr *addr)
{
	struct arpn *an;

	fc_debug("arp: reserving %d.%d.%d.%d",
	    addr->o[0], addr->o[1], addr->o[2], addr->o[3]);
	if ((an = arp_insert(NULL, be32toh(addr->q), 0)) == NULL)
		return (-1);
	an->reserved = 1;
	return (0);
}

/*
 * Analyze a captured ARP packet
 */
int
packet_analyze_arp(packet *p, const void *data, size_t len)
{
	const arp_pkt *ap;
	struct arpn *an;
	uint64_t when;

	if (len < sizeof(arp_pkt)) {
		fc_notice("%d.%03d short ARP packet (%zd < %zd)",
		    p->ts.tv_sec, p->ts.tv_usec / 1000,
		    len, sizeof(arp_pkt));
		return (-1);
	}
	ap = (const arp_pkt *)data;
	fc_debug("\tARP htype 0x%04hx ptype 0x%04hx hlen %hd plen %hd",
	    be16toh(ap->htype), be16toh(ap->ptype), ap->hlen, ap->plen);
	if (be16toh(ap->htype) != arp_type_ether || ap->hlen != 6 ||
	    be16toh(ap->ptype) != arp_type_ipv4 || ap->plen != 4) {
		fc_debug("\tARP packet ignored");
		return (0);
	}
	switch (be16toh(ap->oper)) {
	case arp_oper_who_has:
		fc_debug("\twho-has %d.%d.%d.%d tell %d.%d.%d.%d",
		    ap->tpa.o[0], ap->tpa.o[1], ap->tpa.o[2], ap->tpa.o[3],
		    ap->spa.o[0], ap->spa.o[1], ap->spa.o[2], ap->spa.o[3]);
		break;
	case arp_oper_is_at:
		fc_debug("\t%d.%d.%d.%d is-at %02x:%02x:%02x:%02x:%02x:%02x",
		    ap->tpa.o[0], ap->tpa.o[1], ap->tpa.o[2], ap->tpa.o[3], ap->tha.o[0],
		    ap->tha.o[1], ap->tha.o[2], ap->tha.o[3], ap->tha.o[4], ap->tha.o[5]);
		break;
	default:
		fc_notice("%d.%03d unknown ARP operation 0x%04x", be16toh(ap->oper));
		return (0);
	}
	when = p->ts.tv_sec * 1000 + p->ts.tv_usec / 1000;
	switch (be16toh(ap->oper)) {
	case arp_oper_who_has:
		/* ARP request */
		arp_register(&ap->spa, &ap->sha, when);
		if ((an = arp_insert(NULL, be32toh(ap->tpa.q), when)) == NULL)
			return (-1);
		if (an->last != 0) {
			fc_verbose("%d.%d.%d.%d: last seen %d.%03d",
			    ap->tpa.o[0], ap->tpa.o[1], ap->tpa.o[2], ap->tpa.o[3],
			    an->last / 1000, an->last % 1000);
		}
		if (an->reserved) {
			/* ignore */
			fc_debug("\ttarget address is reserved");
			an->nreq = 0;
		} else if (an->claimed) {
			/* already ours, refresh */
			fc_debug("refreshing %d.%d.%d.%d",
			    ap->tpa.o[0], ap->tpa.o[1], ap->tpa.o[2], ap->tpa.o[3]);
			an->nreq = 0;
			an->last = when;
			if (arp_reply(p->i, ap, an) != 0)
				return (-1);
		} else if (an->nreq == 0 || when - an->last >= 30000) {
			/* new or stale, start over */
			an->nreq = 1;
			an->first = an->last = when;
		} else if (an->nreq >= 3 && when - an->first >= 3000) {
			/* claim new address */
			fc_verbose("claiming %d.%d.%d.%d nreq = %d", ap->tpa.o[0],
			    ap->tpa.o[1], ap->tpa.o[2], ap->tpa.o[3], an->nreq);
			an->claimed = 1;
			an->nreq = 0;
			an->last = when;
			if (arp_reply(p->i, ap, an) != 0)
				return (-1);
		} else {
			an->nreq++;
			an->last = when;
		}
		break;
	case arp_oper_is_at:
		/* ARP reply */
		arp_register(&ap->spa, &ap->sha, when);
		arp_register(&ap->tpa, &ap->tha, when);
		break;
	}
	return (0);
}
