/**
 \file neigh.c
 */
/*
 * Copyright (C) 2010,2011 Freescale Semiconductor, Inc.
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
#include "neigh.h"

#include <ppac_interface.h>

#ifdef NEIGH_RCU_ENABLE
#include "rcu_lock.h"
#endif
#include "neigh.h"
#include "ethernet/eth.h"
#include "bigatomic.h"
#include "mm/mem_cache.h"

#include <assert.h>

static bool __neigh_add(struct neigh_table_t *nt, struct neigh_t **cur_ptr,
			struct neigh_t *new_n, bool replace);

static struct neigh_t *__neigh_lookup(struct neigh_bucket_t *bucket,
				      uint32_t key, uint32_t keylen);

static struct neigh_bucket_t *__neigh_find_bucket(struct neigh_table_t *nt,
				uint32_t key, uint32_t keylen);

static struct neigh_t **__neigh_find(struct neigh_bucket_t *bucket,
				     uint32_t key, uint32_t keylen);

#ifdef STATS_TBD
void neigh_table_print_stats(struct neigh_table_t *nt, bool print_zero)
{
	struct neigh_stats_t *stats;

	stats = nt->stats;
	print_stat64(&stats->lookup_attempts, "neigh_lookup_attempts",
		     print_zero);
	print_stat64(&stats->lookup_hits, "neigh_lookup_hits", print_zero);
	print_stat64(&stats->solicit_errors, "neigh_solicit_errors",
		     print_zero);
	print_stat64(&stats->protocol_errors, "neigh_protocol_errors",
		     print_zero);
}
#endif

int neigh_table_init(struct neigh_table_t *table)
{
	struct neigh_bucket_t *bucket;
	uint32_t entries;
	int _errno, i;

	_errno = posix_memalign((void **)&table->stats,
				L1_CACHE_BYTES, sizeof(*table->stats));
	if (unlikely(_errno < 0))
		return _errno;
	memset(table->stats, 0, sizeof(*table->stats));

	table->free_entries =
	    mem_cache_create(sizeof(struct __neigh_func_combo_t),
			     NEIGH_POOL_SIZE);
	if (unlikely(table->free_entries == NULL))
		return -ENOMEM;

	entries = mem_cache_refill(table->free_entries, NEIGH_POOL_SIZE);
	if (unlikely(entries != NEIGH_POOL_SIZE)) {
		/** \todo	mem_cache_destory(table->free_entries); */
		return -ENOMEM;
	}

	for (i = 0; i < NEIGH_TABLE_BUCKETS; i++) {
		bucket = table->buckets + i;
		bucket->head = NULL;
		bucket->id = i;
		mutex_init(&bucket->wlock);
	}

	return 0;
}


void neigh_table_delete(struct neigh_table_t *table)
{
#if 0
	struct neigh_bucket_t *bucket;
	uint32_t entries;
	int i;

	entries = mem_cache_refill(table->free_entries, NEIGH_POOL_SIZE);
	if (entries != NEIGH_POOL_SIZE)
		return NULL;

	table->free_entries =
	    mem_cache_create(sizeof(struct __neigh_func_combo_t),
			     NEIGH_POOL_SIZE);
	if (table->free_entries == NULL)
		return NULL;
#endif
#ifdef STATS_TBD
	stats_free(table->stats);
#endif
	return;
}


struct neigh_t *neigh_create(struct neigh_table_t *nt)
{
	struct __neigh_func_combo_t *combo;

	combo = mem_cache_alloc(nt->free_entries);
	if (likely(combo)) {
		combo->neigh.funcs = &combo->funcs;
		return &(combo->neigh);
	}
	return NULL;
}

struct neigh_t *neigh_init(struct neigh_table_t *nt, struct neigh_t *n,
			   struct ppac_interface *dev, uint32_t *proto_addr)
{
	nt->constructor(n);
	n->next = NULL;
	n->nt = nt;
	mutex_init(&n->wlock);
	n->dev = dev;
	n->funcs->full_output = NULL;
	n->funcs->reachable_output = &neigh_reachable_output;
	n->funcs->xmit = NULL;
	n->config = &nt->config;
	n->ll_cache = NULL;
	n->output = n->funcs->full_output;
	n->proto_addr[0] = *proto_addr;
	n->neigh_state = NEIGH_STATE_UNKNOWN;
	n->solicitations_sent = 0;
	n->refcnt = refcount_create();
	if (unlikely(NULL == n->refcnt))
		return NULL;

	return n;
}

