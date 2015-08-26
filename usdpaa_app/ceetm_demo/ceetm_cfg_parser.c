/* Copyright (c) 2013 Freescale Semiconductor, Inc.
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

#include "ceetm_cfg_parser.h"
#include <libxml/parser.h>
#include <error.h>

/* The sequence of interpreting CEETM configuration xml file is
 * ceetm, then lni, then channel, then cq or grouA/B
 * The sibling of the same type of node is linked in a list
 * contained in the structure of parent node
 * After interpretation, the whole information can be accessed through
 * a single entry pointed to by pointer ceetm_cfg_root
 */

#define CEETM_CFG_FILE_ROOT_NODE	("ceetm")
#define CEETM_CFG_LNI_NODE		("lni")
#define CEETM_CFG_CHANNEL_NODE		("channel")
#define CEETM_CFG_SHAPER		("control")
#define CEETM_CFG_CHANNEL_NA_GRP	("group")
#define CEETM_CFG_GROUPA_NODE		("groupA")
#define CEETM_CFG_GROUPB_NODE		("groupB")
#define CEETM_CFG_CQ_NODE		("cq")
#define CEETM_CFG_CQ_NA_IDX		("idx")
#define CEETM_CFG_CQ_NA_WEIGHT		("weight")
#define CEETM_CFG_SHAPER_COMMIT		("cr")
#define CEETM_CFG_SHAPER_EXCESS		("er")
#define CEETM_CFG_CQ_NA_OP		("op")
#define CEETM_GROUP_TYPE_A		0
#define CEETM_GROUP_TYPE_B		1
#define KILOBIT				1000
#define MEGABIT				1000000
#define GIGABIT				1000000000

static struct ceetm_lni_info *ceetm_cfg_root;

#define for_all_sibling_nodes(node)	\
	for (; unlikely(node != NULL); node = node->next)

static inline int is_node(xmlNodePtr node, xmlChar *name)
{
	return xmlStrcmp(node->name, name) ? 0 : 1;
}

static void *get_attributes(xmlNodePtr node, xmlChar *attr)
{
	char *atr = (char *)xmlGetProp(node, attr);

	return atr;
}

static uint32_t calc_ratelimit(char *p)
{
	uint32_t rate;
	char last_char;

	rate = atoi(p);
	last_char = p[strlen(p) - 1];
	switch (last_char) {
	case 'k':
	case 'K':
		rate = rate * KILOBIT;
		break;
	case 'm':
	case 'M':
		rate = rate * MEGABIT;
		break;
	case 'g':
	case 'G':
		rate = rate * GIGABIT;
		break;
	default:
		break;
	}

	return rate;
}

int ceetm_cfg_cq_parse(xmlNodePtr cur, struct ceetm_cq_info **info)
{
	struct ceetm_cq_info *cq;
	uint8_t idx;
	uint8_t weight;
	uint8_t er_eligible, cr_eligible;
	char *tmp;

	tmp = (char *)get_attributes(cur, BAD_CAST CEETM_CFG_CQ_NA_IDX);
	if (!tmp) {
		fprintf(stderr, "%s:%hu:%s() error: "
				"attrtibute idx is absent in class queue\n",
				__FILE__, __LINE__, __func__);
		return -EINVAL;
	}
	idx = atoi(tmp);

	if (idx < 0 || idx > 15) {
		fprintf(stderr, "%s:%hu:%s() error: "
				"attrtibute idx is out of range "
				"(0-15) for class queue in class queue\n",
				__FILE__, __LINE__, __func__);
		return -EINVAL;
	}

	if (idx < 8) {
		tmp = (char *)get_attributes(cur, BAD_CAST CEETM_CFG_CQ_NA_OP);
		if (tmp) {
			if (!strcmp(tmp, "cr")) {
				cr_eligible = 1;
				er_eligible = 0;
			} else if (!strcmp(tmp, "er")) {
				cr_eligible = 0;
				er_eligible = 1;
			} else if (!strcmp(tmp, "both")) {
				cr_eligible = 1;
				er_eligible = 1;
			} else if (!strcmp(tmp, "none")) {
				cr_eligible = 0;
				er_eligible = 0;
			} else {
				fprintf(stderr, "%s:%hu:%s() error: "
						"opcode should be 'cr', 'er' "
						"'both' or 'none'\n",
						__FILE__, __LINE__, __func__);
				return -EINVAL;
			}
		} else {
			cr_eligible = 1;
			er_eligible = 1;
		}
	} else {
		tmp = (char *)get_attributes(cur,
				BAD_CAST CEETM_CFG_CQ_NA_WEIGHT);
		if (tmp) {
			weight = atoi(tmp);
			if (weight < 1 || weight > 255) {
				fprintf(stderr, "%s:%hu:%s() error: "
					"attrtibute weight out of "
					"range(1-255) in class queue\n",
					__FILE__, __LINE__, __func__);
				return -EINVAL;
			}
		} else {
			weight = 1;
		}
	}

	cq = malloc(sizeof(struct ceetm_cq_info));
	cq->idx = idx;
	cq->cr_eligible = cr_eligible;
	cq->er_eligible = er_eligible;
	cq->weight = weight;
	*info = cq;

	return 0;
}

