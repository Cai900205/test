/**
 \file fib.c
 \brief FIB table lookup functions
 */
/*
 * Copyright (C) 2012 Freescale Semiconductor, Inc.
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

#include "hash.h"
#include "fib.h"
#include <usdpaa/dma_mem.h>
#include <netinet/ip.h>
#include <ppac_interface.h>
#include <ppac.h>
#include "net/neigh.h"

u8 fib_tree_level[FIBTBL_MAX_LEVEL] = {
	FIBTBL_1STLVL_BITS,
	FIBTBL_2NDLVL_BITS,
	FIBTBL_3RDLVL_BITS,
	FIBTBL_4THLVL_BITS,
	FIBTBL_5THLVL_BITS
};

struct nh_entry {
	struct neigh_t *neighbour;
	uint32_t gwaddr;
	uint16_t port;
	uint16_t refcnt;
	/* reference count - many fib_entries can refer to one same nh_entry.*/
	nh_action_t action;
} __attribute__((packed));

struct fib_entdata {
	struct fib_entdata *parent;
	/*always pointing to the parent data node (NULL for no parent)*/
	struct nh_entry *nh;
	uint16_t refcnt;
	u8 mask;
} __attribute__((packed));

struct fib_entry {
	union {
		struct fib_entry *next;
		struct fib_entdata *data;
	} u;
#define unext u.next
#define udata u.data
#define umask u.data->mask
#define unh u.data->nh
#define uparent u.data->parent
#define urefcnt u.data->refcnt

	u8 end:1;			/* with next level fib_entry or not,
				if set the 'u'->nh will point to an nh_entry.*/
	u8 valid:1;
} __attribute__((packed));

#define FIB_ENTRY_IS_END(p)	((p)->end != 0)
#define FIB_ENTRY_SET_END(p)	((p)->end = 1)
#define FIB_ENTRY_CLR_END(p)	((p)->end = 0)

#define FIB_ENTRY_IS_VALID(p)	((p)->valid != 0)
#define FIB_ENTRY_SET_VALID(p)	((p)->valid = 1)
#define FIB_ENTRY_CLR_VALID(p)	((p)->valid = 0)

#define FIB_ENTRY_GET_MASK(p)	((p)->umask)

struct fib_entry *fib_table;
	/* 16b+4b+4b+4b+4b total 5 grades lookup for IPv4 LPM */
static uint32_t *fib_index;
	/* only index for those 4b sub-table-entries, not the 16b sub-table */
static uint32_t fibidx_size;
struct nh_entry *nh_pool;
uint32_t *nh_index;		/* index for all the nh_pool entries */
uint32_t nhidx_size;
struct fib_entdata *fib_datatbl;
	/* pool for data nodes of nexthop & maskbits info. for fib entries */
uint32_t *data_index;		/*entry data index pool */
uint32_t dataidx_size;		/*entry data index pool size in uint32_t unit*/

struct nh_entry nh_drop, nh_trap;
struct fib_entdata *fib_default;

static struct nh_entry *fib_get_nhentry(struct nh_entry *pool);
static int fib_put_nhentry(struct nh_entry *pool, struct nh_entry *nh);
static int fib_rtadd_subentry(struct fib_entry *subtbl, uint32_t net,
			      struct fib_entdata *data, u8 maskbits,
			      u8 currbits, u8 sublevel, u8 bitoff);

/*
 * Init FIB trie table, entrydata table, nexthop table structures.
 * And the indices of those tables.
 */
