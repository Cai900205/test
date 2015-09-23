/*
 * WARNING!!!
 *
 * The following code implements *partial* compatibility layer between
 * linux style sg_table and vmware style scatterlist.
 *
 * The following divergences and limitations apply to it:
 *
 * - All allocations must be done using sg_alloc_table. Never allocate
 *   a vector of scatterlist entries.
 *
 * - The caller is required to ensure only a single iterator
 *   (sg_for_each) is running over the list at any given time. Failing
 *   to do so will result in the iteration misbehaving.
 *
 * - DMA Mapping/unmapping is supported only using
 *   pci_map_sg/pci_unmap_sg, and only by calling it like
 *   pci_map_sg(dev, sgtable.sgl, ...).
 *
 * - Using sg_next directly might lead to unexpected results,
 *   especially when mixed with sg_for_each
 */

#ifndef __SGLIST_VMKLNX_COMPAT_H
#define __SGLIST_VMKLNX_COMPAT_H

#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define NUM_ELEMS_PER_SGLIST 256
#define MLNX_SG_MAGIC_TBL 0x844fa52c
#define MLNX_SG_MAGIC_NODE 0x543acd84

struct mlnx_sg_node {
	struct scatterlist sg;
	struct list_head list;
	void **virt;
	unsigned int nsg;
	unsigned int ndma_elems;
	int vmkio_manually_alloc;
};

struct sg_table {
	struct list_head sg_head;
	struct sg_table *sgl;
	unsigned int nents;
	unsigned int orig_nents;
	unsigned long magic;
	struct {
		struct mlnx_sg_node* dma_node;
		unsigned int dma_idx;
		struct mlnx_sg_node* virt_node;
		unsigned int virt_idx;
		int total_idx;
	} iter;
};

void sg_set_dma_address(struct sg_table *tbl, dma_addr_t addr);
void sg_set_dma_len(struct sg_table *tbl, unsigned int len);
void mlnx_sg_reset(struct sg_table *tbl);

/* It does not support unmapping from the middle of the sg table */
void mlnx_pci_unmap_sg(struct pci_dev *dev, struct sg_table *tbl,
		int nents, enum dma_data_direction direction);

/* It does not support mapping from the middle of the sg table */
int mlnx_pci_map_sg(struct pci_dev *dev, struct sg_table *tbl, int nents,
		enum dma_data_direction direction);

struct sg_table* mlnx_sg_next(struct sg_table *tbl, int __i);

static inline void *mlnx_sg_virt(struct sg_table *tbl) {
	BUG_ON(tbl->magic != MLNX_SG_MAGIC_TBL);

	return tbl->iter.virt_node->virt[tbl->iter.virt_idx];
}

void sg_free_table(struct sg_table *table);

void mlnx_sg_set_buf(struct sg_table *tbl, void *buf,
		unsigned int buflen);

int sg_alloc_table(struct sg_table *table, unsigned int nents,
					gfp_t gfp_mask);

dma_addr_t mlnx_sg_dma_address(struct sg_table *tbl);
int mlnx_sg_dma_len(struct sg_table *tbl);
void mlnx_sg_set_page(struct sg_table *tbl, struct page *page,
                               unsigned int len, unsigned int offset);

/* Not implemented. Can be implemented if needed */
int mlnx_dma_map_sg(void *dev, struct sg_table *tbl,
								int nents, enum dma_data_direction direction);

/* Not implemented. Can be implemented if needed */
void mlnx_dma_unmap_sg(void *dev,
									struct sg_table *tbl, int nents,
									enum dma_data_direction direction);

struct page* mlnx_sg_page(struct sg_table *tbl);

/* for_each_sg can not be paralleled. Only one process can call it
 * each time, for each sg_table
 */
#undef for_each_sg
#define for_each_sg(sglist, sg, nr, __i)	\
	mlnx_sg_reset(sglist); \
	for (__i = 0, sg = (sglist); __i < (nr); ++__i, sg = mlnx_sg_next(sg, __i))


/* WARNING: If you want to add a new define here, you have to add an undef
 * 			in the file compat/vmware/sglist_vmklinux_compat.c
 */
#define sg_dma_len mlnx_sg_dma_len
#define sg_next mlnx_sg_next
#define pci_unmap_sg mlnx_pci_unmap_sg
#define pci_map_sg mlnx_pci_map_sg
#define sg_set_buf mlnx_sg_set_buf
#define sg_virt mlnx_sg_virt
#define sg_set_page mlnx_sg_set_page
#define sg_page mlnx_sg_page
#define dma_map_sg mlnx_dma_map_sg
#define dma_unmap_sg mlnx_dma_unmap_sg
#define sg_dma_address mlnx_sg_dma_address
#define scatterlist sg_table

#endif /* __SGLIST_VMKLNX_COMPAT_H */
