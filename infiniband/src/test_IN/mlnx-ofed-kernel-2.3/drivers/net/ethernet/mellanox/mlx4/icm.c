/*
 * Copyright (c) 2005, 2006, 2007, 2008 Mellanox Technologies. All rights reserved.
 * Copyright (c) 2006, 2007 Cisco Systems, Inc.  All rights reserved.
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

#include <linux/errno.h>
#include <linux/mm.h>
#include <linux/scatterlist.h>
#include <linux/slab.h>
#include <linux/printk.h>

#include <linux/mlx4/cmd.h>

#include "mlx4.h"
#include "icm.h"
#include "fw.h"

/*
 * We allocate in as big chunks as we can, up to a maximum of 256 KB
 * per chunk.
 */
enum {
	MLX4_ICM_ALLOC_SIZE	= 1 << 18,
	MLX4_TABLE_CHUNK_SIZE	= 1 << 18
};


void __mlx4_free_icm(struct mlx4_dev *dev, struct mlx4_icm *icm,
							int coherent, unsigned int npages_allocated,
							int mapped_sg) {
	int i;
	struct scatterlist *sg;

	if (!icm)
		return;

	if (!coherent && mapped_sg)
		pci_unmap_sg(dev->pdev, icm->mem.sgl, icm->mem.nents,
				PCI_DMA_BIDIRECTIONAL);

	for_each_sg(icm->mem.sgl, sg, npages_allocated, i) {
		if (coherent)
			dma_free_coherent(&dev->pdev->dev, PAGE_SIZE,
					sg_virt(sg), sg_dma_address(sg));
		else
			__free_pages(sg_page(sg), 0);
	}

	sg_free_table(&icm->mem);
	kfree(icm);
}

static void sg_set_dma_address(struct scatterlist *sg, dma_addr_t dma_addr)
{
	sg_dma_address(sg) = dma_addr;
}

static int mlx4_alloc_icm_pages(struct scatterlist *sg, gfp_t gfp_mask, int node)
{
	struct page *page;

	page = alloc_pages_node(node, gfp_mask, 0);
	if (!page) {
		page = alloc_pages(gfp_mask, 0);
		if (!page)
			return -ENOMEM;
	}

	sg_set_page(sg, page, PAGE_SIZE, 0);
	return 0;
}

static int mlx4_alloc_icm_coherent(struct device *dev, struct scatterlist *mem,
					 gfp_t gfp_mask)
{
	dma_addr_t dma_addr;
	void *buf = dma_alloc_coherent(dev, PAGE_SIZE,
				       &dma_addr, gfp_mask);
	if (!buf)
		return -ENOMEM;

	sg_set_dma_address(mem, dma_addr);
	BUG_ON(offset_in_page(buf));
	sg_set_buf(mem, buf, PAGE_SIZE);
	sg_dma_len(mem) = PAGE_SIZE;

	return 0;
}

struct mlx4_icm *mlx4_alloc_icm(struct mlx4_dev *dev, int npages,
				gfp_t gfp_mask, int coherent)
{
	struct mlx4_icm *icm;
	int ret;
	int	i;
	struct scatterlist *sg;
	unsigned int npages_allocated = 0;

	/* We use sg_set_buf for coherent allocs, which assumes low memory */
	BUG_ON(coherent && (gfp_mask & __GFP_HIGHMEM));

	icm = kmalloc_node(sizeof(*icm), GFP_KERNEL,
			   dev->numa_node);
	if (!icm) {
		icm = kmalloc(sizeof(*icm), GFP_KERNEL);
		if (!icm)
			return NULL;
	}

	icm->refcount = 0;

	if (sg_alloc_table(&icm->mem, npages, GFP_KERNEL))
		goto fail;

	for_each_sg(icm->mem.sgl, sg, npages, i) {
		if (coherent)
			ret = mlx4_alloc_icm_coherent(&dev->pdev->dev, sg, gfp_mask);
		else
			ret = mlx4_alloc_icm_pages(sg, gfp_mask, dev->numa_node);
		if (ret)
			goto fail;

		++npages_allocated;
	}

	if (!coherent) {
		ret = pci_map_sg(dev->pdev, icm->mem.sgl,
				npages,
				PCI_DMA_BIDIRECTIONAL);
		if (ret <= 0)
			goto fail;

		icm->mem.nents = ret;
	}

	return icm;

fail:
	__mlx4_free_icm(dev, icm, coherent, npages_allocated, 0);
	return NULL;
}

