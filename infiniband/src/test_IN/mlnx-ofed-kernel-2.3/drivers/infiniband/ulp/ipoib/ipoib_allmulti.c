/*
 * Copyright (c) 2014 Mellanox, LTD. All rights reserved.
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

#include <linux/moduleparam.h>
#include <linux/completion.h>
#include <linux/slab.h>
#include "ipoib.h"

#ifdef CONFIG_INFINIBAND_IPOIB_DEBUG
int allmulti_debug_level;

module_param(allmulti_debug_level, int, 0644);
MODULE_PARM_DESC(allmulti_debug_level,
		 "Enable multicast promisc debug tracing if > 0");
#endif

#define REGISTRATION_DONE 1

union mcp_action_data {
	u8 gid[16];
	struct ipoib_mc_rule rule;
};

struct promisc_mc_work {
	struct delayed_work work;
	struct ipoib_dev_priv *priv;
	enum mc_action action;
	union mcp_action_data data;
};

/* This struct is used for rules validation, so that during callback execution
 * the queue will not handle any other requests that might involve rules.
 * The usual get_table operation is not executed this way because it has no
 * effect on the rules or on MCG creation/deletion processing.
 */
struct promisc_mc_query_context {
	struct ipoib_dev_priv *priv;
	struct ib_sa_query *query;
	int query_id;
	struct completion done;
};

/*
 * Returns the pkey part of the given MGID.
 */
static u16 mgid_get_pkey(u8 mgid[16])
{
	u16 ret;
	ret = (mgid[4] << 8) + mgid[5];
	return ret;
}

/*
* Checks whether or not the provided mgid is an IPoIB mgid.
* Returns 1 if it's either IPoIB ipv4 mgid or IPoIB ipv6 mgid, else 0.
*/
static int is_ipoib_mgid(u8 *mgid, u8 *broadcast)
{
	u8 *bcast_gid =  broadcast + 4;
	u8 ipoib_ipv4_prefix[4] = {bcast_gid[0], bcast_gid[1], 0x40, 0x1b};
	u8 ipoib_ipv6_prefix[4] = {bcast_gid[0], bcast_gid[1], 0x60, 0x1b};

	if (!memcmp(mgid, ipoib_ipv4_prefix, 4))
		return 1;

	if (!memcmp(mgid, ipoib_ipv6_prefix, 4))
		return 1;

	return 0;
}

/*
 * Compares two masked u8 arrays.
 * Returns an integer less than, equals to or greater than 0 if the first masked
 * array is less than, equals or is greater than the second.
 */
static int guid_mask_cmp(u8 *first, u8 *second, u8 *mask, int len)
{
	int i;

	for (i = 0; i < len; i++) {
		if ((first[i] & mask[i]) != (second[i] & mask[i]))
			return (first[i] & mask[i]) - (second[i] & mask[i]);
	}
	return 0;
}

static int ipoib_mc_add_rule(struct rb_root *root,
			     struct ipoib_mc_rule *rule)
{
	struct rb_node **n , *pn = NULL;
	n = &root->rb_node;

	while (*n) {
		struct ipoib_mc_rule *trule;
		int ret;

		pn = *n;
		trule = rb_entry(pn, struct ipoib_mc_rule, rb_node);

		ret = guid_mask_cmp(rule->mgid, trule->mgid, trule->mask,
			       sizeof(union ib_gid));
		if (ret < 0)
			n = &pn->rb_left;
		else if (ret > 0)
			n = &pn->rb_right;
		else
			return -EEXIST;
	}

	rb_link_node(&rule->rb_node, pn, n);
	rb_insert_color(&rule->rb_node, root);
	return 0;
}

static struct ipoib_mc_rule *ipoib_rule_find(union ib_gid gid,
					     struct rb_root *root)
{
	struct rb_node *n;

	n = root->rb_node;

	while (n) {
		struct ipoib_mc_rule *rule;
		int ret;

		rule = rb_entry(n, struct ipoib_mc_rule, rb_node);

		ret = guid_mask_cmp(gid.raw, rule->mgid, rule->mask,
			       sizeof(union ib_gid));
		if (ret < 0)
			n = n->rb_left;
		else if (ret > 0)
			n = n->rb_right;
		else
			return rule;
	}

	return NULL;
}

