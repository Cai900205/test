/*
 * Copyright (c) 2012 Mellanox Technologies. All rights reserved.
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
 /***********************************************************/
/*This file support the handling of the Alias GUID feature. */
/***********************************************************/
#include <rdma/ib_mad.h>
#include <rdma/ib_smi.h>
#include <rdma/ib_cache.h>
#include <rdma/ib_sa.h>
#include <rdma/ib_pack.h>
#include <linux/mlx4/cmd.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/errno.h>
#include <rdma/ib_user_verbs.h>
#include <linux/delay.h>
#include "mlx4_ib.h"

/*
The driver keeps the current state of all guids, as they are in the HW.
Whenever we receive an smp mad GUIDInfo record, the data will be cached.
*/

struct mlx4_alias_guid_work_context {
	u8 port;
	struct mlx4_ib_dev     *dev ;
	struct ib_sa_query     *sa_query;
	struct completion	done;
	int			query_id;
	struct list_head	list;
	int			block_num;
	u8			method;
};

struct mlx4_next_alias_guid_work {
	u8 port;
	u8 block_num;
	struct mlx4_sriov_alias_guid_info_rec_det rec_det;
};


void mlx4_ib_update_cache_on_guid_change(struct mlx4_ib_dev *dev, int block_num,
					 u8 port_num, u8 *p_data)
{
	int i;
	u64 guid_indexes;
	int slave_id;
	int port_index = port_num - 1;
	int gid_index;

	if (!mlx4_is_master(dev->dev))
		return;

	guid_indexes = be64_to_cpu((__force __be64) dev->sriov.alias_guid.
				   ports_guid[port_num - 1].
				   all_rec_per_port[block_num].guid_indexes);
	pr_debug("port: %d, guid_indexes: 0x%llx\n", port_num, guid_indexes);

	for (i = 0; i < NUM_ALIAS_GUID_IN_REC; i++) {
		/* The location of the specific index starts from bit number 4
		 * until bit num 11 */
		if (test_bit(i + 4, (unsigned long *)&guid_indexes)) {

			gid_index = (block_num * NUM_ALIAS_GUID_IN_REC) + i;
			slave_id = mlx4_get_slave_from_port_gid_index(dev->dev, gid_index,
				    port_num);

			if (slave_id >= dev->dev->num_slaves) {
				pr_debug("The last slave: %d\n", slave_id);
				return;
			}

			/* cache the guid: */
			memcpy(&dev->sriov.demux[port_index].guid_cache[gid_index],
			       &p_data[i * GUID_REC_SIZE],
			       GUID_REC_SIZE);
		} else
			pr_debug("Guid number: %d in block: %d"
				 " was not updated\n", i, block_num);
	}
}

static __be64 get_cached_alias_guid(struct mlx4_ib_dev *dev, int port, int index)
{
	if (index >= NUM_ALIAS_GUID_PER_PORT) {
		pr_err("%s: ERROR: asked for index:%d\n", __func__, index);
		return (__force __be64) -1;
	}
	return *(__be64 *)&dev->sriov.demux[port - 1].guid_cache[index];
}


ib_sa_comp_mask mlx4_ib_get_aguid_comp_mask_from_ix(int index)
{
	return IB_SA_COMP_MASK(4 + index);
}

/*
 * Whenever new GUID is set/unset (guid table change) create event and
 * notify the relevant slave (master also should be notified).
 * If the GUID value is not as we have in the cache the slave will not be
 * updated; in this case it waits for the smp_snoop or the port management
 * event to call the function and to update the slave.
 * block_number - the index of the block (16 blocks available)
 * port_number - 1 or 2
 */
