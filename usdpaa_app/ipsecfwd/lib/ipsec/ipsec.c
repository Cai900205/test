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

#include "ipsec/ipsec.h"

static bool __ipsec_lookup(struct ipsec_tunnel_t **entry,
			   struct ipsec_tunnel_table_t *itt,
			   uint32_t tunnel_src_ip, uint32_t tunnel_dst_ip,
			   uint32_t spi, struct ipsec_bucket_t *bucket);

static struct ipsec_bucket_t *__ipsec_find_bucket(struct
						  ipsec_tunnel_table_t
						  *itt, uint32_t tunnel_src_ip,
						  uint32_t tunnel_dst_ip,
						  uint32_t spi);

static struct ipsec_tunnel_t **__ipsec_find_entry(struct ipsec_bucket_t
						  *bucket,
						  uint32_t tunnel_src_ip,
						  uint32_t tunnel_dst_ip,
						  uint32_t spi);

int ipsec_tunnel_table_create(struct ipsec_tunnel_table_t *itt)
{
	uint32_t entries;
	struct ipsec_bucket_t *bucket;
	int ctr;

#ifdef STATS_TBD
	itt->stats =
	    stats_memalign(L1_CACHE_BYTES, sizeof(struct ipsec_statistics_t));

	if (itt->stats == NULL) {
		free(itt);
		return -ENOMEM;
	}
	memset(itt->stats, 0, sizeof(struct ipsec_statistics_t));
#endif

	itt->free_entries = mem_cache_create(sizeof(struct ipsec_tunnel_t),
					     IPSEC_TUNNEL_ENTRIES);

	if (itt->free_entries == NULL) {
#ifdef STATS_TBD
		stats_free(itt->stats);
#endif
		free(itt);
		return -ENOMEM;
	}

	entries = mem_cache_refill(itt->free_entries, IPSEC_TUNNEL_ENTRIES);
	if (entries != IPSEC_TUNNEL_ENTRIES) {
		free(itt->free_entries);
#ifdef STATS_TBD
		stats_free(itt->stats);
#endif
		free(itt);
		return -ENOMEM;
	}

	for (ctr = 0; ctr < IPSEC_TUNNEL_ENTRIES; ctr++) {
		bucket = &(itt->buckets[ctr]);
		bucket->head_entry = NULL;
		bucket->id = ctr;
		mutex_init(&(bucket->wlock));
	}
	return 0;
}

void ipsec_tunnel_table_delete(struct ipsec_tunnel_table_t *itt)
{
	/*TBD: */
#if 0
	uint32_t entries;
	entries = mem_cache_refill(itt->free_entries, IPSEC_TUNNEL_ENTRIES);
	if (entries != IPSEC_TUNNEL_ENTRIES)
		return NULL;

	itt->free_entries = mem_cache_create(sizeof(struct ipsec_tunnel_t),
					     IPSEC_TUNNEL_ENTRIES);

	if (itt->free_entries == NULL)
		return NULL;
#endif
#ifdef STATS_TBD
	stats_free(itt->stats);
#endif
	free(itt);
}

struct ipsec_tunnel_t *ipsec_create_tunnel_entry(struct
						 ipsec_tunnel_table_t *itt)
{
	return mem_cache_alloc(itt->free_entries);
}

bool ipsec_lookup_tunnel_entry(struct ipsec_tunnel_t **entry,
			       struct ipsec_tunnel_table_t *itt,
			       uint32_t tunnel_src_ip, uint32_t tunnel_dst_ip,
			       uint32_t spi)
{
	struct ipsec_bucket_t *bucket;
	bool retval;

	bucket = __ipsec_find_bucket(itt, tunnel_src_ip, tunnel_dst_ip, spi);

	retval =
	    __ipsec_lookup(entry, itt, tunnel_src_ip, tunnel_dst_ip, spi,
			   bucket);

	return retval;
}

bool ipsec_tunnel_fast_lookup(struct ipsec_tunnel_t **entry,
			      struct ipsec_tunnel_table_t *itt,
			      uint32_t tunnel_src_ip, uint32_t tunnel_dst_ip,
			      uint32_t spi, uint32_t ind)
{
	struct ipsec_bucket_t *bucket =
	    &(itt->buckets[ind & IPSEC_HASH_MASK]);
	bool retval;

	retval =
	    __ipsec_lookup(entry, itt, tunnel_src_ip, tunnel_dst_ip, spi,
			   bucket);
	return retval;
}

bool ipsec_add_tunnel_entry(struct ipsec_tunnel_table_t *itt,
			    struct ipsec_tunnel_t *new_entry)
{
	struct ipsec_bucket_t *bucket;
	struct ipsec_tunnel_t **entry_ptr;
	struct ipsec_tunnel_t *prev_entry;
	bool rc;

	rc = false;

#ifdef STATS_TBD
	uint32_t count;
	count = itt->stats->entry_count;