static void update_rules_tree(struct promisc_mc *promisc)
{
	struct rb_node **n, *m;
	struct ipoib_mc_rule *rule;

	/* Cleanup previous rules */
	m = rb_first(&promisc->rules_tree);
	while (m) {
		rule = rb_entry(m, struct ipoib_mc_rule, rb_node);
		m = rb_next(m);
		rb_erase(&rule->rb_node, &promisc->rules_tree);
		kfree(rule);
	}

	/* Move rules from new_rules_tree to rules_tree */
	n = &promisc->new_rules_tree.rb_node;

	while (*n) {
		rule = rb_entry(*n, struct ipoib_mc_rule, rb_node);
		*n = rb_next(*n);
		rb_erase(&rule->rb_node, &promisc->new_rules_tree);
		ipoib_mc_add_rule(&promisc->rules_tree, rule);
	}

	return;
}

/*
 * Returns 1 if the provided gid is a part of IPoIB's multicast tree.
 * This will help to avoid a double attach, since a double detach is not
 * possible and causes lockup.
 */
static int is_mcast_in_multicast_tree(struct ipoib_dev_priv *priv, u8 gid[16])
{
	struct ipoib_mcast *mcast;
	unsigned long flags;

	spin_lock_irqsave(&priv->lock, flags);
	mcast = __ipoib_mcast_find(priv->dev, gid, &priv->multicast_tree);
	if (mcast) {
		ipoib_dbg_mcp(priv,
			      "%s %pI6 MCG already joined by ipoib multicast\n",
			      __func__, gid);
		spin_unlock_irqrestore(&priv->lock, flags);
		return 1;
	}
	spin_unlock_irqrestore(&priv->lock, flags);
	return 0;
}

/*
 * This function receives join and leave notifications from ipoib_multicast.
 * If there's a DONT_JOIN rule regarding this MGid, the notification will NOT
 * be sent to the user.
 */
void ipoib_mc_raise_notification(struct ipoib_dev_priv *priv,
				 struct ib_sa_mcmember_rec *rec,
				 enum notification_type type)
{
	struct ipoib_mc_rule *rule;

	rule = ipoib_rule_find(rec->mgid, &priv->promisc.rules_tree);

	if (!rule || rule->join_status != DONT_JOIN) {
		switch (type) {
		case NOTIFY_JOIN:
			ipoib_mc_join_notify(priv, rec);
			return;
		case NOTIFY_LEAVE:
			ipoib_mc_leave_notify(priv, rec);
			return;
		case NOTIFY_DETAILS:
			ipoib_mcg_details_notify(priv, rec);
			return;
		default:
			ipoib_warn(priv, "Illegal notification type (%d)\n",
				   type);
		}
	}
}

static int ipoib_queue_mcp_work(struct promisc_mc *promisc_mc,
				union mcp_action_data *data,
				enum mc_action action, int delay)
{
	struct promisc_mc_work *work;
	struct ipoib_dev_priv *priv =
		container_of(promisc_mc, struct ipoib_dev_priv, promisc);

	if (!priv->promisc.workqueue)
		return -ENOMEM;

	work = kzalloc(sizeof(*work), GFP_KERNEL);
	if (!work) {
		ipoib_err(priv, "%s: couldn't allocate work struct\n",
			  __func__);
		return -ENOMEM;
	}
	work->priv = priv;
	work->action = action;
	if (data)
		work->data = *data;

	INIT_DELAYED_WORK(&work->work, ipoib_mcp_work_handler);
	queue_delayed_work(priv->promisc.workqueue, &work->work, delay * HZ);
	return 0;
}

int ipoib_queue_mcg_create_work(struct promisc_mc *promisc, u8 gid[16])
{
	return ipoib_queue_mcp_work(promisc, (union mcp_action_data *)gid,
				    MC_CREATE_ACTION, 0);
}

int ipoib_queue_mcg_delete_work(struct promisc_mc *promisc, u8 gid[16])
{
	return ipoib_queue_mcp_work(promisc, (union mcp_action_data *)gid,
				    MC_DELETE_ACTION, 0);
}

int ipoib_queue_add_rule(struct promisc_mc *promisc, struct ipoib_mc_rule rule)
{
	return ipoib_queue_mcp_work(promisc, (union mcp_action_data *)&rule,
				    MC_ADD_RULE_ACTION, 0);
}

