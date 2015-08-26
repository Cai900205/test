/* Copyright (c) 2011-2012 Freescale Semiconductor, Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the name of Freescale Semiconductor nor the
 *       names of its contributors may be used to endorse or promote products
 *       derived from this software without specific prior written permission.
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

#include <fra_cfg_parser.h>
#include <usdpaa/fman.h>
#include <libxml/parser.h>

#define FRA_CFG_FILE_ROOT_NODE	("fra_cfg")

#define FRA_DEFCFG_NODE		("defcfg")
#define FRA_DEFCFG_FILE		("file")

#define RMAN_CFG_NODE		("rman_cfg")
#define RMAN_CFG_TYPE		("type")
#define RMAN_CFG_VALUE		("value")
#define RMAN_CFG_INDEX		("index")
#define RMAN_FQ_BITS_NODE	("fqbits")
#define MD_CREATE_NODE		("md_create")
#define MD_CREATE_MODE		("mode")
#define RMAN_OSID_NODE		("osid")
#define BPID_NODE		("bpid")
#define SG_BPID_NODE		("sgbpid")
#define RMAN_EFQ_NODE		("efq")

#define NETWORK_CFG_NODE	("network_cfg")
#define FMAN_PORT_NODE		("port")
#define FMAN_PORT_NAME		("name")
#define FMAN_PORT_TYPE		("type")
#define FMAN_PORT_NUM		("number")
#define FMAN_NUM		("fm")

#define TRANS_CFG_NODE		("trans_cfg")
#define TRAN_NODE		("transaction")
#define TRAN_NAME		("name")
#define TRAN_TYPE		("type")
#define TRAN_COMMON_VALUE	("value")
#define TRAN_COMMON_MASK	("mask")
#define TRAN_FLOWLVL_NODE	("flowlvl")
#define TRAN_MBOX_NODE		("mbox")
#define TRAN_LTR_NODE		("ltr")
#define TRAN_MSGLEN_NODE	("msglen")
#define TRAN_COS_NODE		("cos")
#define TRAN_STREAMID_NODE	("streamid")

#define DISTS_CFG_NODE		("dists_cfg")
#define DIST_NODE		("distribution")
#define DIST_NAME		("name")
#define DIST_TYPE		("type")
#define DISTREF_NODE		("distributionref")
#define DIST_RIO_PORT_NODE	("rio_port")
#define DIST_RIO_PORT_MASK	("mask")
#define DIST_PORT_NUMBER	("number")
#define DIST_SID_NODE		("sid")
#define DIST_SID_VALUE		("value")
#define DIST_SID_MASK		("mask")
#define DIST_DID_NODE		("did")
#define DIST_DID_VALUE		("value")
#define DIST_FQ_NODE		("queue")
#define DIST_FQ_ID		("base")
#define DIST_FQ_MODE		("mode")
#define DIST_FQ_COUNT		("count")
#define DIST_FQ_WQ		("wq")
#define DIST_TRANREF_NODE	("transactionref")
#define DIST_TRANREF_NAME	("name")
#define DIST_FMAN_PORT_NODE	("fman_port")

#define POLICIES_CFG_NODE	("policies_cfg")
#define POLICY_NODE		("policy")
#define POLICY_NAME		("name")
#define POLICY_ENABLE		("enable")
#define POLICY_DISTORDER_NODE	("dist_order")
#define POLICY_DISTREF_NODE	("distributionref")
#define POLICY_DISTREF_NAME	("name")

const char *RIO_TYPE_TO_STR[] = {
	[RIO_TYPE0] = "Implementation-defined",
	[RIO_TYPE1] = "reserved",
	[RIO_TYPE2] = "NREAD",
	[RIO_TYPE3] = "reserved",
	[RIO_TYPE4] = "reserved",
	[RIO_TYPE5] = "NWrite",
	[RIO_TYPE6] = "SWrite",
	[RIO_TYPE7] = "reserved",
	[RIO_TYPE8] = "Port-Write",
	[RIO_TYPE9] = "Data-streaming",
	[RIO_TYPE10] = "Doorbell",
	[RIO_TYPE11] = "Mailbox"
};

const char *DIST_TYPE_STR[] = {
	[DIST_TYPE_UNKNOWN] = "UNKNOWN",
	[DIST_TYPE_RMAN_RX] = "RMAN_RX",
	[DIST_TYPE_RMAN_TX] = "RMAN_TX",
	[DIST_TYPE_FMAN_RX] = "FMAN_RX",
	[DIST_TYPE_FMAN_TX] = "FMAN_TX",
	[DIST_TYPE_FMAN_TO_RMAN] = "FMAN_TO_RMAN",
	[DIST_TYPE_RMAN_TO_FMAN] = "RMAN_TO_FMAN"
};

const char *FQ_MODE_STR[] = {"direct", "algorithmic"};
const char *MD_CREATE_MODE_STR[] = {"yes", "no"};

static struct fra_cfg *fracfg;

xmlNodePtr fra_cfg_root_node;

#define for_all_sibling_nodes(node)	\
	for (; unlikely(node != NULL); node = node->next)

static void fra_cfg_parse_error(void *ctx, xmlErrorPtr xep)
{
	error(0, 0, "%s:%hu:%s() fra_cfg_parse_error(context(%p),"
		"error pointer %p", __FILE__, __LINE__, __func__,
		ctx, xep);
}

static inline int is_node(xmlNodePtr node, xmlChar *name)
{
	return xmlStrcmp(node->name, name) ? 0 : 1;
}

static void *get_attributes(xmlNodePtr node, xmlChar *attr)
{
	char *atr = (char *)xmlGetProp(node, attr);
	if (unlikely(atr == NULL))
		error(0, 0, "%s:%hu:%s() error: "
			"(Node(%s)->Attribute (%s) not found",
			__FILE__, __LINE__, __func__,
			node->name, attr);
	return atr;
}

static enum dist_type dist_type_str_to_idx(const char *type)
{
	int idx;

	for (idx = 0; idx < ARRAY_SIZE(DIST_TYPE_STR); idx++) {
		if (!strcmp(type, DIST_TYPE_STR[idx]))
			return idx;
	}
	return DIST_TYPE_UNKNOWN;
}

static enum RIO_TYPE rio_type_str_to_idx(const char *type)
{
	int idx;

	for (idx = 0; idx < ARRAY_SIZE(RIO_TYPE_TO_STR); idx++) {
		if (!strcmp(type, RIO_TYPE_TO_STR[idx]))
			return idx;
	}
	return RIO_TYPE0;
}

static void strip_blanks(char *str)
{
	int i, j;
	int len = strlen(str);

	for (i = 0; (i < len) && (str[i] == ' '); i++)
		;

	for (j = 0; (i < len) && (str[i] != ' '); ++i, j++)
		str[j] = str[i];

	str[j] = '\0';
}

#define tran_parse_element(node, value, mask) \
	do {								\
		char *ptr;						\
		ptr = get_attributes(node, BAD_CAST TRAN_COMMON_VALUE);	\
		if (unlikely(ptr == NULL))				\
			return -EINVAL;					\
		value = strtoul(ptr, NULL, 0);				\
		ptr = get_attributes(node, BAD_CAST TRAN_COMMON_MASK);	\
		if (unlikely(ptr == NULL))				\
			return -EINVAL;					\
		mask = strtoul(ptr, NULL, 0);				\
	} while (0)

#define search_member(member, nm, list) \
	do {								  \
		typeof(member) tmp;					  \
		member = NULL;						  \
		if (!nm)						  \
			break;						  \
		list_for_each_entry(tmp, list, node) {			  \
			if (!strncmp(tmp->name, nm, sizeof(tmp->name))) { \
				member = tmp;				  \
				break;					  \
			}						  \
		}							  \
	} while (0)

static int parse_defcfg_file(char *cfg_file, struct list_head *list,
		int (*parse_node)(xmlNodePtr node, struct list_head *list))
{
	xmlErrorPtr xep;
	xmlDocPtr doc;
	xmlNodePtr cur;

	if (!cfg_file || !list || !parse_node)
		return -EINVAL;

	xmlInitParser();
	LIBXML_TEST_VERSION;
	xmlSetStructuredErrorFunc(&xep, fra_cfg_parse_error);
	xmlKeepBlanksDefault(0);

	doc = xmlParseFile(cfg_file);
	if (unlikely(doc == NULL)) {
		error(0, 0, "%s:%hu:%s() xmlParseFile(%s)",
			__FILE__, __LINE__, __func__, cfg_file);
		return -EINVAL;
	}

	cur = xmlDocGetRootElement(doc);
	if (unlikely(cur == NULL)) {
		error(0, 0, "%s:%hu:%s() xml file(%s) empty",
			__FILE__, __LINE__, __func__, cfg_file);
		goto _err;
	}

	parse_node(cur, list);
	xmlFreeDoc(doc);
	return 0;

_err:
	xmlFreeDoc(doc);
	return -EINVAL;
}

static int parse_tran(xmlNodePtr tran_node, struct list_head *list)
{
	char *name;
	char *type;
	xmlNodePtr tranp;
	struct rio_tran *tran;

	if (!tran_node)
		return -EINVAL;

	name = get_attributes(tran_node, BAD_CAST TRAN_NAME);
	search_member(tran, name, list);
	if (!tran) {
		tran = malloc(sizeof(*tran));
		if (!tran)
			return -ENOMEM;
		memset(tran, 0, sizeof(*tran));
		snprintf(tran->name, sizeof(tran->name), name);
		type = get_attributes(tran_node, BAD_CAST TRAN_TYPE);
		tran->type = rio_type_str_to_idx(type);
		list_add_tail(&tran->node, list);
	}

	/* Update tran configuration */
	tranp = tran_node->xmlChildrenNode;
	switch (tran->type) {
	case RIO_TYPE_DBELL:
	case RIO_TYPE_PW:
		for_all_sibling_nodes(tranp) {
			if ((is_node(tranp, BAD_CAST TRAN_FLOWLVL_NODE))) {
				tran_parse_element(tranp, tran->flowlvl,
					tran->flowlvl_mask);
			}
		}
		break;
	case RIO_TYPE_MBOX:
		for_all_sibling_nodes(tranp) {
			if ((is_node(tranp, BAD_CAST TRAN_FLOWLVL_NODE)))
				tran_parse_element(tranp, tran->flowlvl,
						tran->flowlvl_mask);
			else if ((is_node(tranp, BAD_CAST TRAN_MBOX_NODE)))
				tran_parse_element(tranp, tran->mbox.mbox,
						tran->mbox.mbox_mask);
			else if ((is_node(tranp, BAD_CAST TRAN_LTR_NODE)))
				tran_parse_element(tranp, tran->mbox.ltr,
						tran->mbox.ltr_mask);
			else if ((is_node(tranp, BAD_CAST TRAN_MSGLEN_NODE)))
				tran_parse_element(tranp, tran->mbox.msglen,
						tran->mbox.msglen_mask);
		}
		break;
	case RIO_TYPE_DSTR:
		for_all_sibling_nodes(tranp) {
			if ((is_node(tranp, BAD_CAST TRAN_FLOWLVL_NODE)))
				tran_parse_element(tranp, tran->flowlvl,
					tran->flowlvl_mask);
			else if ((is_node(tranp, BAD_CAST TRAN_COS_NODE)))
				tran_parse_element(tranp, tran->dstr.cos,
					tran->dstr.cos_mask);
			else if ((is_node(tranp, BAD_CAST TRAN_STREAMID_NODE)))
				tran_parse_element(tranp,
					tran->dstr.streamid,
					tran->dstr.streamid_mask);
		}
		break;
	default:
		error(0, 0, "transaction %s has"
			" an invalid type %s", name, type);
		return -ENXIO;
	}
	return 0;
}