int fib_init(void)
{
	pr_info("IPv4 FIB table init now... ");

	fib_table = __dma_mem_memalign(L1_CACHE_BYTES,
			sizeof(*fib_table) * FIB_TABLE_SIZE);
	if (unlikely(!fib_table)) {
		pr_err("fib_init error: No memory.");
		return -ENOMEM;
	}
	memset((char *)fib_table, 0, sizeof(*fib_table) * FIB_TABLE_SIZE);

	fibidx_size =
	    (FIB_TABLE_SIZE - FIBTBL_1STLVL_SIZE) / FIBTBL_OTHLVL_SIZE / 32;
	fib_index = __dma_mem_memalign(L1_CACHE_BYTES,
			sizeof(uint32_t) * fibidx_size);
	if (unlikely(!fib_index)) {
		pr_err("fib_init error: No memory.");
		__dma_mem_free(fib_table);
		fib_table = NULL;
		return -ENOMEM;
	}
	memset((char *)fib_index, 0xff, sizeof(uint32_t) * fibidx_size);

	nh_pool = __dma_mem_memalign(L1_CACHE_BYTES,
			sizeof(*nh_pool) * FIB_NHPOOL_SIZE);
	if (unlikely(!nh_pool)) {
		pr_err("fib_init error: No memory.");
		__dma_mem_free(fib_index);
		__dma_mem_free(fib_table);
		fib_table = NULL;
		return -ENOMEM;
	}
	memset((char *)nh_pool, 0, sizeof(*nh_pool) * FIB_NHPOOL_SIZE);

	nhidx_size = FIB_NHPOOL_SIZE / 32;
	nh_index = __dma_mem_memalign(L1_CACHE_BYTES,
			sizeof(uint32_t) * nhidx_size);
	if (unlikely(!nh_index)) {
		pr_err("fib_init error: No memory.");
		__dma_mem_free(nh_pool);
		__dma_mem_free(fib_index);
		__dma_mem_free(fib_table);
		nh_pool = NULL;
		fib_table = NULL;
		return -ENOMEM;
	}
	memset((char *)nh_index, 0xff, sizeof(uint32_t) * nhidx_size);

	fib_datatbl = __dma_mem_memalign(L1_CACHE_BYTES,
			sizeof(*fib_datatbl) * FIB_DPOOL_SIZE);
	if (unlikely(!fib_datatbl)) {
		pr_err("fib_init error: No memory.");
		__dma_mem_free(nh_index);
		__dma_mem_free(nh_pool);
		__dma_mem_free(fib_index);
		__dma_mem_free(fib_table);
		nh_pool = NULL;
		fib_table = NULL;
		return -ENOMEM;
	}
	memset((char *)fib_datatbl, 0, sizeof(*fib_datatbl) * FIB_DPOOL_SIZE);

	dataidx_size = FIB_DPOOL_SIZE / 32;
	data_index = __dma_mem_memalign(L1_CACHE_BYTES,
			sizeof(uint32_t) * dataidx_size);
	if (unlikely(!data_index)) {
		pr_err("fib_init error: No memory.");
		__dma_mem_free(fib_datatbl);
		__dma_mem_free(nh_index);
		__dma_mem_free(nh_pool);
		__dma_mem_free(fib_index);
		__dma_mem_free(fib_table);
		nh_pool = NULL;
		fib_table = NULL;
		fib_datatbl = NULL;
		return -ENOMEM;
	}
	memset((char *)data_index, 0xff, sizeof(uint32_t) * dataidx_size);

	/* init the static nexthop for DROP & TRAP */
	memset((char *)&nh_drop, 0, sizeof(nh_drop));
	nh_drop.action = NH_DROP;
	memset((char *)&nh_trap, 0, sizeof(nh_trap));
	nh_trap.action = NH_RECV;

	pr_info("Done!");

	return 0;
}

static struct fib_entdata *fib_get_datanode(struct fib_entdata *pl)
{
	struct fib_entdata *p;

	p = (struct fib_entdata *)idx_getnode((uint32_t) pl, sizeof(*p),
					      data_index, dataidx_size, 1);

	return p;
}

static int fib_put_datanode(struct fib_entdata *pl, struct fib_entdata *node)
{
	if (--node->refcnt)
		return 0;

	/* before release the 'node', decrease 'node->parent'
					& nexthop refcnt first */
	if (node->parent)
		fib_put_datanode(pl, node->parent);

	fib_put_nhentry(nh_pool, node->nh);

	return idx_putnode((uint32_t) pl, FIB_DPOOL_SIZE, sizeof(*node),
			   data_index, (uint32_t) node, 1);
}

/* get a new sub-table block (16 entries) from the freepool to use.
 * clear the according bit of indexpool to indicate the usage of that node.
 */
static struct fib_entry *fibtab_getsubtbl(struct fib_entry *pl, u8 level)
{
	uint32_t n;
	struct fib_entry *p;