static int callback_input_check(int status, struct ib_inform_info *info,
			 struct ib_sa_notice *notice, int trap_number,
			 unsigned long int flags)
{
	/* We trap reset events ourselves */
	if (status == -ENETRESET) {
		pr_info("%pS: status is -ENETRESET\n",
			__builtin_return_address(0));
		return -ENETRESET;
	}

	if (status == -EAGAIN) {
		/* Can happens during SM handover:
		 * If IPoIB tries to reregister before core level acquired
		 * an updated AH, the registration will fail with this error
		 * code and IPoIB should try again. */
		pr_info("%pS: status is -EAGAIN\n",
			__builtin_return_address(0));
		return -EAGAIN;
	}

	if (!info) {
		pr_err("%pS: info is NULL, status is %d\n",
		       __builtin_return_address(0), status);
		return -EINVAL;
	}

	if (info->trap_number != trap_number) {
		pr_warn("%pS: unexpected trap number %d\n",
			__builtin_return_address(0), info->trap_number);
		return -EINVAL;
	}

	if (!notice) {
		/* it is ok to get notice as NULL after the registration */
		pr_debug("%pS: notice is NULL, status is %d\n",
			 __builtin_return_address(0), status);
		return REGISTRATION_DONE;
	}

	if (unlikely(!test_bit(IPOIB_ALL_MULTI, &flags))) {
		pr_warn("%pS: called with ALL_MULTI flag unset\n",
			__builtin_return_address(0));
		return -EPERM;
	}
	return 0;
}

static int ipoib_inform_info_delete_cb(int status, struct ib_inform_info *info,
				struct ib_sa_notice *notice)
{
	struct ipoib_dev_priv *priv =  info->context;
	struct ib_sa_notice_data_gid *gid_data;
	int ret;

	ret = callback_input_check(status, info, notice,
				   IB_SA_SM_TRAP_DELETE_MC_GROUP, priv->flags);

	if (likely(!ret))
		goto parse_input;

	switch (ret) {
	case -ENETRESET:
		return 0;
	case -EAGAIN:
		ib_sa_unregister_inform_info(priv->promisc.create_mcg);
		if (priv->promisc.registration_delay >=
		    IPOIB_MAX_BACKOFF_SECONDS) {
			ipoib_err(priv, "%s: Registration timeout exceeded, giving up",
				  __func__);
			complete(&priv->promisc.registration_completion);
			return -1;
		}
		priv->promisc.registration_delay *= 2;
		if (!ipoib_queue_mcp_work(&priv->promisc, NULL,
					  MC_REGISTER_DELETE,
					  priv->promisc.registration_delay)) {
			ipoib_err(priv, "%s: Registration falied to queue\n",
				  __func__);
			ib_sa_unregister_inform_info(priv->promisc.create_mcg);
			complete(&priv->promisc.registration_completion);
		}
		return -1;
	case REGISTRATION_DONE:
		priv->promisc.registration_delay = 1;
		priv->promisc.registration_succeeded = 1;
		complete(&priv->promisc.registration_completion);
		return 0;
	default:
		ipoib_warn(priv, "%s: got status %d\n", __func__, ret);
		return 0;
	}

parse_input:
	gid_data = (struct ib_sa_notice_data_gid *)&notice->data_details;
	if (!gid_data) {
		pr_err("%s: gid_data is NULL\n", __func__);
		return 0;
	}

	/* If this is not an IPoIB MCG, return */
	if (!is_ipoib_mgid(gid_data->gid, priv->dev->broadcast)) {
		ipoib_dbg_mcp(priv, "%s: %pI6 is not an IPoIB mgid, skipping\n",
			      __func__, gid_data->gid);
		return 0;
	}

	ipoib_dbg_mcp(priv, "%s: MGID = %pI6\n", __func__, gid_data->gid);

	ipoib_queue_mcg_delete_work(&priv->promisc, gid_data->gid);
	return 0;
}

static int ipoib_inform_info_create_cb(int status, struct ib_inform_info *info,
			 struct ib_sa_notice *notice)
{
	struct ipoib_dev_priv *priv	=  info->context;
	struct ib_sa_notice_data_gid *gid_data;
	int ret;
	u16 pkey;

	ret = callback_input_check(status, info, notice,
				   IB_SA_SM_TRAP_CREATE_MC_GROUP, priv->flags);

	if (likely(!ret))
		goto parse_input;