static int parse_trans_cfg(xmlNodePtr trans_node, struct list_head *list)
{
	char *cfg_file;
	xmlNodePtr cur;

	if (unlikely(!is_node(trans_node, BAD_CAST TRANS_CFG_NODE)))
		return -EINVAL;

	/* First parse the default trans configuration file */
	cur = trans_node->xmlChildrenNode;
	for_all_sibling_nodes(cur) {
		if (unlikely(!is_node(cur, BAD_CAST FRA_DEFCFG_NODE)))
			continue;
		cfg_file = get_attributes(cur, BAD_CAST FRA_DEFCFG_FILE);
		parse_defcfg_file(cfg_file, list, parse_trans_cfg);
	}

	/* Then update the specified configurations */
	cur = trans_node->xmlChildrenNode;
	for_all_sibling_nodes(cur) {
		if (unlikely(!is_node(cur, BAD_CAST TRAN_NODE)))
			continue;
		parse_tran(cur, list);
	}

	return 0;
}

static void fra_trans_finish(struct list_head *list)
{
	struct rio_tran *tran, *temp;

	list_for_each_entry_safe(tran, temp, list, node) {
		list_del(&tran->node);
		free(tran);
	}
}

static int parse_dist_rman_rx(xmlNodePtr distp,
			      struct dist_rman_rx_cfg *rxcfg)
{
	char *ptr;