	n = 1 << fib_tree_level[level];
	p = (struct fib_entry *)idx_getnode((uint32_t) pl, sizeof(*p),
					    fib_index, fibidx_size, n);

	return p;
}

static int fib_link_parent(struct fib_entdata *curr, struct fib_entdata *p)
{
	struct fib_entdata *p1 = curr, *p2 = curr->parent;

	while (p2) {
		if (p2->mask == p->mask)
			return 0;
		if (p2->mask < p->mask) {
			/* should insert 'p' between 'p1' and 'p2' */
			if (!p->parent) {
				p->parent = p2;
				p2->refcnt++;
			} else if (p->parent != p2) {
				/* if 'p' has parent yet, it's parent must be
					same with 'p2' (check for safe)*/
				pr_info
				    ("fib_link_parent error: fib_entdata list \
					corrupted XXX. (parent: 0x%x mask: %d \
							 p2:0x%x p2-mask: %d\n",
				     (unsigned int)p->parent, p->parent->mask,
						(unsigned int)p2, p2->mask);
				return -1;
			}
			p1->parent = p;
			p->refcnt++;
			fib_put_datanode(fib_datatbl, p2);
			return 0;
		}
		p1 = p2;
		p2 = p2->parent;
	};

	/* now 'p' should be the topest level node of this parent tree */
	if ((p2 != curr->parent) && p->parent) {
		pr_info
		    ("fib_link_parent error: fib_entdata tree corrupted XXX. \
				(the topest node should have no parent)\n");
		return -1;
	}
	p1->parent = p;
	p->refcnt++;

	return 0;
}

/* when rtupd_subentry is called, the maskbits must have been smaller than
				current level subtable mask (b2+bitoff) */
static int fib_rtupd_subentry(struct fib_entry *subtbl,
				struct fib_entdata *data, uint32_t maskbits,
				u8 sublevel)
{
	int i, n;
	struct fib_entry *p;

	if (unlikely(sublevel >= FIBTBL_MAX_LEVEL)) {
		pr_err
			("fib_rtupd_subentry error: invalid sublevel \
				param - %d.", sublevel);
		return -EINVAL;
	}

	n = (1 << fib_tree_level[sublevel]);
	p = subtbl;
	for (i = 0; i < n; i++, p++) {
		if (!FIB_ENTRY_IS_VALID(p)) {
			p->udata = data;
			data->refcnt++;
			FIB_ENTRY_SET_END(p);
			FIB_ENTRY_SET_VALID(p);
			continue;
		}
		if (!FIB_ENTRY_IS_END(p)) {
			fib_rtupd_subentry(p->unext, data, maskbits,
							sublevel+1);
			continue;
		}
		if (FIB_ENTRY_GET_MASK(p) > maskbits) {
			fib_link_parent(p->udata, data);
			continue;
		}

		/* need update the entry then */
		if (!data->parent) {
			data->parent = p->udata;
			p->udata->refcnt++;
		} else if (unlikely(data->parent != p->udata)) {
			pr_err("fib_entdata tree corrupted. XXX");
			return -1;
		}
		fib_put_datanode(fib_datatbl, p->udata);
		p->udata = data;
		data->refcnt++;
	}

	return 0;
}

static int fib_rtnew_subentry(struct fib_entry *parentent, uint32_t net,
			      struct fib_entdata *data, u8 maskbits,
			      u8 currbits, u8 sublevel, u8 bitoff)
{
	int ret = 0;
	struct fib_entry *p;

	p = fibtab_getsubtbl(&fib_table[FIBTBL_1STLVL_SIZE], sublevel);
	if (unlikely(!p)) {
		pr_err("fib_rtnew_subentry error: no memory.");
		return -ENOMEM;
	}

	ret =
	    fib_rtadd_subentry(p, net, data, maskbits, currbits, sublevel,
			       bitoff);
	if (unlikely(ret))
		return ret;

	if (FIB_ENTRY_IS_VALID(parentent)) {
		/* if parentent is valid (should also be 'end'),
						do update subtable */
		fib_rtupd_subentry(p, parentent->udata, parentent->umask,
								 sublevel);
		fib_put_datanode(fib_datatbl, parentent->udata);
		FIB_ENTRY_CLR_END(parentent);
	}

	parentent->unext = p;
	FIB_ENTRY_SET_VALID(parentent);

	return 0;
}