	switch (ret) {
	case -ENETRESET:
		return 0;
	case -EAGAIN:
		if (priv->promisc.registration_delay >=
		    IPOIB_MAX_BACKOFF_SECONDS) {
			ipoib_err(priv, "%s: Registration timeout exceeded, giving up",
				  __func__);
			priv->promisc.create_mcg = NULL;
			complete(&priv->promisc.registration_completion);
			return -1;
		}
		priv->promisc.registration_delay *= 2;
		if (ipoib_queue_mcp_work(&priv->promisc, NULL,
					 MC_REGISTER_CREATE,
					 priv->promisc.registration_delay)) {
			ipoib_err(priv, "%s: Registration falied to queue\n",
				  __func__);
			complete(&priv->promisc.registration_completion);
		}
		return -1;
	case REGISTRATION_DONE:
		if (ipoib_queue_mcp_work(&priv->promisc, NULL,
					 MC_REGISTER_DELETE,
					 priv->promisc.registration_delay)) {
			ipoib_err(priv, "%s: Registration falied to queue\n",
				  __func__);
			ib_sa_unregister_inform_info(priv->promisc.create_mcg);
			complete(&priv->promisc.registration_completion);
		}
		return 0;
	default:
		ipoib_warn(priv, "%s: got status %d\n", __func__, ret);
		return 0;
	}

parse_input:
	gid_data = (struct ib_sa_notice_data_gid *)&notice->data_details;
	if (!gid_data) {
		pr_err("%s: gid_data is NULL\n", __func__);
		return 0;
	}

	ipoib_dbg_mcp(priv, "%s: MGID = %pI6\n", __func__, gid_data->gid);

	/* If this is not an IPoIB MCG, return */
	if (!is_ipoib_mgid(gid_data->gid, priv->dev->broadcast)) {
		ipoib_dbg_mcp(priv, "%s: %pI6 is not an IPoIB mgid, skipping\n",
			      __func__, gid_data->gid);
		return 0;
	}

	/* If the MGid doesn't match the priv's PKey, return */
	pkey = mgid_get_pkey(gid_data->gid);
	if (priv->pkey != pkey) {
		ipoib_dbg_mcp(priv, "%s: %pI6 doesn't match the interface's PKey (%x), skipping\n",
			      __func__, gid_data->gid, priv->pkey);
		return 0;
	}

	ipoib_queue_mcg_create_work(&priv->promisc, gid_data->gid);
	return 0;
}

/*
 * Registers to MCG creation / deletion notofications from the SM
 */
static void ipoib_handle_registration(struct ipoib_dev_priv *priv,
				      u16 trap_number)
{
	struct ib_inform_info *mcg;
	int (*callback)(int status, struct ib_inform_info *info,
			struct ib_sa_notice *notice);

	callback = trap_number == IB_SA_SM_TRAP_CREATE_MC_GROUP ?
				  ipoib_inform_info_create_cb :
				  ipoib_inform_info_delete_cb;

	mcg = ib_sa_register_inform_info(&ipoib_sa_client, priv->ca, priv->port,
					 trap_number, GFP_KERNEL, callback,
					 priv);

	if (IS_ERR_OR_NULL(mcg)) {
		pr_err("%s: ib_sa_register_inform_info returned %ld\n",
		       __func__, PTR_ERR(mcg));
		complete(&priv->promisc.registration_completion);
	}

	if (trap_number == IB_SA_SM_TRAP_CREATE_MC_GROUP)
		priv->promisc.create_mcg = mcg;
	else
		priv->promisc.delete_mcg = mcg;
}

/*
 * mc promisc handlers should only run form priv->promisc.work_queue context:
 * void ipoib_mcp_work_handler(struct work_struct *work)
 */
static void ipoib_handle_mcg_create(struct ipoib_dev_priv *priv,
				    u8 gid[16])
{
	struct ipoib_mcast *mcast;
	u8 join_status = NON_MEMBER;
	struct ipoib_mc_rule *rule;

	if (is_mcast_in_multicast_tree(priv, gid))
		return;

	/* alloc new mcast and join it as non member */
	mcast = ipoib_mcast_alloc(priv->dev, 1);
	if (!mcast) {
		ipoib_err(priv, "%s: couldn't allocate mcast group\n",
			  __func__);
		return;
	}
	memcpy(mcast->mcmember.mgid.raw, gid, sizeof(union ib_gid));

	/* Find a rule that matches this group */
	mutex_lock(&priv->promisc.tree_lock);
	rule = ipoib_rule_find(mcast->mcmember.mgid, &priv->promisc.rules_tree);
	if (rule) {
		ipoib_dbg_mcp(priv, "ipoib_handle_mcg_create %s %pI6 RULE was found status %d\n", __func__,
			      mcast->mcmember.mgid.raw, rule->join_status);
		if (rule->join_status == DONT_JOIN) {
			mutex_unlock(&priv->promisc.tree_lock);
			ipoib_mcast_free(mcast);
			return;
		}
		join_status = rule->join_status;
	}
	__ipoib_mcast_add(priv->dev, mcast,
			  &priv->promisc.multicast_tree);
	mutex_unlock(&priv->promisc.tree_lock);
	ipoib_mcast_join(priv->dev, mcast, 0, join_status);
	ipoib_dbg_mcp(priv, "%s %pI6 MCG was added, sent join as %s\n",
		      __func__, gid, status_to_str(join_status));
}

