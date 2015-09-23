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

#include <linux/types.h>
#include <linux/sched.h>
#include <linux/pid.h>
#include <linux/slab.h>
#include <linux/export.h>
#include <linux/vmalloc.h>

#include <rdma/ib_verbs.h>
#include <rdma/ib_umem.h>
#include <rdma/ib_umem_odp.h>

#include "uverbs.h"

void ib_umem_notifier_start_account(struct ib_umem *item)
{
	int notifiers_count;
	mutex_lock(&item->odp_data->umem_mutex);
	/*
	 * Avoid performing another locked operation, as we are
	 * already protected by the wrapping mutex.
	 */
	notifiers_count = atomic_read(&item->odp_data->notifiers_count) + 1;
	if (notifiers_count == 1)
		reinit_completion(&item->odp_data->notifier_completion);
	atomic_set(&item->odp_data->notifiers_count,
		   notifiers_count);

	atomic_inc(&item->context->device->odp_statistics.num_invalidations);

	mutex_unlock(&item->odp_data->umem_mutex);
}
EXPORT_SYMBOL(ib_umem_notifier_start_account);

void ib_umem_notifier_end_account(struct ib_umem *item)
{
	int notifiers_count, notifiers_seq;
	mutex_lock(&item->odp_data->umem_mutex);
	/*
	 * This sequence increase will notify the QP page fault that
	 * the page that is going to be mapped in the spte could have
	 * been freed.
	 */
	notifiers_seq = atomic_read(&item->odp_data->notifiers_seq) + 1;
	atomic_set(&item->odp_data->notifiers_seq,
		   notifiers_seq);
	/*
	 * The above sequence increase must be visible before the
	 * below count decrease, which is ensured by the smp_wmb below
	 * in conjunction with the smp_rmb in mmu_notifier_retry().
	 */
	smp_wmb();

	notifiers_count = atomic_read(&item->odp_data->notifiers_count);
	/*
	 * This is a workaround for the unlikely case where we register a umem
	 * in the middle of an ongoing notifier.
	 */
	if (notifiers_count > 0)
		notifiers_count -= 1;
	else
		pr_warn("Got notifier end call without a previous start call");
	atomic_set(&item->odp_data->notifiers_count,
		   notifiers_count);
	if (notifiers_count == 0)
		complete_all(&item->odp_data->notifier_completion);
	mutex_unlock(&item->odp_data->umem_mutex);
}


static int ib_umem_notifier_release_trampoline(struct ib_umem *item, u64 start,
					       u64 end, void *cookie) {
	/*
	 * Increase the number of notifiers running, to
	 * prevent any further fault handling on this MR.
	 */
	ib_umem_notifier_start_account(item);
	item->odp_data->dying = 1;
	/* Make sure that the fact the umem is dying is out before we release
	 * all pending page faults. */
	smp_wmb();
	complete_all(&item->odp_data->notifier_completion);
	item->context->invalidate_range(item, ib_umem_start(item),
					ib_umem_end(item));
	return 0;
}

static void ib_umem_notifier_release(struct mmu_notifier *mn,
				     struct mm_struct *mm)
{
	struct ib_ucontext *context = container_of(mn, struct ib_ucontext, mn);

	if (!context->invalidate_range)
		return;

	down_read(&context->umem_mutex);

	rbt_ib_umem_for_each_in_range(&context->umem_tree, 0,
				      ULLONG_MAX,
				      ib_umem_notifier_release_trampoline,
				      NULL);

	up_read(&context->umem_mutex);
}

static int invalidate_page_trampoline(struct ib_umem *item, u64 start,
				      u64 end, void *cookie)
{
	ib_umem_notifier_start_account(item);
	item->context->invalidate_range(item, start, start + PAGE_SIZE);
	ib_umem_notifier_end_account(item);
	return 0;
}

static void ib_umem_notifier_invalidate_page(struct mmu_notifier *mn,
					     struct mm_struct *mm,
					     unsigned long address)
{
	struct ib_ucontext *context = container_of(mn, struct ib_ucontext, mn);

	if (!context->invalidate_range)
		return;

	down_read(&context->umem_mutex);
	rbt_ib_umem_for_each_in_range(&context->umem_tree, address,
				      address + PAGE_SIZE,
				      invalidate_page_trampoline, NULL);
	up_read(&context->umem_mutex);
}

