/*
 * Copyright (c) 2014 Mellanox Technologies. All rights reserved.
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

#ifndef IB_UMEM_ODP_H
#define IB_UMEM_ODP_H

#include <rdma/ib_umem.h>
#ifdef CONFIG_INFINIBAND_ON_DEMAND_PAGING
#include <linux/interval_tree.h>
#endif
#include <rdma/ib_verbs.h>

#ifdef CONFIG_INFINIBAND_ON_DEMAND_PAGING
struct umem_odp_node {
	u64 __subtree_last;
	struct rb_node rb;
};
#endif

struct ib_umem_odp {
	/*
	 * An array of the pages included in the on-demand paging umem.
	 * Indices of pages that are currently not mapped into the device will
	 * contain NULL.
	 */
	struct page		**page_list;
	/*
	 * An array of the same size as page_list, with DMA addresses mapped
	 * for pages the pages in page_list. The lower two bits designate
	 * access permissions. See ODP_READ_ALLOWED_BIT and
	 * ODP_WRITE_ALLOWED_BIT.
	 */
	dma_addr_t		*dma_list;
	/*
	 * The umem_mutex protects the page_list and dma_list fields of an ODP
	 * umem, allowing only a single thread to map/unmap pages.
	 */
	struct mutex		umem_mutex;
	void			*private; /* for the HW driver to use. */

	atomic_t		notifiers_seq;
	atomic_t		notifiers_count;
#ifdef CONFIG_INFINIBAND_ON_DEMAND_PAGING
	struct ib_umem		*umem;

	/* Tree tracking */
	struct umem_odp_node	interval_tree;

	struct completion	notifier_completion;
	int			dying;
#endif
};

#ifdef CONFIG_INFINIBAND_ON_DEMAND_PAGING

enum ib_odp_dma_map_flags {
	IB_ODP_DMA_MAP_FOR_PREFETCH	= 1 << 0,
};

int ib_umem_odp_get(struct ib_ucontext *context, struct ib_umem *umem);

void ib_umem_odp_release(struct ib_umem *umem);

int ib_umem_odp_add_statistic_nodes(struct device *dev);

/*
 * The lower 2 bits of the DMA address signal the R/W permissions for
 * the entry. To upgrade the permissions, provide the appropriate
 * bitmask to the map_dma_pages function.
 *
 * Be aware that upgrading a mapped address might result in change of
 * the DMA address for the page.
 */
#define ODP_READ_ALLOWED_BIT  (1<<0ULL)
#define ODP_WRITE_ALLOWED_BIT (1<<1ULL)

#define ODP_DMA_ADDR_MASK (~(ODP_READ_ALLOWED_BIT | ODP_WRITE_ALLOWED_BIT))

int ib_umem_odp_map_dma_pages(struct ib_umem *umem, u64 start_offset, u64 bcnt,
			      u64 access_mask, unsigned long current_seq,
			      enum ib_odp_dma_map_flags flags);

void ib_umem_odp_unmap_dma_pages(struct ib_umem *umem, u64 start_offset,
				 u64 bound);

void rbt_ib_umem_insert(struct umem_odp_node *node, struct rb_root *root);
void rbt_ib_umem_remove(struct umem_odp_node *node, struct rb_root *root);
typedef int (*umem_call_back)(struct ib_umem *item, u64 start, u64 end,
			      void *cookie);
/*
 * Call the callback on each ib_umem in the range. Returns the logical or of
 * the return values of the functions called.
 */
int rbt_ib_umem_for_each_in_range(struct rb_root *root, u64 start, u64 end,
					      umem_call_back cb, void *cookie);

struct umem_odp_node *rbt_ib_umem_iter_first(struct rb_root *root,
					     u64 start, u64 last);
struct umem_odp_node *rbt_ib_umem_iter_next(struct umem_odp_node *node,
					    u64 start, u64 last);

static inline void ib_umem_odp_account_fault_handled(struct ib_device *dev)
{
	atomic_inc(&dev->odp_statistics.num_page_faults);
}

static inline void ib_umem_odp_account_prefetch_handled(struct ib_device *dev)
{
	atomic_inc(&dev->odp_statistics.num_prefetches_handled);
}

static inline void ib_umem_odp_account_invalidations_fault_contentions(struct ib_device *dev)
{
	atomic_inc(&dev->odp_statistics.invalidations_faults_contentions);
}

static inline int ib_umem_mmu_notifier_retry(struct ib_umem *item,
					     unsigned long mmu_seq)
{
	/*
	 * This code is strongly based on the KVM code from
	 * mmu_notifier_retry. Should be called with
	 * item->odp_data->umem_mutex locked.
	 */
	if (unlikely(atomic_read(&item->odp_data->notifiers_count))) {
		ib_umem_odp_account_invalidations_fault_contentions(item->context->device);
		return 1;
	}
	/*
	 * Ensure the read of mmu_notifier_count happens before the read
	 * of mmu_notifier_seq.  This interacts with the smp_wmb() in
	 * mmu_notifier_invalidate_range_end to make sure that the caller
	 * either sees the old (non-zero) value of mmu_notifier_count or
	 * the new (incremented) value of mmu_notifier_seq.
	 */
	smp_rmb();
	if (atomic_read(&item->odp_data->notifiers_seq) != mmu_seq) {
		ib_umem_odp_account_invalidations_fault_contentions(item->context->device);
		return 1;
	}
	return 0;
}

void ib_umem_notifier_start_account(struct ib_umem *item);
void ib_umem_notifier_end_account(struct ib_umem *item);

#else /* CONFIG_INFINIBAND_ON_DEMAND_PAGING */

static inline int ib_umem_odp_get(struct ib_ucontext *context,
				  struct ib_umem *umem)
{
	return -ENOSYS;
}

static inline void ib_umem_odp_release(struct ib_umem *umem) {}

static inline int ib_umem_odp_add_statistic_nodes(struct device *dev)
{
	return 0;
}

#endif /* CONFIG_INFINIBAND_ON_DEMAND_PAGING */

#endif /* IB_UMEM_ODP_H */