static void ipoib_handle_mcg_delete(struct ipoib_dev_priv *priv,
				    u8 gid[16])
{
	struct ipoib_mcast *mcast;
	mutex_lock(&priv->promisc.tree_lock);
	mcast = __ipoib_mcast_find(priv->dev, gid,
				    &priv->promisc.multicast_tree);
	if (!mcast) {
		mutex_unlock(&priv->promisc.tree_lock);
		ipoib_dbg_mcp(priv,
			      "%s: mcast %pI6 was either not joined by ipoib mc promisc or already removed\n",
			      __func__, gid);
		return;
	}

	rb_erase(&mcast->rb_node, &priv->promisc.multicast_tree);
	mutex_unlock(&priv->promisc.tree_lock);

	ipoib_mcast_leave(priv->dev, mcast);

	ipoib_mcast_free(mcast);
	ipoib_dbg_mcp(priv, "%s %pI6 MCG Removed\n", __func__, gid);
}

static void ipoib_handle_add_rule(struct ipoib_dev_priv *priv,
				  struct ipoib_mc_rule rule)
{
	struct ipoib_mc_rule *new_rule;
	int ret;
	new_rule = kzalloc(sizeof(*new_rule), GFP_KERNEL);

	if (!new_rule) {
		ipoib_err(priv, "%s: couldn't allocate rule struct\n",
			  __func__);
		return;
	}
	*new_rule = rule;
	mutex_lock(&priv->promisc.tree_lock);
	ret = ipoib_mc_add_rule(&priv->promisc.new_rules_tree, new_rule);
	mutex_unlock(&priv->promisc.tree_lock);
	if (ret) {
		pr_err("%s: couldn't add rule (%d)\n", __func__, ret);
		kfree(new_rule);
	}
}

void ipoib_validate_gettable_cb(int status, struct ib_sa_mcmember_rec *resp,
				void *context)
{
	struct promisc_mc_query_context *query_context = context;
	struct ipoib_dev_priv *priv = query_context->priv;
	struct net_device *dev = priv->dev;
	struct ipoib_mc_rule *rule;
	struct ipoib_mcast *mcast = NULL;
	int group_present;
	u8 join_status;

	/* error code from core layer, should be followed by NULL resp. */
	if (status < 0) {
		ipoib_printk(KERN_ERR, priv, "%s: received error status %d\n",
			     __func__, status);
		if (resp)
			ipoib_warn(priv, "%s: resp should be NULL when receiving error status\n",
				   __func__);
		goto out;
	}

	/* This shouldn't be possible with status >= 0 */
	if (!resp) {
		ipoib_warn(priv, "%s: status is %d but resp is NULL\n",
			   __func__, status);
		goto out;
	}

	/* If this is not an IPoIB MCG, return */
	if (!is_ipoib_mgid(resp->mgid.raw, priv->dev->broadcast)) {
		ipoib_dbg_mcp(priv, "%s: %pI6 is not an IPoIB mgid, skipping\n",
			      __func__, resp->mgid.raw);
		goto out;
	}

	/* If the group is joined by IPoIB, return */
	if (is_mcast_in_multicast_tree(priv, resp->mgid.raw))
		goto out;

	mutex_lock(&priv->promisc.tree_lock);
	/* find rule and group if exist */
	rule = ipoib_rule_find(resp->mgid, &priv->promisc.rules_tree);
	ipoib_dbg_mcp(priv, "%s: %s an applicable rule\n", __func__, rule ?
		      "found" : "didn't find");

	join_status = rule ? rule->join_status : NON_MEMBER;
	ipoib_dbg_mcp(priv, "%s: join status is %s", __func__,
		      status_to_str(join_status));

	mcast = __ipoib_mcast_find(dev, resp->mgid.raw,
				   &priv->promisc.multicast_tree);

	group_present = (mcast != NULL);
	ipoib_dbg_mcp(priv, "%s: %s group with mgid %pI6\n", __func__,
		      group_present ? "found" : "didn't find",
		      group_present ? mcast->mcmember.mgid.raw : NULL);

	/* first, handle DONT_JOIN */
	if (rule) {
		if (join_status == DONT_JOIN && group_present) {
			rb_erase(&mcast->rb_node,
				 &priv->promisc.multicast_tree);
			ipoib_mcast_leave(dev, mcast);
			ipoib_mcast_free(mcast);
			goto unlock;
		}
	}
	/* Now we'll need an MCG in any other case: */
	if (!group_present) {
		/* allocate MCG and insert it to the tree */
		mcast = ipoib_mcast_alloc(priv->dev, 1);
		if (!mcast) {
			ipoib_err(priv, "%s: couldnt alloc multicast group %pI6\n",
				  __func__, resp->mgid.raw);
			goto unlock;
		}
		memcpy(mcast->mcmember.mgid.raw, resp->mgid.raw,
		       sizeof(union ib_gid));
		__ipoib_mcast_add(dev, mcast,
				  &priv->promisc.multicast_tree);
	} else if (mcast->mcmember.join_state == join_status) {
		goto unlock;
	}

	/* Now we have the group in the tree, check if need to update status */
	if (group_present)
		ipoib_mcast_leave(dev, mcast);

	/* join with the rule's status */
	ipoib_mcast_join(dev, mcast, 0, join_status);

unlock:
	mutex_unlock(&priv->promisc.tree_lock);
out:
	/* eventually we will get status != 0 */
	if (status != 0)
		complete(&query_context->done);

	return;
}

