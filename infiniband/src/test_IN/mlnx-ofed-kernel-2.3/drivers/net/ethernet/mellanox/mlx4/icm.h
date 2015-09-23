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

#ifndef MLX4_ICM_H
#define MLX4_ICM_H

#include <linux/list.h>
#include <linux/pci.h>
#include <linux/mutex.h>
#include <linux/scatterlist.h>

enum {
	MLX4_ICM_PAGE_SHIFT	= 12,
	MLX4_ICM_PAGE_SIZE	= 1 << MLX4_ICM_PAGE_SHIFT,
};

struct mlx4_icm {
	struct sg_table mem;
	int refcount;
};

struct mlx4_dev;

struct mlx4_icm *mlx4_alloc_icm(struct mlx4_dev *dev, int npages,
				gfp_t gfp_mask, int coherent);
void __mlx4_free_icm(struct mlx4_dev *dev, struct mlx4_icm *icm,
					int coherent, unsigned int npages_allocated,
					int mapped_sg);

static inline void mlx4_free_icm(struct mlx4_dev *dev, struct mlx4_icm *icm,
							int coherent) {
	__mlx4_free_icm(dev, icm, coherent, icm->mem.orig_nents, 1);
}

int mlx4_table_get(struct mlx4_dev *dev, struct mlx4_icm_table *table, u32 obj);
void mlx4_table_put(struct mlx4_dev *dev, struct mlx4_icm_table *table, u32 obj);
int mlx4_table_get_range(struct mlx4_dev *dev, struct mlx4_icm_table *table,
			 u32 start, u32 end);
void mlx4_table_put_range(struct mlx4_dev *dev, struct mlx4_icm_table *table,
			  u32 start, u32 end);
int mlx4_init_icm_table(struct mlx4_dev *dev, struct mlx4_icm_table *table,
			u64 virt, int obj_size,	u64 nobj, int reserved,
			int use_lowmem, int use_coherent);
void mlx4_cleanup_icm_table(struct mlx4_dev *dev, struct mlx4_icm_table *table);
void *mlx4_table_find(struct mlx4_icm_table *table, u32 obj, dma_addr_t *dma_handle);

int mlx4_MAP_ICM_AUX(struct mlx4_dev *dev, struct mlx4_icm *icm);
int mlx4_UNMAP_ICM_AUX(struct mlx4_dev *dev);

#endif /* MLX4_ICM_H */