	if (count < IPSEC_TUNNEL_ENTRIES) {
#endif
		bucket =
		    __ipsec_find_bucket(itt, new_entry->tunnel_saddr,
					new_entry->tunnel_daddr,
					new_entry->spi);
#ifdef IPSEC_RCU_ENABLE
		rcu_read_lock();
#endif
		mutex_lock(&(bucket->wlock));
		entry_ptr =
		    __ipsec_find_entry(bucket, new_entry->tunnel_saddr,
				       new_entry->tunnel_daddr,
				       new_entry->spi);
#ifdef IPSEC_RCU_ENABLE
		prev_entry = rcu_dereference(*entry_ptr);
#else
		prev_entry = *entry_ptr;
#endif
		/* Entry not already in table, insert */
		if (prev_entry == NULL) {
			new_entry->next = NULL;
			new_entry->valid = true;

#ifdef IPSEC_RCU_ENABLE
			rcu_assign_pointer(*entry_ptr, new_entry);
#else
			*entry_ptr = new_entry;
#endif
			rc = true;
		}
		mutex_unlock(&(bucket->wlock));
#ifdef IPSEC_RCU_ENABLE
		rcu_read_unlock();
#endif

#ifdef STATS_TBD
	}
#endif

#ifdef STATS_TBD
	if (rc == true) {
		/* TODO: need to call api to id source ip to
		 * route cache as ipsec here */
		decorated_notify_inc_32(&(itt->stats->new_entries));
		decorated_notify_inc_32(&(itt->stats->entry_count));
	}
#endif
	return rc;
}

bool ipsec_remove_tunnel_entry(struct ipsec_tunnel_table_t *itt,
			       uint32_t tunnel_src_ip, uint32_t tunnel_dst_ip,
			       uint32_t spi)
{
	struct ipsec_tunnel_t **entry_ptr;
	struct ipsec_tunnel_t *entry;
	struct ipsec_bucket_t *bucket;
	bool rc;

	rc = false;
	bucket = __ipsec_find_bucket(itt, tunnel_src_ip, tunnel_dst_ip, spi);

#ifdef IPSEC_RCU_ENABLE
	rcu_read_lock();
#endif
	mutex_lock(&(bucket->wlock));
	entry_ptr =
	    __ipsec_find_entry(bucket, tunnel_src_ip, tunnel_dst_ip, spi);
#ifdef IPSEC_RCU_ENABLE
	entry = rcu_dereference(*entry_ptr);
#else
	entry = *entry_ptr;
#endif
	if (entry != NULL) {
#ifdef IPSEC_RCU_ENABLE
		rcu_assign_pointer(*entry_ptr, entry->next);
#else
		*entry_ptr = entry->next;
#endif
	}
	mutex_unlock(&(bucket->wlock));
#ifdef IPSEC_RCU_ENABLE
	rcu_read_unlock();
#endif

	if (entry != NULL) {
		rc = true;
#ifdef STATS_TBD
		itt->stats->entry_count--;
		decorated_notify_inc_32(&(itt->stats->removed_entries));
#endif
#ifdef IPSEC_RCU_ENABLE
		rcu_free(&ipsec_free_entry, entry, itt);
#else
		ipsec_free_entry(itt, entry);
#endif
	}

	return rc;
}

static bool __ipsec_lookup(struct ipsec_tunnel_t **entry,
			   struct ipsec_tunnel_table_t *itt,
			   uint32_t tunnel_src_ip, uint32_t tunnel_dst_ip,
			   uint32_t spi, struct ipsec_bucket_t *bucket)
{
	struct ipsec_tunnel_t **entry_ptr;
	struct ipsec_tunnel_t *result;
	bool rc;

	rc = false;

#ifdef IPSEC_RCU_ENABLE
	rcu_read_lock();
#endif
	entry_ptr =
	    __ipsec_find_entry(bucket, tunnel_src_ip, tunnel_dst_ip, spi);
#ifdef IPSEC_RCU_ENABLE
	result = rcu_dereference(*entry_ptr);
#else
	result = *entry_ptr;
#endif
	if (result != NULL) {
		*entry = result;
#ifdef STATS_TBD
		decorated_notify_inc_64(&(itt->stats->table_hits));
#endif
		rc = true;
	}
#ifdef STATS_TBD
	 else
		decorated_notify_inc_64(&(itt->stats->table_misses));
#endif
#ifdef IPSEC_RCU_ENABLE
	rcu_read_unlock();
#endif

	return rc;
}

static struct ipsec_bucket_t *__ipsec_find_bucket(struct
						  ipsec_tunnel_table_t *itt,
						  uint32_t tunnel_src_ip,
						  uint32_t tunnel_dst_ip,
						  uint32_t spi)
{
	uint32_t hash;

	hash = compute_decap_hash(tunnel_src_ip, tunnel_dst_ip, spi);
	return &(itt->buckets[hash]);
}

static struct ipsec_tunnel_t **__ipsec_find_entry(struct ipsec_bucket_t
						  *bucket,
						  uint32_t tunnel_src_ip,
						  uint32_t tunnel_dst_ip,
						  uint32_t spi)
{
	struct ipsec_tunnel_t **entry_ptr;
	struct ipsec_tunnel_t *entry;

	entry_ptr = &(bucket->head_entry);
#ifdef IPSEC_RCU_ENABLE
	entry = rcu_dereference(*entry_ptr);
#else
	entry = *entry_ptr;
#endif
	while (entry != NULL) {
		if ((entry->valid == true) && (entry->spi == spi)
		    && (entry->tunnel_saddr == tunnel_src_ip)
		    && (entry->tunnel_daddr == tunnel_dst_ip)) {
			break;
		} else {
			entry_ptr = &(entry->next);
		}
#ifdef IPSEC_RCU_ENABLE
		entry = rcu_dereference(*entry_ptr);
#else
		entry = *entry_ptr;
#endif
	}

	return entry_ptr;
}

void ipsec_free_entry(void *ctxt, void *entry)
{
	struct ipsec_tunnel_table_t *itt;

	itt = ctxt;
	mem_cache_free(itt->free_entries, entry);
#ifdef STATS_TBD
	itt->stats->entry_count--;
#endif
}
