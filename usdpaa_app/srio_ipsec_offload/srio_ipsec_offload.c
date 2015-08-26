/* Copyright (c) 2014 Freescale Semiconductor, Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright
 *	 notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *	 notice, this list of conditions and the following disclaimer in the
 *	 documentation and/or other materials provided with the distribution.
 *     * Neither the name of Freescale Semiconductor nor the
 *	 names of its contributors may be used to endorse or promote products
 *	 derived from this software without specific prior written permission.
 *
 *
 * ALTERNATIVELY, this software may be distributed under the terms of the
 * GNU General Public License ("GPL") as published by the Free Software
 * Foundation, either version 2 of that License or (at your option) any
 * later version.
 *
 * THIS SOFTWARE IS PROVIDED BY Freescale Semiconductor ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL Freescale Semiconductor BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <ppac.h>
#include "ppam_if.h"
#include <ppac_interface.h>
#include <usdpaa/fman.h>
#include <fsl_sec/sec.h>

#include <inttypes.h>
#include <netinet/if_ether.h>
#include <netinet/ip.h>
#include <netinet/ip6.h>
#include <linux/if_vlan.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <signal.h>

#include "fmc.h"
#include "usdpaa/fsl_dpa_ipsec.h"
#include "app_config.h"
#include "app_common.h"
#include <rman_interface.h>
#include <rman_fq_interface.h>
#include <rman_cfg.h>
extern int rman_init();

/* All of the following things could be placed into vtables, declared as
 * "extern", implemented elsewhere, and generally made less hacky if you want.
 *
 * For this example however we want to show how ppac.c can be used without
 * any loss of execution speed due to the code separation between PPAC and an
 * application module. Ie. we want an equivalent compilation and the same
 * execution speed as a standalone implementation that has no "ppac"-style
 * modularity. This is the most awkward way to work but yields the least
 * performance to jumps, dereferences, and other cycle-gobblers. More modular
 * and elegant things can be done, by assuming the risks of some associated
 * indirection, dereferencing, [etc].
 *
 * To achieve this, we declare the FQ handling hooks as inlines, all prior to
 * including ppac.c. The code in ppac.c implements its own FQ handlng
 * callbacks and simply assumes that it can "call" these hooks at the
 * appropriate places within those implementations (so our hooks don't need to
 * be real function entities with well-defined addresses that can be stored). If
 * these "hooks" are in fact macros or inlines, they will be expanded in-place
 * by the pre-compiler or compiler, respectively. Ie. the resulting compilation
 * requires no excess jumping back and forth between generic (ppac.c)
 * packet-handling logic and application-specific (reflector.c) code when
 * processing packets.
 */

/* Override the default command prompt */
const char ppam_prompt[] = "srio_ipsec_offload> ";

/* Ports and fman used */
struct ppam_arguments {
	const char *fm;
	const char *ob_eth;
	const char *ib_oh;
	const char *ib_oh_pre;
	const char *ob_oh_pre;
	const char *ob_oh_post;
	const char *max_sa;
	const char *mtu_pre_enc;
	int inb_pol_check;
	const char *outer_tos;
	int ib_ecn;
	int ob_ecn;
	int ib_loop;
	const char *vif;
	const char *vof;
	const char *vipsec;
};

/* Buffer pools */
static struct bpool {
	int bpid;
	unsigned int num;
	unsigned int size;
} bpool[] = {
	{ -1, DMA_MEM_IPF_NUM, DMA_MEM_IPF_SIZE},
	{ IPR_BPID, DMA_MEM_IPR_NUM, DMA_MEM_IPF_SIZE},
	{ OP_BPID, DMA_MEM_OP_NUM, DMA_MEM_OP_SIZE},
	{ IF_BPID, DMA_MEM_IF_NUM, DMA_MEM_IF_SIZE},
	{ -1, 0, 0 }
};

uint32_t bpool_get_size(uint8_t bpid)
{
    return bpool[bpid].size;
}

struct ppam_arguments ppam_args;
struct app_conf app_conf;
static int dpa_ipsec_id;
static struct fmc_model_t *cmodel;
static pthread_t xfrm_tid, neigh_tid;