	for_all_sibling_nodes(distp) {
		if ((is_node(distp, BAD_CAST DIST_RIO_PORT_NODE))) {
			ptr = get_attributes(distp,
				BAD_CAST DIST_PORT_NUMBER);
			if (unlikely(ptr == NULL))
				return -EINVAL;
			rxcfg->rio_port = strtoul(ptr, NULL, 0);
			ptr = get_attributes(distp, BAD_CAST
					     DIST_RIO_PORT_MASK);
			if (unlikely(ptr == NULL))
				return -EINVAL;
			rxcfg->port_mask = strtoul(ptr, NULL, 0);
		} else if ((is_node(distp, BAD_CAST DIST_SID_NODE))) {
			ptr = get_attributes(distp, BAD_CAST DIST_SID_VALUE);
			if (unlikely(ptr == NULL))
				return -EINVAL;
			rxcfg->sid = strtoul(ptr, NULL, 0);
			ptr = get_attributes(distp, BAD_CAST DIST_SID_MASK);
			if (unlikely(ptr == NULL))
				return -EINVAL;
			rxcfg->sid_mask = strtoul(ptr, NULL, 0);
		} else if ((is_node(distp, BAD_CAST DIST_FQ_NODE))) {
			ptr = get_attributes(distp,
				BAD_CAST DIST_FQ_ID);
			if (unlikely(ptr == NULL))
				return -EINVAL;
			rxcfg->fqid = strtoul(ptr, NULL, 0);
			ptr = get_attributes(distp,
				BAD_CAST DIST_FQ_MODE);
			if (unlikely(ptr == NULL))
				return -EINVAL;
			if (!strcmp(ptr, FQ_MODE_STR[DIRECT]))
				rxcfg->fq_mode = DIRECT;
			else
				rxcfg->fq_mode = ALGORITHMIC;
			ptr = get_attributes(distp, BAD_CAST DIST_FQ_WQ);
			if (unlikely(ptr == NULL))
				return -EINVAL;
			rxcfg->wq = strtoul(ptr, NULL, 0);
		} else if ((is_node(distp, BAD_CAST DIST_TRANREF_NODE))) {
			ptr = get_attributes(distp,
				BAD_CAST DIST_TRANREF_NAME);
			if (unlikely(ptr == NULL))
				return -EINVAL;
			strip_blanks(ptr);
			search_member(rxcfg->tran, ptr, &fracfg->trans_list);
		}
	}
	return 0;
}

static int parse_dist_rman_tx(xmlNodePtr distp,
			      struct dist_rman_tx_cfg *txcfg)
{
	char *ptr;

	for_all_sibling_nodes(distp) {
		if ((is_node(distp, BAD_CAST DIST_RIO_PORT_NODE))) {
			ptr = get_attributes(distp,
				BAD_CAST DIST_PORT_NUMBER);
			if (unlikely(ptr == NULL))
				return -EINVAL;
			txcfg->rio_port = strtoul(ptr, NULL, 0);
		} else if ((is_node(distp, BAD_CAST DIST_DID_NODE))) {
			ptr = get_attributes(distp, BAD_CAST DIST_DID_VALUE);
			if (unlikely(ptr == NULL))
				return -EINVAL;
			txcfg->did = strtoul(ptr, NULL, 0);
		} else if ((is_node(distp, BAD_CAST DIST_FQ_NODE))) {
			ptr = get_attributes(distp,
				BAD_CAST DIST_FQ_ID);
			if (unlikely(ptr == NULL))
				return -EINVAL;
			txcfg->fqid = strtoul(ptr, NULL, 0);
			ptr = get_attributes(distp,
				BAD_CAST DIST_FQ_COUNT);
			if (unlikely(ptr == NULL))
				return -EINVAL;
			txcfg->fqs_num = strtoul(ptr, NULL, 0);
			ptr = get_attributes(distp,
				BAD_CAST DIST_FQ_WQ);
			if (unlikely(ptr == NULL))
				return -EINVAL;
			txcfg->wq = strtoul(ptr, NULL, 0);
		} else if ((is_node(distp, BAD_CAST DIST_TRANREF_NODE))) {
			ptr = get_attributes(distp,
				BAD_CAST DIST_TRANREF_NAME);
			if (unlikely(ptr == NULL))
				return -EINVAL;
			strip_blanks(ptr);
			search_member(txcfg->tran, ptr, &fracfg->trans_list);
		}
	}
	return 0;
}