struct neigh_t *neigh_update(struct neigh_t *n,
			const uint8_t *lladdr, uint8_t state)
{
	struct ppac_interface *i;
	struct ether_header eth_hdr;

	mutex_lock(&n->wlock);
	if (n->neigh_state == NEIGH_STATE_UNKNOWN) {
		i = n->dev;
		memcpy(&n->neigh_addr, lladdr, sizeof(n->neigh_addr));

		n->ll_cache = ll_cache_create();
		if (n->ll_cache == NULL) {
			mutex_unlock(&n->wlock);
			return NULL;
		}
		memcpy(eth_hdr.ether_dhost, lladdr,
				sizeof(eth_hdr.ether_dhost));
		memcpy(eth_hdr.ether_shost, &i->port_cfg->fman_if->mac_addr,
				sizeof(eth_hdr.ether_shost));
		i->ppam_data.cache_header(n->ll_cache, &eth_hdr);
		n->output = n->funcs->reachable_output;
		n->neigh_state = state;
	} else {
		mutex_unlock(&n->wlock);
		return NULL;
	}
	mutex_unlock(&n->wlock);

	return n;
}

bool neigh_add(struct neigh_table_t *nt, struct neigh_t *new_n)
{
	struct neigh_bucket_t *bucket;
	struct neigh_t **cur_ptr;
	uint32_t key;
	uint32_t keylen;
	bool retval;

	if (new_n->config == NULL)
		new_n->config = &nt->config;

	key = new_n->proto_addr[0];
	keylen = nt->proto_len;
	bucket = __neigh_find_bucket(nt, key, keylen);
	if (unlikely(bucket == NULL))
		return false;
#ifdef NEIGH_RCU_ENABLE
	rcu_read_lock();
#endif
	mutex_lock(&(bucket->wlock));
	cur_ptr = __neigh_find(bucket, key, keylen);
	if (unlikely(cur_ptr == NULL)) {
		mutex_unlock(&(bucket->wlock));
		return false;
	}
	retval = __neigh_add(nt, cur_ptr, new_n, false);
	mutex_unlock(&(bucket->wlock));
#ifdef NEIGH_RCU_ENABLE
	rcu_read_unlock();
#endif
	return retval;
}

bool neigh_replace(struct neigh_table_t *nt, struct neigh_t *new_n)
{
	struct neigh_bucket_t *bucket;
	struct neigh_t **cur_ptr;
	uint32_t key;
	uint32_t keylen;
	bool retval;

	key = new_n->proto_addr[0];
	keylen = nt->proto_len;
	bucket = __neigh_find_bucket(nt, key, keylen);
	if (unlikely(bucket == NULL))
		return false;
#ifdef NEIGH_RCU_ENABLE
	rcu_read_lock();
#endif
	mutex_lock(&(bucket->wlock));
	cur_ptr = __neigh_find(bucket, key, keylen);
	if (unlikely(cur_ptr == NULL)) {
		mutex_unlock(&(bucket->wlock));
		return false;
	}
	retval = __neigh_add(nt, cur_ptr, new_n, true);
	mutex_unlock(&(bucket->wlock));
#ifdef NEIGH_RCU_ENABLE
	rcu_read_unlock();
#endif

	return retval;
}

bool neigh_remove(struct neigh_table_t *nt, uint32_t key, uint32_t keylen)
{
	struct neigh_bucket_t *bucket;
	struct neigh_t **cur_ptr;
	bool retval;

	bucket = __neigh_find_bucket(nt, key, keylen);
	if (unlikely(bucket == NULL))
		return false;
#ifdef NEIGH_RCU_ENABLE
	rcu_read_lock();
#endif
	mutex_lock(&(bucket->wlock));
	cur_ptr = __neigh_find(bucket, key, keylen);
	if (unlikely(cur_ptr == NULL)) {
		mutex_unlock(&(bucket->wlock));
		return false;
	}
	retval = __neigh_delete(nt, cur_ptr);
	mutex_unlock(&(bucket->wlock));
#ifdef NEIGH_RCU_ENABLE
	rcu_read_unlock();
#endif

	return retval;
}

void neigh_print_entry(struct neigh_t *n)
{
	pr_debug("Neighbor at %p\n", (void *)n);
}

void neigh_exec_per_entry(struct neigh_table_t *nt, nt_execfn_t execfn)
{
	struct neigh_bucket_t *bucket;
	struct neigh_t *n;
	uint32_t i;

	for (i = 0; i < NEIGH_TABLE_BUCKETS; i++) {
#ifdef NEIGH_RCU_ENABLE
		rcu_read_lock();
#endif
		bucket = &nt->buckets[i];
#ifdef NEIGH_RCU_ENABLE
		n = rcu_dereference(bucket->head);
#else
		n = bucket->head;
#endif
		while (n != NULL) {
			execfn(n);
#ifdef NEIGH_RCU_ENABLE
			n = rcu_dereference(n->next);
#else
			n = n->next;
#endif
		}
	}
}

void neigh_table_print(struct neigh_table_t *nt)
{
	pr_debug("IP Address	  MAC address\n");
	neigh_exec_per_entry(nt, &neigh_print_entry);
}

/******************************************************************************
 * START OF PROTOCOL FUNCTIONS
 ******************************************************************************/

