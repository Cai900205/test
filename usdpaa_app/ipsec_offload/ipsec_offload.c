/* Copyright (c) 2011-2013 Freescale Semiconductor, Inc.
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

#if defined(B4860) || defined(T4240) || defined(B4420)
#include "fm_vsp_ext.h"
#endif

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
const char ppam_prompt[] = "ipsec_offload> ";

/* Ports and fman used */
struct ppam_arguments {
	const char *fm;
	const char *ob_eth;
	const char *ib_eth;
	const char *ib_oh;
	const char *ob_oh_pre;
	const char *ob_oh_post;
	const char *max_sa;
	const char *mtu_pre_enc;
	const char *outer_tos;
	int ib_ecn;
	int ob_ecn;
	int ib_loop;
	const char *vif;
	const char *vof;
	const char *vipsec;
	int ib_aggreg;
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

struct ppam_arguments ppam_args;
struct app_conf app_conf;
static int dpa_ipsec_id;
static struct fmc_model_t *cmodel;
static pthread_t xfrm_tid, neigh_tid;

#if defined(B4860) || defined(T4240) || defined(B4420)

static t_Handle	vsp;
static t_Handle	fm_obj;

static int vsp_init(int fman_id, int fm_port_number, e_FmPortType fm_port_type)
{
	int	ret = E_OK;
	t_FmVspParams vsp_params;
	t_FmBufferPrefixContent buf_prefix_cont;

	fm_obj = FM_Open(fman_id);
	if (!fm_obj) {
		fprintf(stderr, "FM_Open NULL handle.\n");
		return -EINVAL;
	}

	/* create a descriptor for the FM VSP module */

	memset(&vsp_params, 0, sizeof(vsp_params));

	vsp_params.h_Fm = fm_obj;
	vsp_params.relativeProfileId = VSP_ID;
	vsp_params.portParams.portId = fm_port_number;
	vsp_params.portParams.portType = fm_port_type;
	vsp_params.extBufPools.numOfPoolsUsed = 1;
	vsp_params.extBufPools.extBufPool[0].id = IF_BPID;
	vsp_params.extBufPools.extBufPool[0].size = VSP_BP_SIZE;

	vsp = FM_VSP_Config(&vsp_params);
	if (!vsp) {
		fprintf(stderr, "FM_VSP_Config NULL\n");
		return -EINVAL;
	}

	/* configure the application buffer (structure, size and content) */

	memset(&buf_prefix_cont, 0, sizeof(buf_prefix_cont));

	buf_prefix_cont.privDataSize = 16;
	buf_prefix_cont.dataAlign = 64;
	buf_prefix_cont.passPrsResult = TRUE;
	buf_prefix_cont.passTimeStamp = TRUE;
	buf_prefix_cont.passHashResult = FALSE;
	buf_prefix_cont.passAllOtherPCDInfo = FALSE;

	ret = FM_VSP_ConfigBufferPrefixContent(vsp,	&buf_prefix_cont);
	if (ret != E_OK) {
		fprintf(stderr, "FM_VSP_ConfigBufferPrefixContent error "
				"for vsp; err: %d\n", ret);
		return ret;
	}

	/* initialize the FM VSP module */

	ret = FM_VSP_Init(vsp);
	if (ret != E_OK) {
		error(0, ret, "FM_VSP_Init error: %d\n", ret);
	}

	return ret;
}

static int vsp_clean(void)
{
	int ret = E_OK;

	if (vsp) {
	    ret = FM_VSP_Free(vsp);
		if (ret != E_OK) {
			fprintf(stderr, "Error FM_VSP_Free: %d", ret);
			return ret;
		}
	}

	FM_Close(fm_obj);

	return E_OK;
}
#endif

static void cleanup_macless_config(char *macless_name)
{
	struct fman_if *__if = NULL;
	struct ether_addr * original_mac = NULL;

	/* get the original mac addr of the interface - the one in fman_if */

	__if = get_fman_if_by_name(macless_name);
	if (!__if )
		goto err;

	original_mac = get_macless_peer_mac(__if);
	if (!original_mac)
		goto err;

	/* restore the mac address */

	if (set_mac_addr(macless_name, original_mac) < 0)
		goto err;

	return;

err:
	fprintf(stderr, "Failed to restore %s mac address\n", macless_name);
}

static int setup_macless_if_tx(struct ppac_interface *i, uint32_t last_fqid,
			       unsigned int *num_tx_fqs, struct qman_fq *fq,
			       char *macless_name)
{
	int ret = 0;
	int loop = 0;
	struct fman_if *__if;
	uint32_t tx_start = 0;
	uint32_t tx_count = 0;
	struct ether_addr mac;

	/* get the ethernet address of the macless port */

	memset(&mac, 0, sizeof(mac));
	ret = get_mac_addr(macless_name, &mac);
	if (ret < 0)
		return ret;

	/* find the corresponding fman interface */

	__if = get_fman_if_by_mac(&mac);
	if (!__if)
		return -ENODEV;

	/* set the name of the macless port */

	set_macless_name(__if, macless_name);

	/* get the macless tx queues - for macless ports only
	 * usdpaa does not create tx queues for oNIC ports */

	if (__if->mac_type == fman_mac_less) {
		tx_start = __if->macless_info.tx_start;
		tx_count = __if->macless_info.tx_count;

		if (!tx_start || !tx_count)
			return -ENODEV;
	}

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
	int ret = 0;
	struct fman_if *__if = NULL;
	uint32_t rx_start = 0;
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

	__if = get_fman_if_by_name(macless_name);
	if (!__if)
		return -ENODEV;

	rx_start = __if->macless_info.rx_start;

	if (!strcmp(macless_name, app_conf.vif)) {
		/* set fqids for vif PCD */
		memset(fmc_path, 0, sizeof(fmc_path));
		port_type = get_port_type(app_conf.ib_eth);
		sprintf(fmc_path, "fm%d/port/%s/%d/dist/"
			"ib_default_dist",
			app_conf.fm, port_type, app_conf.ib_eth->mac_idx);
		ret = set_dist_base_fqid(cmodel, fmc_path, rx_start);
		if (ret < 0)
			goto err;

		memset(fmc_path, 0, sizeof(fmc_path));
		sprintf(fmc_path, "fm%d/port/OFFLINE/%d/ccnode/"
			"ib_post_ip_cc",
			app_conf.fm, app_conf.ib_oh->mac_idx);

#if defined(B4860) || defined(T4240) || defined(B4420)
		ret = set_cc_miss_fqid_with_vsp(cmodel, fmc_path, rx_start);
#else
		ret = set_cc_miss_fqid(cmodel, fmc_path, rx_start);
#endif

		if (ret < 0)
			goto err;

		memset(fmc_path, 0, sizeof(fmc_path));
		sprintf(fmc_path, "fm%d/port/OFFLINE/%d/ccnode/"
			"ib_post_ip6_cc",
			app_conf.fm, app_conf.ib_oh->mac_idx);
#if defined(B4860) || defined(T4240) || defined(B4420)
		ret = set_cc_miss_fqid_with_vsp(cmodel, fmc_path, rx_start);
#else
		ret = set_cc_miss_fqid(cmodel, fmc_path, rx_start);
#endif
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

/*
 * Update the CC model for ib_ip4_local_cc & ib_ip6_local_cc
 * Select the miss FQ ID to be the queue from which the outbound PRE SEC OH port
 * will dequeue.
 */
static int update_ib_local_cc_miss_fq(void)
{
	char fmc_path[64];
	const char *port_type;
	int ret;

	memset(fmc_path, 0, sizeof(fmc_path));
	port_type = get_port_type(app_conf.ob_eth);
	sprintf(fmc_path, "fm%d/port/%s/%d/ccnode/ib_ip4_local_cc",
		app_conf.fm, port_type, app_conf.ob_eth->mac_idx);

	ret = set_cc_miss_fqid(cmodel, fmc_path,
			       ob_oh_pre_tx_fqid[app_conf.fm]);
	if (ret < 0) {
		fprintf(stderr, "Failed to update miss FQ id with %d for ib_ip4_local_cc\n",
			ob_oh_pre_tx_fqid[app_conf.fm]);
		return ret;
	}

	memset(fmc_path, 0, sizeof(fmc_path));
	port_type = get_port_type(app_conf.ob_eth);
	sprintf(fmc_path, "fm%d/port/%s/%d/ccnode/ib_ip6_local_cc",
		app_conf.fm, port_type, app_conf.ob_eth->mac_idx);
	ret = set_cc_miss_fqid(cmodel, fmc_path,
			       ob_oh_pre_tx_fqid[app_conf.fm]);
	if (ret < 0) {
		fprintf(stderr, "Failed to update miss FQ ID with %d for ib_ip6_local_cc\n",
			ob_oh_pre_tx_fqid[app_conf.fm]);
		return ret;
	}

	return 0;
}

/*
 * Update the CC model for
 * ob_rx_dist_udp, ob_rx_dist_tcp, ob_rx_ip4_dist, ob_rx_ip6_dist
 * This update should be done only in dual port mode
 */
static int update_ob_dist_miss_fq(void)
{
	char fmc_path[64];
	const char *port_type;
	int ret;

	memset(fmc_path, 0, sizeof(fmc_path));
	port_type = get_port_type(app_conf.ob_eth);
	sprintf(fmc_path, "fm%d/port/%s/%d/dist/ob_rx_dist_udp",
		app_conf.fm, port_type, app_conf.ob_eth->mac_idx);

	ret = set_dist_base_fqid(cmodel, fmc_path,
				 ob_oh_pre_tx_fqid[app_conf.fm]);
	if (ret < 0) {
		fprintf(stderr, "Failed to update base FQ id with %d for ob_rx_dist_udp\n",
			ob_oh_pre_tx_fqid[app_conf.fm]);
		return ret;
	}

	memset(fmc_path, 0, sizeof(fmc_path));
	port_type = get_port_type(app_conf.ob_eth);
	sprintf(fmc_path, "fm%d/port/%s/%d/dist/ob_rx_dist_tcp",
		app_conf.fm, port_type, app_conf.ob_eth->mac_idx);

	ret = set_dist_base_fqid(cmodel, fmc_path,
				 ob_oh_pre_tx_fqid[app_conf.fm]);
	if (ret < 0) {
		fprintf(stderr, "Failed to update base FQ ID with %d for ob_rx_dist_tcp\n",
			ob_oh_pre_tx_fqid[app_conf.fm]);
		return ret;
	}

	memset(fmc_path, 0, sizeof(fmc_path));
	port_type = get_port_type(app_conf.ob_eth);
	sprintf(fmc_path, "fm%d/port/%s/%d/dist/ob_rx_ip4_dist",
		app_conf.fm, port_type, app_conf.ob_eth->mac_idx);

	ret = set_dist_base_fqid(cmodel, fmc_path,
				 ob_oh_pre_tx_fqid[app_conf.fm]);
	if (ret < 0) {
		fprintf(stderr, "Failed to update base FQ ID with %d for ob_rx_ip4_dist\n",
			ob_oh_pre_tx_fqid[app_conf.fm]);
		return ret;
	}

	memset(fmc_path, 0, sizeof(fmc_path));
	port_type = get_port_type(app_conf.ob_eth);
	sprintf(fmc_path, "fm%d/port/%s/%d/dist/ob_rx_ip6_dist",
		app_conf.fm, port_type, app_conf.ob_eth->mac_idx);

	ret = set_dist_base_fqid(cmodel, fmc_path,
				 ob_oh_pre_tx_fqid[app_conf.fm]);
	if (ret < 0) {
		fprintf(stderr, "Failed to update base FQ ID with %d for ob_rx_ip6_dist\n",
			ob_oh_pre_tx_fqid[app_conf.fm]);
		return ret;
	}

	return 0;
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
	int idx;
	struct ppac_interface *i =
		container_of(p, struct ppac_interface, ppam_data);
	struct qman_fq *fq = &i->tx_fqs[0];

	if (app_conf.ob_oh_post == i->port_cfg->fman_if) {
		fq->fqid = OB_OH_POST_TX_FQID;
		*flags |= PPAM_TX_FQ_NO_BUF_DEALLOC;
	}

	if (app_conf.ob_oh_pre == i->port_cfg->fman_if) {
		*flags |= PPAM_TX_FQ_NO_BUF_DEALLOC;
		ret = setup_macless_if_tx(i, ob_oh_pre_tx_fqid[app_conf.fm],
					  &num_tx_fqs, fq, app_conf.vipsec);
		if (ret < 0)
			goto err;

		ret = set_mac_addr(app_conf.vipsec, &app_conf.ib_eth->mac_addr);
		if (ret < 0)
			goto err;
	}

	if (app_conf.ib_eth == i->port_cfg->fman_if) {
		ret = setup_macless_if_tx(i, IB_TX_FQID, &num_tx_fqs,
					  fq, app_conf.vif);
		if (ret < 0)
			goto err;
		ret = setup_macless_if_rx(i, app_conf.vif);
		if (ret < 0)
			goto err;

		ret = set_mac_addr(app_conf.vif,
				   &i->port_cfg->fman_if->mac_addr);
		if (ret < 0)
			goto err;
	}

	if ((app_conf.ob_eth != app_conf.ib_eth) &&
	    (app_conf.ob_eth == i->port_cfg->fman_if)) {
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
	if (app_conf.ib_oh == i->port_cfg->fman_if)
	{
		/* The IB OH sends traffic to the vipsec interface*/

		idx = if_nametoindex(app_conf.vipsec);
		if (!idx)
			return -ENODEV;
		i->ppam_data.macless_ifindex = idx;

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
	if ((fd->status & SEC_QI_ERR_MASK) == SEC_QI_ERR_BITS &&
	    (fd->status & SEC_QI_STA_MASK) == SEC_QI_ERR_BPD)
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
	static struct ppac_interface *ib_oh_if,
			*ob_oh_if, *eth_if;
	struct ppam_interface *__if;

	/* loop over ppac interfaces and get the ports */
	if (!ib_oh_if && !ob_oh_if && !eth_if) {
		list_for_each_entry(ppac_if, &ifs, node) {
			/* offline ports */
			if (ppac_if->port_cfg->fman_if ==
			    app_conf.ib_oh)
				ib_oh_if = ppac_if;
			else if (ppac_if->port_cfg->fman_if ==
				 app_conf.ob_oh_pre)
				ob_oh_if = ppac_if;

			/* ethernet port */
			if (ppac_if->port_cfg->fman_if ==
			    app_conf.ob_eth)
				eth_if = ppac_if;
		}
	}

	/* one or more ports were not found*/
	if (!ib_oh_if || !ob_oh_if || !eth_if)
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

int ppam_sec_needed()
{
	return 1;
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

	memset(&xfrm_tid, 0, sizeof(pthread_t));
	memset(&neigh_tid, 0, sizeof(pthread_t));

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
		fprintf(stderr, "Error : ib_eth %s invalid\n",
			ppam_args.ib_oh);
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

	/* inbound eth port */
	if (!ppam_args.ib_eth) {
		fprintf(stderr, "Error : ib_eth arg not set\n");
		goto err;
	}
	app_conf.ib_eth = parse_eth_portarg(ppam_args.ib_eth,
					    app_conf.fm);
	if (!app_conf.ib_eth) {
		fprintf(stderr, "Error : ib_eth %s invalid\n",
			ppam_args.ib_eth);
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

	if (app_conf.ib_eth == app_conf.ob_eth &&
	    strcmp(app_conf.vif, app_conf.vof)) {
		strncpy(app_conf.vof, app_conf.vif, sizeof(app_conf.vif));
		printf("WARNING: using %s virtual interface for 1-port conf\n",
		       app_conf.vif);
	}

	if (ppam_args.vipsec)
		strncpy(app_conf.vipsec, ppam_args.vipsec,
				sizeof(app_conf.vipsec));

	/* mtu pre enc */
	if (ppam_args.mtu_pre_enc)
		app_conf.mtu_pre_enc = atoi(ppam_args.mtu_pre_enc);

	if (ppam_args.outer_tos)
		app_conf.outer_tos = atoi(ppam_args.outer_tos);

	if (ppam_args.ib_ecn)
		app_conf.ib_ecn = true;

	if (ppam_args.ob_ecn)
		app_conf.ob_ecn = true;

	if (ppam_args.ib_loop)
		app_conf.ib_loop = true;

	if (ppam_args.ib_aggreg)
		app_conf.ib_aggreg = true;

	ret = init_buffer_pools();
	if (ret < 0) {
		fprintf(stderr, "Buffer pool init failed\n");
		goto err;
	}
	TRACE("Buffer pool initialized\n");

	cmodel = fmc_compile_model();
	if (!cmodel) {
		fprintf(stderr, "PCD model compile failure\n");
		goto bp_cleanup;
	}
	TRACE("PCD compiled\n");

	/*
	 * update the ib_ip4_local_cc & ib_ip6_local_cc miss action only in case
	 * of single port
	 * Classification steps:
	 * ESP? no -> Out Local? no -> In Local? no -> Enqueue to OUT PRE SEC OH
	 */
	if (app_conf.ob_eth == app_conf.ib_eth) {
		ret = update_ib_local_cc_miss_fq();
		if (ret < 0)
			goto bp_cleanup;
	}

	/*
	 * update the ob_rx_dist_udp, ob_rx_dist_tcp, ob_rx_ip4_dist,
	 * ob_rx_ip6_dist base FQ ID only in case of dual port
	 */
	if (app_conf.ob_eth != app_conf.ib_eth) {
		ret = update_ob_dist_miss_fq();
		if (ret < 0)
			goto bp_cleanup;
	}

#if defined(B4860) || defined(T4240) || defined(B4420)

	ret = vsp_init(app_conf.fm, app_conf.ib_oh->mac_idx,
				   e_FM_PORT_TYPE_OH_OFFLINE_PARSING);
	if (ret < 0) {
		fprintf(stderr, "VSP init failed\n");
		goto bp_cleanup;
	}

	TRACE("VSP initialized\n");
#endif

	return 0;

bp_cleanup:
	cleanup_buffer_pools();
err:
	return -1;
}

int ppam_post_tx_init(void)
{
	int ret;
	if (app_conf.ib_loop) {
		fman_if_loopback_enable(app_conf.ib_eth);
		TRACE("Loopback set on inbound port\n");
	}
	fman_if_promiscuous_enable(app_conf.ob_eth);
	printf("Promisc enabled on outbound port\n");

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

	/*
	 * DPA Offload Stats driver does not support multiple instances
	 * Only enable stats for the first created instance
	 */
	if (dpa_ipsec_id == 0) {
		ret = stats_init();
		if (ret < 0) {
			fprintf(stderr, "DPA Stats init failed\n");
			goto err;
		}
		TRACE("DPA Stats initialized\n");
	}

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
	if (neigh_tid) {
		pthread_kill(neigh_tid, SIGUSR1);
		pthread_join(neigh_tid, NULL);
		TRACE("Finished NEIGH messages processing\n");
	}

	if (xfrm_tid) {
		pthread_kill(xfrm_tid, SIGUSR2);
		pthread_join(xfrm_tid, NULL);
		TRACE("Finished XFRM messages processing\n");
	}
}

void ppam_finish(void)
{
	/* Stats are enabled only for the first DPA IPSec instance */
	if (dpa_ipsec_id == 0)
		stats_cleanup();
	ipsec_offload_cleanup(dpa_ipsec_id);
	fmc_cleanup();
	/* reset mac addresses for the macless / oNIC ports */
	cleanup_macless_config(app_conf.vipsec);
	cleanup_macless_config(app_conf.vif);
	cleanup_macless_config(app_conf.vof);
	cleanup_buffer_pools();
#if defined(B4860) || defined(T4240) || defined(B4420)
	vsp_clean();
#endif
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
	{"aggreg", 'a', 0, 0, "Aggregate inbound tunnels"},
	{"fm", 'f', "INT", 0, "FMAN index"},
	{"ob_eth", 'e',	"FILE", 0, "Outbound Ethernet port index"},
	{"ib_eth", 't',	"FILE", 0, "Inbound Ethernet port index"},
	{"ib-oh", 'i', "INT", 0, "Inbound offline port index" },
	{"ob-oh-pre", 'o', "INT", 0, "Outbound pre IPsec offline port index"},
	{"ob-oh-post", 's', "INT", 0, "Outbound post IPsec offline port index"},
	{"max-sa", 'm', "INT", 0, "Maximum number of SA pairs"},
	{"mtu-pre-enc", 'r', "INT", 0, "MTU pre encryption"},
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
	case 'a':
		ppam_args.ib_aggreg = 1;
		break;
	case 'f':
		ppam_args.fm = arg;
		break;
	case 'e':
		ppam_args.ob_eth = arg;
		break;
	case 't':
		ppam_args.ib_eth = arg;
		break;
	case 'i':
		ppam_args.ib_oh = arg;
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
	case 'x':
		ppam_args.outer_tos = arg;
		break;
	case 'y':
		ppam_args.ib_ecn = 1;
		break;
	case 'z':
		ppam_args.ob_ecn = 1;
		break;
	case 'l':
		ppam_args.ib_loop = 1;
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
cli_cmd(list_sa, list_dpa_sa);

/* Inline the PPAC machinery */
#include <ppac.c>