static int parse_dist_rman_to_fman(xmlNodePtr distp,
				   struct dist_rman_to_fman_cfg *r2fcfg)
{
	char *ptr;

	for_all_sibling_nodes(distp) {
		if ((is_node(distp, BAD_CAST DISTREF_NODE))) {
			struct dist_cfg *dist_cfg;
			ptr = get_attributes(distp, BAD_CAST DIST_NAME);
			search_member(dist_cfg, ptr, &fracfg->dists_list);
			if (!dist_cfg)
				return -EINVAL;
			if (dist_cfg->type == DIST_TYPE_RMAN_RX) {
				struct dist_rman_rx_cfg *rmcfg;
				rmcfg = &dist_cfg->dist_rman_rx_cfg;
				r2fcfg->rio_port = rmcfg->rio_port;
				r2fcfg->port_mask = rmcfg->port_mask;
				r2fcfg->sid = rmcfg->sid;
				r2fcfg->sid_mask = rmcfg->sid_mask;
				r2fcfg->fqid = rmcfg->fqid;
				r2fcfg->fq_mode = rmcfg->fq_mode;
				r2fcfg->tran = rmcfg->tran;
				r2fcfg->wq = rmcfg->wq;
			} else if (dist_cfg->type == DIST_TYPE_FMAN_TX) {
				struct dist_fman_tx_cfg *fmcfg;
				fmcfg = &dist_cfg->dist_fman_tx_cfg;
				snprintf(r2fcfg->fman_port_name,
					sizeof(r2fcfg->fman_port_name),
					fmcfg->fman_port_name);
			} else
				return -EINVAL;
		} else if ((is_node(distp, BAD_CAST DIST_RIO_PORT_NODE))) {
			ptr = get_attributes(distp,
				BAD_CAST DIST_PORT_NUMBER);
			if (unlikely(ptr == NULL))
				return -EINVAL;
			r2fcfg->rio_port = strtoul(ptr, NULL, 0);
			ptr = get_attributes(distp, BAD_CAST
					     DIST_RIO_PORT_MASK);
			if (unlikely(ptr == NULL))
				return -EINVAL;
			r2fcfg->port_mask = strtoul(ptr, NULL, 0);
		} else if ((is_node(distp, BAD_CAST DIST_SID_NODE))) {
			ptr = get_attributes(distp, BAD_CAST DIST_SID_VALUE);
			if (unlikely(ptr == NULL))
				return -EINVAL;
			r2fcfg->sid = strtoul(ptr, NULL, 0);
			ptr = get_attributes(distp, BAD_CAST DIST_SID_MASK);
			if (unlikely(ptr == NULL))
				return -EINVAL;
			r2fcfg->sid_mask = strtoul(ptr, NULL, 0);
		} else if ((is_node(distp, BAD_CAST DIST_FQ_NODE))) {
			ptr = get_attributes(distp,
				BAD_CAST DIST_FQ_ID);
			if (unlikely(ptr == NULL))
				return -EINVAL;
			r2fcfg->fqid = strtoul(ptr, NULL, 0);
			ptr = get_attributes(distp,
				BAD_CAST DIST_FQ_MODE);
			if (unlikely(ptr == NULL))
				return -EINVAL;
			if (!strcmp(ptr, FQ_MODE_STR[DIRECT]))
				r2fcfg->fq_mode = DIRECT;
			else
				r2fcfg->fq_mode = ALGORITHMIC;
			ptr = get_attributes(distp, BAD_CAST DIST_FQ_WQ);
			if (unlikely(ptr == NULL))
				return -EINVAL;
			r2fcfg->wq = strtoul(ptr, NULL, 0);
		} else if ((is_node(distp, BAD_CAST DIST_TRANREF_NODE))) {
			ptr = get_attributes(distp,
				BAD_CAST DIST_TRANREF_NAME);
			if (unlikely(ptr == NULL))
				return -EINVAL;
			strip_blanks(ptr);
			search_member(r2fcfg->tran, ptr, &fracfg->trans_list);
		} else if ((is_node(distp, BAD_CAST DIST_FMAN_PORT_NODE))) {
			ptr = get_attributes(distp, BAD_CAST FMAN_PORT_NAME);
			if (unlikely(ptr == NULL))
				return -EINVAL;
			snprintf(r2fcfg->fman_port_name,
				sizeof(r2fcfg->fman_port_name), ptr);
		}
	}
	return 0;
}

static int parse_dist_fman_rx(xmlNodePtr distp,
			      struct dist_fman_rx_cfg *rxcfg)
{
	char *ptr;

	for_all_sibling_nodes(distp) {
		if ((is_node(distp, BAD_CAST DIST_FMAN_PORT_NODE))) {
			ptr = get_attributes(distp, BAD_CAST FMAN_PORT_NAME);
			if (unlikely(ptr == NULL))
				return -EINVAL;
			snprintf(rxcfg->fman_port_name,
				sizeof(rxcfg->fman_port_name), ptr);
		} else if ((is_node(distp, BAD_CAST DIST_FQ_NODE))) {
			ptr = get_attributes(distp, BAD_CAST DIST_FQ_WQ);
			if (unlikely(ptr == NULL))
				return -EINVAL;
			rxcfg->wq = strtoul(ptr, NULL, 0);
		}
	}
	return 0;
}

