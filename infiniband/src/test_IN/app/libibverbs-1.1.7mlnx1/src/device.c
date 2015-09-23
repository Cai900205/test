/*
 * Copyright (c) 2004, 2005 Topspin Communications.  All rights reserved.
 * Copyright (c) 2006, 2007 Cisco Systems, Inc.  All rights reserved.
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

#if HAVE_CONFIG_H
#  include <config.h>
#endif /* HAVE_CONFIG_H */

#include <stdio.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <alloca.h>
#include <errno.h>

#include <infiniband/arch.h>

#include "ibverbs.h"

static pthread_once_t device_list_once = PTHREAD_ONCE_INIT;
static int num_devices;
static struct ibv_device **device_list;

static void count_devices(void)
{
	num_devices = ibverbs_init(&device_list);
}

struct ibv_device **__ibv_get_device_list(int *num)
{
	struct ibv_device **l;
	int i;

	if (num)
		*num = 0;

	pthread_once(&device_list_once, count_devices);

	if (num_devices < 0) {
		errno = -num_devices;
		return NULL;
	}

	l = calloc(num_devices + 1, sizeof (struct ibv_device *));
	if (!l) {
		errno = ENOMEM;
		return NULL;
	}

	for (i = 0; i < num_devices; ++i)
		l[i] = device_list[i];
	if (num)
		*num = num_devices;

	return l;
}
default_symver(__ibv_get_device_list, ibv_get_device_list);

void __ibv_free_device_list(struct ibv_device **list)
{
	free(list);
}
default_symver(__ibv_free_device_list, ibv_free_device_list);

const char *__ibv_get_device_name(struct ibv_device *device)
{
	return device->name;
}
default_symver(__ibv_get_device_name, ibv_get_device_name);

uint64_t __ibv_get_device_guid(struct ibv_device *device)
{
	char attr[24];
	uint64_t guid = 0;
	uint16_t parts[4];
	int i;

	if (ibv_read_sysfs_file(device->ibdev_path, "node_guid",
				attr, sizeof attr) < 0)
		return 0;

	if (sscanf(attr, "%hx:%hx:%hx:%hx",
		   parts, parts + 1, parts + 2, parts + 3) != 4)
		return 0;

	for (i = 0; i < 4; ++i)
		guid = (guid << 16) | parts[i];

	return htonll(guid);
}
default_symver(__ibv_get_device_guid, ibv_get_device_guid);

static int __ibv_exp_modify_cq(struct ibv_cq *cq,
			       struct ibv_exp_cq_attr *attr,
			       int attr_mask)
{
	fprintf(stderr, PFX "Fatal: device doesn't support function.\n");
	return ENOSYS;
}

static int __ibv_exp_query_device(struct ibv_context *context,
				  struct ibv_exp_device_attr *attr)
{
	fprintf(stderr, PFX "Fatal: device doesn't support function.\n");
	return ENOSYS;
}

static int __ibv_exp_post_task(struct ibv_context *context,
			       struct ibv_exp_task *task_list,
			       struct ibv_exp_task **bad_task)
{
	fprintf(stderr, PFX "Fatal: device doesn't support function.\n");
	return ENOSYS;
}

int __ibv_exp_prefetch_mr(struct ibv_mr *mr, struct ibv_exp_prefetch_attr *attr)
{
	fprintf(stderr, PFX "Fatal: device doesn't support function.\n");
	return ENOSYS;
}

static int __ibv_exp_bind_mw(struct ibv_exp_mw_bind *mw_bind)
{
	fprintf(stderr, PFX "Fatal: device doesn't support function.\n");
	return ENOSYS;
}

static int __ibv_exp_arm_dct(struct ibv_exp_dct *dct,
			     struct ibv_exp_arm_attr *attr)
{
	fprintf(stderr, PFX "Fatal: device doesn't support function.\n");
	return ENOSYS;
}

static int __ibv_exp_modify_qp(struct ibv_qp *qp, struct ibv_exp_qp_attr *attr,
			uint64_t exp_attr_mask)
{
	struct verbs_context_exp *vctx;
	int ret;

	vctx = verbs_get_exp_ctx_op(qp->context, drv_exp_modify_qp);
	if (!vctx) {
		errno = ENOSYS;
		return errno;
	}
	ret = vctx->drv_exp_modify_qp(qp, attr, exp_attr_mask);
	if (ret)
		return ret;

	if (exp_attr_mask & IBV_EXP_QP_STATE)
		qp->state = attr->qp_state;

	return 0;
}