static void cleanup_macless_config(char *macless_name)
{
	int ret;
	struct fman_if *__if;
	list_for_each_entry(__if, fman_if_list, node) {
		if ((__if->mac_type != fman_mac_less) ||
		    (__if->macless_info.macless_name != macless_name))
			continue;
		ret = set_mac_addr(macless_name, &__if->macless_info.peer_mac);
		if (ret < 0)
			fprintf(stderr, "Failed to restore %s mac address\n",
				macless_name);
	}
}
static int setup_macless_if_tx(struct ppac_interface *i, uint32_t last_fqid,
			       unsigned int *num_tx_fqs, struct qman_fq *fq,
			       char *macless_name)
{
	struct fman_if *__if;
	int loop;
	uint32_t tx_start = 0;
	uint32_t tx_count = 0;
	struct ether_addr mac;

	memset(&mac, 0, sizeof(mac));

	list_for_each_entry(__if, fman_if_list, node) {
		if (__if->mac_type != fman_mac_less)
			continue;
		get_mac_addr(macless_name, &mac);
		if (!memcmp(&mac.ether_addr_octet,
			    &__if->macless_info.peer_mac.ether_addr_octet,
			    ETH_ALEN)) {
                        strncpy(__if->macless_info.macless_name, macless_name, IFNAMSIZ);
                        __if->macless_info.macless_name[IFNAMSIZ-1] = 0;
			tx_start = __if->macless_info.tx_start;
			tx_count = __if->macless_info.tx_count;
			break;
		}
	}

	if (!tx_start || !tx_count)
		return -ENODEV;

	free(i->tx_fqs);
	i->num_tx_fqs = tx_count + 1;
	i->tx_fqs = malloc(sizeof(*i->tx_fqs) * i->num_tx_fqs);
	if (!i->tx_fqs) {
		__dma_mem_free(i);
		return -ENOMEM;
	}

	fq = &i->tx_fqs[0];
	memset(i->tx_fqs, 0, sizeof(*i->tx_fqs) * i->num_tx_fqs);
	for (loop = 0; loop < i->num_tx_fqs - 1; loop++)
		fq[loop].fqid = tx_start + loop;
	fq[tx_count].fqid = last_fqid;
	*num_tx_fqs = i->num_tx_fqs;

	return 0;
}
/* setup_macless_if_tx has to be called first as the macless_name
 * member is set by it */
static int setup_macless_if_rx(struct ppac_interface *i,
				const char *macless_name)
{
	struct fman_if *__if;
	uint32_t rx_start = 0;
	int ret;
	char fmc_path[64];
	const char *port_type;
	int idx;

	if (strcmp(macless_name, app_conf.vif) &&
	    strcmp(macless_name, app_conf.vof))
		return -ENODEV;

	idx = if_nametoindex(macless_name);
	if (!idx)
		return -ENODEV;
	i->ppam_data.macless_ifindex = idx;

	list_for_each_entry(__if, fman_if_list, node) {
		if (__if->mac_type == fman_mac_less &&
		    __if->macless_info.macless_name &&
		    !strcmp(__if->macless_info.macless_name, macless_name)) {
			rx_start = __if->macless_info.rx_start;
			break;
		}
	}
	if (!rx_start)
		return -ENODEV;

	if (!strcmp(macless_name, app_conf.vif)) {

		memset(fmc_path, 0, sizeof(fmc_path));
		sprintf(fmc_path, "fm%d/port/OFFLINE/%d/ccnode/"
			"ib_post_ip_cc",
			app_conf.fm, app_conf.ib_oh->mac_idx);
		ret = set_cc_miss_fqid(cmodel, fmc_path, rx_start);
		if (ret < 0)
			goto err;
		memset(fmc_path, 0, sizeof(fmc_path));
		sprintf(fmc_path, "fm%d/port/OFFLINE/%d/ccnode/"
			"ib_post_ip6_cc",
			app_conf.fm, app_conf.ib_oh->mac_idx);
		ret = set_cc_miss_fqid(cmodel, fmc_path, rx_start);
		if (ret < 0)
			goto err;

	}
	if (!strcmp(macless_name, app_conf.vof)) {
		/* set fqids for vof PCD */
		memset(fmc_path, 0, sizeof(fmc_path));
		port_type = get_port_type(app_conf.ob_eth);
		sprintf(fmc_path, "fm%d/port/%s/%d/dist/"
			"ob_rx_default_dist",
			app_conf.fm, port_type, app_conf.ob_eth->mac_idx);
		ret = set_dist_base_fqid(cmodel, fmc_path, rx_start);
		if (ret < 0)
			goto err;

		memset(fmc_path, 0, sizeof(fmc_path));
		sprintf(fmc_path, "fm%d/port/OFFLINE/%d/ccnode/"
			"ob_post_ip_cc",
			app_conf.fm, app_conf.ob_oh_post->mac_idx);
		ret = set_cc_miss_fqid(cmodel, fmc_path, rx_start);
		if (ret < 0)
			goto err;

		memset(fmc_path, 0, sizeof(fmc_path));
		sprintf(fmc_path, "fm%d/port/OFFLINE/%d/ccnode/"
			"ob_post_ip6_cc",
			app_conf.fm, app_conf.ob_oh_post->mac_idx);
		ret = set_cc_miss_fqid(cmodel, fmc_path, rx_start);
		if (ret < 0)
			goto err;
	}

	return 0;
err:
	return ret;
}

