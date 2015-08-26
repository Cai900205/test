/**
 \file ip_rc.h
 \brief This file contains the Route Cache related data structures, and defines
 */
/*
 * Copyright (C) 2007-2011 Freescale Semiconductor, Inc.
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
#ifndef LIB_IP_IP_RC_H
#define LIB_IP_IP_RC_H

#include "ip/ip.h"
#include <fsl_fman.h>
#include <mutex.h>
#include "ipfwd/statistics.h"
#include "app_common.h"

#include <usdpaa/compat.h>	/* L1_CACHE_BYTES */

#include <stdio.h>

#ifdef ONE_MILLION_ROUTE_SUPPORT
#define RC_BUCKETS					(1024*1024)
/**< Number of Route cache Buckets */
#define MAX_RC_ENTRIES					(1024*1024)
/**< Number of Route cache entries*/
#else
#define RC_BUCKETS					(1024)
/**< Number of Route cache Buckets */
#define MAX_RC_ENTRIES					(1024)
/**< Number of Route cache entries*/
#endif /* endif 1M support */
#define RC_HASH_MASK					(RC_BUCKETS - 1)
/**< Hash Mask for Route Cache */
#define RC_ENTRY_POOL_SIZE				(RC_BUCKETS << 1)
/**< Size of Route cache entry Pool */
/**
\brief Route Cache Stats
\details This object specifies the Route Cache related Statistics
*/
struct rc_statistics_t {
	stat32_t entry_count;
	/**< Number of enteries in the Route Cache*/
	stat32_t stale_entries;
	/**< Number of stale enteries in the Route Cache*/
	stat32_t new_entries;
	/**< Number of enteries added in the Route Cache*/
	stat32_t removed_entriesi;
	/**< Number of enteries removed in the Route Cache*/
	union stat64_t hits;
	/**< Number of times the LookUp succeeded*/
	union stat64_t misses;
	/**< Number of times the LookUp failed*/
} __attribute__((aligned(L1_CACHE_BYTES)));

/**
 \brief Route Cache Entry Statistics
 */
struct rc_entry_statistics_t {
	union stat64_t hits;		/**< Number of hits on enrry */
	union stat64_t octets;		/**< Number of octets passed thru route */
};

/**
\brief Route Cache Entry
\details Entry contains data, the timestamp when it was last accessed,
 and pointer to the next node.
 */
/* Currently ~21B */
struct rc_entry_t {
	struct rc_entry_t *next;
	/**< Pointer to the next Route Cache Entry*/
#ifdef STATS_TBD
	struct rc_entry_statistics_t *stats;
#endif
	in_addr_t saddr;
	/**< Pointer to the Source Address */
	in_addr_t daddr;
	/**< Pointer to the Destination Address */
	struct rt_dest_t *dest;
	/**< Pointer to the egress interface structure */
	uint8_t tos;
	/**< Type of service */
};

/**
\brief Route Cache Bucket
\details The object contains the pointer to the linked list
 of Route Cache Entries
*/
struct rc_bucket_t {
	mutex_t wlock;			/**< Lock to guard the bucket */
	struct rc_entry_t *head_entry;	/**< Pointer to the linked list of
					     route cache entries route */
};

/**
\brief Route Cache table
\details The object contains stats, free entries, and
 route cache buckets which contain the link list of route entries
*/
struct rc_t {
	struct rc_statistics_t *stats;
	/**< Pointer to the Statistics structure*/
	struct mem_cache_t *free_entries;
	/**< List of Free Route Cache Entries*/
	uint32_t expire_jiffies;
	/**< Number of "jiffies" for which a particular
	 route cache entry is valid after its last access */
	uint32_t proto_len;
	/**< Number of bytes in the Protocol Address */
	uint32_t proto_word_len;
	/**< Number of words in the Protocol Address */
	struct rc_bucket_t buckets[RC_BUCKETS];
	/**< Route Cache Bucket Array */
} __attribute__((aligned(L1_CACHE_BYTES)));

typedef void (*rc_execfn_t) (struct rc_entry_t *);

/**
 \brief Computes IPSec Decap cache hash Value
 \param[in] saddr Source IP Address
 \param[in] daddr Destination IP Address
 \param[in] SPI Field
 \return Returns the Hash index into the IPsec Decap Cache
 */
static inline uint32_t compute_decap_hash(in_addr_t saddr,
					  in_addr_t daddr,
					  uint32_t spi)
{
	uint64_t result;

	result = fman_crc64_init();
	result = fman_crc64_compute_32bit(saddr, result);
	result = fman_crc64_compute_32bit(daddr, result);
	result = fman_crc64_compute_32bit(spi, result);

	return (uint32_t) result & RC_HASH_MASK;
}

/**
 \brief Computes Route cache hash Value
 \param[in] saddr Source IP Address
 \param[in] daddr Destination IP Address
 \return Returns the Hash index into the Route Cache
 */