static int parse_dist_fman_tx(xmlNodePtr distp,
			      struct dist_fman_tx_cfg *txcfg)
{
	char *ptr;
	for_all_sibling_nodes(distp) {
		if ((is_node(distp, BAD_CAST DIST_FMAN_PORT_NODE))) {
			ptr = get_attributes(distp, BAD_CAST FMAN_PORT_NAME);
			if (unlikely(ptr == NULL))
				return -EINVAL;
			snprintf(txcfg->fman_port_name,
				sizeof(txcfg->fman_port_name), ptr);
		}  else if ((is_node(distp, BAD_CAST DIST_FQ_NODE))) {
			ptr = get_attributes(distp, BAD_CAST DIST_FQ_COUNT);
			if (unlikely(ptr == NULL))
				return -EINVAL;
			txcfg->fqs_num = strtoul(ptr, NULL, 0);
			ptr = get_attributes(distp, BAD_CAST DIST_FQ_WQ);
			if (unlikely(ptr == NULL))
				return -EINVAL;
			txcfg->wq = strtoul(ptr, NULL, 0);
		}
	}
	return 0;
}

static int parse_dist_fman_to_rman(xmlNodePtr distp,
				   struct dist_fman_to_rman_cfg *f2rcfg)
{
	char *ptr;

	for_all_sibling_nodes(distp) {
		if ((is_node(distp, BAD_CAST DISTREF_NODE))) {
			struct dist_cfg *dist_cfg;
			ptr = get_attributes(distp, BAD_CAST DIST_NAME);
			search_member(dist_cfg, ptr, &fracfg->dists_list);
			if (!dist_cfg)
				return -EINVAL;
			if (dist_cfg->type == DIST_TYPE_RMAN_TX) {
				struct dist_rman_tx_cfg *rmcfg;
				rmcfg = &dist_cfg->dist_rman_tx_cfg;
				f2rcfg->rio_port = rmcfg->rio_port;
				f2rcfg->did = rmcfg->did;
				f2rcfg->tran = rmcfg->tran;
				f2rcfg->wq = rmcfg->wq;
			} else if (dist_cfg->type == DIST_TYPE_FMAN_RX) {
				struct dist_fman_rx_cfg *fmcfg;
				fmcfg = &dist_cfg->dist_fman_rx_cfg;
				snprintf(f2rcfg->fman_port_name,
					sizeof(f2rcfg->fman_port_name),
					fmcfg->fman_port_name);
				f2rcfg->wq = fmcfg->wq;
			} else
				return -EINVAL;
		} else if ((is_node(distp, BAD_CAST DIST_RIO_PORT_NODE))) {
			ptr = get_attributes(distp,
				BAD_CAST DIST_PORT_NUMBER);
			if (unlikely(ptr == NULL))
				return -EINVAL;
			f2rcfg->rio_port = strtoul(ptr, NULL, 0);
		} else if ((is_node(distp, BAD_CAST DIST_DID_NODE))) {
			ptr = get_attributes(distp, BAD_CAST DIST_DID_VALUE);
			if (unlikely(ptr == NULL))
				return -EINVAL;
			f2rcfg->did = strtoul(ptr, NULL, 0);
		} else if ((is_node(distp, BAD_CAST DIST_TRANREF_NODE))) {
			ptr = get_attributes(distp,
				BAD_CAST DIST_TRANREF_NAME);
			if (unlikely(ptr == NULL))
				return -EINVAL;
			strip_blanks(ptr);
			search_member(f2rcfg->tran, ptr, &fracfg->trans_list);
		} else if ((is_node(distp, BAD_CAST DIST_FMAN_PORT_NODE))) {
			ptr = get_attributes(distp, BAD_CAST FMAN_PORT_NAME);
			if (unlikely(ptr == NULL))
				return -EINVAL;
			snprintf(f2rcfg->fman_port_name,
				sizeof(f2rcfg->fman_port_name), ptr);
		}
	}
	return 0;
}

static int parse_dist(xmlNodePtr dist_node, struct list_head *dist_list)
{
	char *name;
	char *type;
	int err;
	xmlNodePtr distp;
	struct dist_cfg *dist_cfg;

	if (!dist_node)
		return -EINVAL;

	name = get_attributes(dist_node, BAD_CAST DIST_NAME);
	search_member(dist_cfg, name, dist_list);
	if (!dist_cfg) {
		dist_cfg = malloc(sizeof(*dist_cfg));
		if (!dist_cfg)
			return -ENOMEM;
		memset(dist_cfg, 0, sizeof(*dist_cfg));
		snprintf(dist_cfg->name, sizeof(dist_cfg->name), name);
		type = get_attributes(dist_node, BAD_CAST DIST_TYPE);
		dist_cfg->type = dist_type_str_to_idx(type);
		list_add_tail(&dist_cfg->node, dist_list);
	}

	/* Update dist configuration */
	distp = dist_node->xmlChildrenNode;
	switch (dist_cfg->type) {
	case DIST_TYPE_RMAN_RX:
		err = parse_dist_rman_rx(distp, &dist_cfg->dist_rman_rx_cfg);
		break;
	case DIST_TYPE_RMAN_TX:
		err = parse_dist_rman_tx(distp, &dist_cfg->dist_rman_tx_cfg);
		break;
	case DIST_TYPE_FMAN_RX:
		err = parse_dist_fman_rx(distp, &dist_cfg->dist_fman_rx_cfg);
		break;
	case DIST_TYPE_FMAN_TX:
		err = parse_dist_fman_tx(distp, &dist_cfg->dist_fman_tx_cfg);
		break;
	case DIST_TYPE_FMAN_TO_RMAN:
		err = parse_dist_fman_to_rman(distp,
			&dist_cfg->dist_fman_to_rman_cfg);
		break;
	case DIST_TYPE_RMAN_TO_FMAN:
		err = parse_dist_rman_to_fman(distp,
			&dist_cfg->dist_rman_to_fman_cfg);
		break;
	default:
		err = -EINVAL;
		error(0, 0, "Distribution(%s) has an invalid"
			" type attribute", dist_cfg->name);
	}
	return err;
}