/*
 * This function uses an internal query, so that the workqueue is blocked until
 * rules validation is complete. This way new rules received during validation
 * will not be dropped, and MCG create/delete notifications received will be
 * processed according to the new rules.
 */
static int ipoib_handle_validate_rules(struct ipoib_dev_priv *priv)
{
	struct ib_sa_mcmember_rec rec = {};
	struct promisc_mc_query_context context;

	/* change trees */
	mutex_lock(&priv->promisc.tree_lock);
	update_rules_tree(&priv->promisc);
	mutex_unlock(&priv->promisc.tree_lock);
	init_completion(&context.done);
	/* query SM and handle the results with the new tree */
	rec.pkey = priv->pkey;
	context.priv = priv;
	context.query_id =
		ib_sa_mcmember_gettable(&ipoib_sa_client, priv->ca, priv->port,
					&rec, GFP_KERNEL,
					ipoib_validate_gettable_cb, &context,
					&context.query);

	if (context.query_id < 0)
		ipoib_warn(priv, "%s: ib_sa_mcmember_gettable returned %d\n",
			   __func__, context.query_id);
	else {
		ipoib_dbg_mcp(priv,
			      "%s: Applying new rules to known MC groups\n",
			      __func__);
		wait_for_completion(&context.done);
		ipoib_dbg_mcp(priv, "%s: Rules validation is done\n", __func__);
	}


	return context.query_id;
}

/*
 * This function is the core engine of mc promic (ipoib allmulti),
 * it should run ONLY from priv->promisc.workqueue context!
 */
void ipoib_mcp_work_handler(struct work_struct *work)
{
	struct promisc_mc_work *mc_work = container_of(work,
						       struct promisc_mc_work,
						       work.work);
	struct ipoib_dev_priv *priv = mc_work->priv;

	switch (mc_work->action) {
	case MC_REGISTER_CREATE:
		ipoib_handle_registration(priv, IB_SA_SM_TRAP_CREATE_MC_GROUP);
		break;
	case MC_REGISTER_DELETE:
		ipoib_handle_registration(priv, IB_SA_SM_TRAP_DELETE_MC_GROUP);
		break;
	case MC_CREATE_ACTION:
		ipoib_handle_mcg_create(priv, mc_work->data.gid);
		break;
	case MC_DELETE_ACTION:
		ipoib_handle_mcg_delete(priv, mc_work->data.gid);
		break;
	case MC_ADD_RULE_ACTION:
		ipoib_handle_add_rule(priv, mc_work->data.rule);
		break;
	case MC_VALIDATE_RULES_ACTION:
		ipoib_handle_validate_rules(priv);
		break;
	default:
		ipoib_warn(priv, "%s Unsupported action type (%d)\n",
			   __func__, mc_work->action);
	}
	kfree(mc_work);
}

/*
 * Queues a rules validation work into the work queue. This work must be
 * synchronized with notice handling and rules addition.
 */
int ipoib_queue_validate_rules(struct promisc_mc *promisc)
{
	return ipoib_queue_mcp_work(promisc, NULL, MC_VALIDATE_RULES_ACTION, 0);
}

