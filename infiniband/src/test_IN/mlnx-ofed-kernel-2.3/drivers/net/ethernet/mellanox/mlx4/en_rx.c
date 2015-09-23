/*
 * Copyright (c) 2007 Mellanox Technologies. All rights reserved.
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
 *
 */

#include <linux/mlx4/cq.h>
#include <linux/slab.h>
#include <linux/mlx4/qp.h>
#include <linux/skbuff.h>
#include <linux/if_ether.h>
#include <linux/if_vlan.h>
#include <linux/vmalloc.h>
#include <linux/prefetch.h>
#include <linux/mlx4/driver.h>
#ifdef CONFIG_NET_RX_BUSY_POLL
#include <net/busy_poll.h>
#endif

#include "mlx4_en.h"

static const u32 rsskey[10] = { 0xD181C62C, 0xF7F4DB5B, 0x1983A2FC,
				0x943E1ADB, 0xD9389E6B, 0xD1039C2C, 0xA74499AD,
				0x593D56D9, 0xF3253C06, 0x2ADC1FFC};

static const int frag_sizes[] = {
	FRAG_SZ0,
	FRAG_SZ1,
	FRAG_SZ2,
	FRAG_SZ3
};

static int mlx4_alloc_pages(struct mlx4_en_priv *priv,
			    struct mlx4_en_rx_alloc *page_alloc,
			    const struct mlx4_en_frag_info *frag_info,
			    gfp_t _gfp)
{
	int order;
	struct page *page;
	dma_addr_t dma;

	for (order = MLX4_EN_ALLOC_PREFER_ORDER; ;) {
		gfp_t gfp = _gfp | __GFP_COLD;

		if (order)
			gfp |= __GFP_COMP | __GFP_NOWARN;
		page = alloc_pages(gfp, order);
		if (likely(page))
			break;
		if (--order < 0 ||
		    ((PAGE_SIZE << order) < frag_info->frag_size))
			return -ENOMEM;
	}

	dma = dma_map_page(priv->ddev, page, 0, PAGE_SIZE << order,
			   PCI_DMA_FROMDEVICE);

	if (dma_mapping_error(priv->ddev, dma)) {
		put_page(page);
		return -ENOMEM;
	}

	page_alloc->page_size = PAGE_SIZE << order;
	page_alloc->page = page;
	page_alloc->dma = dma;
	page_alloc->page_offset = 0;
	/* Not doing get_page() for each frag is a big win
	 * on asymetric workloads.
	 */
	atomic_set(&page->_count,
		   page_alloc->page_size / frag_info->frag_stride);
	return 0;
}

static int mlx4_en_alloc_frags(struct mlx4_en_priv *priv,
			       struct mlx4_en_rx_desc *rx_desc,
			       struct mlx4_en_rx_alloc *frags,
			       struct mlx4_en_rx_alloc *ring_alloc,
			       gfp_t gfp)
{
	struct mlx4_en_rx_alloc page_alloc[MLX4_EN_MAX_RX_FRAGS];
	const struct mlx4_en_frag_info *frag_info;
	struct page *page;
	dma_addr_t dma;
	int i;

	for (i = 0; i < priv->num_frags; i++) {
		frag_info = &priv->frag_info[i];
		page_alloc[i] = ring_alloc[i];
		page_alloc[i].page_offset += frag_info->frag_stride;

		if (page_alloc[i].page_offset + frag_info->frag_stride <=
		    ring_alloc[i].page_size)
			continue;

		if (mlx4_alloc_pages(priv, &page_alloc[i], frag_info, gfp))
			goto out;
	}

	for (i = 0; i < priv->num_frags; i++) {
		frags[i] = ring_alloc[i];
		dma = ring_alloc[i].dma + ring_alloc[i].page_offset;
		ring_alloc[i] = page_alloc[i];
		rx_desc->data[i].addr = cpu_to_be64(dma);
	}

	return 0;

out:
	while (i--) {
		frag_info = &priv->frag_info[i];
		if (page_alloc[i].page != ring_alloc[i].page) {
			dma_unmap_page(priv->ddev, page_alloc[i].dma,
				       page_alloc[i].page_size,
				       PCI_DMA_FROMDEVICE);
			page = page_alloc[i].page;
			atomic_set(&page->_count, 1);
			put_page(page);
		}
	}
	return -ENOMEM;
}

static void mlx4_en_free_frag(struct mlx4_en_priv *priv,
			      struct mlx4_en_rx_alloc *frags,
			      int i)
{
	const struct mlx4_en_frag_info *frag_info = &priv->frag_info[i];
	u32 next_frag_end = frags[i].page_offset + 2 * frag_info->frag_stride;

	if (next_frag_end > frags[i].page_size)
		dma_unmap_page(priv->ddev, frags[i].dma, frags[i].page_size,
			       PCI_DMA_FROMDEVICE);

	if (frags[i].page)
		put_page(frags[i].page);
}

static void mlx4_en_init_rx_desc(struct mlx4_en_priv *priv,
				 struct mlx4_en_rx_ring *ring,
				 int index)
{
	struct mlx4_en_rx_desc *rx_desc = ring->buf + ring->stride * index;
	int possible_frags;
	int i;

	/* Set size and memtype fields */
	for (i = 0; i < priv->num_frags; i++) {
		rx_desc->data[i].byte_count =
			cpu_to_be32(priv->frag_info[i].frag_size);
		rx_desc->data[i].lkey = cpu_to_be32(priv->mdev->mr.key);
	}

	/* If the number of used fragments does not fill up the ring stride,
	 * remaining (unused) fragments must be padded with null address/size
	 * and a special memory key
	 */
	possible_frags = (ring->stride - sizeof(struct mlx4_en_rx_desc)) /
			 DS_SIZE;
	for (i = priv->num_frags; i < possible_frags; i++) {
		rx_desc->data[i].byte_count = 0;
		rx_desc->data[i].lkey = cpu_to_be32(MLX4_EN_MEMTYPE_PAD);
		rx_desc->data[i].addr = 0;
	}
}

static int mlx4_en_init_allocator(struct mlx4_en_priv *priv,
				  struct mlx4_en_rx_ring *ring)
{
	int i;
	struct mlx4_en_rx_alloc *page_alloc;

	for (i = 0; i < priv->num_frags; i++) {
		const struct mlx4_en_frag_info *frag_info = &priv->frag_info[i];

		if (mlx4_alloc_pages(priv, &ring->page_alloc[i], frag_info,
				     GFP_KERNEL))
			goto out;

		en_dbg(DRV, priv, "  frag %d allocator: - size:%d frags:%d\n",
		       i, ring->page_alloc[i].page_size,
		       atomic_read(&ring->page_alloc[i].page->_count));
	}
	return 0;

out:
	while (i--) {
		struct page *page;

		page_alloc = &ring->page_alloc[i];
		dma_unmap_page(priv->ddev, page_alloc->dma,
			       page_alloc->page_size, PCI_DMA_FROMDEVICE);
		page = page_alloc->page;
		atomic_set(&page->_count, 1);
		put_page(page);
		page_alloc->page = NULL;
	}
	return -ENOMEM;
}