struct ibv_mr *__ibv_exp_create_mr(struct ibv_exp_create_mr_in *in)
{
	fprintf(stderr, PFX "Fatal: device doesn't support function.\n");
	errno = ENOSYS;
	return NULL;
}

struct ibv_exp_mkey_list_container *__ibv_exp_alloc_mkey_list_memory(struct ibv_exp_mkey_list_container_attr  *attr)
{
	fprintf(stderr, PFX "Fatal: device doesn't support function.\n");
	errno = ENOSYS;
	return NULL;
}

int __ibv_exp_dealloc_mkey_list_memory(struct ibv_exp_mkey_list_container *mem)
{
	fprintf(stderr, PFX "Fatal: device doesn't support function.\n");
	errno = ENOSYS;
	return errno;
}

int __ibv_exp_query_mkey(struct ibv_mr *mr,
			 struct ibv_exp_mkey_attr *query_mkey_in)
{
	fprintf(stderr, PFX "Fatal: device doesn't support function.\n");
	errno = ENOSYS;
	return errno;
}

int __ibv_exp_rereg_mr(struct ibv_mr *mr, int flags,
		       struct ibv_pd *pd, void *addr,
		       size_t length, uint64_t access,
		       struct ibv_exp_rereg_mr_attr *attr)
{
	int dofork_onfail = 0;
	int err;
	struct verbs_context_exp *vctx;
	void *old_addr;
	size_t old_len;

	if (attr->comp_mask & ~(IBV_EXP_REREG_MR_ATTR_RESERVED - 1))
		return errno = EINVAL;

	if (flags & ~IBV_EXP_REREG_MR_FLAGS_SUPPORTED)
		return errno = EINVAL;

	if ((flags & IBV_EXP_REREG_MR_CHANGE_TRANSLATION) &&
	    (0 >= length))
		return errno = EINVAL;

	if (!(flags & IBV_EXP_REREG_MR_CHANGE_ACCESS))
		access = 0;

	if ((access & IBV_EXP_ACCESS_ALLOCATE_MR) &&
	    (!(flags & IBV_EXP_REREG_MR_CHANGE_TRANSLATION) ||
	    (addr != NULL)))
			return errno = EINVAL;

	if ((!(access & IBV_EXP_ACCESS_ALLOCATE_MR)) &&
	    (flags & IBV_EXP_REREG_MR_CHANGE_TRANSLATION) &&
	    (addr == NULL))
		return errno = EINVAL;

	vctx = verbs_get_exp_ctx_op(mr->context, drv_exp_rereg_mr);
	if (!vctx)
		return errno = ENOSYS;

	/* If address will be allocated internally fork support is handled by the provider */
	if (!(access & IBV_EXP_ACCESS_ALLOCATE_MR) &&
	    flags & IBV_EXP_REREG_MR_CHANGE_TRANSLATION) {
		err = ibv_dontfork_range(addr, length);
		if (err)
			return err;
		dofork_onfail = 1;
	}

	old_addr = mr->addr;
	old_len = mr->length;
	err = vctx->drv_exp_rereg_mr(mr, flags, pd, addr, length, access, attr);
	if (!err) {
		if (flags & IBV_EXP_REREG_MR_CHANGE_TRANSLATION) {
			ibv_dofork_range(old_addr, old_len);
			if (access & IBV_EXP_ACCESS_ALLOCATE_MR) {
				;
			} else {
				/* In case that internal allocator was used
				     addr already set internally
				*/
				mr->addr    = addr;
				mr->length  = length;
			}
		}
		if (flags & IBV_EXP_REREG_MR_CHANGE_PD)
			mr->pd = pd;
	} else if (dofork_onfail) {
		ibv_dofork_range(addr, length);
	}

	return err;
}

struct ibv_context *__ibv_open_device(struct ibv_device *device)
{
	struct verbs_device *verbs_device = verbs_get_device(device);
	char *devpath;
	int cmd_fd, ret;
	struct ibv_context *context;
	struct verbs_context *context_ex;
	struct verbs_context_exp *context_exp;

	if (asprintf(&devpath, "/dev/infiniband/%s", device->dev_name) < 0)
		return NULL;