int ceetm_cfg_group_parse(xmlNodePtr cur, struct ceetm_group_info **info)
{
	struct ceetm_group_info *group;
	uint8_t idx;
	uint8_t er_eligible, cr_eligible;
	char *tmp;

	tmp = (char *)get_attributes(cur, BAD_CAST CEETM_CFG_CQ_NA_IDX);
	if (!tmp) {
		fprintf(stderr, "%s:%hu:%s() error: "
				"attrtibute idx is absent in CQ group\n",
				__FILE__, __LINE__, __func__);
		return -EINVAL;
	}
	idx = tmp[0] - '0';

	if (idx < 0 || idx > 7) {
		fprintf(stderr, "%s:%hu:%s() error: attrtibute idx is out of "
				"range(0-7) for group in class queue group\n",
				__FILE__, __LINE__, __func__);
		return -EINVAL;
	}

	tmp = (char *)get_attributes(cur, BAD_CAST CEETM_CFG_CQ_NA_OP);
	if (tmp) {
		if (!strcmp(tmp, "cr")) {
			cr_eligible = 1;
			er_eligible = 0;
		} else if (!strcmp(tmp, "er")) {
			cr_eligible = 0;
			er_eligible = 1;
		} else if (!strcmp(tmp, "both")) {
			cr_eligible = 1;
			er_eligible = 1;
		} else if (!strcmp(tmp, "none")) {
			cr_eligible = 0;
			er_eligible = 0;
		} else {
			fprintf(stderr, "%s:%hu:%s() error: opcode should be "
					"'cr', 'er', 'both' or 'none'\n",
					__FILE__, __LINE__, __func__);
			return -EINVAL;
		}
	} else {
		cr_eligible = 1;
		er_eligible = 1;
	}

	group = malloc(sizeof(struct ceetm_group_info));
	group->idx = idx;
	group->cr_eligible = cr_eligible;
	group->er_eligible = er_eligible;
	*info = group;

	return 0;
}