static void mlx4_en_destroy_allocator(struct mlx4_en_priv *priv,
				      struct mlx4_en_rx_ring *ring)
{
	struct mlx4_en_rx_alloc *page_alloc;
	int i;

	for (i = 0; i < priv->num_frags; i++) {
		const struct mlx4_en_frag_info *frag_info = &priv->frag_info[i];

		page_alloc = &ring->page_alloc[i];
		en_dbg(DRV, priv, "Freeing allocator:%d count:%d\n",
		       i, page_count(page_alloc->page));

		dma_unmap_page(priv->ddev, page_alloc->dma,
			       page_alloc->page_size, PCI_DMA_FROMDEVICE);
		while (page_alloc->page_offset + frag_info->frag_stride <
		       page_alloc->page_size) {
			put_page(page_alloc->page);
			page_alloc->page_offset += frag_info->frag_stride;
		}
		page_alloc->page = NULL;
	}
}

static int mlx4_en_prepare_rx_desc(struct mlx4_en_priv *priv,
				   struct mlx4_en_rx_ring *ring,
				   int index, gfp_t gfp)
{
	struct mlx4_en_rx_desc *rx_desc = ring->buf + ring->stride * index;
	struct mlx4_en_rx_alloc *frags = ring->rx_info +
					(index << priv->log_rx_info);

	return mlx4_en_alloc_frags(priv, rx_desc, frags, ring->page_alloc, gfp);
}

static inline void mlx4_en_update_rx_prod_db(struct mlx4_en_rx_ring *ring)
{
	*ring->wqres.db.db = cpu_to_be32(ring->prod & 0xffff);
}

static void mlx4_en_free_rx_desc(struct mlx4_en_priv *priv,
				 struct mlx4_en_rx_ring *ring,
				 int index)
{
	struct mlx4_en_rx_alloc *frags;
	int nr;

	frags = ring->rx_info + (index << priv->log_rx_info);
	for (nr = 0; nr < priv->num_frags; nr++) {
		en_dbg(DRV, priv, "Freeing fragment:%d\n", nr);
		mlx4_en_free_frag(priv, frags, nr);
	}
}

static int mlx4_en_fill_rx_buffers(struct mlx4_en_priv *priv)
{
	struct mlx4_en_rx_ring *ring;
	int ring_ind;
	int buf_ind;
	int new_size;

	for (buf_ind = 0; buf_ind < priv->prof->rx_ring_size; buf_ind++) {
		for (ring_ind = 0; ring_ind < priv->rx_ring_num; ring_ind++) {
			ring = priv->rx_ring[ring_ind];

			if (mlx4_en_prepare_rx_desc(priv, ring,
						    ring->actual_size,
						    GFP_KERNEL)) {
				if (ring->actual_size < MLX4_EN_MIN_RX_SIZE) {
					en_err(priv, "Failed to allocate enough rx buffers\n");
					return -ENOMEM;
				} else {
					new_size = rounddown_pow_of_two(ring->actual_size);
					en_warn(priv, "Only %d buffers allocated reducing ring size to %d\n",
						ring->actual_size, new_size);
					goto reduce_rings;
				}
			}
			ring->actual_size++;
			ring->prod++;
		}
	}
	return 0;

reduce_rings:
	for (ring_ind = 0; ring_ind < priv->rx_ring_num; ring_ind++) {
		ring = priv->rx_ring[ring_ind];
		while (ring->actual_size > new_size) {
			ring->actual_size--;
			ring->prod--;
			mlx4_en_free_rx_desc(priv, ring, ring->actual_size);
		}
	}

	return 0;
}

static void mlx4_en_free_rx_buf(struct mlx4_en_priv *priv,
				struct mlx4_en_rx_ring *ring)
{
	int index;

	en_dbg(DRV, priv, "Freeing Rx buf - cons:%d prod:%d\n",
	       ring->cons, ring->prod);

	/* Unmap and free Rx buffers */
	BUG_ON((u32) (ring->prod - ring->cons) > ring->actual_size);

	while (ring->cons != ring->prod) {
		index = ring->cons & ring->size_mask;
		en_dbg(DRV, priv, "Processing descriptor:%d\n", index);
		mlx4_en_free_rx_desc(priv, ring, index);
		++ring->cons;
	}
}

void mlx4_en_set_num_rx_rings(struct mlx4_en_dev *mdev)
{
	int i;
	int num_of_eqs;
	struct mlx4_dev *dev = mdev->dev;
	struct mlx4_en_port_profile *prof = NULL;

	mlx4_foreach_port(i, dev, MLX4_PORT_TYPE_ETH) {
		prof = &mdev->profile.prof[i];
		if (!dev->caps.comp_pool)
			num_of_eqs = max_t(int, MIN_RX_RINGS,
					   min_t(int,
						 dev->caps.num_comp_vectors,
						 DEF_RX_RINGS));
		else
			num_of_eqs = min_t(int, MAX_MSIX_P_PORT,
					   dev->caps.comp_pool/
					   dev->caps.num_ports);

		prof->rx_ring_num = rounddown_pow_of_two(num_of_eqs);
	}
}

#ifdef CONFIG_COMPAT_LRO_ENABLED
static int mlx4_en_get_frag_hdr(struct skb_frag_struct *frags, void **mac_hdr,
				   void **ip_hdr, void **tcpudp_hdr,
				   u64 *hdr_flags, void *priv)
{
#if (LINUX_VERSION_CODE < KERNEL_VERSION(3,1,0))
	*mac_hdr = page_address(frags->page) + frags->page_offset;
#else
	*mac_hdr = page_address(skb_frag_page(frags)) + frags->page_offset;
#endif
	*ip_hdr = *mac_hdr + ETH_HLEN;
	*tcpudp_hdr = (struct tcphdr *)(*ip_hdr + sizeof(struct iphdr));
	*hdr_flags = LRO_IPV4 | LRO_TCP;

	return 0;
}

static void mlx4_en_lro_init(struct mlx4_en_rx_ring *ring,
			    struct mlx4_en_priv *priv)
{
	ring->lro.lro_mgr.max_aggr		= MLX4_EN_LRO_MAX_AGGR;
	ring->lro.lro_mgr.max_desc		= MLX4_EN_LRO_MAX_DESC;
	ring->lro.lro_mgr.lro_arr		= ring->lro.lro_desc;
	ring->lro.lro_mgr.get_frag_header	= mlx4_en_get_frag_hdr;
	ring->lro.lro_mgr.features		= LRO_F_NAPI;
	ring->lro.lro_mgr.frag_align_pad	= NET_IP_ALIGN;
	ring->lro.lro_mgr.dev			= priv->dev;
	ring->lro.lro_mgr.ip_summed		= CHECKSUM_UNNECESSARY;
	ring->lro.lro_mgr.ip_summed_aggr	= CHECKSUM_UNNECESSARY;
}
#endif

