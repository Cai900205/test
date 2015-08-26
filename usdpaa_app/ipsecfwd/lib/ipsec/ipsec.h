/*
 * Copyright (C) 2011 Freescale Semiconductor, Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN
 * NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED
 * TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef LIB_IPSEC_IPSEC_H
#define LIB_IPSEC_IPSEC_H 1

#include "ip/ip.h"
#include "ipsec/ipsec_common.h"
#include "net/annotations.h"
#include "ip/ip_rc.h"
#include "ip/ip_common.h"

#include <mutex.h>

#define IPSEC_TUNNEL_ENTRIES 1024
#define IPSEC_HASH_MASK (IPSEC_TUNNEL_ENTRIES - 1)

enum ipsec_fq_state {
	PARKED,
	SCHEDULED
};

/**
 \brief Encap/Decap SA for a tunnel
 */
struct ipsec_tunnel_t {
	/**< Next Tunnel Entry in the Tunnel Bucket */
	struct ipsec_tunnel_t *next;
	/**< Source IP address */
	uint32_t saddr;
	/**< Destination IP Address */
	uint32_t daddr;
	/**< Tunnel Source IP Address */
	uint32_t tunnel_saddr;
	/**< Tunnel Destination IP Address */
	uint32_t tunnel_daddr;
	/**< SPI */
	uint32_t spi;
	struct rt_dest_t *dest;
	/**< Tunnel Id */
	uint32_t tunnel_id;
	/**< FQ Id of the Q towards SEC */
	struct qman_fq *qm_fq_to_sec[NUM_TO_SEC_FQ];
	/**< Extended sequence number support */
	bool is_esn;
	/**< Starting Sequence number */
	uint64_t seq_num;
	uint8_t type;
	bool valid;
	bool hb_tunnel;
	int fq_to_sec_index;
	/**< Encryption info */
	struct app_ctrl_sa_algo *ealg;
	/**< Authentication info*/
	struct app_ctrl_sa_algo *aalg;
	/**< Pointer to ctxtA for the tunnel entry */
	void *ctxtA;
	enum ipsec_fq_state fq_state;
	/**< lock for a tunnel */
	spinlock_t tlock;
} __attribute__ ((aligned(64)));

/**
 \brief Bucket into the IPsec Tunnel table
 */
struct ipsec_bucket_t {
	/**< Bucket Id */
	uint32_t id;
	/**< Write Lock */
	spinlock_t wlock;
	/**< Head of the Linked List of Tunnel Entries */
	struct ipsec_tunnel_t *head_entry;
};

/**
 \brief lookup table of ipsec tunnels
 */
struct ipsec_tunnel_table_t {
#ifdef STATS_TBD
	/**< Ipsec Stats */
	struct ipsec_statistics_t *stats;
#endif
	/**< Free Entries in the Tunnel table */
	struct mem_cache_t *free_entries;
	/**< Tunnel Hash Table */
	struct ipsec_bucket_t buckets[IPSEC_TUNNEL_ENTRIES];
};

/**
 \brief Creates the Tunnel hash Table
 \return Pointer to the tunnel table
 */
int ipsec_tunnel_table_create(struct ipsec_tunnel_table_t *itt);

/**
 \brief Lookup in the Tunnel Hash table based on src ip, dst ip, and spi
 \param[in] entry Returns the pointer to the Entry if found
 \param[in] itt Pointer to the Tunnel hash Table in which to search for entry
 \param[in] tunnel_src_ip Tunnel Source IP Address
 \param[in] tunnel_dst_ip Tunnel Destination IP Address
 \param[in] spi SPI
 \return True if entry found, else false
 */
bool ipsec_lookup_tunnel_entry(struct ipsec_tunnel_t **entry,
			       struct ipsec_tunnel_table_t *itt,
			       uint32_t tunnel_src_ip, uint32_t tunnel_dst_ip,
			       uint32_t spi);

/**
 \brief Allocates entry for the Tunnel Table from the list fo free entries
 \param[in] itt Pointer to the Tunnel Table
 \return Returns pointer to a new entry
 */
struct ipsec_tunnel_t *ipsec_create_tunnel_entry(struct ipsec_tunnel_table_t
						 *itt);
/**
 \brief Adds an entry into the Tunnel table
 \param[in] itt Pointer to the Tunnel Table
 \param[in] new_entry Pointer to the Tunnel Entry to be added
 \return Returns pointer to a new entry
 */
bool ipsec_add_tunnel_entry(struct ipsec_tunnel_table_t *itt,
			    struct ipsec_tunnel_t *new_entry);

/**
 \brief Removes an entry from the Tunnel table
 \param[in] itt Pointer to the Tunnel Table
 \param[in] tunnel_src_ip Tunnel Source IP Address
 \param[in] tunnel_dst_ip Tunnel Destination IP Address
 \param[in] spi SPI
 \return True if entry was found and removed, else False
 */
bool ipsec_remove_tunnel_entry(struct ipsec_tunnel_table_t *itt,
			       uint32_t tunnel_src_ip, uint32_t tunnel_dst_ip,
			       uint32_t spi);

/**
 \brief Fast Lookup in the Tunnel Hash table based on src ip, dst ip, and spi
 \param[in] entry Returns the pointer to the Entry if found
 \param[in] itt Pointer to the Tunnel hash Table in which to search for entry
 \param[in] tunnel_src_ip Tunnel Source IP Address
 \param[in] tunnel_dst_ip Tunnel Destination IP Address
 \param[in] spi SPI
 \param[in] index Hash index into the Tunnel table for faster search through
	    Tunnel Bucket
 \return True if entry found, else false
 */
bool ipsec_tunnel_fast_lookup(struct ipsec_tunnel_t **entry,
			      struct ipsec_tunnel_table_t *itt,
			      uint32_t tunnel_src_ip, uint32_t tunnel_dst_ip,
			      uint32_t spi, uint32_t index);

void ipsec_free_entry(void *ctxt, void *entry);

#endif /* ifndef LIB_IPSEC_IPSEC_H */
