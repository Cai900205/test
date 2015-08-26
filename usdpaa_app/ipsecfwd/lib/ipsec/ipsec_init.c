/*
 * Copyright (C) 2011 - 2012 Freescale Semiconductor, Inc.
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

#include "ipsec/ipsec_init.h"

#include "ipsec/ipsec_encap.h"
#include "ipsec/ipsec_decap.h"

#define ENTRIES 1024
#define ENTRIES_POOL_SIZE (ENTRIES << 1)

int32_t ipsec_config_create(void)
{
	struct ipsec_tunnel_config_t *config;
	uint32_t entries;

	config = __dma_mem_memalign(L1_CACHE_BYTES,
				    sizeof(struct ipsec_tunnel_config_t));
	if (config == NULL) {
		fprintf(stderr, "error: %s: dma_mem_memalign failed\n",
			__func__);
		return -ENOMEM;
	}

	config->free_entries =
	    mem_cache_create(sizeof(struct ipsec_tunnel_config_entry_t),
			     ENTRIES_POOL_SIZE);
	if (config->free_entries == NULL) {
		fprintf(stderr, "error: %s: mem_cache_create failed\n",
			__func__);
		__dma_mem_free(config);
		return -ENOMEM;
	}

	entries = mem_cache_refill(config->free_entries, ENTRIES_POOL_SIZE);
	if (entries != ENTRIES_POOL_SIZE) {
		fprintf(stderr, "error: %s: mem_cache_refill failed\n",
			__func__);
		/* TODO: This doesn't seem the reverse of mem_cache_create */
		free(config->free_entries);
		__dma_mem_free(config);
		return -ENOMEM;
	}

	g_ipsec_tunnel_config = config;
	return 0;
}

void ipsec_config_delete(struct ipsec_tunnel_config_t *config)
{
/* TBD */
#if 0
	uint32_t entries;

	entries = mem_cache_refill(config->free_entries, ENTRIES_POOL_SIZE);
	if (entries != ENTRIES_POOL_SIZE)
		return NULL;

	config->free_entries =
	    mem_cache_create(sizeof(struct ipsec_tunnel_config_entry_t),
			     ENTRIES_POOL_SIZE);
	if (config->free_entries == NULL) {
		/* TODO: This doesn't seem the reverse of mem_cache_create */
		free(config);
		return NULL;
	}
#endif
	/* TODO: config->free_entries needs to be freed, too */
	__dma_mem_free(config);
}

struct ipsec_tunnel_config_entry_t *ipsec_config_entry_create(
		struct ipsec_tunnel_config_t *config)
{
	return mem_cache_alloc(config->free_entries);
}

/**
 \brief Create a new Security association entry
 \param[out] ipsec_info contains SA parameters - SA Id, selector info,
	      encryption algo info, authentication algo info
 \return Integer status
 */
int32_t ipsecfwd_create_sa(struct app_ctrl_ipsec_info *ipsec_info,
		struct ipsec_stack_t *ipsec_stack)
{

	struct ipsec_tunnel_config_entry_t *ipsec_tunnel_config_entry;
	struct ipsec_tunnel_config_t *ipsec_tunnel_config =
		g_ipsec_tunnel_config;
	uint32_t *next_hop_addr;
	uint32_t encryption_mode;
	int32_t ret;

	ipsec_tunnel_config_entry =
	    ipsec_config_entry_create(ipsec_tunnel_config);

	if (!ipsec_tunnel_config_entry) {
		fprintf(stderr, "error: %s: Unable to allocate tunnel"
			" config entry\n", __func__);
		return -ENOMEM;
	}

	encryption_mode =
	    (IPSEC_DIR_OUT == ipsec_info->id.dir) ? ENCRYPT : DECRYPT;
	ipsec_tunnel_config_entry->src_ip = ipsec_info->sel.saddr;
	ipsec_tunnel_config_entry->dst_ip = ipsec_info->sel.daddr;
	ipsec_tunnel_config_entry->tunnel_src_ip_addr = ipsec_info->id.saddr;
	ipsec_tunnel_config_entry->tunnel_dst_ip_addr = ipsec_info->id.daddr;
	ipsec_tunnel_config_entry->defgw = ipsec_info->id.defgw;
	ipsec_tunnel_config_entry->hb_tunnel = ipsec_info->hb_tunnel;
	ipsec_tunnel_config_entry->ealg = &ipsec_info->ealg;
	ipsec_tunnel_config_entry->aalg = &ipsec_info->aalg;
	ipsec_tunnel_config_entry->tunnel_id = g_tunnel_id;
	ipsec_tunnel_config_entry->spi = ipsec_info->id.spi;
	ipsec_tunnel_config_entry->is_esn = ipsec_info->id.is_esn;