static inline uint32_t compute_rc_hash(in_addr_t saddr,
				       in_addr_t daddr)
{
	uint64_t result;

	result = fman_crc64_init();
	result = fman_crc64_compute_32bit(saddr, result);
	result = fman_crc64_compute_32bit(daddr, result);
	return (uint32_t) result & RC_HASH_MASK;
}

#define RC_BUCKET_INDEX(__ip_notes) \
	((uint32_t)(__ip_notes->hash_result) & RC_HASH_MASK)

/** \brief			Initializes a route cache
 *  \param[out] rc		Route cache
 *  \param[in]	expire_jiffies	Number of "jiffies" for which a particular route cache entry is
 *				valid after its last access
 *  \param[in]	proto_len	Length of the IP address
 */
struct rc_t *rc_init(uint32_t expire_jiffies, uint32_t proto_len);

/**
 \brief Allocates a Route Entry to be added to the Route Cache
 \param[in] rc Pointer tto the Route Cache
 \return Pointer to the Route Entry
 */
struct rc_entry_t *rc_create_entry(struct rc_t *rc);

/**
   \brief Add an entry to the route cache.
   To add an entry, there must already be sufficient room in the RC.  We check
   by reading the statistic that maintains the entry count.  If it is below
   the maximum, we find the correct bucket, and grab its lock.	We then search
   for the entry we are about to add.  If we find it, we failed, since we
   cannot add the same entry twice.  If the search returns NULL, place this
   entry at the end of the list, and update the entry count.  If it returns
   non-null, do not update the entry count, and do not add the item.
   \param[in] rc Pointer to the route cache
   \param[in] new_entry Pointer to the Entry to be added
   \return 'true' if successfully added, else 'false'
 */
bool rc_add_entry(struct rc_t *rc, struct rc_entry_t *entry);

bool rc_add_update_entry(struct rc_t *rc, struct rc_entry_t *new_entry);

/**
 \brief Free an entry to the route cache.
 \param[in] rc Pointer to the route cache
 \param[in] new_entry Pointer to the Entry to be freed
 \return none
 */
void rc_free_entry(struct rc_t *rc, struct rc_entry_t *entry);

/**
 \brief Removes a particular Entry from the Route Cahe
 \param[in] rc Pointer to the Route Cache
 \param[in] saddr Source IP Address
 \param[in] daddr Destination IP Address
 \return 'true' if removed successfully, else 'false'
 */
bool rc_remove_entry(struct rc_t *rc,
		     in_addr_t saddr,
		     in_addr_t daddr);

/**
 \brief Search the route cache for a particular entry based on src and dst
 \param[in] rc The route cache to search
 \param[in] saddr Source IP Address
 \param[in] daddr Destination IP Address
 \retur rt_dest_t pointer corresponding to the entry if found else NULL.
 */
struct rt_dest_t *rc_lookup(struct rc_t *rc,
			    in_addr_t saddr,
			    in_addr_t daddr);

/**
 \brief Search the route cache dest for a particular entry based in src and dst.
 \param[in] rc The route cache to search
 \param[in] saddr Source IP Address
 \param[in] daddr Destination IP Address
 \param[in] index Has Index into the Route Cache to quickly fine the RC bucket.
 \return rt_dest_t pointer corresponding to the entry if found else NULL.
 */
struct rt_dest_t *rc_fast_lookup(struct rc_t *rc,
				 in_addr_t saddr,
				 in_addr_t daddr,
				 uint32_t index);

/**
 \brief Search the route cache for a particular entry based in src and dst.
 \param[in] rc The route cache to search
 \param[in] saddr Source IP Address
 \param[in] daddr Destination IP Address
 \param[in] index Has Index into the Route Cache to quickly fine the RC bucket.
 \return rc_entry_t pointer corresponding to the entry if found else NULL.
 */
struct rc_entry_t *rc_entry_fast_lookup(struct rc_t *rc,
					in_addr_t saddr,
					in_addr_t daddr,
					uint32_t index);
/**
 \brief Prints the stats related to th eroute cache
 \param[in] rc Pointer to the route cache
 \param[in] execfn Function Pointer to the function to be executed
 for each of the entries
 \return none
 */
void rc_print_statistics(struct rc_t *rc, bool print_zero);

/**
 \brief Executes a function on each of the entries in the route cache
 \param[in] rc Pointer to the route cache
 \param[in] execfn Function Pointer to the function to be executed for
 each of the entries
 \return none
 */
void rc_exec_per_entry(struct rc_t *rc, rc_execfn_t execfn);

/**
 \brief Executes a function on each of the entries in the route cache
 \param[in] rc The route cache to search
 \param[in] saddr Source IP Address
 \param[in] daddr Destination IP Address
 \return Pointer to the entry in the Route Cache if found else NULL.
*/
struct rc_entry_t *rc_entry_lookup(struct rc_t *rc,
				   in_addr_t saddr,
				   in_addr_t daddr);
void __rc_free_entry(void *entry, void *ctxt);
#endif	/* LIB_IP_IP_RC_H */