int mlx4_en_create_rx_ring(struct mlx4_en_priv *priv,
			   struct mlx4_en_rx_ring **pring,
			   u32 size, int node)
{
	struct mlx4_en_dev *mdev = priv->mdev;
	struct mlx4_en_rx_ring *ring;
	int err = -ENOMEM;
	int tmp;
	int this_cpu = numa_node_id();

	ring = kzalloc_node(sizeof(struct mlx4_en_rx_ring), GFP_KERNEL, node);
	if (!ring) {
		ring = kzalloc(sizeof(struct mlx4_en_rx_ring), GFP_KERNEL);
		if (!ring) {
			en_err(priv, "Failed to allocate RX ring structure\n");
			return -ENOMEM;
		}
		ring->numa_node = this_cpu;
	} else
		ring->numa_node = node;

	ring->prod = 0;
	ring->cons = 0;
	ring->size = size;
	ring->size_mask = size - 1;
	ring->stride = priv->stride;
	ring->log_stride = ffs(ring->stride) - 1;
	ring->buf_size = ring->size * ring->stride + TXBB_SIZE;

	tmp = size * roundup_pow_of_two(MLX4_EN_MAX_RX_FRAGS *
					sizeof(struct mlx4_en_rx_alloc));
	ring->rx_info = vmalloc_node(tmp, node);
	if (!ring->rx_info) {
		ring->rx_info = vmalloc(tmp);
		if (!ring->rx_info) {
			err = -ENOMEM;
			goto err_ring;
		}
	}

	en_dbg(DRV, priv, "Allocated rx_info ring at addr:%p size:%d\n",
		 ring->rx_info, tmp);

	/* Allocate HW buffers on provided NUMA node */
	err = mlx4_alloc_hwq_res(mdev->dev, &ring->wqres,
				 ring->buf_size, 2 * PAGE_SIZE);
	if (err)
		goto err_info;

	err = mlx4_en_map_buffer(&ring->wqres.buf, mdev->dev->numa_node);
	if (err) {
		en_err(priv, "Failed to map RX buffer\n");
		goto err_hwq;
	}
	ring->buf = ring->wqres.buf.direct.buf;

	ring->config = priv->config;

#ifdef CONFIG_COMPAT_LRO_ENABLED
	mlx4_en_lro_init(ring, priv);
#endif

	*pring = ring;
	return 0;

err_hwq:
	mlx4_free_hwq_res(mdev->dev, &ring->wqres, ring->buf_size);
err_info:
	vfree(ring->rx_info);
err_ring:
	kfree(ring);

	return err;
}

void mlx4_en_calc_rx_buf(struct net_device *dev)
{
	struct mlx4_en_priv *priv = netdev_priv(dev);
	int eff_mtu = dev->mtu + ETH_HLEN + VLAN_HLEN + ETH_FCS_LEN;
	int buf_size = 0;
	int i = 0;

	while (buf_size < eff_mtu) {
		priv->frag_info[i].frag_size =
			(eff_mtu > buf_size + frag_sizes[i]) ?
				frag_sizes[i] : eff_mtu - buf_size;
		priv->frag_info[i].frag_prefix_size = buf_size;
		priv->frag_info[i].frag_stride =
				ALIGN(frag_sizes[i], SMP_CACHE_BYTES);

		buf_size += priv->frag_info[i].frag_size;
		i++;
	}

	priv->num_frags = i;
	priv->rx_skb_size = eff_mtu;
	priv->log_rx_info = ROUNDUP_LOG2(i * sizeof(struct mlx4_en_rx_alloc));
	en_dbg(DRV, priv, "Rx buffer (effective-mtu:%d num_frags:%d):\n",
	       eff_mtu, priv->num_frags);

	for (i = 0; i < priv->num_frags; i++) {
		en_err(priv,
		       "  frag:%d - size:%d prefix:%d stride:%d\n",
		       i, priv->frag_info[i].frag_size,
		       priv->frag_info[i].frag_prefix_size,
		       priv->frag_info[i].frag_stride);
	}
}

int mlx4_en_activate_rx_rings(struct mlx4_en_priv *priv)
{
	struct mlx4_en_rx_ring *ring;
	int i;
	int ring_ind;
	int err;
	int stride = roundup_pow_of_two(sizeof(struct mlx4_en_rx_desc) +
					DS_SIZE * priv->num_frags);

	for (ring_ind = 0; ring_ind < priv->rx_ring_num; ring_ind++) {
		ring = priv->rx_ring[ring_ind];

		ring->prod = 0;
		ring->cons = 0;
		ring->actual_size = 0;
		ring->cqn = priv->rx_cq[ring_ind]->mcq.cqn;

		ring->stride = stride;
		if (ring->stride <= TXBB_SIZE)
			ring->buf += TXBB_SIZE;

		ring->log_stride = ffs(ring->stride) - 1;
		ring->buf_size = ring->size * ring->stride;

		memset(ring->buf, 0, ring->buf_size);
		mlx4_en_update_rx_prod_db(ring);

		/* Initialize all descriptors */
		for (i = 0; i < ring->size; i++)
			mlx4_en_init_rx_desc(priv, ring, i);

		/* Initialize page allocators */
		en_dbg(DRV, priv, "Ring %d Allocators:\n", ring_ind);
		err = mlx4_en_init_allocator(priv, ring);
		if (err) {
			en_err(priv, "Failed initializing ring allocator\n");

			if (ring->stride <= TXBB_SIZE)
				ring->buf -= TXBB_SIZE;

			ring_ind--;
			goto err_allocator;
		}
	}
	err = mlx4_en_fill_rx_buffers(priv);
	if (err)
		goto err_buffers;

	for (ring_ind = 0; ring_ind < priv->rx_ring_num; ring_ind++) {
		ring = priv->rx_ring[ring_ind];

		ring->size_mask = ring->actual_size - 1;
		mlx4_en_update_rx_prod_db(ring);
	}

	return 0;

err_buffers:
	for (ring_ind = 0; ring_ind < priv->rx_ring_num; ring_ind++)
		mlx4_en_free_rx_buf(priv, priv->rx_ring[ring_ind]);

	ring_ind = priv->rx_ring_num - 1;
err_allocator:
	while (ring_ind >= 0) {
		if (priv->rx_ring[ring_ind]->stride <= TXBB_SIZE)
			priv->rx_ring[ring_ind]->buf -= TXBB_SIZE;
		mlx4_en_destroy_allocator(priv, priv->rx_ring[ring_ind]);
		ring_ind--;
	}

	return err;
}