/* There is no configuration that specifies how many Tx FQs to use
 * per-interface, it's an internal choice for ppac.c and may depend on
 * optimisations, link-speeds, command-line options, etc. Also the Tx FQIDs are
 * dynamically allocated if fqid field of Tx FQ is not changed by this hook,
 * so they're not known until ppac.c has already initialised them.
 * So firstly, the # of Tx FQs is passed in as a parameter
 * here because there's no other place where it could be meaningfully captured.
 * (Note, an interesting alternative would be to have this hook *choose* how
 * many Tx FQs to use!) Secondly, the Tx FQIDs are "notified" to us
 * post-allocation but prior to Rx initialisation. */
static int ppam_interface_init(struct ppam_interface *p,
			       const struct fm_eth_port_cfg *cfg,
			       unsigned int num_tx_fqs,
			       uint32_t *flags __maybe_unused)
{
	int ret;
	struct ppac_interface *i =
		container_of(p, struct ppac_interface, ppam_data);
	struct qman_fq *fq = &i->tx_fqs[0];

	if (app_conf.ob_oh_post == i->port_cfg->fman_if) {
		fq->fqid = OB_OH_POST_TX_FQID;
		*flags |= PPAM_TX_FQ_NO_BUF_DEALLOC;
	}
	if (app_conf.ob_oh_pre == i->port_cfg->fman_if) {
		*flags |= PPAM_TX_FQ_NO_BUF_DEALLOC;
		ret = setup_macless_if_tx(i, OB_OH_PRE_TX_FQID, &num_tx_fqs,
					  fq, app_conf.vipsec);
		if (ret < 0)
			goto err;
		ret = set_mac_addr(app_conf.vipsec,
				   &app_conf.ob_eth->mac_addr);
		if (ret < 0)
			goto err;
	}
	if (app_conf.ib_oh_pre == i->port_cfg->fman_if) {
                fq->fqid = IB_OH_DIST_TX_FQID;
                *flags |= PPAM_TX_FQ_NO_BUF_DEALLOC;
                *flags |= PPAM_TX_FQ_SET_OPCODE11;
	}
        if(app_conf.ob_eth == i->port_cfg->fman_if) {
		ret = setup_macless_if_tx(i, OB_TX_FQID, &num_tx_fqs,
					  fq, app_conf.vof);
		if (ret < 0)
			goto err;
		ret = setup_macless_if_rx(i, app_conf.vof);
		if (ret < 0)
			goto err;
		ret = set_mac_addr(app_conf.vof,
				   &i->port_cfg->fman_if->mac_addr);
		if (ret < 0)
			goto err;
	}
	if (app_conf.ib_oh ==  i->port_cfg->fman_if) {
		fq->fqid = IB_OH_TX_FQID;
	}

	p->num_tx_fqids = num_tx_fqs;
	p->tx_fqids = malloc(p->num_tx_fqids * sizeof(*p->tx_fqids));
	if (!p->tx_fqids)
		return -ENOMEM;
	return 0;
err:
	return ret;
}
static void ppam_interface_finish(struct ppam_interface *p)
{
	free(p->tx_fqids);
}
static void ppam_interface_tx_fqid(struct ppam_interface *p, unsigned idx,
				   uint32_t fqid)
{
       p->tx_fqids[idx] = fqid;
}

static int ppam_rx_error_init(struct ppam_rx_error *p,
			      struct ppam_interface *_if,
			      struct qm_fqd_stashing *stash_opts)
{
	return 0;
}
static void ppam_rx_error_finish(struct ppam_rx_error *p,
				 struct ppam_interface *_if)
{
}
static inline void ppam_rx_error_cb(struct ppam_rx_error *p,
				    struct ppam_interface *_if,
				    const struct qm_dqrr_entry *dqrr)
{
        const struct qm_fd *fd = &dqrr->fd;
	/* don't drop BPDERR SEC errored fds */
	if (fd->status & SEC_QI_ERR_MASK == SEC_QI_ERR_BITS &&
	    fd->status & SEC_QI_STA_MASK == SEC_QI_ERR_BPD)
		return;
	ppac_drop_frame(fd);
}

/* Note: this implementation always maps this rx-default to the first available
 * offline port. Ie. if there are multiple offline ports, only the first gets
 * used */
