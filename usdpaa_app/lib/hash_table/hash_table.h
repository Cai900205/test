
/* Copyright (c) 2014 Freescale Semiconductor, Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright
 *	 notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *	 notice, this list of conditions and the following disclaimer in the
 *	 documentation and/or other materials provided with the distribution.
 *     * Neither the name of Freescale Semiconductor nor the
 *	 names of its contributors may be used to endorse or promote products
 *	 derived from this software without specific prior written permission.
 *
 *
 * ALTERNATIVELY, this software may be distributed under the terms of the
 * GNU General Public License ("GPL") as published by the Free Software
 * Foundation, either version 2 of that License or (at your option) any
 * later version.
 *
 * THIS SOFTWARE IS PROVIDED BY Freescale Semiconductor ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL Freescale Semiconductor BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef __HASH_TABLE_H
#define __HASH_TABLE_H


#include <stdint.h>


struct hash_table;

typedef uint32_t hash_function(uint8_t *key, uint32_t size);


/* Create and initialize a hash table */
struct hash_table *hash_table_create(uint32_t key_size, uint32_t num_buckets,
			uint32_t max_items, hash_function *function);

/* Insert a new key into the hash table */
int hash_table_insert(struct hash_table *tbl, uint8_t *key, void *data);

/*
 * Find the provided key in the hash table and optionally returns the associated
 * user data pointer if successful. [data] is accepted also as NULL.
 */
int hash_table_find(const struct hash_table *tbl, uint8_t *key, void **data);

/*
 * Remove a specific key from the hash table. Optionally returns the user data
 * pointer provided when the entry was inserted. [data] is accepted also as
 * NULL.
 */
int hash_table_remove(struct hash_table *tbl, uint8_t *key, void **data);

/* Free the resources used by the hash table */
void hash_table_destroy(struct hash_table *tbl);


#endif /* __HASH_TABLE_H */
