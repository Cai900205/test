/**
 \file ip_rc.c
 \brief Implements a simple, fast route cache for ip forwarding.
  Currently 1024 RC entries exists in Route Cache
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

/**
 RC Structure
 The route cache is structured as 1024 fixed buckets, allocated in an
 array of size RC_BUCKETS.  Each bucket contains a write lock, and a pointer
 to the head of the chained list of hash entries for this bucket.  Also, it
 contains its bucket id, which is simply an index.
 The hash entries contain information about a particular entry, including
 a valid flag, destination IP address (daddr), egress interface, and
 the next- hop IP.

 The write lock is designed to be used for the write side of an RCU-style
 transaction to add/remove entries from a single bucket.  So, a reader need
 not acquire a traditional lock to read head and subsequent entries in a
 bucket, since the head pointer and all entry's next pointers are atomically
 updated as necessary, so partial updates are never seen.  To modify any
 member of the list, a writer must acquire the write lock.

 To remove an entry, since there is no reader lock, we must guanantee that
 no reader still has a reference to the specified entry.  We do this using
 the rcu_synchronize() call, which forces all CPUs to clear a paticular
 barrier outside of any RCU read-side critical sections.  This guarantees
 that no references are held by this CPU, since it is illegal to hold an
 RCU-protected reference outside of a read-side critical section.

 So, a route cache might look something like this:


 | id = 0 |	| valid = true	|
 | wlock  |	| daddr		|
 | head --|-->	| egress_iface	|
		| next_hop	|
		| next_entry ---|--> NULL

 | id = 1 |
 | wlock  |
 | head --|--> NULL

 . . .


 | id = N |	| valid = false |   | valid = true  |
 | wlock  |	| daddr		|   | daddr	    |
 | head --|-->	| egress_iface	|   | egress_iface  |
		| next_hop	|   | next_hop	    |
		| next_entry ---|-->| next_entry ---|--> NULL

 FINDING ENTRIES
 To find if a particular entry exists, given a destination address (daddr)
 do the following:
 1.  Hash the destination address, and get a bucket id.
 2.  Use this bucket id to get a reference to the correct bucket in the
	bucket array.
 3.  Acquire a pointer (p) to the head pointer - do not dereference yet:


 | id = N |	| valid = false |   | valid = true  |
 | wlock  |	| daddr		|   | daddr	    |
 | head --|-->	| egress_iface	|   | egress_iface  |
  -^------	| next_hop	|   | next_hop	    |
   |		| next_entry ---|-->| next_entry ---|--> NULL
   |		 ---------------     ---------------
   |
   p

 4.  Start a read-side critical section
 5.  Dereference (p), call it (entry) (entry = *p).


  --------     ---------------	   ---------------
 | id = N |   | valid = false |	  | valid = true  |
 | wlock  |   | daddr	      |	  | daddr	  |
 | head --|-->| egress_iface  |	  | egress_iface  |
  -^------    | next_hop      |	  | next_hop	  |
  |	----> | next_entry ---|-->| next_entry ---|--> NULL
  |    |       ---------------	   ---------------
  |    |
  p  entry

 6.  If entry == NULL, goto 9 (assume it's not).
 7.  If the entry->valid == true, and entry->daddr == daddr, then go to 10
     Valid was false, so continuing

 8.  p = &(entry->next_entry):

		 ---------------     ---------------
  --------	| valid = false |   | valid = true  |
 | id = N |	| daddr		|   | daddr	    |
 | wlock  |	| egress_iface	|   | egress_iface  |
 | head --|-->	| next_hop	|   | next_hop	    |
  --------	| next_entry ---|-->| next_entry ---|--> NULL
	---->	 -^-------------     ---------------
	|	  |
	|	  p
       entry

 9.  (entry = *p):

		 --------------	     ---------------
  --------	| valid = false |   | valid = true  |
 | id = N |	| daddr		|   | daddr	    |
 | wlock  |	| egress_iface	|   | egress_iface  |
 | head --|-->	| next_hop	|   | next_hop	    |
  --------	| next_entry ---|-->| next_entry ---|--> NULL
		 -^-------------     ---------------
		  |			^
		  p			|
				entry ---

 9.  Go to 6.

 10.  (entry = *p)
 11.  If entry != NULL, return true, and set next_hop and egress_iface.
 Otherwise, return false.

 ADDING ENTRIES

 DELETING ENTRIES
 */