static int ppam_rx_default_init(struct ppam_rx_default *p,
				struct ppam_interface *_if,
				unsigned int idx,
				struct qm_fqd_stashing *stash_opts)
{
	return 0;
}
static void ppam_rx_default_finish(struct ppam_rx_default *p,
				   struct ppam_interface *_if)
{
}
static inline void ppam_rx_default_cb(struct ppam_rx_default *p,
				      struct ppam_interface *_if,
				      const struct qm_dqrr_entry *dqrr)
{
	const struct qm_fd *fd = &dqrr->fd;
	ppac_drop_frame(fd);
}

static int ppam_tx_error_init(struct ppam_tx_error *p,
			      struct ppam_interface *_if,
			      struct qm_fqd_stashing *stash_opts)
{
	return 0;
}
static void ppam_tx_error_finish(struct ppam_tx_error *p,
				 struct ppam_interface *_if)
{
}
static inline void ppam_tx_error_cb(struct ppam_tx_error *p,
				    struct ppam_interface *_if,
				    const struct qm_dqrr_entry *dqrr)
{
	const struct qm_fd *fd = &dqrr->fd;
	ppac_drop_frame(fd);
}

static int ppam_tx_confirm_init(struct ppam_tx_confirm *p,
				struct ppam_interface *_if,
				struct qm_fqd_stashing *stash_opts)
{
	return 0;
}
static void ppam_tx_confirm_finish(struct ppam_tx_confirm *p,
				   struct ppam_interface *_if)
{
}
static inline void ppam_tx_confirm_cb(struct ppam_tx_confirm *p,
				      struct ppam_interface *_if,
				      const struct qm_dqrr_entry *dqrr)
{
	const struct qm_fd *fd = &dqrr->fd;
	ppac_drop_frame(fd);
}

static int ppam_rx_hash_init(struct ppam_rx_hash *p, struct ppam_interface *_if,
			     unsigned idx, struct qm_fqd_stashing *stash_opts)
{
	struct ppac_interface *ppac_if;
	static struct ppac_interface *ib_oh_if, *ib_oh_pre_if,
			*ob_oh_if, *eth_if;
	struct ppam_interface *__if;

	/* loop over ppac interfaces and get the ports */
	if (!ib_oh_if && !ob_oh_if && !eth_if &&!ib_oh_pre_if) {
		list_for_each_entry(ppac_if, &ifs, node) {
			/* offline ports */
			if (ppac_if->port_cfg->fman_if ==
			    app_conf.ib_oh)
				ib_oh_if = ppac_if;
			else if (ppac_if->port_cfg->fman_if ==
			         app_conf.ob_oh_pre)
				ob_oh_if = ppac_if;
                        else if (ppac_if->port_cfg->fman_if == 
                                 app_conf.ib_oh_pre)
                                 ib_oh_pre_if = ppac_if;

			/* ethernet port */
			if (ppac_if->port_cfg->fman_if ==
			    app_conf.ob_eth)
				eth_if = ppac_if;
		}
	}

    /* one or more ports were not found*/
    if (!ib_oh_if || !ob_oh_if || !eth_if || !ib_oh_pre_if)
        return 0;
    /* inbound mappings : inbound offline Rx - ethernet Tx*/
    if (&ib_oh_if->ppam_data == _if) {
        __if = &eth_if->ppam_data;
	p->tx_fqid = __if->tx_fqids[idx % __if->num_tx_fqids];
	TRACE("Mapping Rx FQ %p:%d --> Tx FQID %d\n",
	      p, idx, p->tx_fqid);
	}

    /* outbound mappings : ethernet Rx - outbound offline Tx*/
    if (&eth_if->ppam_data == _if) {
	__if = &ob_oh_if->ppam_data;
	p->tx_fqid = __if->tx_fqids[idx % __if->num_tx_fqids];
	TRACE("Mapping Rx FQ %p:%d --> Tx FQID %d\n",
	      p, idx, p->tx_fqid);
    }
    return 0;
}

static void ppam_rx_hash_finish(struct ppam_rx_hash *p,
				struct ppam_interface *_if,
				unsigned idx)
{
}

