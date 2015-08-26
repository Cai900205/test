/**
 \file mem_cache.h
 \brief mem_cache structures
 */
/*
 * Copyright (C) 2010 - 2011 Freescale Semiconductor, Inc.
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
#ifndef __LIB_MM_MEM_CACHE_H
#define __LIB_MM_MEM_CACHE_H 1

#include <usdpaa/compat.h>

#include <mutex.h>

#include <stdbool.h>
#include <stdio.h>

/*!
 We assume that our caches cannot ever grow or shrink in size, so the
 "slab" portion of the allocator is removed, simplifying the design.
 */

#define MAX_MEM_CACHES		      47

/**
 \brief Mem Cache Structure
 */
struct mem_cache_t {
	mutex_t mmlock;		/**< Lock for accesing the Mem Cache structure*/
	uint32_t objsize;	/**< Size of the Object*/
	uint32_t obj_allocated;	/**< Number of Allocated Objects*/
	uint32_t free_limit;	/**< Max Number of Free Objects*/
	uint32_t next_free;	/**< Next Free Object in the Array pointed to by ptr_stack*/
	void **ptr_stack;	/**< Pointer to the array of Objects*/
};

/**
   \brief Initializes the CACHE_CACHE variable, as well as all of the General caches.
   \return cache_cache.
 */
struct mem_cache_t *mem_cache_init(void);

/**
   \brief Creates a new object cache in Coherent memory space.
   \param[in] size of each object in the cache.
   \param[in] objcount The number of object that should be initially allocated.
   \return pointer to the newly created cache.
 */
struct mem_cache_t *mem_cache_create(size_t objsize, uint32_t objcount);

/**
  \brief Destroys the specified cache.
  \param[in] The cache to destroy.
  \return Zero on success, non-zero if it cannot be freed.
 */
int32_t mem_cache_destroy(struct mem_cache_t *cachep);

/**
    \brief Allocates memory of a fixed size from a ppmem_cache.
    \param[in] cachep The cache from which to allocate an object.
    \return a pointer to the requested memory of the size contained in cachep,
	or NULL if none is available.
 */
void *mem_cache_alloc(struct mem_cache_t *cachep);

int32_t mem_cache_refill(struct mem_cache_t *cachep, uint32_t count);

/**
   \brief Places an object back in a ppmem cache.
   \param[inout] cachep The cache to which this object should be returned.
   \param[in] objp A pointer to the memory to be returned.
   \return 0 if the operation was successful, non-zero otherwise.  Currently,
   the only cause for an unsuccessful free is freeing more memory back to the
   cache than was originally allocated.
 */
int32_t mem_cache_free(struct mem_cache_t *cachep, void *objp);

void mem_cache_print(struct mem_cache_t *cachep);
extern struct mem_cache_t *cache_cache;

#endif /* __LIB_MM_MEM_CACHE_H */