void mlx4_ib_notify_slaves_on_guid_change(struct mlx4_ib_dev *dev,
					  int block_num, u8 port_num,
					  u8 *p_data)
{
	int i;
	u64 guid_indexes;
	int slave_id;
	enum slave_port_state new_state;
	enum slave_port_state prev_state;
	__be64 tmp_cur_ag, form_cache_ag;
	enum slave_port_gen_event gen_event;
	int slave_base_gid_index;
	int port_gid_index;
	struct mlx4_sriov_alias_guid_info_rec_det *rec;

	if (!mlx4_is_master(dev->dev))
		return;

	rec = &dev->sriov.alias_guid.ports_guid[port_num - 1].
		all_rec_per_port[block_num];
	guid_indexes = be64_to_cpu((__force __be64) dev->sriov.alias_guid.
				   ports_guid[port_num - 1].
				   all_rec_per_port[block_num].guid_indexes);
	pr_debug("port: %d, guid_indexes: 0x%llx\n", port_num, guid_indexes);

	/*calculate the slaves and notify them*/
	for (i = 0; i < NUM_ALIAS_GUID_IN_REC; i++) {
		/* the location of the specific index runs from bits 4..11 */
		if (!(test_bit(i + 4, (unsigned long *)&guid_indexes)))
			continue;
		port_gid_index = (block_num * NUM_ALIAS_GUID_IN_REC) + i;

		slave_id = mlx4_get_slave_from_port_gid_index(dev->dev,
					port_gid_index,
					port_num);
		if (slave_id >= dev->dev->num_slaves) {
			pr_err("Invalid slave_id=%d was detected, num_slaves=%lu\n", slave_id,
			       dev->dev->num_slaves);
			return;
		}
		tmp_cur_ag = *(__be64 *)&p_data[i * GUID_REC_SIZE];
		form_cache_ag = get_cached_alias_guid(dev, port_num,
					port_gid_index);
		/*
		 * Check if guid is not the same as in the cache,
		 * If it is different, wait for the snoop_smp or the port mgmt
		 * change event to update the slave on its port state change
		 */
		if (tmp_cur_ag != form_cache_ag)
			continue;

		/* In case both entries are set but there is an open request from SM
		  * to get a value we should skip that entry. No event should be set and bit should not turned off.
		  * For example this entry was decliened by the SM on that record, once other entries were accepted.
		  *(e.g. SM was down set 2 entries to same value then start SM).
		*/
		if (tmp_cur_ag == MLX4_NOT_SET_GUID &&
		    be64_to_cpu(*(__be64 *)&rec->all_recs[i * GUID_REC_SIZE]) != MLX4_GUID_FOR_DELETE_VAL)
			continue;

		mlx4_gen_guid_change_eqe(dev->dev, slave_id, port_num);
		/* Turn off that bit so that won't generate change event when later other entry
		*   in same block is changed.
		*/
		rec->guid_indexes = rec->guid_indexes &
				~mlx4_ib_get_aguid_comp_mask_from_ix(i);

		/* state machine should be handled only for gid index 0 for that slave which
		     is its base gid index.
		*/
		slave_base_gid_index = mlx4_get_base_gid_ix(dev->dev, slave_id,
							   port_num);
		if (slave_base_gid_index != port_gid_index)
			continue;

		/*2 cases: Valid GUID, and Invalid Guid*/
		if (tmp_cur_ag != MLX4_NOT_SET_GUID) { /*valid GUID*/
			prev_state = mlx4_get_slave_port_state(dev->dev, slave_id, port_num);
			new_state = set_and_calc_slave_port_state(dev->dev, slave_id, port_num,
								  MLX4_PORT_STATE_IB_PORT_STATE_EVENT_GID_VALID,
								  &gen_event);
			pr_debug("slave: %d, port: %d prev_port_state: %d,"
				 " new_port_state: %d, gen_event: %d\n",
				 slave_id, port_num, prev_state, new_state, gen_event);
			if (gen_event == SLAVE_PORT_GEN_EVENT_UP) {
				pr_debug("sending PORT_UP event to slave: %d, port: %d, base gid index=%d\n",
					 slave_id, port_num, slave_base_gid_index);
				mlx4_gen_port_state_change_eqe(dev->dev, slave_id,
							       port_num, MLX4_PORT_CHANGE_SUBTYPE_ACTIVE);
			}
		} else { /* request to invalidate GUID */
			set_and_calc_slave_port_state(dev->dev, slave_id, port_num,
						      MLX4_PORT_STATE_IB_EVENT_GID_INVALID,
						      &gen_event);
			pr_debug("sending PORT DOWN event to slave: %d, port: %d, base gid index=%d\n",
				 slave_id, port_num, slave_base_gid_index);
			mlx4_gen_port_state_change_eqe(dev->dev, slave_id, port_num,
						       MLX4_PORT_CHANGE_SUBTYPE_DOWN);
		}
	}
}

