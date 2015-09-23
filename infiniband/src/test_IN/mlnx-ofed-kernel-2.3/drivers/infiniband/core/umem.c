/*
 * Copyright (c) 2005 Topspin Communications.  All rights reserved.
 * Copyright (c) 2005 Cisco Systems.  All rights reserved.
 * Copyright (c) 2005 Mellanox Technologies. All rights reserved.
 *
 * This software is available to you under a choice of one of two
 * licenses.  You may choose to be licensed under the terms of the GNU
 * General Public License (GPL) Version 2, available from the file
 * COPYING in the main directory of this source tree, or the
 * OpenIB.org BSD license below:
 *
 *     Redistribution and use in source and binary forms, with or
 *     without modification, are permitted provided that the following
 *     conditions are met:
 *
 *      - Redistributions of source code must retain the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer.
 *
 *      - Redistributions in binary form must reproduce the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer in the documentation and/or other materials
 *        provided with the distribution.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include <linux/mm.h>
#include <linux/dma-mapping.h>
#include <linux/sched.h>
#include <linux/export.h>
#include <linux/hugetlb.h>
#include <linux/dma-attrs.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <rdma/ib_umem_odp.h>
#include "uverbs.h"

static int allow_weak_ordering;
module_param_named(weak_ordering, allow_weak_ordering, int, 0444);
MODULE_PARM_DESC(weak_ordering,  "Allow weak ordering for data registered memory");


static void umem_vma_open(struct vm_area_struct *area)
{
	/* Implementation is to prevent high level from merging some
	VMAs in case of unmap/mmap on part of memory area.
	Rlimit is handled as well.
	*/
	unsigned long total_size;
	unsigned long ntotal_pages;

	total_size = area->vm_end - area->vm_start;
	ntotal_pages = PAGE_ALIGN(total_size) >> PAGE_SHIFT;
	/* no locking is needed:
	umem_vma_open is called from vm_open which is always called
	with mm->mmap_sem held for writing.
	*/
	if (current->mm)
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3,2,0))
		current->mm->pinned_vm += ntotal_pages;
#else
		current->mm->locked_vm += ntotal_pages;
#endif
	return;
}

static void umem_vma_close(struct vm_area_struct *area)
{
	/* Implementation is to prevent high level from merging some
	VMAs in case of unmap/mmap on part of memory area.
	Rlimit is handled as well.
	*/
	unsigned long total_size;
	unsigned long ntotal_pages;

	total_size = area->vm_end - area->vm_start;
	ntotal_pages = PAGE_ALIGN(total_size) >> PAGE_SHIFT;
	/* no locking is needed:
	umem_vma_close is called from close which is always called
	with mm->mmap_sem held for writing.
	*/
	if (current->mm)
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3,2,0))
		current->mm->pinned_vm -= ntotal_pages;
#else
		current->mm->locked_vm -= ntotal_pages;
#endif
	return;

}

static const struct vm_operations_struct umem_vm_ops = {
	.open = umem_vma_open,
	.close = umem_vma_close
};

int ib_umem_map_to_vma(struct ib_umem *umem,
				struct vm_area_struct *vma)
{

	int ret;
	unsigned long ntotal_pages;
	unsigned long total_size;
	struct page *page;
	unsigned long vma_entry_number = 0;
	int i;
	unsigned long locked;
	unsigned long lock_limit;
	struct scatterlist *sg;

	/* Total size expects to be already page aligned - verifying anyway */
	total_size = vma->vm_end - vma->vm_start;
	/* umem length expexts to be equal to the given vma*/
	if (umem->length != total_size)
		return -EINVAL;

	ntotal_pages = PAGE_ALIGN(total_size) >> PAGE_SHIFT;
	/* ib_umem_map_to_vma is called as part of mmap
	with mm->mmap_sem held for writing.
	No need to lock.
	*/
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3,2,0))
	locked = ntotal_pages + current->mm->pinned_vm;
#else
	locked = ntotal_pages + current->mm->locked_vm;