void mlx4_en_destroy_rx_ring(struct mlx4_en_priv *priv,
			     struct mlx4_en_rx_ring **pring,
			     u32 size, u16 stride)
{
	struct mlx4_en_dev *mdev = priv->mdev;
	struct mlx4_en_rx_ring *ring = *pring;

	mlx4_en_unmap_buffer(&ring->wqres.buf);
	mlx4_free_hwq_res(mdev->dev, &ring->wqres, size * stride + TXBB_SIZE);
	vfree(ring->rx_info);
	kfree(ring);
	*pring = NULL;
}

void mlx4_en_deactivate_rx_ring(struct mlx4_en_priv *priv,
				struct mlx4_en_rx_ring *ring)
{
	mlx4_en_free_rx_buf(priv, ring);

	if (ring->stride <= TXBB_SIZE)
		ring->buf -= TXBB_SIZE;

	mlx4_en_destroy_allocator(priv, ring);
}

static int mlx4_en_complete_rx_desc(struct mlx4_en_priv *priv,
				    struct mlx4_en_rx_desc *rx_desc,
				    struct mlx4_en_rx_alloc *frags,
				    struct skb_frag_struct *skb_frags_rx,
				    int length)
{
	struct device *dev = priv->ddev;
	struct mlx4_en_frag_info *frag_info;
	int nr;
	dma_addr_t dma;

	/* Collect used fragments while replacing them in the HW descriptors */
	for (nr = 0; nr < priv->num_frags; nr++) {
		frag_info = &priv->frag_info[nr];

		if (length <= frag_info->frag_prefix_size)
			break;

		if (!frags[nr].page)
			goto fail;

		dma = be64_to_cpu(rx_desc->data[nr].addr);
		dma_sync_single_for_cpu(dev, dma, frag_info->frag_size,
					DMA_FROM_DEVICE);

		/* Save pages reference in frags */
		__skb_frag_set_page(&skb_frags_rx[nr], frags[nr].page);
		skb_frag_size_set(&skb_frags_rx[nr], frag_info->frag_size);
		skb_frags_rx[nr].page_offset = frags[nr].page_offset;

		/* Frag no longer at our possition. new owner should release */
		frags[nr].page = NULL;
	}

	/* Adjust size of last fragment to match actual length */
	if (nr > 0)
		skb_frag_size_set(&skb_frags_rx[nr - 1],
				  length -
				  priv->frag_info[nr - 1].frag_prefix_size);

	return nr;

fail:
	while (nr > 0) {
		nr--;
		__skb_frag_unref(&skb_frags_rx[nr]);
	}

	return 0;
}

static inline int mlx4_en_get_hdrlen(unsigned char *data, int max_len)
{
	union mlx4_en_phdr hdr;
	u16 eth_protocol;
	u8 ip_protocol = 0;

	hdr.network = data;
	eth_protocol = hdr.eth->h_proto;
	hdr.network += ETH_HLEN;

	if (eth_protocol == __constant_htons(ETH_P_8021Q)) {
		eth_protocol = hdr.vlan->h_vlan_encapsulated_proto;
		hdr.network += VLAN_HLEN;
	}

	if (eth_protocol == __constant_htons(ETH_P_IP)) {
		if (!(hdr.ipv4->frag_off & htons(IP_OFFSET)))
			ip_protocol = hdr.ipv4->protocol;
		hdr.network += IP_HDR_LEN(hdr.ipv4);

	} else if (eth_protocol == __constant_htons(ETH_P_IPV6)) {
		ip_protocol = hdr.ipv6->nexthdr;
		hdr.network += sizeof(struct ipv6hdr);
	}

	if (ip_protocol == IPPROTO_TCP)
		hdr.network += TCP_HDR_LEN(hdr.tcph);

	else if (ip_protocol == IPPROTO_UDP)
		hdr.network += sizeof(struct udphdr);

	return (hdr.network - data) < max_len ? hdr.network - data : max_len;
}

static struct sk_buff *mlx4_en_rx_skb(struct mlx4_en_priv *priv,
				      struct mlx4_en_rx_desc *rx_desc,
				      struct mlx4_en_rx_alloc *frags,
				      unsigned int length)
{
	struct device *dev = priv->ddev;
	struct sk_buff *skb;
	void *va;
	dma_addr_t dma;
	int hdrlen;
	int used_frags;
	int i;

	skb = netdev_alloc_skb_ip_align(priv->dev, SMALL_PACKET_SIZE);
	if (!skb) {
		en_dbg(RX_ERR, priv, "Failed allocating skb\n");
		return NULL;
	}
	prefetchw(skb->data);

	skb->len = length;
	/*
	 * Get pointer to first fragment so we could copy the headers into the
	 * (linear part of the) skb
	 */
	va = page_address(frags[0].page) + frags[0].page_offset;

	if (length <= SMALL_PACKET_SIZE) {
		/*
		 * We are copying all relevant data to the skb - temporarily
		 * sync buffers for the copy
		 */
		dma = be64_to_cpu(rx_desc->data[0].addr);
		dma_sync_single_for_cpu(dev, dma, length,
					DMA_FROM_DEVICE);
		skb_copy_to_linear_data(skb, va, length);
		dma_sync_single_for_device(dev, dma, length,
					   DMA_FROM_DEVICE);
		skb->tail += length;
	} else {
		/* Move relevant fragments to skb */
		used_frags = mlx4_en_complete_rx_desc(priv, rx_desc, frags,
						      skb_shinfo(skb)->frags,
						      length);

		if (unlikely(!used_frags)) {
			kfree_skb(skb);
			return NULL;
		}

		skb_shinfo(skb)->nr_frags = used_frags;

		/* Copy headers into the skb linear buffer */
		hdrlen = mlx4_en_get_hdrlen(va, SMALL_PACKET_SIZE);
		memcpy(skb->data, va, ALIGN(hdrlen, sizeof(long)));
		skb->tail += hdrlen;

		/* Skip headers in first fragment */
		skb_shinfo(skb)->frags[0].page_offset += hdrlen;

		/* Adjust size of first fragment */
		skb_frag_size_sub(&skb_shinfo(skb)->frags[0], hdrlen);
		skb->data_len = length - hdrlen;

		for (i = 0; i < used_frags; i++)
			skb->truesize += priv->frag_info[i].frag_stride;
	}

	return skb;
}

static void validate_loopback(struct mlx4_en_priv *priv, struct sk_buff *skb)
{
	int i;
	int offset = ETH_HLEN;

	for (i = 0; i < MLX4_LOOPBACK_TEST_PAYLOAD; i++, offset++) {
		if (*(skb->data + offset) != (unsigned char) (i & 0xff))
			return;
	}
	/* Loopback found */
	priv->loopback_ok = 1;
}