	if (encryption_mode == ENCRYPT) {
		ipsec_tunnel_config_entry->seq_num =
				ipsec_info->id.seq_num;

		if (ipsec_tunnel_config_entry->defgw == 0)
			next_hop_addr =
				&ipsec_tunnel_config_entry->tunnel_dst_ip_addr;
		else
			next_hop_addr = &ipsec_tunnel_config_entry->defgw;
	} else {
		ipsec_tunnel_config_entry->seq_num =
				ipsec_info->id.seq_num + 1;

		if (ipsec_tunnel_config_entry->defgw == 0)
			next_hop_addr = &ipsec_tunnel_config_entry->dst_ip;
		else
			next_hop_addr = &ipsec_tunnel_config_entry->defgw;
	}
	ret = ipsec_tunnel_create(ipsec_tunnel_config_entry,
				  ipsec_stack, next_hop_addr, encryption_mode);
	if (ret == 0)
		g_tunnel_id++;

	return ret;
}

/**
 \brief Prepare all data structures to make the DUT ready for
	processing IPsec encapsulation.
 \param[in] config Pointer to IPsec config entry
 \param[in] next_hop_addr Next Hop Address
 \param[in] ipsec_stack Ipsec Stack pointer
 \param[in] ealg Encryption Algo Info
 \param[in] aalg Authentication Algo Info
 \return Status
 */
int32_t ipsec_tunnel_encap_init(struct ipsec_tunnel_config_entry_t *config,
				uint32_t *next_hop_addr,
				struct ipsec_stack_t *ipsec_stack)
{
	struct ipsec_tunnel_t *entry;
	bool tunnel_route;
	struct iphdr *outer_ip_hdr;
	uint8_t scope;

	/* initialize tunnel entry and add to tunnel table */
	entry = ipsec_create_tunnel_entry(&(ipsec_stack->itt));
	if (NULL == entry) {
		fprintf(stderr, "error: %s: Unable to Allocate tunnel entry\n",
			__func__);
		return -ENOMEM;
	}

	entry->tunnel_id = config->tunnel_id;
	entry->hb_tunnel = config->hb_tunnel;
	entry->saddr = config->src_ip;
	entry->daddr = config->dst_ip;
	entry->tunnel_saddr = config->tunnel_src_ip_addr;
	entry->tunnel_daddr = config->tunnel_dst_ip_addr;
	entry->spi = config->spi;
	entry->is_esn = config->is_esn;
	entry->seq_num = config->seq_num;
	entry->ealg = config->ealg;
	entry->aalg = config->aalg;
	mutex_init(&entry->tlock);
	pr_debug("SPI Value is %x\n", config->spi);

	if (false == ipsec_add_tunnel_entry(&(ipsec_stack->itt), entry)) {
		ipsec_free_entry(&(ipsec_stack->itt), entry);
		fprintf(stderr, "error: %s: Tunnel Entry Couldn't be added\n",
			__func__);
		return -EINVAL;
	}
	/* create outer ip, initialize fq - pdb, fq contexts etc. */

	outer_ip_hdr = __dma_mem_memalign(L1_CACHE_BYTES, sizeof(struct iphdr));
	if (outer_ip_hdr == NULL) {
		fprintf(stderr, "error: %s: Unable to allocate memory for"
			" outer ip header\n", __func__);
		return -ENOMEM;
	}

	ipsec_build_outer_ip_hdr(outer_ip_hdr, &config->tunnel_src_ip_addr,
				 &config->tunnel_dst_ip_addr);
	ipsec_encap_init(entry, outer_ip_hdr);
	/* populate route cache */
	tunnel_route = true;
	scope = ROUTE_SCOPE_ENCAP;

	populate_route_cache(config->src_ip, config->dst_ip,
			     *next_hop_addr, ipsec_stack, tunnel_route, entry,
			     false, scope);
	tunnel_route = false;
	scope = ROUTE_SCOPE_GLOBAL;
	/* entry for each remote to remote host-pair */
	populate_route_cache(config->tunnel_src_ip_addr,
			     config->tunnel_dst_ip_addr,
			     *next_hop_addr, ipsec_stack, tunnel_route, entry,
			     false, scope);
	__dma_mem_free(outer_ip_hdr);
	return 0;
}

/**
 \brief Prepare all data structures to make the DUT ready for
	processing IPsec decapsulation.
 \param[in] config Pointer to IPsec config entry
 \param[in] next_hop_addr Next Hop Address
 \param[in] ipsec_stack Ipsec Stack pointer
 \param[in] ealg Encryption Algo Info
 \param[in] aalg Authentication Algo Info
 \return Status
 */
int32_t ipsec_tunnel_decap_init(struct ipsec_tunnel_config_entry_t *config,
				uint32_t *next_hop_addr,
				struct ipsec_stack_t *ipsec_stack)
{
	struct ipsec_tunnel_t *entry;
	bool tunnel_route;
	uint8_t scope;