static int fib_rtadd_subentry(struct fib_entry *subtbl, uint32_t net,
			      struct fib_entdata *data, u8 maskbits,
			      u8 currbits, u8 sublevel, u8 bitoff)
{
	int ret = 0;
	u8 b1 = bitoff, b2;
	uint32_t begin, end;
	struct fib_entry *p;

	if (unlikely(sublevel >= FIBTBL_MAX_LEVEL)) {
		pr_err
		    ("fib_rtadd_subentry error: invalid sublevel param - %d.",
		     sublevel);
		return -EINVAL;
	}

	b2 = fib_tree_level[sublevel];
	begin = (net << b1) >> (32 - b2);
	if (currbits >= b2)
		end = begin + 1;
	else
		end = ((begin >> (b2 - currbits)) + 1) << (b2 - currbits);
	p = &subtbl[begin];
	for (; begin < end; begin++, p++) {
		if (!FIB_ENTRY_IS_VALID(p)) {
			if (b1 + b2 < maskbits) {
				ret =
				    fib_rtnew_subentry(p, net, data, maskbits,
						       currbits - b2,
						       sublevel + 1, b1 + b2);
				if (unlikely(ret))
					return ret;
			} else {
				p->udata = data;
				data->refcnt++;
				FIB_ENTRY_SET_END(p);
				FIB_ENTRY_SET_VALID(p);
			}
			continue;
		}
		/* valid then check the 'end' flag .. */
		if (!FIB_ENTRY_IS_END(p)) {
			if (b1 + b2 < maskbits) {
				ret =
				    fib_rtadd_subentry(p->unext, net, data,
						       maskbits, currbits - b2,
						       sublevel + 1, b1 + b2);
				if (unlikely(ret))
					return ret;
			} else {
				ret = fib_rtupd_subentry(p->unext, data,
						maskbits, sublevel + 1);
				if (unlikely(ret))
					return ret;
			}
			continue;
		}
		/* valid and end then .. */
		if (FIB_ENTRY_GET_MASK(p) > maskbits) {
			fib_link_parent(p->udata, data);
			continue;
		}
		if (unlikely(FIB_ENTRY_GET_MASK(p) == maskbits)) {
			/*pr_err("fib_rtadd_subentry failed - subnet
				0x%x/%d exist already. (UPDATE=REMOVE+ADD)",
							net, maskbits);*/
			return -EEXIST;
		}

		/* need update the entry then ... */
		if (b1 + b2 >= maskbits) {
			if (!data->parent) {
				data->parent = p->udata;
				p->udata->refcnt++;
			} else if (unlikely(data->parent != p->udata)) {
				pr_err("fib_entdata tree corrupted. XXX");
				return -1;
			}
			fib_put_datanode(fib_datatbl, p->udata);
			p->udata = data;
			data->refcnt++;
			continue;
		}
		/* need to allocate new subtable now ... */
		ret =
		    fib_rtnew_subentry(p, net, data, maskbits, currbits - b2,
				       sublevel + 1, b1 + b2);
		if (unlikely(ret))
			return ret;
	}

	return 0;
}

static struct nh_entry *fib_get_nhentry(struct nh_entry *pl)
{
	struct nh_entry *p;

	p = (struct nh_entry *)idx_getnode((uint32_t) pl, sizeof(*p),
					   nh_index, nhidx_size, 1);

	return p;
}

/* put the not-used sub-table block (16 entries) back to freepool and clear it.
 * set the according bit of indexpool to indicate that node is available again.
 */
static int fib_put_nhentry(struct nh_entry *pl, struct nh_entry *nh)
{
	if (unlikely(nh->action != NH_FWD)) {
		nh->refcnt--;
		return 0;
	}

	if (--nh->refcnt)
		return 0;

	return idx_putnode((uint32_t) pl, FIB_NHPOOL_SIZE, sizeof(*nh),
			   nh_index, (uint32_t) nh, 1);
}