	/*
	 * We'll only be doing writes, but we need O_RDWR in case the
	 * provider needs to mmap() the file.
	 */
	cmd_fd = open(devpath, O_RDWR);
	free(devpath);

	if (cmd_fd < 0)
		return NULL;

	if (!verbs_device) {
		context = device->ops.alloc_context(device, cmd_fd);
		if (!context)
			goto err;
	} else {
		/* Library now allocates the context */
		context_exp = calloc(1, sizeof(*context_ex) + sizeof(*context_exp) +
				     verbs_device->size_of_context);
		if (!context_exp) {
			errno = ENOMEM;
			goto err;
		}

		context_ex = (struct verbs_context *)((void *)context_exp + sizeof(*context_exp));
		context_exp->sz = sizeof(*context_exp);
		context_ex->has_comp_mask |= VERBS_CONTEXT_EXP;
		context_ex->context.abi_compat  = __VERBS_ABI_IS_EXTENDED;
		context_ex->sz = sizeof(*context_ex);

		context = &context_ex->context;
		ret = verbs_device->init_context(verbs_device, context, cmd_fd);
		if (ret)
			goto verbs_err;

		/* initialize *all* library ops to either lib calls or
		 * directly to provider calls.
		 * context_ex->lib_new_func1 = __verbs_new_func1;
		 * context_ex->lib_new_func2 = __verbs_new_func2;
		 */

		/* initialize *all* library experimental ops to either lib calls or
		 * directly to provider calls.
		 * context_exp->lib_new_func1 = __verbs_new_func1;
		 * context_exp->lib_new_func2 = __verbs_new_func2;
		 */
		 context_exp->lib_exp_create_qp = context_exp->drv_exp_create_qp;
		 context_exp->lib_exp_query_device =  context_exp->drv_exp_query_device;
		 context_exp->lib_exp_query_port =
			 context_exp->drv_exp_query_port;

		 context_exp->lib_exp_ibv_reg_shared_mr = __ibv_reg_shared_mr;
		 context_exp->lib_exp_ibv_create_flow =
			 context_exp->drv_exp_ibv_create_flow;
		 context_exp->lib_exp_ibv_destroy_flow =
			 context_exp->drv_exp_ibv_destroy_flow;
		 context_exp->lib_exp_modify_cq = (context_exp->drv_exp_modify_cq ?
			 context_exp->drv_exp_modify_cq :
			 __ibv_exp_modify_cq);
		 context_exp->lib_exp_query_device = (context_exp->drv_exp_query_device ?
			 context_exp->drv_exp_query_device :
			 __ibv_exp_query_device);

		 context_exp->lib_exp_modify_qp = __ibv_exp_modify_qp;

		 context_exp->lib_exp_post_task = (context_exp->drv_exp_post_task ?
			 context_exp->drv_exp_post_task :
			 __ibv_exp_post_task);
		 context_exp->lib_exp_reg_mr = __ibv_exp_reg_mr;
		 context_exp->lib_exp_bind_mw = (context_exp->drv_exp_bind_mw ?
			 context_exp->drv_exp_bind_mw :
			 __ibv_exp_bind_mw);
		 context_exp->lib_exp_arm_dct = (context_exp->drv_exp_arm_dct ?
			 context_exp->drv_exp_arm_dct :
			 __ibv_exp_arm_dct);
		 context_exp->lib_exp_create_mr = (context_exp->drv_exp_create_mr ?
			 context_exp->drv_exp_create_mr :
			 __ibv_exp_create_mr);
		 context_exp->lib_exp_query_mkey = (context_exp->drv_exp_query_mkey ?
			 context_exp->drv_exp_query_mkey :
			 __ibv_exp_query_mkey);
		 context_exp->lib_exp_dealloc_mkey_list_memory = (context_exp->drv_exp_dealloc_mkey_list_memory ?
			 context_exp->drv_exp_dealloc_mkey_list_memory :
			 __ibv_exp_dealloc_mkey_list_memory);
		 context_exp->lib_exp_alloc_mkey_list_memory = (context_exp->drv_exp_alloc_mkey_list_memory ?
			 context_exp->drv_exp_alloc_mkey_list_memory :
			 __ibv_exp_alloc_mkey_list_memory);
		 context_exp->lib_exp_prefetch_mr =
			(context_exp->drv_exp_prefetch_mr ?
			context_exp->drv_exp_prefetch_mr :
			__ibv_exp_prefetch_mr);

		 context_exp->drv_exp_rereg_mr = context_exp->exp_rereg_mr;
		 context_exp->exp_rereg_mr = __ibv_exp_rereg_mr;
	}