int ceetm_cfg_channel_parse(xmlNodePtr cur, struct ceetm_channel_info **info)
{
	struct ceetm_channel_info *channel;
	uint8_t num_group;
	uint8_t is_shaping = 0;
	uint32_t cr = 0;
	uint32_t er = 0;
	char *tmp;

	tmp = (char *)get_attributes(cur, BAD_CAST CEETM_CFG_SHAPER);
	if (unlikely(!tmp) ||
		(strcmp(tmp, "shaped") && strcmp(tmp, "unshaped"))) {
		fprintf(stderr, "%s:%hu:%s() error: attrtibute control"
				"is neither <shaped> nor <unshaped> in %s\n",
				__FILE__, __LINE__, __func__,
				CEETM_CFG_CHANNEL_NODE);
		return -EINVAL;
	}
	if (!strcmp(tmp, "shaped")) {
		is_shaping = 1;

		tmp = (char *)get_attributes(cur,
				BAD_CAST CEETM_CFG_SHAPER_COMMIT);
		if (unlikely(!tmp)) {
			fprintf(stderr, "%s:%hu:%s() error: "
					"'cr' are necessary for shaped %s.\n",
					__FILE__, __LINE__, __func__,
					CEETM_CFG_CHANNEL_NODE);
			return -EINVAL;
		}
		cr = calc_ratelimit(tmp);

		tmp = (char *)get_attributes(cur,
				BAD_CAST CEETM_CFG_SHAPER_EXCESS);
		if (unlikely(!tmp)) {
			fprintf(stderr, "%s:%hu:%s() error: "
					"'er' are necessary for shaped %s.\n",
					__FILE__, __LINE__, __func__,
					CEETM_CFG_CHANNEL_NODE);
			return -EINVAL;
		}
		er = calc_ratelimit(tmp);
	}

	tmp = (char *)get_attributes(cur, BAD_CAST CEETM_CFG_CHANNEL_NA_GRP);
	if (!tmp)
		num_group = 0;
	else
		num_group = tmp[0] - '0';

	if (unlikely(num_group > 2)) {
		fprintf(stderr, "%s:%hu:%s() error: attrtibute group"
				"is out of range(0-2) in %s node\n",
				__FILE__, __LINE__, __func__,
				CEETM_CFG_CHANNEL_NODE);
		return -EINVAL;
	}

	channel = calloc(1, sizeof(struct ceetm_channel_info));
	channel->is_shaping = is_shaping;
	channel->cr = cr;
	channel->er = er;
	channel->num_group = num_group;
	*info = channel;

	return 0;
}

int ceetm_cfg_lni_parse(xmlNodePtr cur, struct ceetm_lni_info **info)
{
	struct ceetm_lni_info *lni;
	uint8_t is_shaping = 0;
	uint32_t cr = 0;
	uint32_t er = 0;
	char *tmp;

	tmp = (char *)get_attributes(cur, BAD_CAST CEETM_CFG_SHAPER);
	if (unlikely(!tmp) ||
		(strcmp(tmp, "shaped") && strcmp(tmp, "unshaped"))) {
		fprintf(stderr, "%s:%hu:%s() error: attrtibute control"
				"is neither <shaped> nor <unshaped> in %s\n",
				__FILE__, __LINE__, __func__,
				CEETM_CFG_LNI_NODE);
		return -EINVAL;
	}
	if (!strcmp(tmp, "shaped")) {
		is_shaping = 1;

		tmp = (char *)get_attributes(cur,
				BAD_CAST CEETM_CFG_SHAPER_COMMIT);
		if (unlikely(!tmp)) {
			fprintf(stderr, "%s:%hu:%s() error: "
					"'cr' are necessary for shaped %s\n",
					__FILE__, __LINE__, __func__,
					CEETM_CFG_LNI_NODE);
			return -EINVAL;
		}
		cr = calc_ratelimit(tmp);

		tmp = (char *)get_attributes(cur,
				BAD_CAST CEETM_CFG_SHAPER_EXCESS);
		if (unlikely(!tmp)) {
			fprintf(stderr, "%s:%hu:%s() error: "
					"'er' are necessary for shaped %s.\n",
					__FILE__, __LINE__, __func__,
					CEETM_CFG_LNI_NODE);
			return -EINVAL;
		}
		er = calc_ratelimit(tmp);
	}

	lni = malloc(sizeof(struct ceetm_lni_info));
	lni->is_shaping = is_shaping;
	lni->cr = cr;
	lni->er = er;
	*info = lni;

	return 0;
}


struct ceetm_lni_info *ceetm_cfg_parse(const char *cfg_file)
{
	xmlErrorPtr xep;
	xmlDocPtr doc;
	xmlNodePtr cur;
	xmlNodePtr elem_cur;
	struct ceetm_lni_info *lni;
	int ret;

	xmlInitParser();
	LIBXML_TEST_VERSION;
	xmlSetStructuredErrorFunc(&xep, NULL);
	xmlKeepBlanksDefault(0);

	doc = xmlParseFile(cfg_file);
	if (unlikely(doc == NULL)) {
		fprintf(stderr, "%s:%hu:%s() error: xmlParseFile(%s)\n",
				__FILE__, __LINE__, __func__, cfg_file);
		return NULL;
	}