/* return index of record that should be updated based on lowest rescheduled time */
static int get_low_record_time_index(struct mlx4_ib_dev *dev, u8 port,
					u64 *low_record_resch_time)
{
	int record_index = -1;
	unsigned long low_record_time = 0;
	struct mlx4_sriov_alias_guid_info_rec_det rec;
	int j;

	for (j = 0; j < NUM_ALIAS_GUID_REC_IN_PORT; j++) {
		rec = dev->sriov.alias_guid.ports_guid[port].all_rec_per_port[j];
		if (rec.status == MLX4_GUID_INFO_STATUS_IDLE) {

			if (record_index == -1 || rec.time_to_retry < low_record_time) {
				record_index = j;
				low_record_time = rec.time_to_retry;
			}
		}
	}
	if (low_record_resch_time)
		*low_record_resch_time = low_record_time;

	return record_index;

}

static void aliasguid_query_handler(int status,
				    struct ib_sa_guidinfo_rec *guid_rec,
				    void *context)
{
	struct mlx4_ib_dev *dev;
	struct mlx4_alias_guid_work_context *cb_ctx = context;
	u8 port_index;
	int i;
	struct mlx4_sriov_alias_guid_info_rec_det *rec;
	unsigned long flags, flags1;
	unsigned int resched_delay_sec = 0;
	u64 declined_guid_indexes = 0;

	if (!context)
		return;

	dev = cb_ctx->dev;
	port_index = cb_ctx->port - 1;
	rec = &dev->sriov.alias_guid.ports_guid[port_index].
		all_rec_per_port[cb_ctx->block_num];

	if (status) {
		rec->status = MLX4_GUID_INFO_STATUS_IDLE;
		pr_debug("(port: %d) failed: status = %d\n",
			 cb_ctx->port, status);
		goto out;
	}

	if (guid_rec->block_num != cb_ctx->block_num) {
		pr_err("block num mismatch: %d != %d\n",
		       cb_ctx->block_num, guid_rec->block_num);
		goto out;
	}

	pr_debug("lid/port: %d/%d, block_num: %d\n",
		 be16_to_cpu(guid_rec->lid), cb_ctx->port,
		 guid_rec->block_num);

	rec = &dev->sriov.alias_guid.ports_guid[port_index].
		all_rec_per_port[guid_rec->block_num];

	rec->status = MLX4_GUID_INFO_STATUS_SET;
	rec->method = MLX4_GUID_INFO_RECORD_SET;

	for (i = 0 ; i < NUM_ALIAS_GUID_IN_REC; i++) {
		__be64 tmp_cur_ag;
		__be64 tmp_requested_value;
		int entry_declined = 0;

		/* check whether we asked SM to change that entry, if not we skip it.
		  * For example - non base gid index that was set to to FFFFs, SM will
		  * return its value which is 0, no reason to mark as declined.
		*/
		if (!(rec->guid_indexes & mlx4_ib_get_aguid_comp_mask_from_ix(i))) {
			/* For entry 0, we didn't ask to make a change but need
			  * to update its value locally to be exposed via admin_guid interface.
			  */
			if (!guid_rec->block_num && !i && cb_ctx->method == MLX4_GUID_INFO_RECORD_SET)
				memcpy(&rec->all_recs[i * GUID_REC_SIZE],
				       &guid_rec->guid_info_list[i * GUID_REC_SIZE],
				       GUID_REC_SIZE);
			continue;
		}

		tmp_cur_ag = *(__be64 *)&guid_rec->guid_info_list[i * GUID_REC_SIZE];
		tmp_requested_value = *(__be64 *)&rec->all_recs[i * GUID_REC_SIZE];

		if ((cb_ctx->method == MLX4_GUID_INFO_RECORD_DELETE)
		    && (MLX4_NOT_SET_GUID == tmp_cur_ag)) {
			if (be64_to_cpu(tmp_requested_value) != MLX4_GUID_FOR_DELETE_VAL)
				entry_declined = 1;
			pr_debug("%s:Record num %d in block_num:%d "
				"was deleted by SM,ownership by %d "
				"(0 = driver, 1=sysAdmin, 2=None) requested val(0x%llx), %s\n",
				__func__, i, guid_rec->block_num,
				rec->ownership, be64_to_cpu(tmp_requested_value),
				entry_declined ? "retry to set requested value" : "");
			if (!entry_declined)
				continue;
		/* check if the SM didn't assign one of the records.
		 * if it didn't, if it was not sysadmin request:
		 * ask the SM to give a new GUID, (instead of the driver request).
		 */
		} else if (tmp_cur_ag == MLX4_NOT_SET_GUID) {
			/* In case retry time is set (admin mode) print warning only on first failure per entry */
			if (rec->all_recs_retry_schedule[i] == 0)
				mlx4_ib_warn(&dev->ib_dev, "%s:Record num %d in "
					     "block_num: %d was declined by SM, "
					     "ownership by %d (0 = driver, 1=sysAdmin,"
					     " 2=None), request value was (0x%llx)\n", __func__, i,
					     guid_rec->block_num, rec->ownership,
					      be64_to_cpu(*(__be64 *)&
							 rec->all_recs[i * GUID_REC_SIZE]));
			entry_declined = 1;
		} else {
		       /* properly assigned record. */
		       /* We save the GUID we just got from the SM in the
			* admin_guid in order to be persistent, and in the
			* request from the sm the process will ask for the same GUID */
			if (tmp_cur_ag != tmp_requested_value &&
			    tmp_requested_value != MLX4_NOT_SET_GUID) {
				/* the sysadmin assignment failed, print warning only on first failure per entry */
				if (rec->all_recs_retry_schedule[i] == 0)
					mlx4_ib_warn(&dev->ib_dev, "%s: Failed to set"
						     " admin guid after SysAdmin "
						     "configuration. "
						     "Record num %d in block_num:%d "
						     "was declined by SM, "
						     "new val(0x%llx) was kept, SM returned (0x%llx)\n",
						      __func__, i,
						     guid_rec->block_num,
						     be64_to_cpu(*(__be64 *)&
								 rec->all_recs[i * GUID_REC_SIZE]),
								 be64_to_cpu(tmp_cur_ag));
				entry_declined = 1;
			} else {
				memcpy(&rec->all_recs[i * GUID_REC_SIZE],
				       &guid_rec->guid_info_list[i * GUID_REC_SIZE],
				       GUID_REC_SIZE);
			}
		}

		if (entry_declined) {
			declined_guid_indexes |= mlx4_ib_get_aguid_comp_mask_from_ix(i);
			/* declined /vlaue not set  - re-asking in next call */
			rec->status = MLX4_GUID_INFO_STATUS_IDLE;
			rec->guid_indexes |= mlx4_ib_get_aguid_comp_mask_from_ix(i);
			if (rec->ownership == MLX4_GUID_DRIVER_ASSIGN) {
				/* if it is driver assign, asks for new GUID from SM, previous value was rejected */
				*(__be64 *)&rec->all_recs[i * GUID_REC_SIZE] = MLX4_NOT_SET_GUID;
			/* once record is in sysadmin assign set the retry time, otherwise let it happen immediately */
			} else if (rec->ownership == MLX4_GUID_SYSADMIN_ASSIGN) {
				/* Mark the record as not assigned, and let it
				 * be sent again in the next work sched.*/
				rec->all_recs_retry_schedule[i] = rec->all_recs_retry_schedule[i] == 0 ?  1 :
							min((unsigned int)60, rec->all_recs_retry_schedule[i] * 2);
				/* using the minimum value among all entries in that record */
				resched_delay_sec = (resched_delay_sec == 0) ?
					rec->all_recs_retry_schedule[i] : min(resched_delay_sec, rec->all_recs_retry_schedule[i]);
			}
		} else {
			rec->all_recs_retry_schedule[i] = 0;
		}
	}

	/* Temporarly turning off the the declined bit to prevent notify slaves on non
	     real change. (e.g. empty GUID value).
	*/
	rec->guid_indexes &= ~declined_guid_indexes;
	/*
	The func is call here to close the cases when the
	sm doesn't send smp, so in the sa response the driver
	notifies the slave.
	*/
	mlx4_ib_notify_slaves_on_guid_change(dev, guid_rec->block_num,
					     cb_ctx->port,
					     guid_rec->guid_info_list);

	rec->guid_indexes |= declined_guid_indexes;
out:
	spin_lock_irqsave(&dev->sriov.going_down_lock, flags);
	spin_lock_irqsave(&dev->sriov.alias_guid.ag_work_lock, flags1);
	if (!dev->sriov.is_going_down) {

		/* looking for min time for next pending record */
		if (resched_delay_sec) {
			struct timespec n;
			u64 curr_time;
			u64 low_record_resch_time;

			getnstimeofday(&n);
			curr_time = timespec_to_ns(&n);
			rec->time_to_retry = curr_time + resched_delay_sec * NSEC_PER_SEC;
			get_low_record_time_index(dev, port_index, &low_record_resch_time);
			resched_delay_sec = (low_record_resch_time < curr_time) ? 0 :
				div_u64((low_record_resch_time - curr_time) , NSEC_PER_SEC);
		} else {
			rec->time_to_retry = 0;
		}
		queue_delayed_work(dev->sriov.alias_guid.ports_guid[port_index].wq,
				   &dev->sriov.alias_guid.ports_guid[port_index].
				   alias_guid_work,
				   msecs_to_jiffies(resched_delay_sec * 1000));
	}

	if (cb_ctx->sa_query) {
		list_del(&cb_ctx->list);
		kfree(cb_ctx);
	} else
		complete(&cb_ctx->done);
	spin_unlock_irqrestore(&dev->sriov.alias_guid.ag_work_lock, flags1);
	spin_unlock_irqrestore(&dev->sriov.going_down_lock, flags);
}