	context->device = device;
	context->cmd_fd = cmd_fd;
	pthread_mutex_init(&context->mutex, NULL);

	return context;

verbs_err:
	free(context_exp);
err:
	close(cmd_fd);
	return NULL;
}
default_symver(__ibv_open_device, ibv_open_device);

int __ibv_close_device(struct ibv_context *context)
{
	int async_fd = context->async_fd;
	int cmd_fd   = context->cmd_fd;

	struct verbs_context_exp *context_exp;

	context_exp = verbs_get_exp_ctx(context);

	if (context_exp) {
		struct verbs_device *verbs_device = verbs_get_device(context->device);
		verbs_device->uninit_context(verbs_device, context);
		free(context_exp);
	} else {
		context->device->ops.free_context(context);
	}

	close(async_fd);
	close(cmd_fd);

	return 0;
}
default_symver(__ibv_close_device, ibv_close_device);

int __ibv_get_async_event(struct ibv_context *context,
			  struct ibv_async_event *event)
{
	struct ibv_kern_async_event ev;
	struct verbs_context_exp *vctx;
	struct ibv_srq_legacy *ibv_srq_legacy = NULL;
	struct ibv_qp *qp;
	enum ibv_event_rsc_type rsc_type;

	if (read(context->async_fd, &ev, sizeof ev) != sizeof ev)
		return -1;

	event->event_type = ev.event_type;
	rsc_type = ev.rsc_type;

	switch (rsc_type) {
	case IBV_EVENT_RSC_CQ:
		event->element.cq = (void *)(uintptr_t)ev.element;
		switch (event->event_type) {
		case IBV_EVENT_CQ_ERR:
			break;
		default:
			fprintf(stderr, "Invalid CQ event (%d)\n", event->event_type);
		}
		break;

	case IBV_EVENT_RSC_QP:
		event->element.qp = (void *)(uintptr_t)ev.element;
		switch (event->event_type) {
		case IBV_EVENT_QP_FATAL:
		case IBV_EVENT_QP_REQ_ERR:
		case IBV_EVENT_QP_ACCESS_ERR:
		case IBV_EVENT_COMM_EST:
		case IBV_EVENT_SQ_DRAINED:
		case IBV_EVENT_PATH_MIG:
		case IBV_EVENT_PATH_MIG_ERR:
		case IBV_EVENT_QP_LAST_WQE_REACHED:
			qp = ibv_find_xrc_qp(event->element.qp->qp_num);
			if (qp) {
				/* This is XRC reciever QP created by the legacy API */
				event->event_type |= IBV_XRC_QP_EVENT_FLAG;
				event->element.qp = NULL;
				event->element.xrc_qp_num = qp->qp_num;
			}
			break;
		default:
			fprintf(stderr, "Invalid QP event (%d)\n", event->event_type);
		}
		break;

	case IBV_EVENT_RSC_DCT:
		event->element.dct = (void *)(uintptr_t)ev.element;
		switch (event->event_type) {
		case IBV_EXP_EVENT_DCT_KEY_VIOLATION:
		case IBV_EXP_EVENT_DCT_ACCESS_ERR:
		case IBV_EXP_EVENT_DCT_REQ_ERR:
			break;
		default:
			fprintf(stderr, "Invalid DCT event (%d)\n", event->event_type);
		}
		break;

	case IBV_EVENT_RSC_SRQ:
		vctx = verbs_get_exp_ctx_op(context, drv_exp_get_legacy_xrc);
		if (vctx)
			/* ev.elemant is ibv_srq comes from the kernel, in case there is leagcy one
			  * it should be returened instead.
			*/
			ibv_srq_legacy = vctx->drv_exp_get_legacy_xrc((void *) (uintptr_t) ev.element);

		event->element.srq = (ibv_srq_legacy) ? (void *)ibv_srq_legacy :
						(void *) (uintptr_t) ev.element;
		switch (event->event_type) {
		case IBV_EVENT_SRQ_ERR:
		case IBV_EVENT_SRQ_LIMIT_REACHED:
			break;
		default:
			fprintf(stderr, "Invalid SRQ event (%d)\n", event->event_type);
		}

		break;

	case IBV_EVENT_RSC_DEVICE:
		event->element.port_num = ev.element;
		switch (event->event_type) {
		case IBV_EVENT_DEVICE_FATAL:
		case IBV_EVENT_PORT_ACTIVE:
		case IBV_EVENT_PORT_ERR:
		case IBV_EVENT_LID_CHANGE:
		case IBV_EVENT_PKEY_CHANGE:
		case IBV_EVENT_SM_CHANGE:
		case IBV_EVENT_CLIENT_REREGISTER:
			break;
		default:
			fprintf(stderr, "Invalid Device event (%d)\n", event->event_type);
		}
		break;
	default:
		fprintf(stderr, "Invalid resource type (%d)\n", rsc_type);
	}