#endif
	lock_limit = rlimit(RLIMIT_MEMLOCK) >> PAGE_SHIFT;

	if ((locked > lock_limit) && !capable(CAP_IPC_LOCK))
		return -ENOMEM;

	for_each_sg(umem->sg_head.sgl, sg, umem->npages, i) {
		/* We reached end of vma - going out from loop */
		if (vma_entry_number >= ntotal_pages)
			goto end;
		page = sg_page(sg);
		if (PageLRU(page) || PageAnon(page)) {
			/* Above cases are not supported
			    as of page fault issues for that VMA.
			*/
			ret = -ENOSYS;
			goto err_vm_insert;
		}
		ret = vm_insert_page(vma, vma->vm_start +
			(vma_entry_number << PAGE_SHIFT), page);
		if (ret < 0)
			goto err_vm_insert;

		vma_entry_number++;
	}

end:
	/* We expect to have enough pages   */
	if (vma_entry_number >= ntotal_pages) {
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3,2,0))
		current->mm->pinned_vm = locked;
#else
		current->mm->locked_vm = locked;
#endif
		vma->vm_ops =  &umem_vm_ops;
		return 0;
	}
	/* Not expected but if we reached here
	    not enough pages were available to be mapped into vma.
	*/
	ret = -EINVAL;
	WARN(1, KERN_WARNING
		"ib_umem_map_to_vma: number of pages mismatched(%lu,%lu)\n",
				vma_entry_number, ntotal_pages);

err_vm_insert:

	zap_vma_ptes(vma, vma->vm_start, total_size);
	return ret;

}
EXPORT_SYMBOL(ib_umem_map_to_vma);

static void ib_cmem_release(struct kref *ref)
{

	struct ib_cmem *cmem;
	struct ib_cmem_block *cmem_block, *tmp;
	unsigned long ntotal_pages;

	cmem = container_of(ref, struct ib_cmem, refcount);

	list_for_each_entry_safe(cmem_block, tmp, &cmem->ib_cmem_block, list) {
		__free_pages(cmem_block->page, cmem->block_order);
		list_del(&cmem_block->list);
		kfree(cmem_block);
	}
	/* no locking is needed:
	ib_cmem_release is called from vm_close which is always called
	with mm->mmap_sem held for writing.
	The only exception is when the process shutting down but in that case
	counter not relevant any more.*/
	if (current->mm) {
		ntotal_pages = PAGE_ALIGN(cmem->length) >> PAGE_SHIFT;
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3,2,0))
		current->mm->pinned_vm -= ntotal_pages;
#else
		current->mm->locked_vm -= ntotal_pages;
#endif
	}
	kfree(cmem);

}

/**
 * ib_cmem_release_contiguous_pages - release memory allocated by
 *                                              ib_cmem_alloc_contiguous_pages.
 * @cmem: cmem struct to release
 */
void ib_cmem_release_contiguous_pages(struct ib_cmem *cmem)
{
	kref_put(&cmem->refcount, ib_cmem_release);
}
EXPORT_SYMBOL(ib_cmem_release_contiguous_pages);

static void cmem_vma_open(struct vm_area_struct *area)
{
	struct ib_cmem *ib_cmem;
	ib_cmem = (struct ib_cmem *)(area->vm_private_data);
	
	/* vm_open and vm_close are always called with mm->mmap_sem held for
	writing. The only exception is when the process is shutting down, at
	which point vm_close is called with no locks held, but since it is
	after the VMAs have been detached, it is impossible that vm_open will
	be called. Therefore, there is no need to synchronize the kref_get and
	kref_put calls.*/
	kref_get(&ib_cmem->refcount);
}

static void cmem_vma_close(struct vm_area_struct *area)
{
	struct ib_cmem *cmem;
	cmem = (struct ib_cmem *)(area->vm_private_data);

	ib_cmem_release_contiguous_pages(cmem);

}


static const struct vm_operations_struct cmem_contig_pages_vm_ops = {
	.open = cmem_vma_open,
	.close = cmem_vma_close
};

/**
 * ib_cmem_map_contiguous_pages_to_vma - map contiguous pages into VMA
 * @ib_cmem: cmem structure returned by ib_cmem_alloc_contiguous_pages
 * @vma: VMA to inject pages into.
 */
int ib_cmem_map_contiguous_pages_to_vma(struct ib_cmem *ib_cmem,
						struct vm_area_struct *vma)
{