static void invalidate_guid_record(struct mlx4_ib_dev *dev, u8 port, int index,
					int is_init)
{
	int i;
	u64 cur_admin_val;
	ib_sa_comp_mask comp_mask = 0;
	int physical_entry;
	int driver_ownership = (dev->sriov.alias_guid.
		    ports_guid[port - 1].all_rec_per_port[index].ownership ==
		    MLX4_GUID_DRIVER_ASSIGN) ? 1 : 0;

	dev->sriov.alias_guid.ports_guid[port - 1].all_rec_per_port[index].status
		= MLX4_GUID_INFO_STATUS_IDLE;
	dev->sriov.alias_guid.ports_guid[port - 1].all_rec_per_port[index].method
		= MLX4_GUID_INFO_RECORD_SET;

	/* calculate the comp_mask for that record.*/
	for (i = 0; i < NUM_ALIAS_GUID_IN_REC; i++) {
		cur_admin_val =
			*(u64 *)&dev->sriov.alias_guid.ports_guid[port - 1].
			all_rec_per_port[index].all_recs[GUID_REC_SIZE * i];

		if (is_init && driver_ownership) {
			/* on startup if driver assigned mode check whether entry is base gid index of slave,
			  * if not skip it and put FFFFs instead to let administrator
			  * know that currently not set.
			*/
			physical_entry = (index * GUID_REC_SIZE) + i;
			if (!mlx4_is_base_gid_idx(physical_entry, dev->dev, port)) {
				*(u64 *)&dev->sriov.alias_guid.ports_guid[port - 1].
				all_rec_per_port[index].all_recs[GUID_REC_SIZE * i] =
					MLX4_GUID_FOR_DELETE_VAL;
				continue;
			}
		}

		/*
		check the admin value: if it's for delete (~00LL) or
		it is the first guid of the first record (hw guid) or
		the records is not in ownership of the sysadmin and the sm doesn't
		need to assign GUIDs, then don't put it up for assignment.
		*/
		if (MLX4_GUID_FOR_DELETE_VAL == cur_admin_val ||
		    (!index && !i) ||
		    MLX4_GUID_NONE_ASSIGN == dev->sriov.alias_guid.
		    ports_guid[port - 1].all_rec_per_port[index].ownership)
			continue;
		comp_mask |= mlx4_ib_get_aguid_comp_mask_from_ix(i);
	}
	dev->sriov.alias_guid.ports_guid[port - 1].
		all_rec_per_port[index].guid_indexes = comp_mask;
}