	if (context->ops.async_event)
		context->ops.async_event(event);

	return 0;
}
default_symver(__ibv_get_async_event, ibv_get_async_event);

void __ibv_ack_async_event(struct ibv_async_event *event)
{
	int is_legacy_xrc = 0;
	struct ibv_exp_dct *dct;

	if (event->event_type & IBV_XRC_QP_EVENT_FLAG) {
		event->event_type ^= IBV_XRC_QP_EVENT_FLAG;
		is_legacy_xrc = 1;
	}

	switch (event->event_type) {
	case IBV_EVENT_CQ_ERR:
	{
		struct ibv_cq *cq = event->element.cq;

		pthread_mutex_lock(&cq->mutex);
		++cq->async_events_completed;
		pthread_cond_signal(&cq->cond);
		pthread_mutex_unlock(&cq->mutex);

		return;
	}

	case IBV_EVENT_QP_FATAL:
	case IBV_EVENT_QP_REQ_ERR:
	case IBV_EVENT_QP_ACCESS_ERR:
	case IBV_EVENT_COMM_EST:
	case IBV_EVENT_SQ_DRAINED:
	case IBV_EVENT_PATH_MIG:
	case IBV_EVENT_PATH_MIG_ERR:
	case IBV_EVENT_QP_LAST_WQE_REACHED:
	{
		struct ibv_qp *qp = event->element.qp;

		if (is_legacy_xrc) {
		/* Looking for ibv_qp for this XRC reciever QPN */
			qp = ibv_find_xrc_qp(event->element.xrc_qp_num);
			/* Even if found a qp making sure that it matches, would like
			* to prevent rare case while pointer value was matched to qp number.
			*/
			if (!qp || qp->qp_num != event->element.xrc_qp_num) {
				fprintf(stderr, PFX "Warning: ibv_ack_async_event, XRC qpn=%u wasn't found\n",
					event->element.xrc_qp_num);
				return;
			}
		}

		pthread_mutex_lock(&qp->mutex);
		++qp->events_completed;
		pthread_cond_signal(&qp->cond);
		pthread_mutex_unlock(&qp->mutex);

		return;
	}

	case IBV_EVENT_SRQ_ERR:
	case IBV_EVENT_SRQ_LIMIT_REACHED:
	{
		struct ibv_srq *srq = event->element.srq;

		if (srq->handle == LEGACY_XRC_SRQ_HANDLE) {
			struct ibv_srq_legacy *ibv_srq_legacy =
					(struct ibv_srq_legacy *) srq;
			srq = ibv_srq_legacy->ibv_srq;
		}

		/* We should use here the internal mutx/cond even in legacy mode */
		pthread_mutex_lock(&srq->mutex);
		++srq->events_completed;
		pthread_cond_signal(&srq->cond);
		pthread_mutex_unlock(&srq->mutex);

		return;
	}

	case IBV_EXP_EVENT_DCT_KEY_VIOLATION:
	case IBV_EXP_EVENT_DCT_ACCESS_ERR:
	case IBV_EXP_EVENT_DCT_REQ_ERR:
		dct = event->element.dct;
		pthread_mutex_lock(&dct->mutex);
		dct->events_completed++;
		pthread_cond_signal(&dct->cond);
		pthread_mutex_unlock(&dct->mutex);
		break;

	default:
		return;
	}
}
default_symver(__ibv_ack_async_event, ibv_ack_async_event);