#ifdef CONFIG_COMPAT_LRO_ENABLED
static inline int mlx4_en_can_lro(__be16 status)
{
	static __be16 status_all;
	static __be16 status_ipv4_ipok_tcp;

	status_all		= cpu_to_be16(
					MLX4_CQE_STATUS_IPV4    |
					MLX4_CQE_STATUS_IPV4F   |
					MLX4_CQE_STATUS_IPV6    |
					MLX4_CQE_STATUS_IPV4OPT |
					MLX4_CQE_STATUS_TCP     |
					MLX4_CQE_STATUS_UDP     |
					MLX4_CQE_STATUS_IPOK);

	status_ipv4_ipok_tcp	= cpu_to_be16(
					MLX4_CQE_STATUS_IPV4    |
					MLX4_CQE_STATUS_IPOK    |
					MLX4_CQE_STATUS_TCP);

	status &= status_all;
	return status == status_ipv4_ipok_tcp;
}
#endif

static inline int invalid_cqe(struct mlx4_en_priv *priv,
			      struct mlx4_cqe *cqe)
{
	/* Drop packet on bad receive or bad checksum */
	if (unlikely((cqe->owner_sr_opcode & MLX4_CQE_OPCODE_MASK) ==
		     MLX4_CQE_OPCODE_ERROR)) {
		en_err(priv, "CQE completed in error - vendor syndrom:%d syndrom:%d\n",
		       ((struct mlx4_err_cqe *)cqe)->vendor_err_syndrome,
		       ((struct mlx4_err_cqe *)cqe)->syndrome);
		return 1;
	}
	if (unlikely(cqe->badfcs_enc & MLX4_CQE_BAD_FCS)) {
		en_dbg(RX_ERR, priv, "Accepted frame with bad FCS\n");
		return 1;
	}

	return 0;
}

static __wsum csum_ipv6_magic_nofold(const struct in6_addr *saddr,
				     const struct in6_addr *daddr,
				     __u32 len, unsigned short proto,
				     __wsum sum)
{
	__wsum res = sum;

	res = csum_add(res, saddr->in6_u.u6_addr32[0]);
	res = csum_add(res, saddr->in6_u.u6_addr32[1]);
	res = csum_add(res, saddr->in6_u.u6_addr32[2]);
	res = csum_add(res, saddr->in6_u.u6_addr32[3]);
	res = csum_add(res, daddr->in6_u.u6_addr32[0]);
	res = csum_add(res, daddr->in6_u.u6_addr32[1]);
	res = csum_add(res, daddr->in6_u.u6_addr32[2]);
	res = csum_add(res, daddr->in6_u.u6_addr32[3]);
	res = csum_add(res, len);
	res = csum_add(res, htonl(proto));

	return res;
}

/* When hardware doesn't strip the vlan, we need to calculate the checksum
 * over it and add it to the hardware's checksum calculation
 */
static inline __wsum get_fixed_vlan_csum(__wsum hw_checksum,
					 struct vlan_hdr *vlanh)
{
	return csum_add(hw_checksum, *(__wsum *)vlanh);
}
/* HW adds the pseudo header although OS stack doesn't expect it,
 * Therefore we are subtracting it from the checksum
 */
static inline __wsum get_fixed_ipv4_csum(__wsum hw_checksum, struct iphdr *iph)
{
	__u16 length_for_csum = 0;
	__wsum csum_pseudo_header = 0;

	length_for_csum = (be16_to_cpu(iph->tot_len) - IP_HDR_LEN(iph));
	csum_pseudo_header = csum_tcpudp_nofold(iph->saddr, iph->daddr,
						length_for_csum, iph->protocol, 0);
	return csum_sub(hw_checksum, csum_pseudo_header);
}

/* HW adds the pseudo header and does not calculate csum of ip,
 * although OS stack expects csum which includes ip header
 * and does not include pseudo header,
 * Therefore we are subtracting pseudo header from the checksum
 * and adding ip header csum to it.
 */
static inline __wsum get_fixed_ipv6_csum(__wsum hw_checksum,
					 struct ipv6hdr *ipv6h)
{
	__wsum csum_pseudo_header = 0;

	if (ipv6h->nexthdr == IPPROTO_FRAGMENT || ipv6h->nexthdr == IPPROTO_HOPOPTS)
		return 0;

	hw_checksum = csum_add(hw_checksum, cpu_to_be16(ipv6h->nexthdr));
	csum_pseudo_header = csum_ipv6_magic_nofold(&ipv6h->saddr,
						    &ipv6h->daddr,
						    ipv6h->payload_len,
						    ipv6h->nexthdr,
						    0);
	hw_checksum = csum_sub(hw_checksum, csum_pseudo_header);

	return csum_partial(ipv6h, sizeof(struct ipv6hdr), hw_checksum);
}

static void mlx4_en_refill_rx_buffers(struct mlx4_en_priv *priv,
				      struct mlx4_en_rx_ring *ring)
{
	int index = ring->prod & ring->size_mask;

	while ((u32) (ring->prod - ring->cons) < ring->actual_size) {
		if (mlx4_en_prepare_rx_desc(priv, ring, index, GFP_ATOMIC))
			break;
		ring->prod++;
		index = ring->prod & ring->size_mask;
	}
}