static void ipoib_mcmember_gettable_cb(int status,
				       struct ib_sa_mcmember_rec *resp,
				       void *context)
{
	struct ipoib_dev_priv *priv = context;

	/* error code from core layer, should be followed by NULL resp. */
	if (status < 0) {
		ipoib_printk(KERN_ERR, priv, "%s: received error status %d\n",
			     __func__, status);
		if (resp)
			ipoib_warn(priv, "%s: resp should be NULL when receiving error status\n",
				   __func__);
		goto out;
	}

	/* This shouldn't be possible with status >= 0 */
	if (!resp) {
		ipoib_warn(priv, "%s: status is %d but resp is NULL\n",
			   __func__, status);
		goto out;
	}

	/* If this is not an IPoIB MCG, return */
	if (!is_ipoib_mgid(resp->mgid.raw, priv->dev->broadcast)) {
		ipoib_dbg_mcp(priv, "%s: %pI6 is not an IPoIB mgid, skipping\n",
			      __func__, resp->mgid.raw);
		goto out;
	}

	ipoib_mc_raise_notification(priv, resp, NOTIFY_DETAILS);
out:
	/* eventually we will get status != 0 */
	if (status != 0) {
		complete(&priv->promisc.get_table_done);
		priv->promisc.get_table_query = NULL;
	}
}

int ipoib_get_mcg_table(struct promisc_mc *promisc)
{
	struct ib_sa_mcmember_rec rec = {};
	struct ipoib_dev_priv *priv =
		container_of(promisc, struct ipoib_dev_priv, promisc);

	if (promisc->get_table_query)
		return -EBUSY;

	rec.pkey = priv->pkey;
	init_completion(&promisc->get_table_done);
	promisc->get_table_query_id =
		ib_sa_mcmember_gettable(&ipoib_sa_client, priv->ca, priv->port,
					&rec, GFP_KERNEL,
					ipoib_mcmember_gettable_cb, priv,
					&promisc->get_table_query);
	if (promisc->get_table_query_id < 0) {
		ipoib_warn(priv, "%s: ib_sa_mcmember_gettable returned %d\n",
			   __func__, promisc->get_table_query_id);
		priv->promisc.get_table_query = NULL;
		complete(&promisc->get_table_done);
	}
	return promisc->get_table_query_id;
}

/*
 * Initializes resources used by ipoib promiscuous multicast.
 */
int ipoib_promisc_mc_init(struct promisc_mc *promisc)
{
	struct ipoib_dev_priv *priv =
		container_of(promisc, struct ipoib_dev_priv, promisc);

	mutex_init(&promisc->tree_lock);
	init_completion(&promisc->get_table_done);
	complete(&promisc->get_table_done);
	promisc->get_table_query = NULL;

	/* create the MC workqueue */
	promisc->workqueue = create_singlethread_workqueue("notice");
	if (!promisc->workqueue) {
		ipoib_printk(KERN_CRIT, priv, "%s: couldn't allocate notice workqueue\n",
			     __func__);
		return -ENOMEM;
	}

	ipoib_dbg_mcp(priv, "Notice workqueue is ready\n");
	return 0;
}

/*
 * Starts ipoib_promiscuous:
 *	Registers to II notifications.
 *	Calls get table and applies rules to the groups.
 */
int ipoib_promisc_mc_start(struct promisc_mc *promisc)
{
	int ret = 0;
	struct ib_port_attr attr;
	struct ipoib_dev_priv *priv =
		container_of(promisc, struct ipoib_dev_priv, promisc);

	if (ib_query_port(priv->ca, priv->port, &attr) ||
	    attr.state != IB_PORT_ACTIVE) {
		ipoib_dbg_mcp(priv, "%s: port state is not ACTIVE (state = %d)\n",
			      __func__, attr.state);
		return -EPERM;
	}

	set_bit(IPOIB_ALL_MULTI, &priv->flags);
	if (promisc->create_mcg)
		goto update_mcgs;

	promisc->registration_delay = 1;
	promisc->registration_succeeded = 0;

	/* register to MCG creation notification from the SM */
	init_completion(&promisc->registration_completion);
	ret = ipoib_queue_mcp_work(promisc, NULL, MC_REGISTER_CREATE, 0);
	if (ret)
		goto out;

	wait_for_completion(&promisc->registration_completion);

	if (!promisc->registration_succeeded) {
		ipoib_err(priv, "Promiscuous multicast failed to start\n");
		ret = -EAGAIN;
		goto out;
	}

update_mcgs:
	ret = ipoib_queue_mcp_work(promisc, NULL,
				   MC_VALIDATE_RULES_ACTION, 0);
	if (ret)
		goto unregister;

	ipoib_dbg_mcp(priv, "Promiscuous multicast is now started\n");
	goto out;

unregister:
	ib_sa_unregister_inform_info(promisc->create_mcg);
	promisc->create_mcg = NULL;
	ib_sa_unregister_inform_info(promisc->delete_mcg);
	promisc->delete_mcg = NULL;
out:
	if (ret)
		clear_bit(IPOIB_ALL_MULTI, &priv->flags);
	return ret;
}