/*
 * Add a new network route entry to the FIB table.
 * For maskbits < 16, expand to multiple level-1 trie-entries.
 * If there have already subnet route entries in the FIB table,
 * just check and modify their parent pointers.
 *
 * Note: multiple trie-entries might points to same one entry-data
 * entry (refcnt indicates that) and
 *	multiple entry-data entries might points to same one nexthop entry
 *	(not shared yet. Needs optimizing here XXX.)
 * Also, htonl/ntohl functions should be invoked here which is skipped due
 * to PPC big Endian reason.
 */
int fib_add_route(uint32_t ipaddr, u8 maskbits, uint32_t gw, u16 port,
		  nh_action_t act, struct neigh_t *n)
{
	int ret;
	uint32_t netaddr = ipaddr & (0xffffffff << (32 - maskbits));
			/* should use ntohl((htonl(ipaddr)&xxxx)) here. */
	struct nh_entry *nh = NULL;
	struct fib_entdata *dp;

	switch (act) {
	case NH_FWD:
		{
			/* should first check for existing nexthop entry to
				reuse here XXX - later will add hash table
				to manage nexthops */
			nh = fib_get_nhentry(nh_pool);
			if (unlikely(!nh)) {
				printf("NO NH MEM\n");
				return -ENOMEM;
			}

			nh->action = act;
			nh->gwaddr = gw;
			nh->port = port;
			nh->neighbour = n;
		}
		break;
	case NH_DROP:
		{
			nh = &nh_drop;
		}
		break;
	case NH_RECV:
		{
			nh = &nh_trap;
		}
		break;
	default:
		pr_err("fib_add_route error: invalid action param - %d.",
			  act);
		return -EINVAL;
	}

	if (unlikely(maskbits == 0)) {
		nh->refcnt++;	/* add the refcnt first in order for
					fib_put_nhentry at exceptions */
		/* add default route */
		if (unlikely(ipaddr)) {
			pr_err
			    ("fib_add_route error: default route should be \
					 NET<0.0.0.0>/MASK<0.0.0.0>	\
					 instead of NET<0x%x>/0.",
			     ipaddr);
			fib_put_nhentry(nh_pool, nh);
			return -EINVAL;
		}
		if (fib_default) {
			/* update directly */
			fib_put_nhentry(nh_pool, fib_default->nh);
				/* release the old nexthop entry */
		} else {
			fib_default = fib_get_datanode(fib_datatbl);
			if (unlikely(!fib_default)) {
				pr_err
				    ("fib_add_route error: get default route \
						data node failed - NOMEM.");
				fib_put_nhentry(nh_pool, nh);
				return -ENOMEM;
			}
			fib_default->refcnt++;	/*after getting a new node, all
			the data fields in the node are inited to 0 already. */
		}
		fib_default->nh = nh;
		/* for default route, we choose not to set each sub-routes'
					parent pointer. Just leave them NULL.*/
		return 0;
	}

	nh->refcnt++;		/* add the refcnt first in order for
					fib_put_nhentry at exceptions */
	dp = fib_get_datanode(fib_datatbl);
	if (unlikely(!dp)) {
		pr_err
		    ("fib_add_route error: get data node for	\
				network %x/%d failed - NOMEM.",
		     netaddr, maskbits);
		fib_put_nhentry(nh_pool, nh);
		return -ENOMEM;
	}
	dp->mask = maskbits;
	dp->nh = nh;

	ret =
	    fib_rtadd_subentry(fib_table, netaddr, dp, maskbits, maskbits, 0,
			       0);
	if (unlikely(ret)) {
		if (ret != -EEXIST)
			pr_err("fib_add_route %x/%d failed - errno %d.",
				  netaddr, maskbits, ret);
		fib_put_datanode(fib_datatbl, dp);
		/* data node might not be turely released to pool due to
						refcnt limit. leak memory XXX*/
	}
	return ret;
}

/* do basic ip header check here */
int ip_header_check(struct iphdr *iph)
{
	if (unlikely(iph->version != 4)) {
		pr_err("ip_header_check error: invalid version - %d.",
			  iph->version);
		return -1;
	}
	if (unlikely(iph->ihl < 5)) {
		pr_err("ip_header_check error: header too short - %d.",
			  iph->ihl);
		return -1;
	}
	if (unlikely(iph->tot_len < 4 * iph->ihl)) {
		pr_err("ip_header_check error: total length too short - %u.",
			  iph->tot_len);
		return -1;
	}
	if (unlikely((iph->saddr == 0) || (iph->saddr >> 24) >= 224)) {
		pr_err
		    ("ip_header_check error: invalid source ip address - 0x%x.",
		     iph->saddr);
		return -1;
	}

	return 0;
}