static int invalidate_range_start_trampoline(struct ib_umem *item, u64 start,
					     u64 end, void *cookie)
{
	ib_umem_notifier_start_account(item);
	item->context->invalidate_range(item, start, end);
	return 0;
}

static void ib_umem_notifier_invalidate_range_start(struct mmu_notifier *mn,
						    struct mm_struct *mm,
						    unsigned long start,
						    unsigned long end)
{
	struct ib_ucontext *context = container_of(mn, struct ib_ucontext, mn);

	if (!context->invalidate_range)
		return;

	down_read(&context->umem_mutex);
	rbt_ib_umem_for_each_in_range(&context->umem_tree, start,
				      end,
				      invalidate_range_start_trampoline, NULL);
	up_read(&context->umem_mutex);
}

static int invalidate_range_end_trampoline(struct ib_umem *item, u64 start,
					   u64 end, void *cookie)
{
	ib_umem_notifier_end_account(item);
	return 0;
}

static void ib_umem_notifier_invalidate_range_end(struct mmu_notifier *mn,
						  struct mm_struct *mm,
						  unsigned long start,
						  unsigned long end)
{
	struct ib_ucontext *context = container_of(mn, struct ib_ucontext, mn);

	if (!context->invalidate_range)
		return;

	down_read(&context->umem_mutex);
	rbt_ib_umem_for_each_in_range(&context->umem_tree, start,
				      end,
				      invalidate_range_end_trampoline, NULL);
	up_read(&context->umem_mutex);
}

static struct mmu_notifier_ops ib_umem_notifiers = {
	.release                    = ib_umem_notifier_release,
	.invalidate_page            = ib_umem_notifier_invalidate_page,
	.invalidate_range_start     = ib_umem_notifier_invalidate_range_start,
	.invalidate_range_end       = ib_umem_notifier_invalidate_range_end,
};

int ib_umem_odp_get(struct ib_ucontext *context, struct ib_umem *umem)
{
	int ret_val;
	struct pid *our_pid;
	struct mm_struct *mm = get_task_mm(current);
	BUG_ON(!mm);

	/* Prevent creating ODP MRs in child processes */
	rcu_read_lock();
	our_pid = get_task_pid(current->group_leader, PIDTYPE_PID);
	rcu_read_unlock();
	put_pid(our_pid);
	if (context->tgid != our_pid) {
		ret_val = -EINVAL;
		goto out_mm;
	}

	umem->hugetlb = 0;
	umem->odp_data = kzalloc(sizeof(*umem->odp_data), GFP_KERNEL);
	if (!umem->odp_data) {
		ret_val = -ENOMEM;
		goto out_mm;
	}
	umem->odp_data->umem = umem;

	mutex_init(&umem->odp_data->umem_mutex);

	init_completion(&umem->odp_data->notifier_completion);

	umem->odp_data->page_list = vzalloc(ib_umem_num_pages(umem) *
					    sizeof(*umem->odp_data->page_list));
	if (!umem->odp_data->page_list) {
		ret_val = -ENOMEM;
		goto out_odp_data;
	}

	umem->odp_data->dma_list = vzalloc(ib_umem_num_pages(umem) *
					  sizeof(*umem->odp_data->dma_list));
	if (!umem->odp_data->dma_list) {
		ret_val = -ENOMEM;
		goto out_page_list;
	}

	/*
	 * When using MMU notifiers, we will get a
	 * notification before the "current" task (and MM) is
	 * destroyed. We use the umem_mutex lock to synchronize.
	 */
	down_write(&context->umem_mutex);
	context->odp_mrs_count++;
	if (likely(ib_umem_start(umem) != ib_umem_end(umem)))
		rbt_ib_umem_insert(&umem->odp_data->interval_tree,
				   &context->umem_tree);
	downgrade_write(&context->umem_mutex);

	if (context->odp_mrs_count == 1) {
		/*
		 * Note that at this point, no MMU notifier is running
		 * for this context!
		 */
		INIT_HLIST_NODE(&context->mn.hlist);
		context->mn.ops = &ib_umem_notifiers;
		/*
		 * Lock-dep detects a false positive for mmap_sem vs.
		 * umem_mutex, due to not grasping downgrade_write correctly.
		 */
		lockdep_off();
		ret_val = mmu_notifier_register(&context->mn, mm);
		lockdep_on();
		if (ret_val) {
			pr_err("Failed to register mmu_notifier %d\n", ret_val);
			ret_val = -EBUSY;
			goto out_mutex;
		}
	}

	up_read(&context->umem_mutex);

	/*
	 * Note that doing an mmput can cause a notifier for the relevant mm.
	 * If the notifier is called while we hold the umem_mutex, this will
	 * cause a deadlock. Therefore, we release the reference only after we
	 * released the mutex.
	 */
	mmput(mm);
	return 0;

out_mutex:
	up_read(&context->umem_mutex);
	vfree(umem->odp_data->dma_list);
out_page_list:
	vfree(umem->odp_data->page_list);
out_odp_data:
	kfree(umem->odp_data);
out_mm:
	mmput(mm);
	return ret_val;
}