static int init_buffer_pools(void)
{
	const struct bpool *bp = bpool;
	int ret;

	/* - map DMA mem */
	dma_mem_generic = dma_mem_create(DMA_MAP_FLAG_ALLOC, NULL,
			DMA_MAP_SIZE);
	if (!dma_mem_generic) {
		fprintf(stderr, "error: dma_mem init, continuing\n");
		return -EINVAL;
	}

	ret = bman_alloc_bpid(&app_conf.ipf_bpid);
	if (ret < 0) {
		fprintf(stderr, "Cannot allocate bpid for ipf bpool\n");
		return ret;
	}
	bpool[0].bpid = app_conf.ipf_bpid;

	while (bp->bpid != -1) {
		int err = ppac_prepare_bpid(bp->bpid, bp->num, bp->size, 256,
					    bp->bpid == app_conf.ipf_bpid ?
					    0 : 1,
					    NULL, NULL);
		if (err) {
			fprintf(stderr, "error: bpool (%d) init failure\n",
				bp->bpid);
			return err;
		}
		bp++;
	}

	return 0;
}

void cleanup_buffer_pools(void)
{
	dma_mem_destroy(dma_mem_generic);
	bman_release_bpid(app_conf.ipf_bpid);
}

static struct fman_if *parse_eth_portarg(const char *p, int fm_idx)
{
	char *pch;
	int port_idx, port_type;
	pch = strtok((char *)p, ",");
	port_idx = atoi(pch);
	pch = strtok(NULL, ",");
	if (!pch)
		return NULL;
	port_type = atoi(pch);
	return get_fif(fm_idx, port_idx, port_type);
}

static struct fman_if *parse_offline_portarg(const char *p, int fm_idx)
{
	int port_idx;
	port_idx = atoi(p);
	return get_fif(fm_idx, port_idx, fman_offline);
}

int ppam_init(void)
{
	int ret;

	/* mandatory cmdline args */
	/* fm index */
	if (!ppam_args.fm) {
		fprintf(stderr, "Error : fm arg not set\n");
		goto err;
	}
	app_conf.fm = atoi(ppam_args.fm);
	/* outbound eth port */
	if (!ppam_args.ob_eth) {
		fprintf(stderr, "Error : ob_eth arg not set\n");
		goto err;
	}
	app_conf.ob_eth = parse_eth_portarg(ppam_args.ob_eth,
					    app_conf.fm);
	if (!app_conf.ob_eth) {
		fprintf(stderr, "Error : ob_eth %s invalid\n",
			ppam_args.ob_eth);
		goto err;
	}

	/* inbound offline port */
	if (!ppam_args.ib_oh) {
		fprintf(stderr, "Error : ib_oh arg not set\n");
		goto err;
	}
	app_conf.ib_oh = parse_offline_portarg(ppam_args.ib_oh,
					       app_conf.fm);
	if (!app_conf.ib_oh) {
		fprintf(stderr, "Error : ib_oh %s invalid\n",
			ppam_args.ib_oh);
		goto err;
	}
	/* inbound pre SEC  offline port */
	if (!ppam_args.ib_oh_pre) {
		fprintf(stderr, "Error : ib_oh_pre arg not set\n");
		goto err;
	}
	app_conf.ib_oh_pre = parse_offline_portarg(ppam_args.ib_oh_pre,
					       app_conf.fm);
	if (!app_conf.ib_oh_pre) {
		fprintf(stderr, "Error : ib_oh_pre %s invalid\n",
			ppam_args.ib_oh_pre);
		goto err;
	}

	/* outbound pre SEC offline port */
	if (!ppam_args.ob_oh_pre) {
		fprintf(stderr, "Error : ib_oh_pre arg not set\n");
		goto err;
	}
	app_conf.ob_oh_pre = parse_offline_portarg(ppam_args.ob_oh_pre,
						   app_conf.fm);
	if (!app_conf.ob_oh_pre) {
		fprintf(stderr, "Error : ob_oh_pre %s invalid\n",
			ppam_args.ob_oh_pre);
		goto err;
	}

	/* outbound post SEC offline port */
	if (!ppam_args.ob_oh_post) {
		fprintf(stderr, "Error : ib_oh_post arg not set\n");
		goto err;
	}
	app_conf.ob_oh_post = parse_offline_portarg(ppam_args.ob_oh_post,
						     app_conf.fm);
	if (!app_conf.ob_oh_post) {
		fprintf(stderr, "Error : ob_oh_post %s invalid\n",
			ppam_args.ob_oh_post);
		goto err;
	}

	/* max sa pairs */
	if (!ppam_args.max_sa) {
		fprintf(stderr, "Error : max-sa arg not set\n");
		goto err;
	}
	app_conf.max_sa = atoi(ppam_args.max_sa);
	/* optionals */
	if (ppam_args.vif)
		strncpy(app_conf.vif, ppam_args.vif, sizeof(app_conf.vif));

	if (ppam_args.vof)
		strncpy(app_conf.vof, ppam_args.vof, sizeof(app_conf.vof));

	if (ppam_args.vipsec)
		strncpy(app_conf.vipsec, ppam_args.vipsec, sizeof(app_conf.vipsec));

	/* mtu pre enc */
	if (ppam_args.mtu_pre_enc)
		app_conf.mtu_pre_enc = atoi(ppam_args.mtu_pre_enc);

	/* if true - perform inbound policy verification */
	if (ppam_args.inb_pol_check)
		app_conf.inb_pol_check = true;

	if (ppam_args.outer_tos)
		app_conf.outer_tos = atoi(ppam_args.outer_tos);

	if (ppam_args.ib_ecn)
		app_conf.ib_ecn = true;

	if (ppam_args.ob_ecn)
		app_conf.ob_ecn = true;

	if (ppam_args.ib_loop)
		app_conf.ib_loop = true;

	ret = init_buffer_pools();
	if (ret < 0) {
		fprintf(stderr, "Buffer pool init failed\n");
		goto err;
	}
	TRACE("Buffer pool initialized\n");

    ret = rman_init();
    if (unlikely(ret < 0)) {
        error(0, -ret, "rman_init()");
        return -ret;
    }
	TRACE("RMan initialized\n");

	cmodel = fmc_compile_model();
	if (!cmodel) {
		fprintf(stderr, "PCD model compile failure\n");
		goto bp_cleanup;
	}
	TRACE("PCD applied\n");
	return 0;

bp_cleanup:
	cleanup_buffer_pools();
err:
	return -1;
}