struct neigh_t *neigh_lookup(struct neigh_table_t *nt, uint32_t key,
			     uint32_t keylen)
{
	struct neigh_bucket_t *bucket;
	struct neigh_t *n;

	bucket = __neigh_find_bucket(nt, key, keylen);
	if (unlikely(bucket == NULL))
		return NULL;
#ifdef STATS_TBD
	decorated_notify_inc_64(&nt->stats->lookup_attempts);
#endif
	n = __neigh_lookup(bucket, key, keylen);
	if (likely(n != NULL)) {
#ifdef STATS_TBD
		decorated_notify_inc_64(&nt->stats->lookup_hits);
#endif
	}

	return n;
}

void neigh_reachable_output(struct neigh_t *n, void *notes, void *ll_payload)
{
	struct ppac_interface *i;

	i = n->dev;
	i->ppam_data.set_header(i, ll_payload, NULL, &n->neigh_addr);
	ppac_send_frame(0, notes);
}

/******************************************************************************
 * START OF PRIVATE FUNCTIONS
 ******************************************************************************/

bool __neigh_add(struct neigh_table_t *nt, struct neigh_t **cur_ptr,
		 struct neigh_t *new_n, bool replace)
{
	struct neigh_t *current;
	uint32_t count;

	assert(nt != NULL);
	assert(new_n != NULL);

	count = nt->stats->entries + 1;
#ifdef NEIGH_RCU_ENABLE
	current = rcu_dereference(*cur_ptr);
#else
	current = *cur_ptr;
#endif
	/* If there is an existing n, replace it */
	if (current != NULL) {
		if (replace == true) {
			new_n->next = current->next;
			if (false == __neigh_delete(nt, cur_ptr))
				return false;
		} else {
			return false;
		}
	} else {
		/* If there is not an existing n, check if there is room */
		if (likely(count < NEIGH_TABLE_MAX_ENTRIES)) {
#ifdef STATS_TBD
			decorated_notify_inc_32(&nt->stats->entries);
#endif
			new_n->next = NULL;
		} else {
			return false;
		}
	}
#ifdef NEIGH_RCU_ENABLE
	rcu_assign_pointer(*cur_ptr, new_n);
#else
	*cur_ptr = new_n;
#endif
	return true;
}

bool __neigh_delete(struct neigh_table_t *nt, struct neigh_t **nptr)
{
	struct neigh_t *n;

#ifdef NEIGH_RCU_ENABLE
	n = rcu_dereference(*nptr);
#else
	n = *nptr;
#endif
	if (n != NULL) {
#ifdef NEIGH_RCU_ENABLE
		rcu_assign_pointer(*nptr, n->next);
/*TBD		rcu_free(&__neigh_free, n, nt);*/
		__neigh_free(n, nt);
#else
		*nptr = n->next;
		__neigh_free(n, nt);
#endif
		nt->stats->entries--;
	}
	return (n != NULL);
}

static struct neigh_t *__neigh_lookup(struct neigh_bucket_t *bucket,
				      uint32_t key, uint32_t keylen)
{
	struct neigh_t *n;
	struct neigh_t **cur_ptr;

#ifdef NEIGH_RCU_ENABLE
	rcu_read_lock();
#endif
	cur_ptr = __neigh_find(bucket, key, keylen);
	if (unlikely(cur_ptr == NULL))
		return NULL;
#ifdef NEIGH_RCU_ENABLE
	n = rcu_dereference(*cur_ptr);
	rcu_read_unlock();
#else
	n = *cur_ptr;
#endif
	return n;
}

static struct neigh_bucket_t *__neigh_find_bucket(struct neigh_table_t *nt,
						  uint32_t key, uint32_t keylen)
{
	uint32_t hash;

	hash = compute_neigh_hash(&key, keylen);
	if (unlikely(hash >= NEIGH_TABLE_BUCKETS))
		return NULL;
	return &(nt->buckets[hash]);
}

static struct neigh_t **__neigh_find(struct neigh_bucket_t *bucket,
				     uint32_t key, uint32_t keylen)
{
	struct neigh_t **nptr;
	struct neigh_t *n;

	/*
	   This n is RCU protected, but this does not require a
	   rcu_deference, since this simply acquires the address of the
	   protected entity - this value will NOT change, and is not protected
	   by the RCU lock.
	 */
	nptr = &(bucket->head);
	if (unlikely(nptr == NULL))
		return NULL;
#ifdef NEIGH_RCU_ENABLE
	n = rcu_dereference(*nptr);
#else
	n = *nptr;
#endif
	while (n != NULL) {
		if (n->proto_addr[0] == key) {
			break;
		} else {
			/* Does not require rcu_dereference, addr only */
			nptr = &(n->next);
		}
#ifdef NEIGH_RCU_ENABLE
		n = rcu_dereference(*nptr);
#else
		n = *nptr;
#endif
	}

	return nptr;
}

void __neigh_free(void *n, void *ctxt)
{
	struct neigh_table_t *nt;

	nt = ctxt;
	mem_cache_free(nt->free_entries, n);
}