static int mlx4_MAP_ICM(struct mlx4_dev *dev, struct mlx4_icm *icm, u64 virt)
{
	return mlx4_map_cmd(dev, MLX4_CMD_MAP_ICM, icm, virt);
}

static int mlx4_UNMAP_ICM(struct mlx4_dev *dev, u64 virt, u32 page_count)
{
	return mlx4_cmd(dev, virt, page_count, 0, MLX4_CMD_UNMAP_ICM,
			MLX4_CMD_TIME_CLASS_B, MLX4_CMD_NATIVE);
}

int mlx4_MAP_ICM_AUX(struct mlx4_dev *dev, struct mlx4_icm *icm)
{
	return mlx4_map_cmd(dev, MLX4_CMD_MAP_ICM_AUX, icm, -1);
}

int mlx4_UNMAP_ICM_AUX(struct mlx4_dev *dev)
{
	return mlx4_cmd(dev, 0, 0, 0, MLX4_CMD_UNMAP_ICM_AUX,
			MLX4_CMD_TIME_CLASS_B, MLX4_CMD_NATIVE);
}

int mlx4_table_get(struct mlx4_dev *dev, struct mlx4_icm_table *table, u32 obj)
{
	u32 i = (obj & (table->num_obj - 1)) /
			(MLX4_TABLE_CHUNK_SIZE / table->obj_size);
	int ret = 0;

	mutex_lock(&table->mutex);

	if (table->icm[i]) {
		++table->icm[i]->refcount;
		goto out;
	}

	table->icm[i] = mlx4_alloc_icm(dev, MLX4_TABLE_CHUNK_SIZE >> PAGE_SHIFT,
				       (table->lowmem ? GFP_KERNEL : GFP_HIGHUSER) |
				       __GFP_NOWARN, table->coherent);
	if (!table->icm[i]) {
		ret = -ENOMEM;
		goto out;
	}

	if (mlx4_MAP_ICM(dev, table->icm[i], table->virt +
			 (u64) i * MLX4_TABLE_CHUNK_SIZE)) {
		mlx4_free_icm(dev, table->icm[i], table->coherent);
		table->icm[i] = NULL;
		ret = -ENOMEM;
		goto out;
	}

	++table->icm[i]->refcount;

out:
	mutex_unlock(&table->mutex);
	return ret;
}

void mlx4_table_put(struct mlx4_dev *dev, struct mlx4_icm_table *table, u32 obj)
{
	u32 i;
	u64 offset;

	i = (obj & (table->num_obj - 1)) / (MLX4_TABLE_CHUNK_SIZE / table->obj_size);

	mutex_lock(&table->mutex);

	if (--table->icm[i]->refcount == 0) {
		offset = (u64) i * MLX4_TABLE_CHUNK_SIZE;

		if (!mlx4_UNMAP_ICM(dev, table->virt + offset,
				    MLX4_TABLE_CHUNK_SIZE / MLX4_ICM_PAGE_SIZE)) {
			mlx4_free_icm(dev, table->icm[i], table->coherent);
			table->icm[i] = NULL;
		} else {
			pr_warn("mlx4_core: mlx4_UNMAP_ICM failed.\n");
		}
	}

	mutex_unlock(&table->mutex);
}


void *mlx4_table_find(struct mlx4_icm_table *table, u32 obj,
			dma_addr_t *dma_handle)
{
	int offset, dma_offset, i;
	u64 idx;
	struct mlx4_icm *icm;
	void *address = NULL;
	struct scatterlist *sg;

	if (!table->lowmem)
		return NULL;

	mutex_lock(&table->mutex);
	idx = (u64) (obj & (table->num_obj - 1)) * table->obj_size;
	icm = table->icm[idx / MLX4_TABLE_CHUNK_SIZE];
	dma_offset = offset = idx % MLX4_TABLE_CHUNK_SIZE;

	if (!icm)
		goto out;

	for_each_sg(icm->mem.sgl, sg, icm->mem.orig_nents, i){
		if (dma_handle && dma_offset >= 0) {
			if (sg_dma_len(sg) > dma_offset)
				*dma_handle = sg_dma_address(sg) +
						dma_offset;
			dma_offset -= sg_dma_len(sg);
		}
		/*
		 * DMA mapping can merge pages but not split them,
		 * so if we found the page, dma_handle has already
		 * been assigned to.
		 */
		if (PAGE_SIZE > offset) {
			address = sg_virt(sg);
			mutex_unlock(&table->mutex);
			return address + offset;
		}
		offset -= PAGE_SIZE;
	}

out:
	mutex_unlock(&table->mutex);
	return NULL;
}