/* We are going to call the SM */
static int set_guid_rec(struct ib_device *ibdev,
			u8 port, int index,
			struct mlx4_sriov_alias_guid_info_rec_det *rec_det)
{
	int err;
	struct mlx4_ib_dev *dev = to_mdev(ibdev);
	struct ib_sa_guidinfo_rec guid_info_rec;
	ib_sa_comp_mask comp_mask;
	struct ib_port_attr attr;
	struct mlx4_alias_guid_work_context *callback_context;
	unsigned long resched_delay, flags, flags1;
	struct list_head *head =
		&dev->sriov.alias_guid.ports_guid[port - 1].cb_list;

	err = __mlx4_ib_query_port(ibdev, port, &attr, 1);
	if (err) {
		pr_debug("mlx4_ib_query_port failed (err: %d), port: %d\n",
			 err, port);
		return err;
	}
	/*check the port was configured by the sm, otherwise no need to send */
	if (attr.state != IB_PORT_ACTIVE) {
		pr_debug("port %d not active...rescheduling\n", port);
		resched_delay = 5 * HZ;
		err = -EAGAIN;
		goto new_schedule;
	}

	callback_context = kmalloc(sizeof *callback_context, GFP_KERNEL);
	if (!callback_context) {
		err = -ENOMEM;
		resched_delay = HZ * 5;
		goto new_schedule;
	}
	callback_context->port = port;
	callback_context->dev = dev;
	callback_context->block_num = index;
	callback_context->method = rec_det->method;
	memset(&guid_info_rec, 0, sizeof (struct ib_sa_guidinfo_rec));