int ppam_post_tx_init(void)
{
	int ret;

	ret = fmc_apply_model();
	if (ret < 0) {
		fprintf(stderr, "PCD model apply failure (%d)\n", ret);
		goto err;
	}

	ret = ipsec_offload_init(&dpa_ipsec_id);
	if (ret < 0) {
		fprintf(stderr, "DPA IPsec init failure (%d)\n", ret);
		goto err;
	}
	TRACE("DPA IPsec offloading initialized\n");

	ret = stats_init();
	if (ret < 0) {
		fprintf(stderr, "DPA Stats init failed\n");
		goto err;
	}
	TRACE("DPA Stats initialized\n");

	return 0;
err:
	return -1;
}

int ppam_thread_init(void)
{
	static bool ran;
	int ret;
	if (ran)
		return 0;

	ret = setup_xfrm_msgloop(dpa_ipsec_id, &xfrm_tid);
	if (ret < 0) {
		fprintf(stderr, "XFRM message loop start failure (%d)\n", ret);
		return ret;
	}
	TRACE("Started XFRM messages processing\n");

	ret = setup_neigh_loop(&neigh_tid);
	if (ret < 0) {
		fprintf(stderr, "NEIGH message loop start failure (%d)\n", ret);
		return ret;
	}
	TRACE("Started NEIGH messages processing\n");

	ran = true;

	return 0;
}

void ppam_post_finish_rx(void)
{
	int ret __maybe_unused;
	ret = pthread_kill(neigh_tid, SIGTERM);
	TRACE("Finished NEIGH messages processing (%d)\n", ret);
	pthread_kill(xfrm_tid, SIGTERM);
	ret = pthread_join(xfrm_tid, NULL);
	TRACE("Finished XFRM messages processing (%d)\n", ret);
}

void ppam_finish(void)
{
	stats_cleanup();
	ipsec_offload_cleanup(dpa_ipsec_id);
	fmc_cleanup();
	cleanup_macless_config(app_conf.vipsec);
	cleanup_macless_config(app_conf.vif);
	cleanup_macless_config(app_conf.vof);
	cleanup_buffer_pools();
}

/* Swap 6-byte MAC headers "efficiently" (hopefully) */
static inline void ether_header_swap(struct ether_header *prot_eth)
{
	register u32 a, b, c;
	u32 *overlay = (u32 *)prot_eth;
	a = overlay[0];
	b = overlay[1];
	c = overlay[2];
	overlay[0] = (b << 16) | (c >> 16);
	overlay[1] = (c << 16) | (a >> 16);
	overlay[2] = (a << 16) | (b >> 16);
}