int mlx4_en_process_rx_cq(struct net_device *dev,
			  struct mlx4_en_cq *cq,
			  int budget)
{
	struct mlx4_en_priv *priv = netdev_priv(dev);
	struct mlx4_en_dev *mdev = priv->mdev;
	struct mlx4_cqe *cqe;
	struct mlx4_cq *mcq = &cq->mcq;
	struct mlx4_en_rx_ring *ring = priv->rx_ring[cq->ring];
	struct mlx4_en_rx_alloc *frags;
	struct mlx4_en_rx_desc *rx_desc;
	struct net_device_stats *stats = &priv->stats;
	struct sk_buff *skb;
	void *page_addr;
	int index;
	int nr;
	unsigned int length;
	int polled = 0;
	int factor = priv->cqe_factor;
	u32 cons_index = mcq->cons_index;
	u32 size_mask = ring->size_mask;
	int size = cq->size;
	struct mlx4_cqe *buf = cq->buf;
	u64 timestamp;
	int ip_summed;
#ifdef CONFIG_COMPAT_VXLAN_ENABLED
	bool l2_tunnel;
#endif
#ifdef CONFIG_COMPAT_LRO_ENABLED
	struct skb_frag_struct lro_frag;
#endif
	__wsum hw_checksum = 0;

	if (!priv->port_up)
		return 0;

	/* We assume a 1:1 mapping between CQEs and Rx descriptors, so Rx
	 * descriptor offset can be deduced from the CQE index instead of
	 * reading 'cqe->index' */
	index = cons_index & size_mask;
	cqe = mlx4_en_get_cqe(buf, index, priv->cqe_size) + factor;

	/* Process all completed CQEs */
	while (XNOR(cqe->owner_sr_opcode & MLX4_CQE_OWNER_MASK,
		    cons_index & size)) {

		rx_desc = ring->buf + (index << ring->log_stride);
		frags = ring->rx_info + (index << priv->log_rx_info);
		prefetchw(frags[0].page);

		/* make sure we read the CQE after we read the ownership bit */
		rmb();

		/* Prefetch packet header */
		page_addr = page_address(frags[0].page) + frags[0].page_offset;
		prefetch(page_addr);
#if L1_CACHE_BYTES < 128
		prefetch(page_addr + L1_CACHE_BYTES);
#endif

		/* Drop packet on bad receive or bad checksum */
		if (unlikely(invalid_cqe(priv, cqe)))
			goto next;

		/* Check if we need to drop the packet if SRIOV is not enabled
		 * and not performing the selftest or flb disabled
		 */
		if (priv->flags & MLX4_EN_FLAG_RX_FILTER_NEEDED) {
			struct ethhdr *ethh;
			dma_addr_t dma;

			/* Get pointer to first fragment since we haven't skb
			 * yet and cast it to ethhdr struct
			 */
			dma = be64_to_cpu(rx_desc->data[0].addr);
			dma_sync_single_for_cpu(priv->ddev, dma, sizeof(*ethh),
						DMA_FROM_DEVICE);
			ethh = (struct ethhdr *)(page_addr);

			if (is_multicast_ether_addr(ethh->h_dest)) {
				struct mlx4_mac_entry *entry;
#if (LINUX_VERSION_CODE < KERNEL_VERSION(3,9,0))
				struct hlist_node *n;
#endif
				struct hlist_head *bucket;
				unsigned int mac_hash;

				/* Drop the packet, since HW loopback-ed it */
				mac_hash = ethh->h_source[MLX4_EN_MAC_HASH_IDX];
				bucket = &priv->mac_hash[mac_hash];
				rcu_read_lock();
#if (LINUX_VERSION_CODE < KERNEL_VERSION(3,9,0))
				hlist_for_each_entry_rcu(entry, n, bucket,
							 hlist) {
#else
				hlist_for_each_entry_rcu(entry, bucket,
							 hlist) {
#endif
					if (ether_addr_equal_64bits(entry->mac,
							ethh->h_source)) {
						rcu_read_unlock();
						goto next;
					}
				}
				rcu_read_unlock();
			}
		}
		/*
		 * Packet is OK - process it.
		 */
		length = be32_to_cpu(cqe->byte_cnt);
		length -= ring->fcs_del;
		ring->bytes += length;
		ring->packets++;
#ifdef CONFIG_COMPAT_VXLAN_ENABLED
		l2_tunnel = (dev->hw_enc_features & NETIF_F_RXCSUM) &&
			(cqe->vlan_my_qpn & cpu_to_be32(MLX4_CQE_L2_TUNNEL));
#endif

		if (likely(dev->features & NETIF_F_RXCSUM)) {
			if ((cqe->status & cpu_to_be16(MLX4_CQE_STATUS_TCP)) ||
			    (cqe->status & cpu_to_be16(MLX4_CQE_STATUS_UDP))) {
				if (cqe->checksum == cpu_to_be16(0xffff)) {
					ring->csum_ok++;
					ip_summed = CHECKSUM_UNNECESSARY;
#ifdef CONFIG_COMPAT_LRO_ENABLED
					/* traffic eligible for LRO */
					if ((dev->features & NETIF_F_LRO) &&
					    mlx4_en_can_lro(cqe->status) &&
					    (ring->config.hwtstamp.rx_filter ==
					     HWTSTAMP_FILTER_NONE) &&
					    length <= priv->frag_info[0].frag_size &&
#ifdef CONFIG_COMPAT_VXLAN_ENABLED
					     !l2_tunnel &&
#endif
					     !(be32_to_cpu(cqe->vlan_my_qpn) &
					       MLX4_CQE_VLAN_PRESENT_MASK)) {
						nr = mlx4_en_complete_rx_desc(priv, rx_desc, frags,
									      &lro_frag, length);

						if (nr != 1) {
							stats->rx_dropped++;
							goto next;
						}

						/* Push it up the stack (LRO) */
						lro_receive_frags(&ring->lro.lro_mgr, &lro_frag,
								  length, priv->frag_info[0].frag_stride,
								  NULL, 0);
						goto next;
					}
#endif
				} else {
					ip_summed = CHECKSUM_NONE;
					ring->csum_none++;
				}
			} else {
				if (!(priv->rx_csum_mode_port &
				    MLX4_RX_CSUM_MODE_VAL_NON_TCP_UDP) ||
				    (!(cqe->status &
				      cpu_to_be16(MLX4_CQE_STATUS_IPV4)) &&
				     !(cqe->status &
				      cpu_to_be16(MLX4_CQE_STATUS_IPV6)))) {
					ip_summed = CHECKSUM_NONE;
					ring->csum_none++;
				} else {
					ip_summed = CHECKSUM_COMPLETE;
				}
			}
		} else {
			ring->csum_none++;
			ip_summed = CHECKSUM_NONE;
		}

		/* any other kind of traffic goes here */
		skb = mlx4_en_rx_skb(priv, rx_desc, frags, length);
		if (!skb) {
			stats->rx_dropped++;
			goto next;
		}

		/* check for loopback */
		if (unlikely(priv->validate_loopback)) {
			validate_loopback(priv, skb);
			kfree_skb(skb);
			goto next;
		}

		if (ip_summed == CHECKSUM_COMPLETE) {
			void *hdr = (u8 *)skb->data + sizeof(struct ethhdr);

			hw_checksum = csum_unfold(cqe->checksum);
			if (((struct ethhdr *)skb->data)->h_proto == htons(ETH_P_8021Q) &&
			    !(ring->config.flags & MLX4_EN_RX_VLAN_OFFLOAD)) {
				hw_checksum =
					get_fixed_vlan_csum(hw_checksum, hdr);
				hdr += sizeof(struct vlan_hdr);
			}
			if (cqe->status & cpu_to_be16(MLX4_CQE_STATUS_IPV4)) {
				skb->csum = get_fixed_ipv4_csum(hw_checksum, hdr);
			} else if (cqe->status & cpu_to_be16(MLX4_CQE_STATUS_IPV6)) {
				__wsum calculated_fixed_csum =
					get_fixed_ipv6_csum(hw_checksum, hdr);

				/* 0 is invalid value in 1's complement
				 * csum calculation.
				 */
				if (!calculated_fixed_csum) {
					ip_summed = CHECKSUM_NONE;
					ring->csum_none++;
				} else {
					skb->csum = calculated_fixed_csum;
				}
			}
		}

		skb->ip_summed = ip_summed;
		skb->protocol = eth_type_trans(skb, dev);
		skb_record_rx_queue(skb, cq->ring);

#ifdef CONFIG_COMPAT_VXLAN_ENABLED
		if (l2_tunnel)
			skb->encapsulation = 1;
#endif

#ifdef CONFIG_COMPAT_NETIF_F_RXHASH
		if (dev->features & NETIF_F_RXHASH)
#if (LINUX_VERSION_CODE < KERNEL_VERSION(3,14,0))
			skb->rxhash = be32_to_cpu(cqe->immed_rss_invalid);
#else
			skb_set_hash(skb,
				     be32_to_cpu(cqe->immed_rss_invalid),
				     PKT_HASH_TYPE_L3);
#endif
#endif

		/* process VLAN traffic */
		if ((be32_to_cpu(cqe->vlan_my_qpn) &
		     MLX4_CQE_VLAN_PRESENT_MASK) &&
		     ring->config.flags & MLX4_EN_RX_VLAN_OFFLOAD) {
#if (LINUX_VERSION_CODE < KERNEL_VERSION(3,1,0))
			if (priv->vlgrp) {
				vlan_gro_receive(&cq->napi, priv->vlgrp,
						 be16_to_cpu(cqe->sl_vid),
						 skb);
				goto next;
			}
#endif
#if (LINUX_VERSION_CODE < KERNEL_VERSION(3,10,0))
			__vlan_hwaccel_put_tag(skb, be16_to_cpu(cqe->sl_vid));
#else
			__vlan_hwaccel_put_tag(skb, htons(ETH_P_8021Q), be16_to_cpu(cqe->sl_vid));
#endif

		/* process time stamps */
		} else if (ring->config.hwtstamp.rx_filter == HWTSTAMP_FILTER_ALL) {
			timestamp = mlx4_en_get_cqe_ts(cqe);
			mlx4_en_fill_hwtstamps(mdev, skb_hwtstamps(skb),
					       timestamp);
		}

#ifdef CONFIG_NET_RX_BUSY_POLL
		skb_mark_napi_id(skb, &cq->napi);
#endif

		/* Push it up the stack */
		if (mlx4_en_cq_ll_polling(cq))
			netif_receive_skb(skb);
		else
			napi_gro_receive(&cq->napi, skb);

next:
		for (nr = 0; nr < priv->num_frags; nr++)
			mlx4_en_free_frag(priv, frags, nr);

		++cons_index;
		index = cons_index & size_mask;
		cqe = mlx4_en_get_cqe(buf, index, priv->cqe_size) + factor;
		if (++polled == budget) {
			/* we are here because we reached the NAPI budget */
			goto out;
		}
	}

out:
#ifdef CONFIG_COMPAT_LRO_ENABLED
	if (dev->features & NETIF_F_LRO)
		lro_flush_all(&priv->rx_ring[cq->ring]->lro.lro_mgr);
#endif
	AVG_PERF_COUNTER(priv->pstats.rx_coal_avg, polled);
	mcq->cons_index = cons_index;
	mlx4_cq_set_ci(mcq);
	wmb(); /* ensure HW sees CQ consumer before we post new buffers */
	ring->cons = mcq->cons_index;
	mlx4_en_refill_rx_buffers(priv, ring);
	mlx4_en_update_rx_prod_db(ring);
	return polled;
}

void mlx4_en_rx_irq(struct mlx4_cq *mcq)
{
	struct mlx4_en_cq *cq = container_of(mcq, struct mlx4_en_cq, mcq);
	struct mlx4_en_priv *priv = netdev_priv(cq->dev);

	if (priv->port_up)
		napi_schedule(&cq->napi);
	else
		mlx4_en_arm_cq(priv, cq);
}

/* Rx CQ polling - called by NAPI */
int mlx4_en_poll_rx_cq(struct napi_struct *napi, int budget)
{
	struct mlx4_en_cq *cq = container_of(napi, struct mlx4_en_cq, napi);
	struct net_device *dev = cq->dev;
	struct mlx4_en_priv *priv = netdev_priv(dev);
	int done;

	if (!mlx4_en_cq_lock_napi(cq))
		return budget;

	done = mlx4_en_process_rx_cq(dev, cq, budget);

	mlx4_en_cq_unlock_napi(cq);

	/* If we used up all the quota - we're probably not done yet... */
	cq->tot_rx += done;
	if (done == budget) {
		INC_PERF_COUNTER(priv->pstats.napi_quota);
		if (cq->tot_rx >= MLX4_EN_MIN_RX_ARM) {
			napi_complete(napi);
			mlx4_en_arm_cq(priv, cq);
			cq->tot_rx = 0;
			return 0;
		}
	} else {
		/* Done for now */
		napi_complete(napi);
		mlx4_en_arm_cq(priv, cq);
		cq->tot_rx = 0;
		return done;
	}
	return budget;
}

/* RSS related functions */

static int mlx4_en_config_rss_qp(struct mlx4_en_priv *priv, int qpn,
				 struct mlx4_en_rx_ring *ring,
				 enum mlx4_qp_state *state,
				 struct mlx4_qp *qp)
{
	struct mlx4_en_dev *mdev = priv->mdev;
	struct mlx4_qp_context *context;
	int err = 0;

	context = kmalloc(sizeof *context , GFP_KERNEL);
	if (!context) {
		en_err(priv, "Failed to allocate qp context\n");
		return -ENOMEM;
	}

	err = mlx4_qp_alloc(mdev->dev, qpn, qp);
	if (err) {
		en_err(priv, "Failed to allocate qp #%x\n", qpn);
		goto out;
	}
	qp->event = mlx4_en_sqp_event;

	memset(context, 0, sizeof *context);
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,39)) || defined (CONFIG_COMPAT_NEW_TX_RING_SCHEME)
	mlx4_en_fill_qp_context(priv, ring->actual_size,
				ring->stride, 0, 0,
				qpn, ring->cqn, -1,
				context, MLX4_EN_NO_VLAN);