	guid_info_rec.lid = cpu_to_be16(attr.lid);
	guid_info_rec.block_num = index;

	memcpy(guid_info_rec.guid_info_list, rec_det->all_recs,
	       GUID_REC_SIZE * NUM_ALIAS_GUID_IN_REC);
	comp_mask = IB_SA_GUIDINFO_REC_LID | IB_SA_GUIDINFO_REC_BLOCK_NUM |
		rec_det->guid_indexes;

	init_completion(&callback_context->done);
	spin_lock_irqsave(&dev->sriov.alias_guid.ag_work_lock, flags1);
	list_add_tail(&callback_context->list, head);
	spin_unlock_irqrestore(&dev->sriov.alias_guid.ag_work_lock, flags1);

	callback_context->query_id =
		ib_sa_guid_info_rec_query(dev->sriov.alias_guid.sa_client,
					  ibdev, port, &guid_info_rec,
					  comp_mask, rec_det->method, 1000,
					  GFP_KERNEL, aliasguid_query_handler,
					  callback_context,
					  &callback_context->sa_query);
	if (callback_context->query_id < 0) {
		pr_debug("ib_sa_guid_info_rec_query failed, query_id: "
			 "%d. will reschedule to the next 1 sec.\n",
			 callback_context->query_id);
		spin_lock_irqsave(&dev->sriov.alias_guid.ag_work_lock, flags1);
		list_del(&callback_context->list);
		kfree(callback_context);
		spin_unlock_irqrestore(&dev->sriov.alias_guid.ag_work_lock, flags1);
		resched_delay = 1 * HZ;
		err = -EAGAIN;
		goto new_schedule;
	}
	err = 0;
	goto out;

new_schedule:
	spin_lock_irqsave(&dev->sriov.going_down_lock, flags);
	spin_lock_irqsave(&dev->sriov.alias_guid.ag_work_lock, flags1);
	invalidate_guid_record(dev, port, index, 0);
	if (!dev->sriov.is_going_down) {
		queue_delayed_work(dev->sriov.alias_guid.ports_guid[port - 1].wq,
				   &dev->sriov.alias_guid.ports_guid[port - 1].alias_guid_work,
				   resched_delay);
	}
	spin_unlock_irqrestore(&dev->sriov.alias_guid.ag_work_lock, flags1);
	spin_unlock_irqrestore(&dev->sriov.going_down_lock, flags);

out:
	return err;
}

void mlx4_ib_invalidate_all_guid_record(struct mlx4_ib_dev *dev, int port)
{
	int i;
	unsigned long flags, flags1;

	pr_debug("port %d\n", port);

	spin_lock_irqsave(&dev->sriov.going_down_lock, flags);
	spin_lock_irqsave(&dev->sriov.alias_guid.ag_work_lock, flags1);
	for (i = 0; i < NUM_ALIAS_GUID_REC_IN_PORT; i++)
		invalidate_guid_record(dev, port, i, 0);

	if (mlx4_is_master(dev->dev) && !dev->sriov.is_going_down) {
		/*
		make sure no work waits in the queue, if the work is already
		queued(not on the timer) the cancel will fail. That is not a problem
		because we just want the work started.

		in kernel < 3.7.0 we should use __cancel_delayed_work in IRQ context
		in kernel >= 3.7.0 cancel_delayed_work function is safe to call from
		any context including IRQ handler
		http://lxr.linux.no/linux+v3.7/kernel/workqueue.c#L2981
		*/
#if LINUX_VERSION_CODE < KERNEL_VERSION(3,7,0)
		__cancel_delayed_work(&dev->sriov.alias_guid.
				      ports_guid[port - 1].alias_guid_work);
#else
		cancel_delayed_work(&dev->sriov.alias_guid.
                                    ports_guid[port - 1].alias_guid_work);
#endif
		queue_delayed_work(dev->sriov.alias_guid.ports_guid[port - 1].wq,
				   &dev->sriov.alias_guid.ports_guid[port - 1].alias_guid_work,
				   0);
	}
	spin_unlock_irqrestore(&dev->sriov.alias_guid.ag_work_lock, flags1);
	spin_unlock_irqrestore(&dev->sriov.going_down_lock, flags);
}