static inline void ppam_rx_hash_cb(struct ppam_rx_hash *p,
				   const struct qm_dqrr_entry *dqrr)
{
	void *addr;
	void *annotations;
	struct ether_header *prot_eth;
	const struct qm_fd *fd = &dqrr->fd;
	struct qm_fd _fd;
	uint32_t tx_fqid;
	void *next_header;
	uint16_t proto, len = 0;
	bool continue_parsing;

	annotations = __dma_mem_ptov(qm_fd_addr(fd));
	TRACE("Rx: 2fwd	 fqid=%d\n", dqrr->fqid);
	switch (fd->format)	{
	case qm_fd_contig:
		TRACE("FD format = qm_fd_contig\n");
		addr = annotations + fd->offset;
		prot_eth = addr;
		break;

	case qm_fd_sg:
		TRACE("FD format = qm_fd_sg\n");
		addr = annotations + fd->offset;
		prot_eth = __dma_mem_ptov(qm_sg_entry_get64(addr)) +
				((struct qm_sg_entry *)addr)->offset;
		break;

	default:
		TRACE("FD format not supported!\n");
		BUG();
	}

	next_header = (prot_eth + 1);
	proto = prot_eth->ether_type;
	len = sizeof(struct ether_header);

	TRACE("	     phys=0x%"PRIx64", virt=%p, offset=%d, len=%d, bpid=%d\n",
	      qm_fd_addr(fd), addr, fd->offset, fd->length20, fd->bpid);
	TRACE("	     dhost="ETH_MAC_PRINTF_FMT"\n",
	      prot_eth->ether_dhost[0], prot_eth->ether_dhost[1],
	      prot_eth->ether_dhost[2], prot_eth->ether_dhost[3],
	      prot_eth->ether_dhost[4], prot_eth->ether_dhost[5]);
	TRACE("	     shost="ETH_MAC_PRINTF_FMT"\n",
	      prot_eth->ether_shost[0], prot_eth->ether_shost[1],
	      prot_eth->ether_shost[2], prot_eth->ether_shost[3],
	      prot_eth->ether_shost[4], prot_eth->ether_shost[5]);
	TRACE("	     ether_type=%04x\n", prot_eth->ether_type);
	/* Eliminate ethernet broadcasts. */
	if (prot_eth->ether_dhost[0] & 0x01) {
		TRACE("	     -> dropping broadcast packet\n");
	} else {
	continue_parsing = true;
	while (continue_parsing) {
		switch (proto) {
		case ETHERTYPE_VLAN:
			TRACE("	       -> it's ETHERTYPE_VLAN!\n");
			{
			struct vlan_hdr *vlanhdr = (struct vlan_hdr *)
							(next_header);

			proto = vlanhdr->type;
			next_header = (void *)vlanhdr + sizeof(struct vlan_hdr);
			len = len + sizeof(struct vlan_hdr);
			}
			break;
		case ETHERTYPE_IP:

			TRACE("	       -> it's ETHERTYPE_IP!\n");
			{
			struct iphdr *iphdr = (typeof(iphdr))(next_header);
	#ifdef ENABLE_TRACE
			u8 *src = (void *)&iphdr->saddr;
			u8 *dst = (void *)&iphdr->daddr;
			TRACE("		  ver=%d,ihl=%d,tos=%d,len=%d,id=%d\n",
			      iphdr->version, iphdr->ihl, iphdr->tos,
			      iphdr->tot_len, iphdr->id);
			TRACE("		  frag_off=%d,ttl=%d,prot=%d,"
			      "csum=0x%04x\n", iphdr->frag_off, iphdr->ttl,
			      iphdr->protocol, iphdr->check);
			TRACE("		  src=%d.%d.%d.%d\n",
			      src[0], src[1], src[2], src[3]);
			TRACE("		  dst=%d.%d.%d.%d\n",
			      dst[0], dst[1], dst[2], dst[3]);
	#endif
			TRACE("Tx: 2fwd	 fqid=%d\n", p->tx_fqid);
			TRACE("	     phys=0x%"PRIx64", offset=%d, len=%d,"
					" bpid=%d\n", qm_fd_addr(fd),
					fd->offset, fd->length20, fd->bpid);

			/* Frame received on Offline port PCD FQ range */
			if (!p->tx_fqid) {
				BUG_ON(fd->offset < sizeof(tx_fqid));
				tx_fqid = *(uint32_t *)annotations;
			} else {
				tx_fqid = p->tx_fqid;
			}
			/* IPv4 frame may contain ESP padding */
			_fd = *fd;
			ppac_send_frame(tx_fqid, &_fd);
			continue_parsing = false;
			}
			return;
		case ETHERTYPE_IPV6:
			TRACE("        -> it's ETHERTYPE_IPv6!\n");
			{
			struct ip6_hdr *ip6_hdr =
					(typeof(ip6_hdr))(next_header);
			TRACE("Tx: 2fwd  fqid=%d\n", p->tx_fqid);
			TRACE("      phys=0x%"PRIx64", offset=%d, len=%d,"
					" bpid=%d\n", qm_fd_addr(fd),
					fd->offset, fd->length20, fd->bpid);
			/* Frame received on Offline port PCD FQ range */
			if (!p->tx_fqid) {
				BUG_ON(fd->offset < sizeof(tx_fqid));
				tx_fqid = *(uint32_t *)annotations;
			} else {
				tx_fqid = p->tx_fqid;
			}
			/* IPv6 may contain ESP padding */
			_fd = *fd;
			ppac_send_frame(tx_fqid, fd);
			continue_parsing = false;
			}
			return;
		case ETHERTYPE_ARP:
			TRACE("	       -> it's ETHERTYPE_ARP!\n");
	#ifdef ENABLE_TRACE
			{
			struct ether_arp *arp = (typeof(arp))(next_header);
			TRACE("		  hrd=%d, pro=%d, hln=%d, pln=%d,"
			      " op=%d\n", arp->arp_hrd, arp->arp_pro,
			      arp->arp_hln, arp->arp_pln, arp->arp_op);
			}
	#endif
			TRACE("		  -> dropping ARP packet\n");
			ppac_drop_frame(fd);
			continue_parsing = false;
			break;
		default:
			TRACE("	       -> it's UNKNOWN (!!) type 0x%04x\n",
			      prot_eth->ether_type);
			TRACE("		  -> dropping unknown packet\n");
			ppac_drop_frame(fd);
			continue_parsing = false;
			break;
		}
	}
	}
}
const char ppam_doc[] = "Offloading demo application";