	cur = xmlDocGetRootElement(doc);
	if (unlikely(cur == NULL)) {
		fprintf(stderr, "%s:%hu:%s() error: xml file(%s) empty\n",
				__FILE__, __LINE__, __func__, cfg_file);
		goto xml_err;
	}

	if (unlikely(!is_node(cur, BAD_CAST CEETM_CFG_FILE_ROOT_NODE))) {
		fprintf(stderr, "%s:%hu:%s() error: xml file(%s) does not"
			"have %s node\n", __FILE__, __LINE__, __func__,
			cfg_file, CEETM_CFG_FILE_ROOT_NODE);
		goto xml_err;
	}
	cur = cur->xmlChildrenNode;
	/* only one lni is permitted for each ethernet interface */
	if (unlikely(!is_node(cur, BAD_CAST CEETM_CFG_LNI_NODE))) {
		fprintf(stderr, "%s:%hu:%s() error: xml file(%s) does not"
			"have %s node\n", __FILE__, __LINE__, __func__,
			cfg_file, CEETM_CFG_LNI_NODE);
		goto xml_err;
	}

	ret = ceetm_cfg_lni_parse(cur, &lni);
	if (ret)
		goto elem_err;

	INIT_LIST_HEAD(&lni->channel_list);
	cur = cur->xmlChildrenNode;
	for_all_sibling_nodes(cur)  {
		struct ceetm_channel_info *channel;

		ret = ceetm_cfg_channel_parse(cur, &channel);
		if (ret)
			goto elem_err;

		list_add_tail(&channel->list, &lni->channel_list);

		INIT_LIST_HEAD(&channel->cq_list);
		elem_cur = cur->xmlChildrenNode;
		for_all_sibling_nodes(elem_cur) {
			struct ceetm_cq_info *cq;
			struct ceetm_group_info *group;

			if (is_node(elem_cur, BAD_CAST CEETM_CFG_CQ_NODE)) {
				ret = ceetm_cfg_cq_parse(elem_cur, &cq);
				if (ret)
					goto elem_err;

				if (cq->idx >= 8 && channel->num_group == 0) {
					fprintf(stderr, "%s:%hu:%s() error: "
						"No group in the channel.\n",
						__FILE__, __LINE__, __func__);
					goto elem_err;
				}

				list_add_tail(&cq->list, &channel->cq_list);
			} else if (is_node(elem_cur,
					BAD_CAST CEETM_CFG_GROUPA_NODE)) {
				ret = ceetm_cfg_group_parse(elem_cur, &group);
				if (ret)
					goto elem_err;
				channel->group_a = group;
			} else if (is_node(elem_cur,
					BAD_CAST CEETM_CFG_GROUPB_NODE)) {
				ret = ceetm_cfg_group_parse(elem_cur, &group);
				if (ret)
					goto elem_err;
				channel->group_b = group;
			} else {
				continue;
			}
		}
		if (channel->num_group && !channel->group_a) {
			fprintf(stderr, "%s:%hu:%s() error: "
				"No definition for group A in the channel.\n",
				__FILE__, __LINE__, __func__);
			goto elem_err;
		}
		if (channel->num_group > 1 && !channel->group_b) {
			fprintf(stderr, "%s:%hu:%s() error: "
				"No definition for group B in the channel.\n",
				__FILE__, __LINE__, __func__);
			goto elem_err;
		}
	}

	ceetm_cfg_root = lni;
	return lni;

elem_err:
	ceetm_cfg_clean();
xml_err:
	xmlFreeDoc(doc);
	return NULL;
}

/* Release memory that was allocated to save CEETM configuration information */
void ceetm_cfg_clean(void)
{
	struct ceetm_channel_info *channel;
	struct ceetm_cq_info *cq;

	if (ceetm_cfg_root == NULL)
		return;

	list_for_each_entry(channel, &ceetm_cfg_root->channel_list, list) {
		list_for_each_entry(cq, &channel->cq_list, list) {
			free(cq);
		}
		free(channel);
	}
	free(ceetm_cfg_root);
	ceetm_cfg_root = NULL;
}