void ib_umem_odp_release(struct ib_umem *umem)
{
	struct ib_ucontext *context = umem->context;

	/*
	 * Ensure that no more pages are mapped in the umem.
	 *
	 * It is the driver's responsibility to ensure, before calling us,
	 * that the hardware will not attempt to access the MR any more.
	 */
	ib_umem_odp_unmap_dma_pages(umem, ib_umem_start(umem),
				    ib_umem_end(umem));

	down_write(&context->umem_mutex);
	if (likely(ib_umem_start(umem) != ib_umem_end(umem)))
		rbt_ib_umem_remove(&umem->odp_data->interval_tree,
				   &context->umem_tree);
	context->odp_mrs_count--;

	/*
	 * Downgrade the lock to a read lock. This ensures that the notifiers
	 * (who lock the mutex for reading) will be able to finish, and we
	 * will be able to enventually obtain the mmu notifiers SRCU. Note
	 * that since we are doing it atomically, no other user could register
	 * and unregister while we do the check.
	 */
	downgrade_write(&context->umem_mutex);
	if (!context->odp_mrs_count) {
		struct task_struct *owning_process = NULL;
		struct mm_struct *owning_mm        = NULL;
		owning_process = get_pid_task(context->tgid,
					      PIDTYPE_PID);
		if (owning_process == NULL)
			/*
			 * The process is already dead, notifier were removed
			 * already.
			 */
			goto out;

		owning_mm = get_task_mm(owning_process);
		if (owning_mm == NULL)
			/*
			 * The process' mm is already dead, notifier were
			 * removed already.
			 */
			goto out_put_task;
		mmu_notifier_unregister(&context->mn, owning_mm);

		mmput(owning_mm);

out_put_task:
		put_task_struct(owning_process);
	}
out:
	up_read(&context->umem_mutex);

	vfree(umem->odp_data->dma_list);
	vfree(umem->odp_data->page_list);
	kfree(umem->odp_data);
	kfree(umem);
}

/*
 * Map for DMA and insert a single page into the on-demand paging page tables.
 *
 * @umem: the umem to insert the page to.
 * @page_index: index in the umem to add the page to.
 * @page: the page struct to map and add.
 * @access_mask: access permissions needed for this page.
 * @current_seq: sequence number for synchronization with invalidations.
 *               the sequence number is taken from
 *               umem->odp_data->notifiers_seq.
 *
 * The function returns -EFAULT if the DMA mapping operation fails. It returns
 * -EAGAIN if a concurrent invalidation prevents us from updating the page.
 *
 * The page is released via put_page even if the operation failed. For
 * on-demand pinning, the page is released whenever it isn't stored in the
 * umem.
 */