	int ret;
	unsigned long page_entry;
	unsigned long ntotal_pages;
	unsigned long ncontig_pages;
	unsigned long total_size;
	struct page *page;
	unsigned long vma_entry_number = 0;
	struct ib_cmem_block *ib_cmem_block = NULL;

	total_size = vma->vm_end - vma->vm_start;
	if (ib_cmem->length != total_size)
		return -EINVAL;

	if (total_size != PAGE_ALIGN(total_size)) {
		WARN(1,
		"ib_cmem_map: total size %lu not aligned to page size\n",
		total_size);
		return -EINVAL;
	}

	ntotal_pages = total_size >> PAGE_SHIFT;
	ncontig_pages = 1 << ib_cmem->block_order;

	list_for_each_entry(ib_cmem_block, &(ib_cmem->ib_cmem_block), list) {
		page = ib_cmem_block->page;
		for (page_entry = 0; page_entry < ncontig_pages; page_entry++) {
			/* We reached end of vma - going out from both loops */
			if (vma_entry_number >= ntotal_pages)
				goto end;

			ret = vm_insert_page(vma, vma->vm_start +
				(vma_entry_number << PAGE_SHIFT), page);
			if (ret < 0)
				goto err_vm_insert;

			vma_entry_number++;
			page++;
		}
	}

end:

	/* We expect to have enough pages   */
	if (vma_entry_number >= ntotal_pages) {
		vma->vm_ops =  &cmem_contig_pages_vm_ops;
		vma->vm_private_data = ib_cmem;
		return 0;
	}
	/* Not expected but if we reached here
	    not enough contiguous pages were registered
	*/
	ret = -EINVAL;

err_vm_insert:

	zap_vma_ptes(vma, vma->vm_start, total_size);
	return ret;

}
EXPORT_SYMBOL(ib_cmem_map_contiguous_pages_to_vma);



/**
 * ib_cmem_alloc_contiguous_pages - allocate contiguous pages
*  @context: userspace context to allocate memory for
 * @total_size: total required size for that allocation.
 * @page_size_order: order of one contiguous page.
 */
struct ib_cmem *ib_cmem_alloc_contiguous_pages(struct ib_ucontext *context,
				unsigned long total_size,
				unsigned long page_size_order)
{
	struct ib_cmem *cmem;
	unsigned long ntotal_pages;
	unsigned long ncontiguous_pages;
	unsigned long ncontiguous_groups;
	struct page *page;
	int i;
	int ncontiguous_pages_order;
	struct ib_cmem_block *ib_cmem_block;
	unsigned long locked;
	unsigned long lock_limit;

	if (page_size_order < PAGE_SHIFT || page_size_order > 31)
		return ERR_PTR(-EINVAL);

	cmem = kzalloc(sizeof *cmem, GFP_KERNEL);
	if (!cmem)
		return ERR_PTR(-ENOMEM);

	kref_init(&cmem->refcount);
	cmem->context   = context;
	INIT_LIST_HEAD(&cmem->ib_cmem_block);

	/* Total size is expected to be already page aligned -
	    verifying anyway.
	*/
	ntotal_pages = PAGE_ALIGN(total_size) >> PAGE_SHIFT;
	/* ib_cmem_alloc_contiguous_pages is called as part of mmap
	with mm->mmap_sem held for writing.
	No need to lock
	*/
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3,2,0))
	locked     = ntotal_pages + current->mm->pinned_vm;
#else
	locked     = ntotal_pages + current->mm->locked_vm;