#include "ip_rc.h"

#include "mm/mem_cache.h"
#ifdef IP_RCU_ENABLE
#include "rcu_lock.h"
#endif

#include <usdpaa/dma_mem.h>

#define BYTES_PER_WORD sizeof(uint32_t)

struct rc_bucket_t *__rc_find_bucket(struct rc_t *rc,
				     in_addr_t saddr,
				     in_addr_t daddr);

struct rc_entry_t **__rc_find_entry(struct rc_t *rc,
				    struct rc_bucket_t *bucket,
				    in_addr_t saddr,
				    in_addr_t daddr);

struct rt_dest_t *__rc_lookup(struct rc_t *rc,
			      struct rc_bucket_t *bucket,
			      in_addr_t saddr,
			      in_addr_t daddr);

struct rc_entry_t *rc_entry_fast_lookup(struct rc_t *rc,
					in_addr_t saddr,
					in_addr_t daddr,
					uint32_t idx)
{
	struct rc_entry_t *entry = NULL;
	struct rc_entry_t **entry_ptr;
	struct rc_bucket_t *bucket;

	BUG_ON(idx >= RC_BUCKETS);
	bucket = &(rc->buckets[idx]);
#ifdef IP_RCU_ENABLE
	rcu_read_lock();
#endif
	entry_ptr = __rc_find_entry(rc, bucket, saddr, daddr);
#ifdef IP_RCU_ENABLE
	entry = rcu_dereference(*entry_ptr);
	if (entry != NULL) {
#else
	entry = *entry_ptr;
	if (entry != NULL) {
#endif
#ifdef STATS_TBD
		decorated_notify_inc_64(&(rc->stats->hits));
#endif
	}
#ifdef IP_RCU_ENABLE
	rcu_read_unlock();
#endif
	return entry;
}

struct rc_entry_t *rc_entry_lookup(struct rc_t *rc,
				   in_addr_t saddr,
				   in_addr_t daddr)
{
	struct rc_bucket_t *bucket;
	struct rc_entry_t *entry;
	struct rc_entry_t **entry_ptr;

	bucket = __rc_find_bucket(rc, saddr, daddr);
#ifdef IP_RCU_ENABLE
	rcu_read_lock();
#endif
	entry_ptr = __rc_find_entry(rc, bucket, saddr, daddr);
#ifdef IP_RCU_ENABLE
	entry = rcu_dereference(*entry_ptr);
	if (entry != NULL) {
#else
	entry = *entry_ptr;
	if (entry != NULL) {
#endif
#ifdef STATS_TBD
		decorated_notify_inc_64(&(rc->stats->hits));
#endif
	} else {
#ifdef STATS_TBD
		decorated_notify_inc_64(&(rc->stats->misses));
#endif
	}

#ifdef IP_RCU_ENABLE
	rcu_read_unlock();
#endif
	return entry;
}

struct rc_t *rc_init(uint32_t expire_jiffies, uint32_t proto_len)
{
	int i;
	uint32_t entries;
	struct rc_bucket_t *bucket;
	struct rc_t *rc;

	BUG_ON((proto_len % BYTES_PER_WORD) != 0);

	/* Allocate memory for route cache from dma_mem region.
	This region is permanently mapped and so won't suffer TLB
	faults unlike conventional memory allocations */
	rc = __dma_mem_memalign(L1_CACHE_BYTES, sizeof(struct rc_t));
	if (rc == NULL) {
		pr_err("%s : Route Cache Creation Failed", __func__);
		return NULL;
	}
	rc->stats =
		__dma_mem_memalign(L1_CACHE_BYTES,
				   sizeof(struct rc_statistics_t));
	if (rc->stats == NULL) {
		pr_err("%s : Unable to allocate Route Cache Stats\n",
							__func__);
		return NULL;
	}
	memset(rc->stats, 0, sizeof(struct rc_statistics_t));
	rc->free_entries = mem_cache_create(sizeof(struct rc_entry_t),
					    RC_ENTRY_POOL_SIZE);
	if (unlikely(rc->free_entries == NULL)) {
		pr_err("%s : Unable to create Free Route Cache"
				"Entries\n", __func__);
		__dma_mem_free(rc->stats);
		return NULL;
	}

	entries = mem_cache_refill(rc->free_entries, RC_ENTRY_POOL_SIZE);
	if (unlikely(entries != RC_ENTRY_POOL_SIZE)) {
		__dma_mem_free(rc->stats);
		/** \todo mem_cache_destory(rc->free_entries); */
		return NULL;
	}

	rc->expire_jiffies = expire_jiffies;
	rc->proto_len = proto_len;
	rc->proto_word_len = proto_len / BYTES_PER_WORD;
	for (i = 0; i < ARRAY_SIZE(rc->buckets); i++) {
		bucket = &(rc->buckets[i]);
		bucket->head_entry = NULL;
		mutex_init(&bucket->wlock);
	}

	return rc;
}

void rc_delete(struct rc_t *rc)
{
#if 0
	int i;
	uint32_t entries;

	entries = mem_cache_refill(rc->free_entries, RC_ENTRY_POOL_SIZE);
	if (entries != RC_ENTRY_POOL_SIZE)
		return NULL;

	rc->free_entries = mem_cache_create(sizeof(struct rc_entry_t),
					    RC_ENTRY_POOL_SIZE);
	if (rc->free_entries == NULL)
		return NULL;
#endif
	__dma_mem_free(rc->stats);

	__dma_mem_free(rc);

	return;
}

struct rt_dest_t *rc_lookup(struct rc_t *rc,
			    in_addr_t saddr,
			    in_addr_t daddr)
{
	struct rc_bucket_t *bucket;
	struct rt_dest_t *dest;

	bucket = __rc_find_bucket(rc, saddr, daddr);
	dest = __rc_lookup(rc, bucket, saddr, daddr);
	/*
	   Do not need to hold a reference here - it is not held across
	   across iterations, so it will be caught by RCU
	   refcount_acquire(&dest->refcnt);
	 */

	return dest;
}

struct rt_dest_t *rc_fast_lookup(struct rc_t *rc,
				 in_addr_t saddr,
				 in_addr_t daddr,
				 uint32_t idx)
{
	struct rc_bucket_t *bucket;
	struct rt_dest_t *dest;

	BUG_ON(idx >= RC_BUCKETS);
	bucket = &(rc->buckets[idx]);
	dest = __rc_lookup(rc, bucket, saddr, daddr);
	/*
	   Do not need to hold a reference here - it is not held across
	   across iterations, so it will be caught by RCU
	   refcount_acquire(&dest->refcnt);
	 */

	return dest;
}

struct rc_entry_t *rc_create_entry(struct rc_t *rc)
{
	return mem_cache_alloc(rc->free_entries);
}

void rc_free_entry(struct rc_t *rc, struct rc_entry_t *entry)
{
	mem_cache_free(rc->free_entries, entry);
}

bool rc_add_entry(struct rc_t *rc, struct rc_entry_t *new_entry)
{
	struct rc_bucket_t *bucket;
	struct rc_entry_t **entry_ptr;
	struct rc_entry_t *prev_entry;
#ifdef STATS_TBD
	uint32_t count;
#endif
	bool success;

	success = false;
	/* No decorated api required as USDPAA application
	 would update rc table */
#ifdef STATS_TBD
	count = rc->stats->entry_count;
	if (count < MAX_RC_ENTRIES) {
#endif
		bucket = __rc_find_bucket(rc, new_entry->saddr, new_entry->daddr);
#ifdef IP_RCU_ENABLE
		rcu_read_lock();
#endif
		mutex_lock(&(bucket->wlock));
		entry_ptr = __rc_find_entry(rc, bucket,
					    new_entry->saddr, new_entry->daddr);
#ifdef IP_RCU_ENABLE
		prev_entry = rcu_dereference(*entry_ptr);
#else
		prev_entry = *entry_ptr;
#endif
		/* We did NOT find in the entry already in the list, insert */
		if (prev_entry == NULL) {
			new_entry->next = NULL;
#ifdef IP_RCU_ENABLE
			rcu_assign_pointer(*entry_ptr, new_entry);
#else
			*entry_ptr = new_entry;
#endif
			success = true;
		}
		mutex_unlock(&(bucket->wlock));
#ifdef IP_RCU_ENABLE
		rcu_read_unlock();
#endif
#ifdef STATS_TBD
	}

	if (success == true) {
		decorated_notify_inc_32(&(rc->stats->new_entries));
		decorated_notify_inc_32(&(rc->stats->entry_count));
	}
#endif
	return success;
}

bool rc_add_update_entry(struct rc_t *rc, struct rc_entry_t *new_entry)
{
	struct rc_bucket_t *bucket;
	struct rc_entry_t **entry_ptr;
	struct rc_entry_t *prev_entry;
#ifdef STATS_TBD
	uint32_t count;
#endif
	bool success;

	success = false;
#ifdef STATS_TBD
	count = rc->stats->entry_count;
	if (count < MAX_RC_ENTRIES) {
#endif
		bucket = __rc_find_bucket(rc, new_entry->saddr, new_entry->daddr);
#ifdef IP_RCU_ENABLE
		rcu_read_lock();
#endif
		mutex_lock(&(bucket->wlock));
		entry_ptr = __rc_find_entry(rc, bucket,
					    new_entry->saddr, new_entry->daddr);
#ifdef IP_RCU_ENABLE
		prev_entry = rcu_dereference(*entry_ptr);
#else
		prev_entry = *entry_ptr;
#endif
		/* We did NOT find in the entry already in the list, insert */
		if (prev_entry == NULL) {
			new_entry->next = NULL;
#ifdef IP_RCU_ENABLE
			rcu_assign_pointer(*entry_ptr, new_entry);
#else
			*entry_ptr = new_entry;
#endif
			success = true;
		} else {
			prev_entry->dest = new_entry->dest;
		}
		mutex_unlock(&(bucket->wlock));
#ifdef IP_RCU_ENABLE
		rcu_read_unlock();
#endif
#ifdef STATS_TBD
	}

	if (success == true) {
		decorated_notify_inc_32(&(rc->stats->new_entries));
		decorated_notify_inc_32(&(rc->stats->entry_count));
	}
#endif
	return success;
}

bool rc_remove_entry(struct rc_t *rc,
		     in_addr_t saddr,
		     in_addr_t daddr)
{
	struct rc_entry_t **entry_ptr;
	struct rc_entry_t *entry;
	struct rc_bucket_t *bucket;

	bucket = __rc_find_bucket(rc, saddr, daddr);

#ifdef IP_RCU_ENABLE
	rcu_read_lock();
#endif
	mutex_lock(&(bucket->wlock));
	entry_ptr = __rc_find_entry(rc, bucket, saddr, daddr);
#ifdef IP_RCU_ENABLE
	entry = rcu_dereference(*entry_ptr);
#else
	entry = *entry_ptr;
#endif
	if (entry != NULL) {
		/* TODO: Need an rcu_dereference for entry_ptr here? */
#ifdef IP_RCU_ENABLE
		rcu_assign_pointer(*entry_ptr, entry->next);
#else
		*entry_ptr = entry->next;
#endif
	}
	mutex_unlock(&(bucket->wlock));
#ifdef IP_RCU_ENABLE
	rcu_read_unlock();
#endif
	if (entry != NULL) {
#ifdef STATS_TBD
		rc->stats->entry_count--;
		decorated_notify_inc_32(&(rc->stats->stale_entries));
		decorated_notify_inc_32(&(rc->stats->removed_entries));
#endif
#ifdef IP_RCU_ENABLE
/*TBD		rcu_free(&__rc_free_entry, entry, rc);*/
		__rc_free_entry(entry, rc);
#else
		__rc_free_entry(entry, rc);
#endif

	}

	return (entry != NULL);
}

void rc_exec_per_entry(struct rc_t *rc, rc_execfn_t execfn)
{
	struct rc_bucket_t *bucket;
	struct rc_entry_t *entry;
	uint32_t i;

	for (i = 0; i < RC_BUCKETS; i++) {
#ifdef IP_RCU_ENABLE
		rcu_read_lock();
#endif
		bucket = &rc->buckets[i];
#ifdef IP_RCU_ENABLE
		entry = rcu_dereference(bucket->head_entry);
#else
		entry = bucket->head_entry;
#endif
		while (entry != NULL) {
			execfn(entry);
#ifdef IP_RCU_ENABLE
			entry = rcu_dereference(entry->next);
#else
			entry = entry->next;
#endif
		}
	}
}

struct rt_dest_t *__rc_lookup(struct rc_t *rc,
			      struct rc_bucket_t *bucket,
			      in_addr_t saddr,
			      in_addr_t daddr)
{
	struct rc_entry_t *entry;
	struct rc_entry_t **entry_ptr;
	struct rt_dest_t *dest;

	dest = NULL;
#ifdef IP_RCU_ENABLE
	rcu_read_lock();
#endif
	entry_ptr = __rc_find_entry(rc, bucket, saddr, daddr);
#ifdef IP_RCU_ENABLE
	entry = rcu_dereference(*entry_ptr);
	if (entry != NULL) {
#else
	entry = *entry_ptr;
	if (entry != NULL) {
#endif
		dest = entry->dest;
#ifdef STATS_TBD
		decorated_notify_inc_64(&(rc->stats->hits));
#endif
	} else {
#ifdef STATS_TBD
		decorated_notify_inc_64(&(rc->stats->misses));
#endif
	}
#ifdef IP_RCU_ENABLE
	rcu_read_unlock();
#endif
	return dest;
}

struct rc_bucket_t *__rc_find_bucket(struct rc_t *rc,
				     in_addr_t saddr,
				     in_addr_t daddr)
{
	uint32_t hash;

	hash = compute_rc_hash(saddr, daddr);
	pr_debug("Bucket hash is %x\n", hash);
	pr_debug("Src = 0x%x Dest = 0x%x\n", saddr, daddr);
	return &(rc->buckets[hash]);
}


struct rc_entry_t **__rc_find_entry(struct rc_t *rc,
				    struct rc_bucket_t *bucket,
				    in_addr_t saddr,
				    in_addr_t daddr)
{
	struct rc_entry_t **entry_ptr;
	struct rc_entry_t *entry;

	/*
	   This entry is RCU protected, but this does not require a
	   rcu_deference, since this simply acquires the address of the
	   protected entity - this value will NOT change, and is not protected
	   by the RCU lock.
	 */
	entry_ptr = &(bucket->head_entry);
#ifdef IP_RCU_ENABLE
	entry = rcu_dereference(*entry_ptr);
#else
	entry = *entry_ptr;
#endif
	while (entry != NULL) {
		if ((entry->saddr == saddr)
		    && (entry->daddr == daddr)) {
			break;
		} else {
			/* Again, does not require rcu_deference, addr only */
			entry_ptr = &(entry->next);
		}
#ifdef IP_RCU_ENABLE
		entry = rcu_dereference(*entry_ptr);
#else
		entry = *entry_ptr;
#endif
	}

	return entry_ptr;
}

#ifdef STATS_TBD
void rc_print_statistics(struct rc_t *rc, bool print_zero)
{
	print_stat64(&(rc->stats->hits), "rc_hits", print_zero);
	print_stat64(&(rc->stats->misses), "rc_misses", print_zero);
}
#endif

void __rc_free_entry(void *entry, void *ctxt)
{
	struct rc_t *rc;

	rc = ctxt;
	mem_cache_free(rc->free_entries, entry);
	rc->stats->stale_entries--;
}