static int ib_umem_odp_map_dma_single_page(
		struct ib_umem *umem,
		int page_index,
		struct page *page,
		u64 access_mask,
		unsigned long current_seq,
		enum ib_odp_dma_map_flags flags)
{
	struct ib_device *dev = umem->context->device;
	dma_addr_t dma_addr;
	int stored_page = 0;
	int ret = 0;
	mutex_lock(&umem->odp_data->umem_mutex);
	/*
	 * Note: we avoid writing if seq is different from the initial seq, to
	 * handle case of a racing notifier. This check also allows us to bail
	 * early if we have a notifier running in parallel with us.
	 */
	if (ib_umem_mmu_notifier_retry(umem, current_seq)) {
		ret = -EAGAIN;
		goto out;
	}
	if (!(umem->odp_data->dma_list[page_index])) {
		dma_addr = ib_dma_map_page(dev,
					   page,
					   0, PAGE_SIZE,
					   DMA_BIDIRECTIONAL);
		if (ib_dma_mapping_error(dev, dma_addr)) {
			ret = -EFAULT;
			goto out;
		}
		umem->odp_data->dma_list[page_index] = dma_addr | access_mask;
		umem->odp_data->page_list[page_index] = page;
		if (flags & IB_ODP_DMA_MAP_FOR_PREFETCH)
			atomic_inc(&dev->odp_statistics.num_prefetch_pages);
		else
			atomic_inc(&dev->odp_statistics.num_page_fault_pages);

		stored_page = 1;
	} else if (umem->odp_data->page_list[page_index] == page) {
		umem->odp_data->dma_list[page_index] |= access_mask;
	} else {
		pr_err("error: got different pages in IB device and from get_user_pages. IB device page: %p, gup page: %p\n"
		       , umem->odp_data->page_list[page_index], page);
	}

out:
	mutex_unlock(&umem->odp_data->umem_mutex);

	/* On Demand Paging - avoid pinning the page */
	if (umem->context->invalidate_range || !stored_page)
		put_page(page);

	return ret;
}

/**
 * ib_umem_odp_map_dma_pages - Pin and DMA map userspace memory in an ODP MR.
 *
 * Pins the range of pages passed in the argument, and maps them to
 * DMA addresses. The DMA addresses of the mapped pages is updated in
 * umem->odp_data->dma_list.
 *
 * Returns the number of pages mapped in success, negative error code
 * for failure.
 * An -EAGAIN error code is returned when a concurrent mmu notifier prevents
 * the function from completing its task.
 *
 * @umem: the umem to map and pin
 * @user_virt: the address from which we need to map.
 * @bcnt: the minimal number of bytes to pin and map. The mapping might be
 *        bigger due to alignment, and may also be smaller in case of an error
 *        pinning or mapping a page. The actual pages mapped is returned in
 *        the return value.
 * @access_mask: bit mask of the requested access permissions for the given
 *               range.
 * @current_seq: the MMU notifiers sequance value for synchronization with
 *               invalidations. the sequance number is read from
 *               umem->odp_data->notifiers_seq before calling this function
 * @flags: IB_ODP_DMA_MAP_FOR_PREEFTCH is used to indicate that the function
 *	   was called from the prefetch verb. IB_ODP_DMA_MAP_FOR_PAGEFAULT is
 *	   used to indicate that the function was called from a pagefault
 *	   handler.
 */
int ib_umem_odp_map_dma_pages(struct ib_umem *umem, u64 user_virt, u64 bcnt,
			      u64 access_mask, unsigned long current_seq,
			      enum ib_odp_dma_map_flags flags)
{
	struct task_struct *owning_process  = NULL;
	struct mm_struct   *owning_mm       = NULL;
	struct page       **local_page_list = NULL;
	u64 off;
	int j, k, ret = 0, start_idx, npages = 0;

	if (access_mask == 0)
		return -EINVAL;

	if (user_virt < ib_umem_start(umem) ||
	    user_virt + bcnt > ib_umem_end(umem))
		return -EFAULT;

	local_page_list = (struct page **)__get_free_page(GFP_KERNEL);
	if (!local_page_list)
		return -ENOMEM;

	off = user_virt & (~PAGE_MASK);
	user_virt = user_virt & PAGE_MASK;
	bcnt += off; /* Charge for the first page offset as well. */

	owning_process = get_pid_task(umem->context->tgid, PIDTYPE_PID);
	if (owning_process == NULL) {
		ret = -EINVAL;
		goto out_no_task;
	}

	owning_mm = get_task_mm(owning_process);
	if (owning_mm == NULL) {
		ret = -EINVAL;
		goto out_put_task;
	}

	start_idx = (user_virt - ib_umem_start(umem)) >> PAGE_SHIFT;
	k = start_idx;