#endif
	lock_limit = rlimit(RLIMIT_MEMLOCK) >> PAGE_SHIFT;

	if ((locked > lock_limit) && !capable(CAP_IPC_LOCK))
		goto err_alloc;

	/* How many contiguous pages do we need in 1 block */
	ncontiguous_pages = (1 << page_size_order) >> PAGE_SHIFT;
	ncontiguous_pages_order = ilog2(ncontiguous_pages);
	ncontiguous_groups = (ntotal_pages >> ncontiguous_pages_order)  +
		(!!(ntotal_pages & (ncontiguous_pages - 1)));
	
	/* Checking MAX_ORDER to prevent WARN via calling alloc_pages below */
	if (ncontiguous_pages_order >= MAX_ORDER)
		goto err_alloc;
	/* we set block_order before starting allocation to prevent
	   a leak in a failure flow in ib_cmem_release.
	   cmem->length has at that step value 0 from kzalloc as expected */
	cmem->block_order = ncontiguous_pages_order;
	for (i = 0; i < ncontiguous_groups; i++) {
		/* Allocating the managed entry */
		ib_cmem_block = kmalloc(sizeof(struct ib_cmem_block),
				GFP_KERNEL);
		if (!ib_cmem_block)
			goto err_alloc;

		page =  alloc_pages(GFP_HIGHUSER | __GFP_ZERO | __GFP_COMP |
					__GFP_NOWARN, ncontiguous_pages_order);
		if (!page) {
			kfree(ib_cmem_block);
			/* We should deallocate previous succeeded allocatations
			     if exists.
			*/
			goto err_alloc;
		}

		ib_cmem_block->page = page;
		list_add_tail(&ib_cmem_block->list, &cmem->ib_cmem_block);
	}

	cmem->length = total_size;

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3,2,0))
	current->mm->pinned_vm = locked;
#else
	current->mm->locked_vm = locked;
#endif
	return cmem;

err_alloc:

	ib_cmem_release_contiguous_pages(cmem);
	return ERR_PTR(-ENOMEM);

}
EXPORT_SYMBOL(ib_cmem_alloc_contiguous_pages);

static struct ib_umem *peer_umem_get(struct ib_peer_memory_client *ib_peer_mem,
				       struct ib_umem *umem, unsigned long addr,
				       int dmasync, int invalidation_supported)
{
	int ret;
	const struct peer_memory_client *peer_mem = ib_peer_mem->peer_mem;
	struct invalidation_ctx *invalidation_ctx = NULL;

	umem->ib_peer_mem = ib_peer_mem;
	if (invalidation_supported) {
		invalidation_ctx = kzalloc(sizeof(*invalidation_ctx), GFP_KERNEL);
		if (!invalidation_ctx) {
			ret = -ENOMEM;
			goto end;
		}
		umem->invalidation_ctx = invalidation_ctx;
		invalidation_ctx->umem = umem;
		mutex_lock(&ib_peer_mem->lock);
		ret = ib_peer_insert_context(ib_peer_mem, invalidation_ctx,
			&invalidation_ctx->context_ticket);
		/* unlock before calling get pages to prevent a dead-lock from the callback */
		mutex_unlock(&ib_peer_mem->lock);
		if (ret)
			goto end;
	}

	ret = peer_mem->get_pages(addr, umem->length, umem->writable, 1,
				&umem->sg_head, 
				umem->peer_mem_client_context,
				invalidation_ctx ?
				(void *)invalidation_ctx->context_ticket : NULL);

	if (invalidation_ctx) {
		/* taking the lock back, checking that wasn't invalidated at that time */
		mutex_lock(&ib_peer_mem->lock);
		if (invalidation_ctx->peer_invalidated) {
			printk(KERN_ERR "peer_umem_get: pages were invalidated by peer\n");
			ret = -EINVAL;
		}
	}

	if (ret)
		goto out;

	umem->page_size = peer_mem->get_page_size
					(umem->peer_mem_client_context);
	if (umem->page_size <= 0)
		goto put_pages;

	umem->address = addr;
	ret = peer_mem->dma_map(&umem->sg_head,
					umem->peer_mem_client_context,
					umem->context->device->dma_device,
					dmasync,
					&umem->nmap);
	if (ret)
		goto put_pages;

	ib_peer_mem->stats.num_reg_pages +=
			umem->nmap * (umem->page_size >> PAGE_SHIFT);
	ib_peer_mem->stats.num_alloc_mrs += 1;
	return umem;

put_pages:

	peer_mem->put_pages(umem->peer_mem_client_context,
					&umem->sg_head);
out:
	if (invalidation_ctx) {
		ib_peer_remove_context(ib_peer_mem, invalidation_ctx->context_ticket);
		mutex_unlock(&umem->ib_peer_mem->lock);
	}

end:
	if (invalidation_ctx)
		kfree(invalidation_ctx);

	ib_put_peer_client(ib_peer_mem, umem->peer_mem_client_context,
				umem->peer_mem_srcu_key);
	kfree(umem);
	return ERR_PTR(ret);
}
static void peer_umem_release(struct ib_umem *umem)
{
	struct ib_peer_memory_client *ib_peer_mem = umem->ib_peer_mem;
	const struct peer_memory_client *peer_mem = ib_peer_mem->peer_mem;
	struct invalidation_ctx *invalidation_ctx = umem->invalidation_ctx;

	if (invalidation_ctx) {

		int peer_callback;
		int inflight_invalidation;
		/* If we are not under peer callback we must take the lock before removing
		  * core ticket from the tree and releasing its umem.
		  * It will let any inflight callbacks to be ended safely.
		  * If we are under peer callback or under error flow of reg_mr so that context
		  * wasn't activated yet lock was already taken.
		*/
		if (invalidation_ctx->func && !invalidation_ctx->peer_callback)
			mutex_lock(&ib_peer_mem->lock);
		ib_peer_remove_context(ib_peer_mem, invalidation_ctx->context_ticket);
		/* make sure to check inflight flag after took the lock and remove from tree.
		  * in addition, from that point using local variables for peer_callback and
		  * inflight_invalidation as after the complete invalidation_ctx can't be accessed
		  * any more as it may be freed by the callback.
		*/
		peer_callback = invalidation_ctx->peer_callback;
		inflight_invalidation = invalidation_ctx->inflight_invalidation;
		if (inflight_invalidation)
			complete(&invalidation_ctx->comp);
		/* On peer callback lock is handled externally */
		if (!peer_callback)
			/* unlocking before put_pages */
			mutex_unlock(&ib_peer_mem->lock);
		/* in case under callback context or callback is pending let it free the invalidation context */
		if (!peer_callback && !inflight_invalidation)
			kfree(invalidation_ctx);
	}

	peer_mem->dma_unmap(&umem->sg_head,
					umem->peer_mem_client_context,
					umem->context->device->dma_device);
	peer_mem->put_pages(&umem->sg_head,
					  umem->peer_mem_client_context);

	ib_peer_mem->stats.num_dereg_pages +=
			umem->nmap * (umem->page_size >> PAGE_SHIFT);
	ib_peer_mem->stats.num_dealloc_mrs += 1;
	ib_put_peer_client(ib_peer_mem, umem->peer_mem_client_context,
				umem->peer_mem_srcu_key);
	kfree(umem);

	return;

}