static const struct argp_option argp_opts[] = {
	{"fm", 'f', "INT", 0, "FMAN index"},
	{"ob_eth", 'e',	"FILE", 0, "Outbound Ethernet port index"},
	{"ib-oh", 'i', "INT", 0, "Inbound offline port index" },
	{"ib-oh-pre", 'h', "INT", 0, "OH4 port index"},
	{"ob-oh-pre", 'o', "INT", 0, "Outbound pre IPsec offline port index"},
	{"ob-oh-post", 's', "INT", 0, "Outbound post IPsec offline port index"},
	{"max-sa", 'm', "INT", 0, "Maximum number of SA pairs"},
	{"mtu-pre-enc", 'r', "INT", 0, "MTU pre encryption"},
	{"inb-pol-check" , 'c' , 0, 0, "Inbound policy verification"},
	{"outer-tos", 'x', "INT", 0, "Outer header TOS field"},
	{"ib-ecn", 'y', 0, 0, "Inbound ECN tunneling"},
	{"ob-ecn", 'z', 0, 0, "Outbound ECN tunneling"},
	{"ib-loop", 'l', 0, 0, "Loopback on inbound Ethernet port"},
	{"vif", 'v', "FILE", 0 , "Virtual inbound interface name"},
	{"vof", 'w', "FILE", 0 , "Virtual outbound interface name"},
	{"vipsec", 'u', "FILE", 0 , "IPsec interface name"},
	{}
};

static error_t parse_opts(int key, char *arg, struct argp_state *state)
{
	switch (key) {
	case 'f':
		ppam_args.fm = arg;
		break;
	case 'e':
		ppam_args.ob_eth = arg;
		break;
	case 'i':
		ppam_args.ib_oh = arg;
		break;
	case 'h':
		ppam_args.ib_oh_pre = arg;
		break;
	case 'o':
		ppam_args.ob_oh_pre = arg;
		break;
	case 's':
		ppam_args.ob_oh_post = arg;
		break;
	case 'm':
		ppam_args.max_sa = arg;
		break;
	case 'r':
		ppam_args.mtu_pre_enc = arg;
		break;
	case 'c':
		ppam_args.inb_pol_check = 1;
		break;
	case 'x':
		ppam_args.outer_tos = arg;
		break;
	case 'y':
		ppam_args.ib_ecn = 1;
		break;
	case 'z':
		ppam_args.ob_ecn = 1;
		break;
	case 'v':
		ppam_args.vif = arg;
		break;
	case 'w':
		ppam_args.vof = arg;
		break;
	 case 'u':
		ppam_args.vipsec = arg;
		break;
	default:
		return ARGP_ERR_UNKNOWN;
	}
	return 0;
}

const struct argp ppam_argp = {argp_opts, parse_opts, 0, ppam_doc};

cli_cmd(sa_stats, show_sa_stats);
cli_cmd(eth_stats, show_eth_stats);
cli_cmd(ib_reass_stats, show_ib_reass_stats);
cli_cmd(ipsec_stats, show_ipsec_stats);

/* Inline the PPAC machinery */
#include <ppac.c>