/*
 * search the FBI tries (totally 5 levels) with packet's DIP.
 * if found, the destination 'port' is put to fmb->devNo and
 * the 'gateway IP' is put to 'gwaddr'.
 *
 * Note: here only FWD action is valid. (test code)
 */
int ip_route_lookup(uint32_t daddr, uint32_t *gwaddr,
			struct annotations_t *notes)
{
	int i, idx;
	u8 b1 = 0;
	uint32_t ipaddr;
	struct fib_entry *p = fib_table;

	ipaddr = daddr;
	for (i = 0; i < FIBTBL_MAX_LEVEL; i++) {
		idx = (ipaddr << b1) >> (32 - fib_tree_level[i]);
		p += idx;
		if (!FIB_ENTRY_IS_VALID(p)) {
			/* check the DEFAULT route */
			if (!fib_default)
				return FIB_RET_MISS;
			if (unlikely(fib_default->nh->action != NH_FWD)) {
				pr_debug("fib_default->nh->action:%d",
					  fib_default->nh->action);
				return -fib_default->nh->action;
			}

			*gwaddr = fib_default->nh->gwaddr;
			notes->neighbor = fib_default->nh->neighbour;
			return 0;
		}
		if (FIB_ENTRY_IS_END(p)) {
			if (unlikely(p->unh->action != NH_FWD))
				return -p->unh->action;
			*gwaddr = p->unh->gwaddr;
			notes->neighbor = p->unh->neighbour;
			return 0;
		}
		p = p->unext;
		b1 += fib_tree_level[i];
	}
	/* never reach here */
	return FIB_RET_MISS;
}

/* below functions are used for debugging */

static int fib_subtbl_dump(struct fib_entry *subtbl, uint32_t size, int numtab)
{
	int i;
	int n = numtab;
	struct fib_entry *p = subtbl;

	for (i = 0; i < size; i++, p++) {
		if (!FIB_ENTRY_IS_VALID(p))
			continue;
		n = numtab;
		while (n > 0) {
			pr_info("\t");
			n--;
		}
		pr_info("0x%x ", i);
		if (FIB_ENTRY_IS_END(p)) {
			pr_info("/ %d --> ACT: %d GW: 0x%x PORT: %d.\n",
				 p->umask, p->unh->action, p->unh->gwaddr,
				 p->unh->port);
		} else {
			pr_info("\n");
			fib_subtbl_dump(p->unext, 16, numtab + 1);
		}
	}
	return 0;
}

int fib_table_dump(void)
{
	if (!fib_table) {
		pr_err("fib_table_dump: FIB table not init-ed yet.");
		return -1;
	}

	pr_info("-------------FIB TABLE DUMP-------------------");

	pr_info
	    ("fib_table: 0x%p(size: 0x%x)\n fib_index: 0x%p(0x%x)\n	\
		 nh_pool: 0x%p(0x%x)\n nh_index: 0x%p(0x%x)\n	\
		 datatbl: 0x%p(0x%x) dataindex: 0x%p(0x%x)",
	     fib_table, sizeof(*fib_table) * FIB_TABLE_SIZE, fib_index,
	     fibidx_size, nh_pool, sizeof(*nh_pool) * FIB_NHPOOL_SIZE,
	     nh_index, nhidx_size, fib_datatbl,
	     sizeof(*fib_datatbl) * FIB_DPOOL_SIZE,
	     data_index, dataidx_size);

	fib_subtbl_dump(fib_table, 65536, 0);

	if (fib_default) {
		pr_info("---------DEFAULT ROUTE----------");

		pr_info("0.0.0.0 / 0 --> ACT: %d GW: 0x%x PORT: %d.",
			 fib_default->nh->action, fib_default->nh->gwaddr,
			 fib_default->nh->port);
	}
	pr_info("-------------FIB TABLE END--------------------");

	return 0;
}

int fib_entdata_dump(void)
{
	return 0;
}