static void __ib_umem_release(struct ib_device *dev, struct ib_umem *umem, int dirty)
{
	struct scatterlist *sg;
	struct page *page;
	int i;

	if (umem->nmap > 0)
		ib_dma_unmap_sg(dev, umem->sg_head.sgl,
				    umem->nmap,
				    DMA_BIDIRECTIONAL);

	for_each_sg(umem->sg_head.sgl, sg, umem->npages, i) {

		page = sg_page(sg);
		if (umem->writable && dirty)
			set_page_dirty_lock(page);
		put_page(page);
	}

	sg_free_table(&umem->sg_head);
	return;

}

void ib_umem_activate_invalidation_notifier(struct ib_umem *umem,
					       umem_invalidate_func_t func,
					       void *cookie)
{
	struct invalidation_ctx *invalidation_ctx = umem->invalidation_ctx;

	invalidation_ctx->func = func;
	invalidation_ctx->cookie = cookie;

	/* from that point any pending invalidations can be called */
	mutex_unlock(&umem->ib_peer_mem->lock);
	return;
}
EXPORT_SYMBOL(ib_umem_activate_invalidation_notifier);
/**
 * ib_umem_get - Pin and DMA map userspace memory.
 *
 * If access flags indicate ODP memory, avoid pinning. Instead, stores
 * the mm for future page fault handling in conjuction with MMU notifiers.
 *
 * @context: userspace context to pin memory for
 * @addr: userspace virtual address to start at
 * @size: length of region to pin
 * @access: IB_ACCESS_xxx flags for memory being pinned
 * @dmasync: flush in-flight DMA when the memory region is written
 */