	/* initialize tunnel entry and add to tunnel table */
	entry = ipsec_create_tunnel_entry(&(ipsec_stack->itt));
	if (NULL == entry) {
		fprintf(stderr, "error: %s: Unable to Allocate tunnel entry\n",
			__func__);
		return -ENOMEM;
	}
	entry->tunnel_id = config->tunnel_id;
	entry->hb_tunnel = config->hb_tunnel;
	entry->saddr = config->src_ip;
	entry->daddr = config->dst_ip;
	entry->tunnel_saddr = config->tunnel_src_ip_addr;
	entry->tunnel_daddr = config->tunnel_dst_ip_addr;
	entry->spi = config->spi;
	entry->is_esn = config->is_esn;
	entry->seq_num = config->seq_num;
	entry->ealg = config->ealg;
	entry->aalg = config->aalg;
	mutex_init(&entry->tlock);
	pr_debug("Added Decap Tunnel src = %x, dst = %x, spi = %x\n",
		 config->tunnel_src_ip_addr, config->tunnel_dst_ip_addr,
		 config->spi);

	if (false == ipsec_add_tunnel_entry(&(ipsec_stack->itt), entry)) {
		ipsec_free_entry(&(ipsec_stack->itt), entry);
		fprintf(stderr, "error: %s: Tunnel Entry Couldn't be added\n",
			__func__);
		return -ENOMEM;
	}

	/* create the pdb based on tunnel sa information */
	ipsec_decap_init(entry);

	/* populate route cache */
	tunnel_route = true;
	scope = ROUTE_SCOPE_DECAP;
	/* entry for each remote to remote host-pair */
	populate_route_cache(config->tunnel_src_ip_addr,
			     config->tunnel_dst_ip_addr, *next_hop_addr,
			     ipsec_stack, tunnel_route, entry, false, scope);

	/* populate route cache */
	tunnel_route = false;
	/* entry for each remote to remote host-pair */
	populate_route_cache(config->src_ip, config->dst_ip, *next_hop_addr,
			     ipsec_stack, tunnel_route, entry, true, scope);
	return 0;
}
int32_t ipsec_tunnel_create(struct ipsec_tunnel_config_entry_t *config,
			    struct ipsec_stack_t *ipsec_stack,
			    uint32_t *next_hop_addr, uint32_t mode)
{
	config->aalg->alg_key_ptr = g_split_key;
	config->aalg->alg_key_len = 40;

	if (mode == ENCRYPT) {
		return ipsec_tunnel_encap_init(config, next_hop_addr,
					       ipsec_stack);
	}
	return ipsec_tunnel_decap_init(config, next_hop_addr, ipsec_stack);
}

/* This routine has been modified to support ipsec traffic */
void populate_route_cache(uint32_t src_ip,
			  uint32_t dst_ip,
			  uint32_t hop_ip,
			  struct ipsec_stack_t *ipsec_stack,
			  bool tunnel_route, struct ipsec_tunnel_t *tunnel,
			  bool add_entry, uint8_t scope)
{
	struct rc_entry_t *entry;
	struct rt_dest_t *dest;

	dest = rt_dest_alloc(&(ipsec_stack->ip_stack.rt));
	if (NULL == dest) {
		fprintf(stderr, "error: %s: Unable to allocate route table"
			" entry\n", __func__);
		return;
	}

	dest->next = NULL;
	pr_debug("Creating Route Cache for src = %x and destination %x,"
			"hop_ip = %x\n", src_ip, dst_ip, hop_ip);
	dest->scope = scope;

	if (tunnel_route == true) {
		dest->dev = NULL;
		dest->neighbor = NULL;
		dest->tunnel = tunnel;	/* Required for encap tunnel */
		if (scope == ROUTE_SCOPE_DECAP)
			return;
	} else {
		dest->neighbor =
		    neigh_lookup(&(ipsec_stack->ip_stack.arp_table),
				 hop_ip,
				 ipsec_stack->ip_stack.arp_table.proto_len);
		if (dest->neighbor == NULL) {
			fprintf(stderr, "error: %s: Neighbor not found\n",
				__func__);
			return;
		}
		dest->dev = dest->neighbor->dev;
		tunnel->dest = dest;
		if (add_entry == false)
			return;
	}

	entry = rc_create_entry(ipsec_stack->ip_stack.rc);
	if (NULL == entry) {
		fprintf(stderr, "error: %s: No free RC entry available\n",
			__func__);
		rt_dest_try_free(&(ipsec_stack->ip_stack.rt), dest);
		return;
	}
	entry->saddr = src_ip;
	entry->daddr = dst_ip;

#ifdef STATS_TBD
	entry->stats = stats_memalign(L1_CACHE_BYTES,
			sizeof(struct rc_entry_statistics_t));
	if (NULL == entry->stats) {
		fprintf(stderr, "error: %s: Unable to allocate memory for"
			" route stats\n", __func__);
		rt_dest_try_free(&(ipsec_stack->ip_stack.rt), dest);
		return;
	}
	memset(entry->stats, 0, sizeof(struct rc_entry_statistics_t));
#endif

	refcount_acquire(dest->refcnt);
	entry->dest = dest;
	entry->tos = 0;
	if (rc_add_update_entry(ipsec_stack->ip_stack.rc, entry) == false) {
		fprintf(stdout, "info: %s: Route cache entry updated\n",
			__func__);
		rc_free_entry(ipsec_stack->ip_stack.rc, entry);
	}
}