	while (bcnt > 0) {
		down_read(&owning_mm->mmap_sem);
		/*
		 * Note: this might result in redundent page getting. We can
		 * avoid this by checking dma_list to be 0 before calling
		 * get_user_pages. However, this make the code much more
		 * complex (and doesn't gain us much performance in most use
		 * cases).
		 */
		npages = get_user_pages(owning_process, owning_mm,
				     user_virt, min_t(size_t,
						      (bcnt - 1 + PAGE_SIZE) /
						      PAGE_SIZE,
						      PAGE_SIZE /
						      sizeof(struct page *)),
				     access_mask & ODP_WRITE_ALLOWED_BIT, 0,
				     local_page_list, NULL);
		up_read(&owning_mm->mmap_sem);

		if (npages < 0)
			break;

		bcnt -= min_t(size_t, npages << PAGE_SHIFT, bcnt);
		user_virt += npages << PAGE_SHIFT;
		for (j = 0; j < npages; ++j) {
			ret = ib_umem_odp_map_dma_single_page(umem, k,
				local_page_list[j], access_mask,
				current_seq, flags);
			if (ret < 0)
				break;
			k++;
		}

		if (ret < 0) {
			/* Release left over pages when handling errors. */
			for (++j; j < npages; ++j)
				put_page(local_page_list[j]);
			break;
		}
	}

	if (ret >= 0) {
		if (npages < 0 && k == start_idx)
			ret = npages;
		else
			ret = k - start_idx;
	}

	mmput(owning_mm);
out_put_task:
	put_task_struct(owning_process);
out_no_task:
	free_page((unsigned long) local_page_list);
	return ret;
}
EXPORT_SYMBOL(ib_umem_odp_map_dma_pages);

void ib_umem_odp_unmap_dma_pages(struct ib_umem *umem, u64 virt,
				 u64 bound)
{
	int idx;
	u64 addr;
	struct ib_device *dev = umem->context->device;
	virt  = max_t(u64, virt,  ib_umem_start(umem));
	bound = min_t(u64, bound, ib_umem_end(umem));
	/* Note that during the run of this function, the
	 * notifiers_count of the MR is > 0, preventing any racing
	 * faults from completion. We might be racing with other
	 * invalidations, so we must make sure we free each page only
	 * once. */
	for (addr = virt; addr < bound; addr += PAGE_SIZE) {
		idx = (addr - ib_umem_start(umem)) / PAGE_SIZE;
		mutex_lock(&umem->odp_data->umem_mutex);
		if (umem->odp_data->page_list[idx]) {
			struct page *page = umem->odp_data->page_list[idx];
#ifdef CONFIG_COMPAT_USE_COMPOUND_TRANS_HEAD
			struct page *head_page = compound_trans_head(page);
#else
			struct page *head_page = compound_head(page);
#endif
			dma_addr_t dma_addr = umem->odp_data->dma_list[idx] &
					      ODP_DMA_ADDR_MASK;

			WARN_ON(!dma_addr);

			ib_dma_unmap_page(dev, dma_addr, PAGE_SIZE,
					  DMA_BIDIRECTIONAL);

			if (umem->odp_data->dma_list[idx] &
			    ODP_WRITE_ALLOWED_BIT)
				/*
				 * set_page_dirty prefers being called with
				 * the page lock. However, MMU notifiers are
				 * called sometimes with and sometimes without
				 * the lock. We rely on the umem_mutex instead
				 * to prevent other mmu notifiers from
				 * continuing and allowing the page mapping to
				 * be removed.
				 */
				set_page_dirty(head_page);
			/* on demand pinning support */
			if (!umem->context->invalidate_range)
				put_page(page);
			umem->odp_data->page_list[idx] = NULL;
			umem->odp_data->dma_list[idx] = 0;
			atomic_inc(&dev->odp_statistics.num_invalidation_pages);
		}
		mutex_unlock(&umem->odp_data->umem_mutex);
	}
}
EXPORT_SYMBOL(ib_umem_odp_unmap_dma_pages);