#else
	mlx4_en_fill_qp_context(priv, ring->actual_size, ring->stride, 0, 0,
				qpn, ring->cqn, context, MLX4_EN_NO_VLAN);
#endif
	context->db_rec_addr = cpu_to_be64(ring->wqres.db.dma);

	/* Cancel FCS removal if FW allows */
	if (mdev->dev->caps.flags & MLX4_DEV_CAP_FLAG_FCS_KEEP) {
		context->param3 |= cpu_to_be32(1 << 29);
		ring->fcs_del = ETH_FCS_LEN;
	} else
		ring->fcs_del = 0;

	err = mlx4_qp_to_ready(mdev->dev, &ring->wqres.mtt, context, qp, state);
	if (err) {
		mlx4_qp_remove(mdev->dev, qp);
		mlx4_qp_free(mdev->dev, qp);
	}
	mlx4_en_update_rx_prod_db(ring);
out:
	kfree(context);
	return err;
}

int mlx4_en_create_drop_qp(struct mlx4_en_priv *priv)
{
	int err;
	u32 qpn;

	err = mlx4_qp_reserve_range(priv->mdev->dev, 1, 1, &qpn,
				    MLX4_RESERVE_A0_RSS);
	if (err) {
		en_err(priv, "Failed reserving drop qpn\n");
		return err;
	}
	err = mlx4_qp_alloc(priv->mdev->dev, qpn, &priv->drop_qp);
	if (err) {
		en_err(priv, "Failed allocating drop qp\n");
		mlx4_qp_release_range(priv->mdev->dev, qpn, 1);
		return err;
	}

	return 0;
}

