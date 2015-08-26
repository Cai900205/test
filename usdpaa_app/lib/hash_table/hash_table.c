
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

#include "hash_table.h"

#include <errno.h>
#include <error.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>


struct hash_table_node {
	/* The key used by the hash table */
	uint8_t *key;
	/* Pointer to user item */
	void *data;
	/* Next node in bucket */
	struct hash_table_node *next;
};

struct hash_table {
	/* Items currently in the table */
	uint32_t items;
	/* Maximum number of elements supported by table */
	uint32_t max_items;
	/* Number of hash buckets */
	uint32_t num_buckets;
	/* The size of the key supported by the table */
	uint32_t key_size;
	/* List of hash table entries */
	struct hash_table_node **nodes;
	/* Pointer to the hash function used to this hash table */
	hash_function *hash_func;
};


/* Create and initialize a hash table */
struct hash_table *hash_table_create(uint32_t key_size, uint32_t num_buckets,
				uint32_t max_items, hash_function *function)
{
	struct hash_table *new_tbl;

	/* (max_items == 0) is interpreted as "unlimited" */

	if (!num_buckets || !key_size) {
		error(0, EINVAL, "Hash table parameters");
		return NULL;
	}

	/* Check if the lookup function was provided. */
	if (!function) {
		error(0, EINVAL, "No lookup function was specified");
		return NULL;
	}

	/* Check if the number of buckets is a power of 2. Fail if not. */
	if (num_buckets & (num_buckets - 1)) {
		error(0, EINVAL, "Number of HASH buckets must be a power of 2");
		return NULL;
	}

	/* Allocate memory for hash table structure */
	new_tbl = malloc(sizeof(struct hash_table));
	if (!new_tbl) {
		error(0, ENOMEM, "No more memory for internal hash table structure");
		return NULL;
	}

	/* Allocate memory for array of hash table entries */
	new_tbl->nodes = calloc(sizeof(struct hash_table_node *), num_buckets);
	if (!new_tbl->nodes) {
		error(0, ENOMEM, "No more memory for the hash buckets");
		free(new_tbl);
		return NULL;
	}

	new_tbl->hash_func      = function;
	new_tbl->num_buckets    = num_buckets;
	new_tbl->key_size       = key_size;
	new_tbl->max_items      = max_items;
	new_tbl->items          = 0;

	return new_tbl;
}

/* Search for a specific key into the hash table  */
int hash_table_find(const struct hash_table *tbl, uint8_t *key, void **data)
{
	struct hash_table_node *current;
	unsigned int index;

	if (data)
		*data = NULL;

	if (!key || !tbl) {
		error(0, EINVAL, "Did not provide table or key to hash_table_find");
		return -EINVAL;
	}

	/* If table is empty there is nothing to do. */
	if (tbl->items == 0)
		return -ENOENT;

	index = tbl->hash_func(key, tbl->key_size) & (tbl->num_buckets - 1);

	for (current = tbl->nodes[index]; current; current = current->next)
		if (memcmp(current->key, key, tbl->key_size) == 0) {
			if (data)
				*data = current->data;
			return 0;
		}

	return -ENOENT;
}

/* Insert a new key into the hash table */
int hash_table_insert(struct hash_table *tbl, uint8_t *key, void *data)
{
	struct hash_table_node *new;
	unsigned int index;

	/*
	 * Will not allow the user to insert NULL data pointer because there
	 * will be no way for him/her to tell the difference between the
	 * situation when the item was not found and when the item was found
	 * but it was NULL.
	 */
	if (!key || !data || !tbl) {
		error(0, EINVAL, "Hash table insert params");
		return -EINVAL;
	}

	/*
	 * Check the maximum number of table elements. max_items = 0 means "no
	 * limit".
	 */
	if (tbl->max_items > 0 && tbl->items >= tbl->max_items)
		return -ENOSPC;

	if (tbl->items > 0 && !hash_table_find(tbl, key, NULL))
		return -EEXIST;

	index = tbl->hash_func(key, tbl->key_size) & (tbl->num_buckets - 1);

	/* Allocate memory for the new element */
	new = malloc(sizeof(*new));
	if (!new) {
		error(0, ENOMEM, "No more memory for new hash table entry");
		return -ENOMEM;
	}

	new->key = malloc(tbl->key_size);
	if (!new->key) {
		error(0, ENOMEM, "No more memory for new hash table key");
		free(new);
		return -ENOMEM;
	}

	/* Insert element */
	memcpy(new->key, key, tbl->key_size);
	new->data = data;
	new->next = tbl->nodes[index];
	tbl->nodes[index] = new;
	tbl->items++;

	return 0;
}

/* Remove a specific key from the hash table */
int hash_table_remove(struct hash_table *tbl, uint8_t *key, void **data)
{
	struct hash_table_node *current, *previous = NULL;
	unsigned int index;

	if (data)
		*data = NULL;

	if (!key || !tbl) {
		error(0, EINVAL, "Hash table remove params");
		return -EINVAL;
	}

	/* If table is empty there is nothing to do */
	if (tbl->items == 0)
		return -ENOENT;

	index = tbl->hash_func(key, tbl->key_size) & (tbl->num_buckets - 1);

	/* Search in this bucket */
	for (current = tbl->nodes[index]; current; current = current->next) {
		if (memcmp(current->key, key, tbl->key_size) == 0) {
			free(current->key);
			if (previous)
				previous->next = current->next;
			else
				tbl->nodes[index] = current->next;
			if (data)
				*data = current->data;
			free(current);
			tbl->items--;
			return 0;
		}
		previous = current;
	}

	return -ENOENT;
}

/* Free the resources used by the hash table */
void hash_table_destroy(struct hash_table *tbl)
{
	int i = 0;
	struct hash_table_node *arr, *tmp;

	if (!tbl)
		return;

	for (i = 0; i < tbl->num_buckets; i++) {
		arr = tbl->nodes[i];
		while (arr) {
			tmp = arr;
			arr = arr->next;
			free(tmp->key);
			free(tmp);
		}
	}

	free(tbl->nodes);
	free(tbl);
}