static int parse_dists_cfg(xmlNodePtr dists_node, struct list_head *list)
{
	char *cfg_file;
	xmlNodePtr cur;

	if (unlikely(!is_node(dists_node, BAD_CAST DISTS_CFG_NODE)))
		return -EINVAL;

	/* First parse the default dists configuration file */
	cur = dists_node->xmlChildrenNode;
	for_all_sibling_nodes(cur) {
		if (unlikely(!is_node(cur, BAD_CAST FRA_DEFCFG_NODE)))
			continue;
		cfg_file = get_attributes(cur, BAD_CAST FRA_DEFCFG_FILE);
		parse_defcfg_file(cfg_file, list, parse_dists_cfg);
	}

	/* Then update the specified  dist configurations */
	cur = dists_node->xmlChildrenNode;
	for_all_sibling_nodes(cur) {
		if (unlikely(!is_node(cur, BAD_CAST DIST_NODE)))
			continue;
		parse_dist(cur, list);
	}
	return 0;
}

static void fra_dists_finish(struct list_head *list)
{
	struct dist_cfg *dist, *temp;

	list_for_each_entry_safe(dist, temp, list, node) {
		list_del(&dist->node);
		free(dist);
	}
}

static void dist_order_cfg_free(struct dist_order_cfg *dist_order_cfg)
{
	if (!dist_order_cfg)
		return;

	if (dist_order_cfg->node.prev && dist_order_cfg->node.next)
		list_del(&dist_order_cfg->node);
	free(dist_order_cfg);
}

static int parse_dist_order(xmlNodePtr cur,
			    struct list_head *dist_order_cfg_list)
{
	char *name;
	xmlNodePtr distrefp;
	struct dist_order_cfg *dist_order_cfg;
	struct dist_cfg *dist_cfg, *next_dist_cfg;
	int err = -ENXIO;

	while (cur) {
		if (!is_node(cur, BAD_CAST POLICY_DISTORDER_NODE)) {
			cur = cur->next;
			continue;
		}

		dist_order_cfg = malloc(sizeof(struct dist_order_cfg));
		if (!dist_order_cfg)
			return -ENOMEM;
		memset(dist_order_cfg, 0, sizeof(*dist_order_cfg));

		distrefp = cur->xmlChildrenNode;
		dist_cfg = dist_order_cfg->dist_cfg;
		while (distrefp) {
			if (!is_node(distrefp, BAD_CAST POLICY_DISTREF_NODE)) {
				distrefp = distrefp->next;
				continue;
			}

			name = get_attributes(distrefp, BAD_CAST
					      POLICY_DISTREF_NAME);
			search_member(next_dist_cfg, name, &fracfg->dists_list);
			if (!next_dist_cfg) {
				error(0, 0, "Can not find distribution(%s)\n",
					name);
				goto _err;
			}
			if (!dist_cfg) {
				next_dist_cfg->sequence_number = 1;
				dist_order_cfg->dist_cfg = next_dist_cfg;
			} else {
				dist_cfg->next = next_dist_cfg;
				dist_cfg->next->sequence_number =
					dist_cfg->sequence_number + 1;
			}

			dist_cfg = next_dist_cfg;
			distrefp = distrefp->next;
		}

		if (!dist_order_cfg->dist_cfg) {
			err = -EINVAL;
			goto _err;
		}

		list_add_tail(&dist_order_cfg->node, dist_order_cfg_list);
		cur = cur->next;
	}
	return 0;
_err:
	dist_order_cfg_free(dist_order_cfg);
	return err;
}

static void dist_order_cfg_finish(struct list_head *list)
{
	struct dist_order_cfg *dist_order_cfg, *temp;

	list_for_each_entry_safe(dist_order_cfg, temp, list, node) {
		list_del(&dist_order_cfg->node);
		free(dist_order_cfg);
	}
}

static int parse_policy(xmlNodePtr node, struct list_head *list)
{
	char *name, *enable;
	struct policy_cfg *policy_cfg;

	if (!node)
		return -EINVAL;

	name = get_attributes(node, BAD_CAST POLICY_NAME);
	search_member(policy_cfg, name, list);
	if (!policy_cfg) {
		policy_cfg = malloc(sizeof(*policy_cfg));
		if (!policy_cfg)
			return -ENOMEM;
		memset(policy_cfg, 0, sizeof(*policy_cfg));
		snprintf(policy_cfg->name, sizeof(policy_cfg->name), name);
		INIT_LIST_HEAD(&policy_cfg->dist_order_cfg_list);
		list_add_tail(&policy_cfg->node, list);
	}

	enable = get_attributes(node, BAD_CAST POLICY_ENABLE);
	if (enable && !strcmp(enable, "yes"))
		policy_cfg->enable = 1;
	else
		policy_cfg->enable = 0;

	parse_dist_order(node->xmlChildrenNode,
			&policy_cfg->dist_order_cfg_list);
	return 0;
}

int parse_policies_cfg(xmlNodePtr node, struct list_head *list)
{
	char *cfg_file;
	xmlNodePtr cur;

	if (unlikely(!is_node(node, BAD_CAST POLICIES_CFG_NODE)))
		return -EINVAL;

	/* First parse the default policies configuration file */
	cur = node->xmlChildrenNode;
	for_all_sibling_nodes(cur) {
		if (!is_node(cur, BAD_CAST FRA_DEFCFG_NODE))
			continue;
		cfg_file = get_attributes(cur, BAD_CAST FRA_DEFCFG_FILE);
		parse_defcfg_file(cfg_file, list, parse_policies_cfg);
	}

	/* Then, update the specified  policy configurations */
	cur = node->xmlChildrenNode;
	for_all_sibling_nodes(cur) {
		if (unlikely(!is_node(cur, BAD_CAST POLICY_NODE)))
			continue;
		parse_policy(cur, list);
	}
	return 0;
}