void mlx4_en_destroy_drop_qp(struct mlx4_en_priv *priv)
{
	u32 qpn;

	qpn = priv->drop_qp.qpn;
	mlx4_qp_remove(priv->mdev->dev, &priv->drop_qp);
	mlx4_qp_free(priv->mdev->dev, &priv->drop_qp);
	mlx4_qp_release_range(priv->mdev->dev, qpn, 1);
}

/* Allocate rx qp's and configure them according to rss map */
int mlx4_en_config_rss_steer(struct mlx4_en_priv *priv)
{
	struct mlx4_en_dev *mdev = priv->mdev;
	struct mlx4_en_rss_map *rss_map = &priv->rss_map;
	struct mlx4_qp_context context;
	struct mlx4_rss_context *rss_context;
	int rss_rings;
	void *ptr;
	u8 rss_mask = (MLX4_RSS_IPV4 | MLX4_RSS_TCP_IPV4 | MLX4_RSS_IPV6 |
			MLX4_RSS_TCP_IPV6);
	int i;
	int err = 0;
	int good_qps = 0;

	en_dbg(DRV, priv, "Configuring rss steering\n");
	err = mlx4_qp_reserve_range(mdev->dev, priv->rx_ring_num,
				    priv->rx_ring_num,
				    &rss_map->base_qpn, 0);
	if (err) {
		en_err(priv, "Failed reserving %d qps\n", priv->rx_ring_num);
		return err;
	}

	for (i = 0; i < priv->rx_ring_num; i++) {
		priv->rx_ring[i]->qpn = rss_map->base_qpn + i;
		err = mlx4_en_config_rss_qp(priv, priv->rx_ring[i]->qpn,
					    priv->rx_ring[i],
					    &rss_map->state[i],
					    &rss_map->qps[i]);
		if (err)
			goto rss_err;

		++good_qps;
	}

	/* Configure RSS indirection qp */
	err = mlx4_qp_alloc(mdev->dev, priv->base_qpn, &rss_map->indir_qp);
	if (err) {
		en_err(priv, "Failed to allocate RSS indirection QP\n");
		goto rss_err;
	}
	rss_map->indir_qp.event = mlx4_en_sqp_event;
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,39)) || defined (CONFIG_COMPAT_NEW_TX_RING_SCHEME)
	mlx4_en_fill_qp_context(priv, 0, 0, 0, 1, priv->base_qpn,
				priv->rx_ring[0]->cqn, -1, &context,
				MLX4_EN_NO_VLAN);
#else
	mlx4_en_fill_qp_context(priv, 0, 0, 0, 1, priv->base_qpn,
				priv->rx_ring[0]->cqn, &context,
				MLX4_EN_NO_VLAN);
#endif

	if (!priv->prof->rss_rings || priv->prof->rss_rings > priv->rx_ring_num)
		rss_rings = priv->rx_ring_num;
	else
		rss_rings = priv->prof->rss_rings;

	ptr = ((void *) &context) + offsetof(struct mlx4_qp_context, pri_path)
					+ MLX4_RSS_OFFSET_IN_QPC_PRI_PATH;
	rss_context = ptr;
	rss_context->base_qpn = cpu_to_be32(ilog2(rss_rings) << 24 |
					    (rss_map->base_qpn));
	rss_context->default_qpn = cpu_to_be32(rss_map->base_qpn);
	if (priv->mdev->profile.udp_rss) {
		rss_mask |=  MLX4_RSS_UDP_IPV4 | MLX4_RSS_UDP_IPV6;
		rss_context->base_qpn_udp = rss_context->default_qpn;
	}

#ifdef CONFIG_COMPAT_VXLAN_ENABLED
	if (mdev->dev->caps.tunnel_offload_mode == MLX4_TUNNEL_OFFLOAD_MODE_VXLAN) {
		en_info(priv, "Setting RSS context tunnel type to RSS on inner headers\n");
		rss_mask |= MLX4_RSS_BY_INNER_HEADERS;
	}
#endif

	rss_context->flags = rss_mask;

	if (priv->pflags & MLX4_EN_PRIV_FLAGS_RSS_HASH_XOR) {
		rss_context->hash_fn = MLX4_RSS_HASH_XOR;
	} else {
		rss_context->hash_fn = MLX4_RSS_HASH_TOP;
		for (i = 0; i < 10; i++)
			rss_context->rss_key[i] = cpu_to_be32(rsskey[i]);
	}

	err = mlx4_qp_to_ready(mdev->dev, &priv->res.mtt, &context,
			       &rss_map->indir_qp, &rss_map->indir_state);
	if (err)
		goto indir_err;

	return 0;

indir_err:
	mlx4_qp_modify(mdev->dev, NULL, rss_map->indir_state,
		       MLX4_QP_STATE_RST, NULL, 0, 0, &rss_map->indir_qp);
	mlx4_qp_remove(mdev->dev, &rss_map->indir_qp);
	mlx4_qp_free(mdev->dev, &rss_map->indir_qp);
rss_err:
	for (i = 0; i < good_qps; i++) {
		mlx4_qp_modify(mdev->dev, NULL, rss_map->state[i],
			       MLX4_QP_STATE_RST, NULL, 0, 0, &rss_map->qps[i]);
		mlx4_qp_remove(mdev->dev, &rss_map->qps[i]);
		mlx4_qp_free(mdev->dev, &rss_map->qps[i]);
	}
	mlx4_qp_release_range(mdev->dev, rss_map->base_qpn, priv->rx_ring_num);
	return err;
}

void mlx4_en_release_rss_steer(struct mlx4_en_priv *priv)
{
	struct mlx4_en_dev *mdev = priv->mdev;
	struct mlx4_en_rss_map *rss_map = &priv->rss_map;
	int i;

	mlx4_qp_modify(mdev->dev, NULL, rss_map->indir_state,
		       MLX4_QP_STATE_RST, NULL, 0, 0, &rss_map->indir_qp);
	mlx4_qp_remove(mdev->dev, &rss_map->indir_qp);
	mlx4_qp_free(mdev->dev, &rss_map->indir_qp);
	rss_map->indir_qp.qpn = 0;

	for (i = 0; i < priv->rx_ring_num; i++) {
		mlx4_qp_modify(mdev->dev, NULL, rss_map->state[i],
			       MLX4_QP_STATE_RST, NULL, 0, 0, &rss_map->qps[i]);
		mlx4_qp_remove(mdev->dev, &rss_map->qps[i]);
		mlx4_qp_free(mdev->dev, &rss_map->qps[i]);
	}
	mlx4_qp_release_range(mdev->dev, rss_map->base_qpn, priv->rx_ring_num);
}