static ssize_t show_num_page_fault_pages(struct device *device,
					 struct device_attribute *attr,
					 char *buf)
{
	struct ib_uverbs_device *dev = dev_get_drvdata(device);

	if (!dev)
		return -ENODEV;

	return sprintf(buf, "%d\n",
		       atomic_read(&dev->ib_dev->odp_statistics.num_page_fault_pages));
}
static DEVICE_ATTR(num_page_fault_pages, S_IRUGO,
		   show_num_page_fault_pages, NULL);

static ssize_t show_num_invalidation_pages(struct device *device,
					   struct device_attribute *attr,
					   char *buf)
{
	struct ib_uverbs_device *dev = dev_get_drvdata(device);

	if (!dev)
		return -ENODEV;

	return sprintf(buf, "%d\n",
		       atomic_read(&dev->ib_dev->odp_statistics.num_invalidation_pages));
}
static DEVICE_ATTR(num_invalidation_pages, S_IRUGO,
		   show_num_invalidation_pages, NULL);

static ssize_t show_num_invalidations(struct device *device,
				      struct device_attribute *attr,
				      char *buf)
{
	struct ib_uverbs_device *dev = dev_get_drvdata(device);

	if (!dev)
		return -ENODEV;

	return sprintf(buf, "%d\n",
		       atomic_read(&dev->ib_dev->odp_statistics.num_invalidations));
}
static DEVICE_ATTR(num_invalidations, S_IRUGO, show_num_invalidations, NULL);


static ssize_t show_invalidations_faults_contentions(struct device *device,
						     struct device_attribute *attr,
						     char *buf)
{
	struct ib_uverbs_device *dev = dev_get_drvdata(device);

	if (!dev)
		return -ENODEV;

	return sprintf(buf, "%d\n",
		       atomic_read(&dev->ib_dev->odp_statistics.invalidations_faults_contentions));
}
static DEVICE_ATTR(invalidations_faults_contentions, S_IRUGO,
		   show_invalidations_faults_contentions, NULL);

static ssize_t show_num_page_faults(struct device *device,
				    struct device_attribute *attr,
				    char *buf)
{
	struct ib_uverbs_device *dev = dev_get_drvdata(device);

	if (!dev)
		return -ENODEV;

	return sprintf(buf, "%d\n",
		       atomic_read(&dev->ib_dev->odp_statistics.num_page_faults));
}
static DEVICE_ATTR(num_page_faults, S_IRUGO, show_num_page_faults, NULL);

static ssize_t show_num_prefetches_handled(struct device *device,
					   struct device_attribute *attr,
					   char *buf)
{
	struct ib_uverbs_device *dev = dev_get_drvdata(device);

	if (!dev)
		return -ENODEV;

	return sprintf(buf, "%d\n",
		       atomic_read(&dev->ib_dev->odp_statistics.num_prefetches_handled));
}
static DEVICE_ATTR(num_prefetches_handled, S_IRUGO,
		   show_num_prefetches_handled, NULL);


static ssize_t show_num_prefetch_pages(struct device *device,
				       struct device_attribute *attr,
				       char *buf)
{
	struct ib_uverbs_device *dev = dev_get_drvdata(device);

	if (!dev)
		return -ENODEV;

	return sprintf(buf, "%d\n",
		       atomic_read(&dev->ib_dev->odp_statistics.num_prefetch_pages));
}
static DEVICE_ATTR(num_prefetch_pages, S_IRUGO,
		   show_num_prefetch_pages, NULL);

int ib_umem_odp_add_statistic_nodes(struct device *dev)
{
	int ret;

	ret = device_create_file(dev, &dev_attr_num_page_fault_pages);
	if (ret)
		return ret;
	ret = device_create_file(dev, &dev_attr_num_invalidation_pages);
	if (ret)
		return ret;
	ret = device_create_file(dev, &dev_attr_num_invalidations);
	if (ret)
		return ret;
	ret = device_create_file(dev,
				 &dev_attr_invalidations_faults_contentions);
	if (ret)
		return ret;
	ret = device_create_file(dev, &dev_attr_num_page_faults);
	if (ret)
		return ret;
	ret = device_create_file(dev, &dev_attr_num_prefetches_handled);
	if (ret)
		return ret;
	ret = device_create_file(dev, &dev_attr_num_prefetch_pages);
	if (ret)
		return ret;

	return 0;
}
EXPORT_SYMBOL(ib_umem_odp_add_statistic_nodes);
