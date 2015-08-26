/**
 \file mem_cache.c
 */
/*
 * Copyright (C) 2010-2011 Freescale Semiconductor, Inc.
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

#include <usdpaa/compat.h>
#include <usdpaa/dma_mem.h>

#include <usdpaa/compat.h>

#include <mutex.h>

#include "mem_cache.h"
#include "app_common.h"

struct mem_cache_t *cache_cache;

static struct mem_cache_t *__mem_cache_create(size_t objsize,
					      uint32_t objcount,
					      void *cache_struct_mem,
					      void *ptr_array_mem);

static void *__mem_cache_slab_alloc(uint32_t size);

static uint32_t __mem_cache_refill(struct mem_cache_t *cachep,
				   uint32_t count, void *refill_mem);

struct mem_cache_t *mem_cache_init(void)
{
	void *cache_mem;
	void *ptr_array_mem;
	void *first_cache;
	uint32_t mem_space;
	uint32_t ptr_array_space;

	/* Allocate memory for all of the caches, and zero it */
	/* Memory for soft caches should be allocated from dma_mem region.
	This region is permanently mapped and so won't suffer TLB faults
	unlike conventional memory allocations */
	mem_space = sizeof(struct mem_cache_t) * (MAX_MEM_CACHES);
	cache_mem = __dma_mem_memalign(L1_CACHE_BYTES, mem_space);
	if (unlikely(!cache_mem))
		return NULL;

	memset(cache_mem, 0, mem_space);

	ptr_array_space = sizeof(uintptr_t) * MAX_MEM_CACHES;
	ptr_array_mem = __dma_mem_memalign(L1_CACHE_BYTES, ptr_array_space);
	if (ptr_array_mem == NULL) {
		__dma_mem_free(cache_mem);
		return NULL;
	}


	memset(ptr_array_mem, 0, ptr_array_space);

	/* The memory for the rest of the caches starts right past this one */
	cache_cache = __mem_cache_create(sizeof(struct mem_cache_t),
					 MAX_MEM_CACHES - 1,
					 cache_mem, ptr_array_mem);

	first_cache = (struct mem_cache_t *)cache_mem + 1;
	__mem_cache_refill(cache_cache, MAX_MEM_CACHES - 1, first_cache);

	return cache_cache;
}

struct mem_cache_t *mem_cache_create(size_t objsize, uint32_t capacity)
{
	struct mem_cache_t *cachep;
	void *parray_mem;

	cachep = mem_cache_alloc(cache_cache);
	if (cachep == NULL)
		return NULL;

	parray_mem = __dma_mem_memalign(L1_CACHE_BYTES,
					sizeof(void *) * capacity);
	if (parray_mem == NULL) {
		mem_cache_free(cache_cache, cachep);
		return NULL;
	}

	cachep->objsize = objsize;
	cachep->free_limit = capacity;
	cachep->ptr_stack = parray_mem;
	cachep->next_free = 0;
	cachep->obj_allocated = 0;
	mutex_init(&(cachep->mmlock));

	return cachep;
}

/*
 * Allocates memory of a fixed size from a ppmem_cache.
 */
void *mem_cache_alloc(struct mem_cache_t *cachep)
{
	int32_t idx;
	void *retval;

	mutex_lock(&(cachep->mmlock));
	idx = cachep->next_free - 1;
	if (likely(idx >= 0)) {
		retval = cachep->ptr_stack[idx];
		cachep->next_free = idx;
		cachep->obj_allocated += 1;
	} else {
		retval = NULL;
	}
	mutex_unlock(&(cachep->mmlock));

	return retval;
}

/*
 * Places an object back in a ppmem cache.
 */
int32_t mem_cache_free(struct mem_cache_t *cachep, void *objp)
{
	int32_t idx, retval;

	mutex_lock(&(cachep->mmlock));
	idx = cachep->next_free;
	if (likely(idx < (int32_t) cachep->free_limit)) {
		cachep->ptr_stack[idx] = objp;
		cachep->next_free++;
		cachep->obj_allocated -= 1;
		retval = 0;
	} else {
		retval = -1;
	}
	mutex_unlock(&(cachep->mmlock));

	return retval;
}

/**
 \brief Refills the cache with buffers
 \param[in] cachep
 \param[in] count
 \return status of refill command
 \note Currently, we are adding zone->align to all memory slab allocated
  memory regions, and then using this as a keepout to store prefixed ppmalloc
  tags.	 Hack, but...works.  Needs to be done the right way eventually.
 */

int32_t mem_cache_refill(struct mem_cache_t *cachep, uint32_t count)
{
	void *mem;
	uint32_t retval, max_objs;

	mutex_lock(&(cachep->mmlock));

	max_objs = cachep->obj_allocated + cachep->next_free + count;
	if (max_objs > cachep->free_limit) {
		retval = -1;
		goto drop;
	}

	mem = __mem_cache_slab_alloc((count * cachep->objsize) +
				     L1_CACHE_BYTES);
	if (mem == NULL) {
		retval = 0;
	} else {
		mem += L1_CACHE_BYTES;
		retval = __mem_cache_refill(cachep, count, mem);
	}

drop:
	mutex_unlock(&(cachep->mmlock));

	return retval;
}

static void *__mem_cache_slab_alloc(uint32_t size)
{
	return __dma_mem_memalign(L1_CACHE_BYTES, size);
}

static uint32_t __mem_cache_refill(struct mem_cache_t *cachep,
				   uint32_t count, void *refill_mem)
{
	uint32_t idx, new_index, objs_added;
	void *objp;

	BUG_ON(refill_mem == NULL);

	idx = cachep->next_free;
	new_index = ((idx + count) <= cachep->free_limit) ?
	    (idx + count) : cachep->free_limit;
	objs_added = (new_index - idx);
	objp = refill_mem;
	for (; idx < new_index; idx++) {
		cachep->ptr_stack[idx] = objp;
		objp += cachep->objsize;
	}
	cachep->next_free = new_index;

	return objs_added;
}

static struct mem_cache_t *__mem_cache_create(size_t objsize,
					      uint32_t objcount,
					      void *cache_struct_mem,
					      void *ptr_array_mem)
{
	struct mem_cache_t *cachep;

	cachep = (struct mem_cache_t *)cache_struct_mem;
	if (cachep != NULL) {
		cachep->objsize = objsize;
		cachep->free_limit = objcount;
		cachep->ptr_stack = ptr_array_mem;
		cachep->next_free = 0;
		mutex_init(&(cachep->mmlock));
		cachep->obj_allocated = 0;
	}
	return cachep;
}