/*
 * Flushes promiscuous_mc MCG list during dev flush.
 * No need to flush ... just remove at stop.
 *
 */
static void ipoib_promisc_mc_flush(struct promisc_mc *promisc)
{
	struct ipoib_dev_priv *priv =
		container_of(promisc, struct ipoib_dev_priv, promisc);
	struct rb_node *n;
	struct ipoib_mcast *mcast, *tmcast;
	LIST_HEAD(remove_list);

	/* Clean MC tree */
	if (&priv->promisc.multicast_tree == NULL)
		return;

	mutex_lock(&priv->promisc.tree_lock);
	n = rb_first(&priv->promisc.multicast_tree);
	while (n) {
		mcast = rb_entry(n, struct ipoib_mcast, rb_node);
		n = rb_next(n);
		rb_erase(&mcast->rb_node, &priv->promisc.multicast_tree);
		list_add_tail(&mcast->list, &remove_list);
	}
	mutex_unlock(&priv->promisc.tree_lock);

	/*seperate between the wait to the leave.*/
	list_for_each_entry_safe(mcast, tmcast, &remove_list, list)
		if (test_bit(IPOIB_MCAST_JOIN_STARTED, &mcast->flags))
			wait_for_completion(&mcast->done);

	list_for_each_entry_safe(mcast, tmcast, &remove_list, list) {
		ipoib_mcast_leave(priv->dev, mcast);
		ipoib_mcast_free(mcast);
	}
}

/*
 * Stops ipoib_promisc during interface down:
 *	Unregisters from II notifications.
 *	Flushes rules.
 *	Disables ALLMULTI.
 */
int ipoib_promisc_mc_stop(struct promisc_mc *promisc)
{
	struct ipoib_dev_priv *priv =
		container_of(promisc, struct ipoib_dev_priv, promisc);

	/* Unregister II notifications */
	if (promisc->create_mcg) {
		ib_sa_unregister_inform_info(promisc->create_mcg);
		promisc->create_mcg = NULL;
	}
	ipoib_dbg_mcp(priv, "%s: unregistered IB_SA_SM_TRAP_CREATE_MC_GROUP\n",
		      __func__);

	if (promisc->delete_mcg) {
		ib_sa_unregister_inform_info(promisc->delete_mcg);
		promisc->delete_mcg = NULL;
	}
	ipoib_dbg_mcp(priv, "%s: unregistered IB_SA_SM_TRAP_DELETE_MC_GROUP\n",
		      __func__);

	/* Wait for get_mcg query to finish if need to */
	if (!IS_ERR_OR_NULL(promisc->get_table_query)) {
		ib_sa_cancel_query(promisc->get_table_query_id,
				   promisc->get_table_query);
		wait_for_completion(&promisc->get_table_done);
		promisc->get_table_query = NULL;
	}

	flush_workqueue(promisc->workqueue);
	ipoib_promisc_mc_flush(promisc);
	clear_bit(IPOIB_ALL_MULTI, &priv->flags);
	ipoib_dbg_mcp(priv, "MC Promisc is now stopped\n");
	return 0;
}

int ipoib_promisc_mc_destroy(struct promisc_mc *promisc)
{
	struct ipoib_dev_priv *priv =
		container_of(promisc, struct ipoib_dev_priv, promisc);
	struct rb_node *n;
	struct ipoib_mc_rule *rule;

	if (promisc->workqueue) {
		flush_workqueue(promisc->workqueue);
		destroy_workqueue(promisc->workqueue);
		promisc->workqueue = NULL;
		ipoib_dbg_mcp(priv, "Notice workqueue was destroyed\n");
	}

	ipoib_promisc_mc_flush(promisc);

	/* Clear rules */
	mutex_lock(&priv->promisc.tree_lock);
	n = rb_first(&priv->promisc.rules_tree);
	while (n) {
		rule = rb_entry(n, struct ipoib_mc_rule, rb_node);
		n = rb_next(n);
		rb_erase(&rule->rb_node, &priv->promisc.rules_tree);
		kfree(rule);
	}

	n = rb_first(&priv->promisc.new_rules_tree);
	while (n) {
		rule = rb_entry(n, struct ipoib_mc_rule, rb_node);
		n = rb_next(n);
		rb_erase(&rule->rb_node, &priv->promisc.new_rules_tree);
		kfree(rule);
	}
	mutex_unlock(&priv->promisc.tree_lock);

	return 0;
}