static void fra_policies_finish(struct list_head *list)
{
	struct policy_cfg *policy_cfg, *temp;

	list_for_each_entry_safe(policy_cfg, temp, list, node) {
		list_del(&policy_cfg->node);
		dist_order_cfg_finish(&policy_cfg->dist_order_cfg_list);
		free(policy_cfg);
	}
}

static int parse_rman_cfgfile(char *cfg_file, struct rman_cfg *cfg);

static int parse_rman_cfg(xmlNodePtr cur, struct rman_cfg *cfg)
{
	char *ptr;
	xmlNodePtr cfgptr;
	enum RIO_TYPE type;

	/* First parse the default RMan configuration file */
	cfgptr = cur->xmlChildrenNode;
	while (cfgptr) {
		if (is_node(cfgptr, BAD_CAST FRA_DEFCFG_NODE)) {
			ptr = get_attributes(cfgptr, BAD_CAST FRA_DEFCFG_FILE);
			parse_rman_cfgfile(ptr, cfg);
		}
		cfgptr = cfgptr->next;
	}

	/* Then update the specified RMan configuration */
	cfgptr = cur->xmlChildrenNode;
	while (cfgptr) {
		if ((is_node(cfgptr, BAD_CAST RMAN_FQ_BITS_NODE))) {
			ptr = get_attributes(cfgptr, BAD_CAST RMAN_CFG_TYPE);
			if (unlikely(ptr == NULL))
				return -EINVAL;
			type = rio_type_str_to_idx(ptr);
			ptr = get_attributes(cfgptr, BAD_CAST RMAN_CFG_VALUE);
			if (unlikely(ptr == NULL))
				return -EINVAL;
			cfg->fq_bits[type] = strtoul(ptr, NULL, 0);
		} else if ((is_node(cfgptr, BAD_CAST MD_CREATE_NODE))) {
			ptr = get_attributes(cfgptr, BAD_CAST MD_CREATE_MODE);
			if (unlikely(ptr == NULL))
				return -EINVAL;
			if (!strcmp(ptr, MD_CREATE_MODE_STR[0]))
				cfg->md_create = 0;
			else
				cfg->md_create = 1;
		} else if ((is_node(cfgptr, BAD_CAST RMAN_OSID_NODE))) {
			ptr = get_attributes(cfgptr, BAD_CAST RMAN_CFG_VALUE);
			if (unlikely(ptr == NULL))
				return -EINVAL;
			if (!strcmp(ptr, "yes"))
				cfg->osid = 1;
			else
				cfg->osid = 0;
		} else if ((is_node(cfgptr, BAD_CAST BPID_NODE))) {
			ptr = get_attributes(cfgptr, BAD_CAST RMAN_CFG_TYPE);
			if (unlikely(ptr == NULL))
				return -EINVAL;
			type = rio_type_str_to_idx(ptr);
			ptr = get_attributes(cfgptr, BAD_CAST RMAN_CFG_VALUE);
			if (unlikely(ptr == NULL))
				return -EINVAL;
			cfg->bpid[type] = strtoul(ptr, NULL, 0);
		} else if ((is_node(cfgptr, BAD_CAST SG_BPID_NODE))) {
			ptr = get_attributes(cfgptr, BAD_CAST RMAN_CFG_VALUE);
			if (unlikely(ptr == NULL))
				return -EINVAL;
			cfg->sgbpid = strtoul(ptr, NULL, 0);
		} else if ((is_node(cfgptr, BAD_CAST RMAN_EFQ_NODE))) {
			ptr = get_attributes(cfgptr, BAD_CAST RMAN_CFG_VALUE);
			if (ptr && !strcmp(ptr, "yes"))
				cfg->efq = 1;
			else
				cfg->efq = 0;
		}
		cfgptr = cfgptr->next;
	}
	return 0;
}

static int parse_rman_cfgfile(char *cfg_file, struct rman_cfg *cfg)
{
	xmlErrorPtr xep;
	xmlDocPtr doc;
	xmlNodePtr cur;

	xmlInitParser();
	LIBXML_TEST_VERSION;
	xmlSetStructuredErrorFunc(&xep, fra_cfg_parse_error);
	xmlKeepBlanksDefault(0);

	doc = xmlParseFile(cfg_file);
	if (unlikely(doc == NULL)) {
		error(0, 0, "%s:%hu:%s() xmlParseFile(%s)",
			__FILE__, __LINE__, __func__, cfg_file);
		return -EINVAL;
	}

	cur = xmlDocGetRootElement(doc);
	if (unlikely(cur == NULL)) {
		error(0, 0, "%s:%hu:%s() xml file(%s) empty",
			__FILE__, __LINE__, __func__, cfg_file);
		goto _err;
	}

	if (unlikely(!is_node(cur, BAD_CAST RMAN_CFG_NODE))) {
		error(0, 0, "%s:%hu:%s() xml file(%s) does not"
			"have %s node", __FILE__, __LINE__, __func__,
			cfg_file, RMAN_CFG_NODE);
		goto _err;
	}

	parse_rman_cfg(cur, cfg);
	xmlFreeDoc(doc);
	return 0;

_err:
	xmlFreeDoc(doc);
	return -EINVAL;
}