struct ib_umem *ib_umem_get_ex(struct ib_ucontext *context, unsigned long addr,
			    size_t size, int access, int dmasync,
			    int invalidation_supported)
{
	struct ib_umem *umem;
	struct page **page_list;
	struct vm_area_struct **vma_list;
	unsigned long locked;
	unsigned long lock_limit;
	unsigned long cur_base;
	unsigned long npages;
	int ret;
	int i;
	DEFINE_DMA_ATTRS(attrs);
	struct scatterlist *sg, *sg_list_start;
	int need_release = 0;

	if (dmasync)
		dma_set_attr(DMA_ATTR_WRITE_BARRIER, &attrs);
	else if (allow_weak_ordering)
		dma_set_attr(DMA_ATTR_WEAK_ORDERING, &attrs);


	if (!can_do_mlock())
		return ERR_PTR(-EPERM);

	umem = kzalloc(sizeof *umem, GFP_KERNEL);
	if (!umem)
		return ERR_PTR(-ENOMEM);

	umem->context   = context;
	umem->length    = size;
	umem->address   = addr;
	umem->page_size = PAGE_SIZE;
	/*
	 * We ask for writable memory if any of the following
	 * access flags are set.  "Local write" and "remote write"
	 * obviously require write access.  "Remote atomic" can do
	 * things like fetch and add, which will modify memory, and
	 * "MW bind" can change permissions by binding a window.
	 */
	umem->writable  = !!(access &
		(IB_ACCESS_LOCAL_WRITE   | IB_ACCESS_REMOTE_WRITE |
		 IB_ACCESS_REMOTE_ATOMIC | IB_ACCESS_MW_BIND));

	umem->odp_data = NULL;

	if (invalidation_supported || context->peer_mem_private_data) {

		struct ib_peer_memory_client *peer_mem_client;

		peer_mem_client =  ib_get_peer_client(context, addr, size,
					&umem->peer_mem_client_context,
					&umem->peer_mem_srcu_key);
		if (peer_mem_client)
			return peer_umem_get(peer_mem_client, umem, addr,
					dmasync, invalidation_supported);
	}

	if (access & IB_ACCESS_ON_DEMAND) {
		ret = ib_umem_odp_get(context, umem);
		if (ret) {
			kfree(umem);
			return ERR_PTR(ret);
		}
		return umem;
	}

	/* We assume the memory is from hugetlb until proved otherwise */
	umem->hugetlb   = 1;

	page_list = (struct page **) __get_free_page(GFP_KERNEL);
	if (!page_list) {
		kfree(umem);
		return ERR_PTR(-ENOMEM);
	}

	/*
	 * if we can't alloc the vma_list, it's not so bad;
	 * just assume the memory is not hugetlb memory
	 */
	vma_list = (struct vm_area_struct **) __get_free_page(GFP_KERNEL);
	if (!vma_list)
		umem->hugetlb = 0;

	npages = ib_umem_num_pages(umem);

	down_write(&current->mm->mmap_sem);

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3,2,0))
	locked     = npages + current->mm->pinned_vm;
#else
	locked     = npages + current->mm->locked_vm;
#endif
	lock_limit = rlimit(RLIMIT_MEMLOCK) >> PAGE_SHIFT;

	if ((locked > lock_limit) && !capable(CAP_IPC_LOCK)) {
		ret = -ENOMEM;
		goto out;
	}

	cur_base = addr & PAGE_MASK;

	if (npages == 0) {
		ret = -EINVAL;
		goto out;
	}

	ret = sg_alloc_table(&umem->sg_head, npages, GFP_KERNEL);
	if (ret)
		goto out;

	need_release = 1;
	sg_list_start = umem->sg_head.sgl;

	while (npages) {
		ret = get_user_pages(current, current->mm, cur_base,
				     min_t(unsigned long, npages,
					   PAGE_SIZE / sizeof (struct page *)),
				     1, !umem->writable, page_list, vma_list);
		if (ret < 0)
			goto out;

		umem->npages += ret;
		cur_base += ret * PAGE_SIZE;
		npages	 -= ret;

		for_each_sg(sg_list_start, sg, ret, i) {

			if (vma_list && !is_vm_hugetlb_page(vma_list[i]))
				umem->hugetlb = 0;

			sg_set_page(sg, page_list[i], PAGE_SIZE, 0);
		}

		/* preparing for next loop */
		sg_list_start = sg;
	}

	umem->nmap = ib_dma_map_sg_attrs(context->device,
				  umem->sg_head.sgl,
				  umem->npages,
				  DMA_BIDIRECTIONAL,
				  &attrs);

	if (umem->nmap <= 0) {
		ret = -ENOMEM;
		goto out;
	}

	ret = 0;