/* The function returns the next record that was
 * not configured (or failed to be configured) */
static int get_next_record_to_update(struct mlx4_ib_dev *dev, u8 port,
				     struct mlx4_next_alias_guid_work *rec)
{

	unsigned long flags;
	int record_index;
	int ret = 0;

	spin_lock_irqsave(&dev->sriov.alias_guid.ag_work_lock, flags);
	record_index = get_low_record_time_index(dev, port, NULL);

	if (record_index < 0) {
		ret = -ENOENT;
		goto out;
	}

	memcpy(&rec->rec_det,
	       &dev->sriov.alias_guid.ports_guid[port].all_rec_per_port[record_index],
	       sizeof(struct mlx4_sriov_alias_guid_info_rec_det));
	rec->port = port;
	rec->block_num = record_index;
	dev->sriov.alias_guid.ports_guid[port].all_rec_per_port[record_index].status =
		MLX4_GUID_INFO_STATUS_PENDING;
out:
	spin_unlock_irqrestore(&dev->sriov.alias_guid.ag_work_lock, flags);
	return ret;
}

static void alias_guid_work(struct work_struct *work)
{
	struct delayed_work *delay = to_delayed_work(work);
	int ret = 0;
	struct mlx4_next_alias_guid_work *rec;
	struct mlx4_sriov_alias_guid_port_rec_det *sriov_alias_port =
		container_of(delay, struct mlx4_sriov_alias_guid_port_rec_det,
			     alias_guid_work);
	struct mlx4_sriov_alias_guid *sriov_alias_guid = sriov_alias_port->parent;
	struct mlx4_ib_sriov *ib_sriov = container_of(sriov_alias_guid,
						struct mlx4_ib_sriov,
						alias_guid);
	struct mlx4_ib_dev *dev = container_of(ib_sriov, struct mlx4_ib_dev, sriov);

	rec = kzalloc(sizeof *rec, GFP_KERNEL);
	if (!rec) {
		pr_err("alias_guid_work: No Memory\n");
		return;
	}

	pr_debug("starting [port: %d]...\n", sriov_alias_port->port + 1);
	ret = get_next_record_to_update(dev, sriov_alias_port->port, rec);
	if (ret) {
		pr_debug("No more records to update.\n");
		goto out;
	}

	set_guid_rec(&dev->ib_dev, rec->port + 1, rec->block_num,
		     &rec->rec_det);

out:
	kfree(rec);
}


void mlx4_ib_init_alias_guid_work(struct mlx4_ib_dev *dev, int port)
{
	unsigned long flags, flags1;

	if (!mlx4_is_master(dev->dev))
		return;
	spin_lock_irqsave(&dev->sriov.going_down_lock, flags);
	spin_lock_irqsave(&dev->sriov.alias_guid.ag_work_lock, flags1);
	if (!dev->sriov.is_going_down) {
		/* If there is pending one should cancell then run, otherwise won't run till previous
		* one is ended as same work struct is used.
		*/
		cancel_delayed_work(&dev->sriov.alias_guid.ports_guid[port].alias_guid_work);
		queue_delayed_work(dev->sriov.alias_guid.ports_guid[port].wq,
			   &dev->sriov.alias_guid.ports_guid[port].alias_guid_work, 0);
	}
	spin_unlock_irqrestore(&dev->sriov.alias_guid.ag_work_lock, flags1);
	spin_unlock_irqrestore(&dev->sriov.going_down_lock, flags);
}

void mlx4_ib_destroy_alias_guid_service(struct mlx4_ib_dev *dev)
{
	int i;
	struct mlx4_ib_sriov *sriov = &dev->sriov;
	struct mlx4_alias_guid_work_context *cb_ctx;
	struct mlx4_sriov_alias_guid_port_rec_det *det;
	struct ib_sa_query *sa_query;
	unsigned long flags;

	for (i = 0 ; i < dev->num_ports; i++) {
		cancel_delayed_work(&dev->sriov.alias_guid.ports_guid[i].alias_guid_work);
		det = &sriov->alias_guid.ports_guid[i];
		spin_lock_irqsave(&sriov->alias_guid.ag_work_lock, flags);
		while (!list_empty(&det->cb_list)) {
			cb_ctx = list_entry(det->cb_list.next,
					    struct mlx4_alias_guid_work_context,
					    list);
			sa_query = cb_ctx->sa_query;
			cb_ctx->sa_query = NULL;
			list_del(&cb_ctx->list);
			spin_unlock_irqrestore(&sriov->alias_guid.ag_work_lock, flags);
			ib_sa_cancel_query(cb_ctx->query_id, sa_query);
			wait_for_completion(&cb_ctx->done);
			kfree(cb_ctx);
			spin_lock_irqsave(&sriov->alias_guid.ag_work_lock, flags);
		}
		spin_unlock_irqrestore(&sriov->alias_guid.ag_work_lock, flags);
	}
	for (i = 0 ; i < dev->num_ports; i++) {
		flush_workqueue(dev->sriov.alias_guid.ports_guid[i].wq);
		destroy_workqueue(dev->sriov.alias_guid.ports_guid[i].wq);
	}
	ib_sa_unregister_client(dev->sriov.alias_guid.sa_client);
	kfree(dev->sriov.alias_guid.sa_client);
}