static int parse_network_cfg(xmlNodePtr cur, struct list_head *list)
{
	char *ptr;
	xmlNodePtr cfgptr;
	struct fra_fman_port_cfg *cfg;

	/* First parse the default Network configuration file */
	cfgptr = cur->xmlChildrenNode;
	while (cfgptr) {
		if (!(is_node(cfgptr, BAD_CAST FRA_DEFCFG_NODE))) {
			cfgptr = cfgptr->next;
			continue;
		}
		ptr = get_attributes(cfgptr, BAD_CAST FRA_DEFCFG_FILE);
		parse_defcfg_file(ptr, list, parse_network_cfg);
		cfgptr = cfgptr->next;
	}

	/* Then update the specified Network configuration */
	cfgptr = cur->xmlChildrenNode;
	while (cfgptr) {
		if (!(is_node(cfgptr, BAD_CAST FMAN_PORT_NODE))) {
			cfgptr = cfgptr->next;
			continue;
		}
		ptr = get_attributes(cfgptr, BAD_CAST FMAN_PORT_NAME);
		search_member(cfg, ptr, list);
		if (!cfg) {
			cfg = malloc(sizeof(*cfg));
			if (!cfg)
				return -ENOMEM;
			memset(cfg, 0, sizeof(*cfg));
			snprintf(cfg->name, sizeof(cfg->name), ptr);
			list_add_tail(&cfg->node, list);
		}

		ptr = get_attributes(cfgptr, BAD_CAST FMAN_PORT_TYPE);
		if (ptr)
			cfg->port_type = (strcmp(ptr, "OFFLINE") == 0) ?
					 fman_offline :
					 (strcmp(ptr, "10G") == 0) ?
					 fman_mac_10g :
					 (strcmp(ptr, "ONIC") == 0) ?
					 fman_onic : fman_mac_1g;

		ptr = (char *)get_attributes(cfgptr, BAD_CAST FMAN_PORT_NUM);
		if (ptr)
			cfg->port_num = strtoul(ptr, NULL, 0);

		ptr = (char *)get_attributes(cfgptr, BAD_CAST FMAN_NUM);
		if (ptr)
			cfg->fman_num = strtoul(ptr, NULL, 0);

		cfgptr = cfgptr->next;
	}
	return 0;
}

static void fra_network_finish(struct list_head *list)
{
	struct fra_fman_port_cfg *port, *temp;

	list_for_each_entry_safe(port, temp, list, node) {
		list_del(&port->node);
		free(port);
	}
}

struct fra_cfg *fra_parse_cfgfile(const char *cfg_file)
{
	xmlErrorPtr xep;
	xmlDocPtr doc;
	xmlNodePtr cur;
	struct fra_cfg *fra_cfg = NULL;
	struct policy_cfg *policy_cfg;

	xmlInitParser();
	LIBXML_TEST_VERSION;
	xmlSetStructuredErrorFunc(&xep, fra_cfg_parse_error);
	xmlKeepBlanksDefault(0);

	doc = xmlParseFile(cfg_file);
	if (unlikely(doc == NULL)) {
		error(0, 0, "%s:%hu:%s() xmlParseFile(%s)",
			__FILE__, __LINE__, __func__, cfg_file);
		return NULL;
	}

	fra_cfg_root_node = xmlDocGetRootElement(doc);
	cur = fra_cfg_root_node;
	if (unlikely(cur == NULL)) {
		error(0, 0, "%s:%hu:%s() xml file(%s) empty",
			__FILE__, __LINE__, __func__, cfg_file);
		goto _err;
	}

	if (unlikely(!is_node(cur, BAD_CAST FRA_CFG_FILE_ROOT_NODE))) {
		error(0, 0, "%s:%hu:%s() xml file(%s) does not"
			"have %s node", __FILE__, __LINE__, __func__,
			cfg_file, FRA_CFG_FILE_ROOT_NODE);
		goto _err;
	}

	fra_cfg = malloc(sizeof(*fra_cfg));
	if (!fra_cfg) {
		error(0, errno, "malloc(fra_cfg memory)");
		goto _err;
	}
	memset(fra_cfg, 0, sizeof(*fra_cfg));
	fracfg = fra_cfg;
	INIT_LIST_HEAD(&fra_cfg->fman_port_cfg_list);
	INIT_LIST_HEAD(&fra_cfg->trans_list);
	INIT_LIST_HEAD(&fra_cfg->dists_list);
	INIT_LIST_HEAD(&fra_cfg->policies_list);

	cur = fra_cfg_root_node->xmlChildrenNode;
	for_all_sibling_nodes(cur) {
		if (is_node(cur, BAD_CAST RMAN_CFG_NODE)) {
			parse_rman_cfg(cur, &fra_cfg->rman_cfg);
			continue;
		}

		if (is_node(cur, BAD_CAST NETWORK_CFG_NODE)) {
			parse_network_cfg(cur,
				&fra_cfg->fman_port_cfg_list);
			continue;
		}
		if (is_node(cur, BAD_CAST TRANS_CFG_NODE)) {
			parse_trans_cfg(cur,
				&fra_cfg->trans_list);
			continue;
		}
	}

	cur = fra_cfg_root_node->xmlChildrenNode;
	for_all_sibling_nodes(cur) {
		if (is_node(cur, BAD_CAST DISTS_CFG_NODE)) {
			parse_dists_cfg(cur, &fra_cfg->dists_list);
			break;
		}
	}

	cur = fra_cfg_root_node->xmlChildrenNode;
	for_all_sibling_nodes(cur) {
		if (is_node(cur, BAD_CAST POLICIES_CFG_NODE)) {
			parse_policies_cfg(cur, &fra_cfg->policies_list);
			break;
		}
	}

	list_for_each_entry(policy_cfg, &fra_cfg->policies_list, node)
		if (policy_cfg->enable) {
			fra_cfg->policy_cfg = policy_cfg;
			break;
		}

	if (!fra_cfg->policy_cfg)
		goto _err;

	return fra_cfg;
_err:
	fra_cfg_release(fra_cfg);
	xmlFreeDoc(doc);
	return NULL;
}

void fra_cfg_release(struct fra_cfg *fra_cfg)
{
	if (!fra_cfg)
		return;

	fra_network_finish(&fra_cfg->fman_port_cfg_list);
	fra_trans_finish(&fra_cfg->trans_list);
	fra_dists_finish(&fra_cfg->dists_list);
	fra_policies_finish(&fra_cfg->policies_list);
	free(fra_cfg);
}