int mlx4_table_get_range(struct mlx4_dev *dev, struct mlx4_icm_table *table,
			 u32 start, u32 end)
{
	int inc = MLX4_TABLE_CHUNK_SIZE / table->obj_size;
	int err;
	u32 i;

	for (i = start; i <= end; i += inc) {
		err = mlx4_table_get(dev, table, i);
		if (err)
			goto fail;
	}

	return 0;

fail:
	while (i > start) {
		i -= inc;
		mlx4_table_put(dev, table, i);
	}

	return err;
}

void mlx4_table_put_range(struct mlx4_dev *dev, struct mlx4_icm_table *table,
			  u32 start, u32 end)
{
	u32 i;

	for (i = start; i <= end; i += MLX4_TABLE_CHUNK_SIZE / table->obj_size)
		mlx4_table_put(dev, table, i);
}

int mlx4_init_icm_table(struct mlx4_dev *dev, struct mlx4_icm_table *table,
			u64 virt, int obj_size,	u64 nobj, int reserved,
			int use_lowmem, int use_coherent)
{
	int obj_per_chunk;
	int num_icm;
	unsigned chunk_size;
	int i;
	u64 size;

	obj_per_chunk = MLX4_TABLE_CHUNK_SIZE / obj_size;
	num_icm = div_u64((nobj + obj_per_chunk - 1), obj_per_chunk);

	table->icm	  = kcalloc(num_icm, sizeof(*table->icm), GFP_KERNEL);
	if (!table->icm)
		return -ENOMEM;
	table->virt     = virt;
	table->num_icm  = num_icm;
	table->num_obj  = nobj;
	table->obj_size = obj_size;
	table->lowmem   = use_lowmem;
	table->coherent = use_coherent;
	mutex_init(&table->mutex);

	size = (u64) nobj * obj_size;
	for (i = 0; i * MLX4_TABLE_CHUNK_SIZE < reserved * obj_size; ++i) {
		chunk_size = MLX4_TABLE_CHUNK_SIZE;
		if ((i + 1) * MLX4_TABLE_CHUNK_SIZE > size)
			chunk_size = PAGE_ALIGN(size -
					i * MLX4_TABLE_CHUNK_SIZE);

		table->icm[i] = mlx4_alloc_icm(dev, chunk_size >> PAGE_SHIFT,
					       (use_lowmem ? GFP_KERNEL : GFP_HIGHUSER) |
					       __GFP_NOWARN, use_coherent);
		if (!table->icm[i])
			goto err;
		if (mlx4_MAP_ICM(dev, table->icm[i], virt + i * MLX4_TABLE_CHUNK_SIZE)) {
			mlx4_free_icm(dev, table->icm[i], use_coherent);
			table->icm[i] = NULL;
			goto err;
		}

		/*
		 * Add a reference to this ICM chunk so that it never
		 * gets freed (since it contains reserved firmware objects).
		 */
		++table->icm[i]->refcount;
	}

	return 0;

err:
	for (i = 0; i < num_icm; ++i)
		if (table->icm[i]) {
			if (!mlx4_UNMAP_ICM(dev,
					    virt + i * MLX4_TABLE_CHUNK_SIZE,
					    MLX4_TABLE_CHUNK_SIZE / MLX4_ICM_PAGE_SIZE)) {
				mlx4_free_icm(dev, table->icm[i], use_coherent);
			} else {
				pr_warn("mlx4_core: mlx4_UNMAP_ICM failed.\n");
				return -ENOMEM;
			}
		}
	kfree(table->icm);

	return -ENOMEM;
}

void mlx4_cleanup_icm_table(struct mlx4_dev *dev, struct mlx4_icm_table *table)
{
	int i, err = 0;

	for (i = 0; i < table->num_icm; ++i)
		if (table->icm[i]) {
			err = mlx4_UNMAP_ICM(dev,
					     table->virt + i * MLX4_TABLE_CHUNK_SIZE,
					     MLX4_TABLE_CHUNK_SIZE / MLX4_ICM_PAGE_SIZE);
			if (!err) {
				mlx4_free_icm(dev, table->icm[i],
					      table->coherent);
			} else {
				pr_warn("mlx4_core: mlx4_UNMAP_ICM failed.\n");
				break;
			}
		}

	if (!err)
		kfree(table->icm);
}