int mlx4_ib_init_alias_guid_service(struct mlx4_ib_dev *dev)
{
	char alias_wq_name[15];
	int ret = 0;
	int i, j, k;
	union ib_gid gid;

	if (!mlx4_is_master(dev->dev))
		return 0;
	dev->sriov.alias_guid.sa_client =
		kzalloc(sizeof *dev->sriov.alias_guid.sa_client, GFP_KERNEL);
	if (!dev->sriov.alias_guid.sa_client)
		return -ENOMEM;

	ib_sa_register_client(dev->sriov.alias_guid.sa_client);

	spin_lock_init(&dev->sriov.alias_guid.ag_work_lock);

	for (i = 1; i <= dev->num_ports; ++i) {
		if (dev->ib_dev.query_gid(&dev->ib_dev , i, 0, &gid)) {
			ret = -EFAULT;
			goto err_unregister;
		}
	}

	for (i = 0 ; i < dev->num_ports; i++) {
		memset(&dev->sriov.alias_guid.ports_guid[i], 0,
		       sizeof (struct mlx4_sriov_alias_guid_port_rec_det));
		/*Check if the SM doesn't need to assign the GUIDs*/
		for (j = 0; j < NUM_ALIAS_GUID_REC_IN_PORT; j++) {
			if (mlx4_ib_sm_guid_assign) {
				dev->sriov.alias_guid.ports_guid[i].
					all_rec_per_port[j].
					ownership = MLX4_GUID_DRIVER_ASSIGN;
				continue;
			}
			dev->sriov.alias_guid.ports_guid[i].all_rec_per_port[j].
					ownership = MLX4_GUID_NONE_ASSIGN;
			/*mark each val as it was deleted,
			  till the sysAdmin will give it valid val*/
			for (k = 0; k < NUM_ALIAS_GUID_IN_REC; k++) {
				*(__be64 *)&dev->sriov.alias_guid.ports_guid[i].
					all_rec_per_port[j].all_recs[GUID_REC_SIZE * k] =
						cpu_to_be64(MLX4_GUID_FOR_DELETE_VAL);
			}
		}
		INIT_LIST_HEAD(&dev->sriov.alias_guid.ports_guid[i].cb_list);
		/*prepare the records, set them to be allocated by sm*/
		for (j = 0 ; j < NUM_ALIAS_GUID_REC_IN_PORT; j++)
			invalidate_guid_record(dev, i + 1, j, 1);

		dev->sriov.alias_guid.ports_guid[i].parent = &dev->sriov.alias_guid;
		dev->sriov.alias_guid.ports_guid[i].port  = i;
		snprintf(alias_wq_name, sizeof alias_wq_name, "alias_guid%d", i);
		dev->sriov.alias_guid.ports_guid[i].wq =
			create_singlethread_workqueue(alias_wq_name);
		if (!dev->sriov.alias_guid.ports_guid[i].wq) {
			ret = -ENOMEM;
			goto err_thread;
		}
		INIT_DELAYED_WORK(&dev->sriov.alias_guid.ports_guid[i].alias_guid_work,
			  alias_guid_work);
	}
	return 0;

err_thread:
	for (--i; i >= 0; i--) {
		destroy_workqueue(dev->sriov.alias_guid.ports_guid[i].wq);
		dev->sriov.alias_guid.ports_guid[i].wq = NULL;
	}

err_unregister:
	ib_sa_unregister_client(dev->sriov.alias_guid.sa_client);
	kfree(dev->sriov.alias_guid.sa_client);
	dev->sriov.alias_guid.sa_client = NULL;
	pr_err("init_alias_guid_service: Failed. (ret:%d)\n", ret);
	return ret;
}