out:
	if (ret < 0) {
		if (need_release)
			__ib_umem_release(context->device, umem, 0);
		kfree(umem);
	} else
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3,2,0))
		current->mm->pinned_vm = locked;
#else
		current->mm->locked_vm = locked;
#endif

	up_write(&current->mm->mmap_sem);
	if (vma_list)
		free_page((unsigned long) vma_list);
	free_page((unsigned long) page_list);

	return ret < 0 ? ERR_PTR(ret) : umem;
}
EXPORT_SYMBOL(ib_umem_get_ex);
struct ib_umem *ib_umem_get(struct ib_ucontext *context, unsigned long addr,
			    size_t size, int access, int dmasync)
{
	return ib_umem_get_ex(context, addr,
			    size, access, dmasync, 0);
}
EXPORT_SYMBOL(ib_umem_get);

static void ib_umem_account(struct work_struct *work)
{
	struct ib_umem *umem = container_of(work, struct ib_umem, work);

	down_write(&umem->mm->mmap_sem);
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3,2,0))
	umem->mm->pinned_vm -= umem->diff;
#else
	umem->mm->locked_vm -= umem->diff;
#endif
	up_write(&umem->mm->mmap_sem);
	mmput(umem->mm);
	kfree(umem);
}

/**
 * ib_umem_release - release memory pinned with ib_umem_get
 * @umem: umem struct to release
 */
void ib_umem_release(struct ib_umem *umem)
{
	struct ib_ucontext *context = umem->context;
	struct mm_struct *mm;
	unsigned long diff;
	if (umem->ib_peer_mem) {
		peer_umem_release(umem);
		return;
	}

	if (umem->odp_data) {
		ib_umem_odp_release(umem);
		return;
	}

	__ib_umem_release(umem->context->device, umem, 1);

	mm = get_task_mm(current);
	if (!mm) {
		kfree(umem);
		return;
	}

	diff = ib_umem_num_pages(umem);

	/*
	 * We may be called with the mm's mmap_sem already held.  This
	 * can happen when a userspace munmap() is the call that drops
	 * the last reference to our file and calls our release
	 * method.  If there are memory regions to destroy, we'll end
	 * up here and not be able to take the mmap_sem.  In that case
	 * we defer the vm_locked accounting to the system workqueue.
	 */
	if (context->closing) {
		if (!down_write_trylock(&mm->mmap_sem)) {
			INIT_WORK(&umem->work, ib_umem_account);
			umem->mm   = mm;
			umem->diff = diff;

			queue_work(ib_wq, &umem->work);
			return;
		}
	} else
		down_write(&mm->mmap_sem);

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3,2,0))
	current->mm->pinned_vm -= diff;
#else
	current->mm->locked_vm -= diff;
#endif
	up_write(&mm->mmap_sem);
	mmput(mm);
	kfree(umem);
}
EXPORT_SYMBOL(ib_umem_release);

int ib_umem_page_count(struct ib_umem *umem)
{
	int shift;
	int i;
	int n;
	struct scatterlist *sg;

	if (umem->odp_data)
		return ib_umem_num_pages(umem);

	shift = ilog2(umem->page_size);

	n = 0;
	for_each_sg(umem->sg_head.sgl, sg, umem->nmap, i)
		n += sg_dma_len(sg) >> shift;

	return n;
}
EXPORT_SYMBOL(ib_umem_page_count);

/*
 * Copy from the given ib_umem's pages to the given buffer.
 *
 * umem - the umem to copy from
 * offset - offset to start copying from
 * dst - destination buffer
 * length - buffer length
 *
 * Returns the number of copied bytes, or an error code.
 */
int ib_umem_copy_from(struct ib_umem *umem, size_t offset, void *dst,
		      size_t length)
{
	size_t end = offset + length;

	if (offset > umem->length || end > umem->length || end < offset) {
		pr_err("ib_umem_copy_from not in range. offset: %zd umem length: %zd end: %zd\n"
		       , offset, umem->length, end);
		return -EINVAL;
	}

	return sg_pcopy_to_buffer(umem->sg_head.sgl, umem->nmap, dst, length,
			offset + ib_umem_offset(umem));
}
EXPORT_SYMBOL(ib_umem_copy_from);
