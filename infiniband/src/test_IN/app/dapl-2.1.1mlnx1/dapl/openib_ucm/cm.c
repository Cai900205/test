/*
 * Copyright (c) 2009 Intel Corporation.  All rights reserved.
 *
 * This Software is licensed under one of the following licenses:
 *
 * 1) under the terms of the "Common Public License 1.0" a copy of which is
 *    available from the Open Source Initiative, see
 *    http://www.opensource.org/licenses/cpl.php.
 *
 * 2) under the terms of the "The BSD License" a copy of which is
 *    available from the Open Source Initiative, see
 *    http://www.opensource.org/licenses/bsd-license.php.
 *
 * 3) under the terms of the "GNU General Public License (GPL) Version 2" a
 *    copy of which is available from the Open Source Initiative, see
 *    http://www.opensource.org/licenses/gpl-license.php.
 *
 * Licensee has the right to choose one of the above licenses.
 *
 * Redistributions of source code must retain the above copyright
 * notice and one of the license notices.
 *
 * Redistributions in binary form must reproduce both the above copyright
 * notice, one of the license notices in the documentation
 * and/or other materials provided with the distribution.
 */

#include "dapl.h"
#include "dapl_adapter_util.h"
#include "dapl_evd_util.h"
#include "dapl_cr_util.h"
#include "dapl_name_service.h"
#include "dapl_ib_util.h"
#include "dapl_ep_util.h"
#include "dapl_sp_util.h"
#include "dapl_osd.h"


#if defined(_WIN32)
#include <rdma\winverbs.h>
#else				// _WIN32
enum DAPL_FD_EVENTS {
	DAPL_FD_READ = POLLIN,
	DAPL_FD_WRITE = POLLOUT,
	DAPL_FD_ERROR = POLLERR
};

struct dapl_fd_set {
	int index;
	struct pollfd set[DAPL_FD_SETSIZE];
};

static struct dapl_fd_set *dapl_alloc_fd_set(void)
{
	return dapl_os_alloc(sizeof(struct dapl_fd_set));
}

static void dapl_fd_zero(struct dapl_fd_set *set)
{
	set->index = 0;
}

static int dapl_fd_set(DAPL_SOCKET s, struct dapl_fd_set *set,
		       enum DAPL_FD_EVENTS event)
{
	if (set->index == DAPL_FD_SETSIZE - 1) {
		dapl_log(DAPL_DBG_TYPE_ERR,
			 "SCM ERR: cm_thread exceeded FD_SETSIZE %d\n",
			 set->index + 1);
		return -1;
	}

	set->set[set->index].fd = s;
	set->set[set->index].revents = 0;
	set->set[set->index++].events = event;
	return 0;
}

static enum DAPL_FD_EVENTS dapl_poll(DAPL_SOCKET s, enum DAPL_FD_EVENTS event)
{
	struct pollfd fds;
	int ret;

	fds.fd = s;
	fds.events = event;
	fds.revents = 0;
	ret = poll(&fds, 1, 0);
	dapl_log(DAPL_DBG_TYPE_THREAD, " dapl_poll: fd=%d ret=%d, evnts=0x%x\n",
		 s, ret, fds.revents);
	if (ret == 0)
		return 0;
	else if (fds.revents & (POLLERR | POLLHUP | POLLNVAL)) 
		return DAPL_FD_ERROR;
	else 
		return fds.revents;
}

static int dapl_select(struct dapl_fd_set *set, int time_ms)
{
	int ret;

	dapl_dbg_log(DAPL_DBG_TYPE_THREAD, " dapl_select: sleep, fds=%d\n",
		     set->index);
	ret = poll(set->set, set->index, time_ms);
	dapl_dbg_log(DAPL_DBG_TYPE_THREAD, " dapl_select: wakeup, ret=0x%x\n", ret);
	return ret;
}
#endif

/* forward declarations */
static int ucm_reply(dp_ib_cm_handle_t cm);
static void ucm_accept(ib_cm_srvc_handle_t cm, ib_cm_msg_t *msg);
static void ucm_connect_rtu(dp_ib_cm_handle_t cm, ib_cm_msg_t *msg);
static void ucm_accept_rtu(dp_ib_cm_handle_t cm, ib_cm_msg_t *msg);
static int ucm_send(ib_hca_transport_t *tp, ib_cm_msg_t *msg, DAT_PVOID p_data, DAT_COUNT p_size);
static void ucm_disconnect_final(dp_ib_cm_handle_t cm);
DAT_RETURN dapli_cm_disconnect(dp_ib_cm_handle_t cm);
DAT_RETURN dapli_cm_connect(DAPL_EP *ep, dp_ib_cm_handle_t cm);

/* Service ids - port space */
static uint16_t ucm_get_port(ib_hca_transport_t *tp, uint16_t port)
{
	int i = 0;
	
	dapl_os_lock(&tp->plock);
	/* get specific ID */
	if (port) {
		if (tp->sid[port] == 0) {
			tp->sid[port] = 1;
			i = port;
		}
		goto done;
	} 
	
	/* get any free ID */
	for (i = 0xffff; i > 0; i--) {
		if (tp->sid[i] == 0) {
			tp->sid[i] = 1;
			break;
		}
	}
done:
	dapl_os_unlock(&tp->plock);
	return i;
}

static void ucm_free_port(ib_hca_transport_t *tp, uint16_t port)
{
	dapl_os_lock(&tp->plock);
	tp->sid[port] = 0;
	dapl_os_unlock(&tp->plock);
}

static void ucm_check_timers(dp_ib_cm_handle_t cm, int *timer)
{
	DAPL_OS_TIMEVAL time;

        dapl_os_lock(&cm->lock);
	dapl_os_get_time(&time); 
	switch (cm->state) {
	case DCM_REP_PENDING: 
		*timer = cm->hca->ib_trans.cm_timer; 
		/* wait longer each retry */
		if ((time - cm->timer)/1000 > 
		    (cm->hca->ib_trans.rep_time << cm->retries)) {
			dapl_log(DAPL_DBG_TYPE_CM_WARN,
				 " CM_REQ retry %p %d [lid, port, cqp, iqp]:"
				 " %x %x %x %x -> %x %x %x %x Time(ms) %d > %d\n",
				 cm, cm->retries+1,
				 ntohs(cm->msg.saddr.ib.lid), ntohs(cm->msg.sport),
				 ntohl(cm->msg.sqpn), ntohl(cm->msg.saddr.ib.qpn),
				 ntohs(cm->msg.daddr.ib.lid), ntohs(cm->msg.dport),
				 ntohl(cm->msg.dqpn), ntohl(cm->msg.daddr.ib.qpn),
				 (time - cm->timer)/1000,
				 cm->hca->ib_trans.rep_time << cm->retries);
			cm->retries++;
			DAPL_CNTR(((DAPL_IA *)dapl_llist_peek_head(&cm->hca->ia_list_head)), DCNT_IA_CM_ERR_REQ_RETRY);
			dapl_os_unlock(&cm->lock);
			dapli_cm_connect(cm->ep, cm);
			return;
		}
		break;
	case DCM_RTU_PENDING: 
		*timer = cm->hca->ib_trans.cm_timer;  
		if ((time - cm->timer)/1000 > 
		    (cm->hca->ib_trans.rtu_time << cm->retries)) {
			dapl_log(DAPL_DBG_TYPE_CM_WARN,
				 " CM_REPLY retry %d %s [lid, port, cqp, iqp]:"
				 " %x %x %x %x -> %x %x %x %x r_pid %x Time(ms) %d > %d\n",
				 cm->retries+1,
				 dapl_cm_op_str(ntohs(cm->msg.op)),
				 ntohs(cm->msg.saddr.ib.lid), ntohs(cm->msg.sport),
				 ntohl(cm->msg.sqpn), ntohl(cm->msg.saddr.ib.qpn),
				 ntohs(cm->msg.daddr.ib.lid), ntohs(cm->msg.dport),
				 ntohl(cm->msg.dqpn), ntohl(cm->msg.daddr.ib.qpn),
				 ntohl(cm->msg.d_id),
				 (time - cm->timer)/1000, 
				 cm->hca->ib_trans.rtu_time << cm->retries);
			cm->retries++;
			DAPL_CNTR(((DAPL_IA *)dapl_llist_peek_head(&cm->hca->ia_list_head)), DCNT_IA_CM_ERR_REP_RETRY);
			dapl_os_unlock(&cm->lock);
			ucm_reply(cm);
			return;
		}
		break;
	case DCM_DISC_PENDING: 
		*timer = cm->hca->ib_trans.cm_timer; 
		/* wait longer each retry */
		if ((time - cm->timer)/1000 > 
		    (cm->hca->ib_trans.rtu_time << cm->retries)) {
			dapl_log(DAPL_DBG_TYPE_CM_WARN,
				 " CM_DREQ retry %d [lid, port, cqp, iqp]:"
				 " %x %x %x %x -> %x %x %x %x r_pid %x Time(ms) %d > %d\n",
				 cm->retries+1,
				 ntohs(cm->msg.saddr.ib.lid), ntohs(cm->msg.sport),
				 ntohl(cm->msg.sqpn), ntohl(cm->msg.saddr.ib.qpn),
				 ntohs(cm->msg.daddr.ib.lid), ntohs(cm->msg.dport),
				 ntohl(cm->msg.dqpn), ntohl(cm->msg.daddr.ib.qpn),
				 ntohl(cm->msg.d_id),
				 (time - cm->timer)/1000, 
				 cm->hca->ib_trans.rtu_time << cm->retries);
			cm->retries++;
			DAPL_CNTR(((DAPL_IA *)dapl_llist_peek_head(&cm->hca->ia_list_head)), DCNT_IA_CM_ERR_DREQ_RETRY);
			dapl_os_unlock(&cm->lock);
			dapli_cm_disconnect(cm);
                        return;
		}
		break;
	default:
		break;
	}
	dapl_os_unlock(&cm->lock);
}

/* SEND CM MESSAGE PROCESSING */

/* Get CM UD message from send queue, called with s_lock held */
static ib_cm_msg_t *ucm_get_smsg(ib_hca_transport_t *tp)
{
	ib_cm_msg_t *msg = NULL; 
	int ret, polled = 1, hd = tp->s_hd;

	hd++;

	if (hd == tp->qpe)
		hd = 0;
retry:
	if (hd == tp->s_tl) {
		msg = NULL;
		if (polled % 1000000 == 0)
			dapl_log(DAPL_DBG_TYPE_WARN,
				 " ucm_get_smsg: FULLq hd %d == tl %d,"
				 " completions stalled, polls=%d\n",
				 hd, tp->s_tl, polled);
	}
	else {
		msg = &tp->sbuf[hd];
		tp->s_hd = hd; /* new hd */
	}

	/* if empty, process some completions */
	if (msg == NULL) {
		struct ibv_wc wc;

		/* process completions, based on UCM_TX_BURST */
		ret = ibv_poll_cq(tp->scq, 1, &wc);
		if (ret < 0) {
			dapl_log(DAPL_DBG_TYPE_WARN,
				" get_smsg: cq %p %s\n",
				tp->scq, strerror(errno));
			return NULL;
		}
		/* free up completed sends, update tail */
		if (ret > 0)
			tp->s_tl = (int)wc.wr_id;

		polled++;
		goto retry;
	}
	DAPL_CNTR_DATA(((DAPL_IA *)dapl_llist_peek_head(&tp->hca->ia_list_head)), DCNT_IA_CM_ERR_REQ_FULLQ, polled > 1 ? 1:0);
	DAPL_CNTR_DATA(((DAPL_IA *)dapl_llist_peek_head(&tp->hca->ia_list_head)), DCNT_IA_CM_REQ_FULLQ_POLL, polled - 1);
	return msg;
}

/* RECEIVE CM MESSAGE PROCESSING */

static int ucm_post_rmsg(ib_hca_transport_t *tp, ib_cm_msg_t *msg)
{	
	struct ibv_recv_wr recv_wr, *recv_err;
	struct ibv_sge sge;
        
	recv_wr.next = NULL;
	recv_wr.sg_list = &sge;
	recv_wr.num_sge = 1;
	recv_wr.wr_id = (uint64_t)(uintptr_t) msg;
	sge.length = sizeof(ib_cm_msg_t) + sizeof(struct ibv_grh);
	sge.lkey = tp->mr_rbuf->lkey;
	sge.addr = (uintptr_t)((char *)msg - sizeof(struct ibv_grh));
	
	return (ibv_post_recv(tp->qp, &recv_wr, &recv_err));
}

static int ucm_reject(ib_hca_transport_t *tp, ib_cm_msg_t *msg)
{
	ib_cm_msg_t	smsg;

	/* setup op, rearrange the src, dst cm and addr info */
	(void)dapl_os_memzero(&smsg, sizeof(smsg));
	smsg.ver = htons(DCM_VER);
	smsg.op = htons(DCM_REJ_CM);
	smsg.dport = msg->sport;
	smsg.dqpn = msg->sqpn;
	smsg.sport = msg->dport; 
	smsg.sqpn = msg->dqpn;

	dapl_os_memcpy(&smsg.daddr, &msg->saddr, sizeof(union dcm_addr));
	
	/* no dst_addr IB info in REQ, init lid, gid, get type from saddr */
	smsg.saddr.ib.lid = tp->addr.ib.lid; 
	smsg.saddr.ib.qp_type = msg->saddr.ib.qp_type;
	dapl_os_memcpy(&smsg.saddr.ib.gid[0],
		       &tp->addr.ib.gid, 16); 

	dapl_os_memcpy(&smsg.saddr, &msg->daddr, sizeof(union dcm_addr));

	dapl_dbg_log(DAPL_DBG_TYPE_CM, 
		     " CM reject -> LID %x, QPN %x PORT %x\n", 
		     ntohs(smsg.daddr.ib.lid),
		     ntohl(smsg.dqpn), ntohs(smsg.dport));

	DAPL_CNTR(((DAPL_IA *)dapl_llist_peek_head(&tp->hca->ia_list_head)), DCNT_IA_CM_ERR_REJ_TX);
	return (ucm_send(tp, &smsg, NULL, 0));
}

static void ucm_process_recv(ib_hca_transport_t *tp, 
			     ib_cm_msg_t *msg, 
			     dp_ib_cm_handle_t cm)
{
	dapl_os_lock(&cm->lock);
	switch (cm->state) {
	case DCM_LISTEN: /* passive */
		dapl_os_unlock(&cm->lock);
		ucm_accept(cm, msg);
		break;
	case DCM_RTU_PENDING: /* passive */
		dapl_os_unlock(&cm->lock);
		ucm_accept_rtu(cm, msg);
		break;
	case DCM_REP_PENDING: /* active */
		dapl_os_unlock(&cm->lock);
		ucm_connect_rtu(cm, msg);
		break;
	case DCM_CONNECTED: /* active and passive */
		/* DREQ, change state and process */
		cm->retries = 2; 
		if (ntohs(msg->op) == DCM_DREQ) {
			cm->state = DCM_DISC_RECV;
			dapl_os_unlock(&cm->lock);
			dapli_cm_disconnect(cm);
			break;
		} 
		/* active: RTU was dropped, resend */
		if (ntohs(msg->op) == DCM_REP) {
			dapl_log(DAPL_DBG_TYPE_CM_WARN,
				 " RESEND RTU: op %s st %s [lid, port, cqp, iqp]:"
				 " %x %x %x %x -> %x %x %x %x r_pid %x\n",
				  dapl_cm_op_str(ntohs(cm->msg.op)),
				  dapl_cm_state_str(cm->state),
				 ntohs(cm->msg.saddr.ib.lid), ntohs(cm->msg.sport),
				 ntohl(cm->msg.sqpn), ntohl(cm->msg.saddr.ib.qpn),
				 ntohs(cm->msg.daddr.ib.lid), ntohs(cm->msg.dport),
				 ntohl(cm->msg.dqpn), ntohl(cm->msg.daddr.ib.qpn),
				 ntohl(cm->msg.d_id));

			cm->msg.op = htons(DCM_RTU);
			ucm_send(&cm->hca->ib_trans, &cm->msg, NULL, 0);

			DAPL_CNTR(((DAPL_IA *)dapl_llist_peek_head(&cm->hca->ia_list_head)), DCNT_IA_CM_ERR_RTU_RETRY);
		}
		dapl_os_unlock(&cm->lock);
		break;
	case DCM_DISC_PENDING: /* active and passive */
		/* DREQ or DREP, finalize */
		dapl_os_unlock(&cm->lock);
		ucm_disconnect_final(cm);
		break;
	case DCM_DISCONNECTED:
	case DCM_FREE:
		/* DREQ dropped, resend */
		if (ntohs(msg->op) == DCM_DREQ) {
			dapl_log(DAPL_DBG_TYPE_CM_WARN,
				" RESEND DREP: op %s st %s [lid, port, qpn]:"
				" %x %x %x -> %x %x %x\n", 
				dapl_cm_op_str(ntohs(msg->op)), 
				dapl_cm_state_str(cm->state),
				ntohs(msg->saddr.ib.lid), 
				ntohs(msg->sport),
				ntohl(msg->saddr.ib.qpn), 
				ntohs(msg->daddr.ib.lid), 
				ntohs(msg->dport),
				ntohl(msg->daddr.ib.qpn));  
			cm->msg.op = htons(DCM_DREP);
			ucm_send(&cm->hca->ib_trans, &cm->msg, NULL, 0); 
			
			DAPL_CNTR(((DAPL_IA *)dapl_llist_peek_head(&cm->hca->ia_list_head)), DCNT_IA_CM_ERR_DREP_RETRY);

		} else if (ntohs(msg->op) != DCM_DREP){
			/* DREP ok to ignore, any other print warning */
			dapl_log(DAPL_DBG_TYPE_WARN,
				" ucm_recv: UNEXPECTED MSG on cm %p"
				" <- op %s, st %s spsp %x sqpn %x\n", 
				cm, dapl_cm_op_str(ntohs(msg->op)),
				dapl_cm_state_str(cm->state),
				ntohs(msg->sport), ntohl(msg->sqpn));
			DAPL_CNTR(((DAPL_IA *)dapl_llist_peek_head(&cm->hca->ia_list_head)), DCNT_IA_CM_ERR_UNEXPECTED);
		}
		dapl_os_unlock(&cm->lock);
		break;
	case DCM_REJECTED:
		if (ntohs(msg->op) == DCM_REJ_USER) {
			DAPL_CNTR(((DAPL_IA *)dapl_llist_peek_head(&cm->hca->ia_list_head)), DCNT_IA_CM_USER_REJ_RX);
			dapl_os_unlock(&cm->lock);
			break;
		}
	default:
		dapl_log(DAPL_DBG_TYPE_CM_WARN,
			" ucm_recv: Warning, UNKNOWN state"
			" <- op %s, %s spsp %x sqpn %x slid %x\n",
			dapl_cm_op_str(ntohs(msg->op)),
			dapl_cm_state_str(cm->state),
			ntohs(msg->sport), ntohl(msg->sqpn),
			ntohs(msg->saddr.ib.lid));
		dapl_os_unlock(&cm->lock);
		break;
	}
}

/* Find matching CM object for this receive message, return CM reference, timer */
dp_ib_cm_handle_t ucm_cm_find(ib_hca_transport_t *tp, ib_cm_msg_t *msg)
{
	dp_ib_cm_handle_t cm = NULL, next, found = NULL;
	struct dapl_llist_entry	**list;
	DAPL_OS_LOCK *lock;
	int listenq = 0;

	/* conn list first, duplicate requests for DCM_REQ */
	list = &tp->list;
	lock = &tp->lock;

retry_listenq:
	dapl_os_lock(lock);
        if (!dapl_llist_is_empty(list))
		next = dapl_llist_peek_head(list);
	else
		next = NULL;

	while (next) {
		cm = next;
		next = dapl_llist_next_entry(list,
					     (DAPL_LLIST_ENTRY *)&cm->local_entry);
		if (cm->state == DCM_DESTROY || cm->state == DCM_FREE)
			continue;
		
		/* CM sPORT + QPN, match is good enough for listenq */
		if (listenq && 
		    cm->msg.sport == msg->dport && 
		    cm->msg.sqpn == msg->dqpn) {
			found = cm;
			break;
		}	 
		/* connectq, check src and dst plus id's, check duplicate conn_reqs */
		if (!listenq && 
		    cm->msg.sport == msg->dport && cm->msg.sqpn == msg->dqpn && 
		    cm->msg.dport == msg->sport && cm->msg.dqpn == msg->sqpn && 
		    cm->msg.daddr.ib.lid == msg->saddr.ib.lid) {
			if (ntohs(msg->op) != DCM_REQ) {
				found = cm;
				break; 
			} else {
				/* duplicate; bail and throw away */
				dapl_os_unlock(lock);
				dapl_log(DAPL_DBG_TYPE_CM_WARN,
					 " DUPLICATE: cm %p op %s (%s) st %s"
					 " [lid, port, cqp, iqp]:"
					 " %x %x %x %x <- (%x %x %x %x :"
					 " %x %x %x %x) -> %x %x %x %x\n",
					 cm, dapl_cm_op_str(ntohs(msg->op)),
					 dapl_cm_op_str(ntohs(cm->msg.op)),
					 dapl_cm_state_str(cm->state),
					 ntohs(cm->msg.daddr.ib.lid), ntohs(cm->msg.dport),
					 ntohl(cm->msg.dqpn), ntohl(cm->msg.daddr.ib.qpn),
					 ntohs(msg->saddr.ib.lid), ntohs(msg->sport),
					 ntohl(msg->sqpn), ntohl(msg->saddr.ib.qpn),
					 ntohs(msg->daddr.ib.lid), ntohs(msg->dport),
					 ntohl(msg->dqpn), ntohl(msg->daddr.ib.qpn),
					 ntohs(cm->msg.saddr.ib.lid), ntohs(cm->msg.sport),
					 ntohl(cm->msg.sqpn), ntohl(cm->msg.saddr.ib.qpn));

					DAPL_CNTR(((DAPL_IA *)dapl_llist_peek_head(&cm->hca->ia_list_head)), DCNT_IA_CM_ERR_REQ_DUP);

					return NULL;
			}
		}
	}
	dapl_os_unlock(lock);

	/* no duplicate request on connq, check listenq for new request */
	if (ntohs(msg->op) == DCM_REQ && !listenq && !found) {
		listenq = 1;
		list = &tp->llist;
		lock = &tp->llock;
		goto retry_listenq;
	}

	/* not match on listenq for valid request, send reject */
	if (ntohs(msg->op) == DCM_REQ && !found) {
		dapl_log(DAPL_DBG_TYPE_WARN,
			" ucm_recv: NO LISTENER for %s %x %x i%x c%x"
			" < %x %x %x, sending reject\n", 
			dapl_cm_op_str(ntohs(msg->op)), 
			ntohs(msg->daddr.ib.lid), ntohs(msg->dport), 
			ntohl(msg->daddr.ib.qpn), ntohl(msg->sqpn),
			ntohs(msg->saddr.ib.lid), ntohs(msg->sport), 
			ntohl(msg->saddr.ib.qpn));

		ucm_reject(tp, msg);
	}

	if (!found) {
		dapl_log(DAPL_DBG_TYPE_CM_WARN,
			 " NO MATCH: op %s [lid, port, cqp, iqp, pid]:"
			 " %x %x %x %x %x <- %x %x %x %x l_pid %x r_pid %x\n",
			 dapl_cm_op_str(ntohs(msg->op)),
			 ntohs(msg->daddr.ib.lid), ntohs(msg->dport),
			 ntohl(msg->dqpn), ntohl(msg->daddr.ib.qpn),
			 ntohl(msg->d_id), ntohs(msg->saddr.ib.lid),
			 ntohs(msg->sport), ntohl(msg->sqpn),
			 ntohl(msg->saddr.ib.qpn), ntohl(msg->s_id),
			 ntohl(msg->d_id));

		if (ntohs(msg->op) == DCM_DREP) {
			DAPL_CNTR(((DAPL_IA *)dapl_llist_peek_head(&cm->hca->ia_list_head)), DCNT_IA_CM_ERR_DREP_DUP);
		}
	}

	return found;
}

/* Get rmsgs from CM completion queue, 10 at a time */
static void ucm_recv(ib_hca_transport_t *tp)
{
	struct ibv_wc wc[10];
	ib_cm_msg_t *msg;
	dp_ib_cm_handle_t cm;
	int i, ret, notify = 0;
	struct ibv_cq *ibv_cq = NULL;
	DAPL_HCA *hca;

	/* POLLIN on channel FD */
	ret = ibv_get_cq_event(tp->rch, &ibv_cq, (void *)&hca);
	if (ret == 0) {
		ibv_ack_cq_events(ibv_cq, 1);
	}
retry:	
	ret = ibv_poll_cq(tp->rcq, 10, wc);
	if (ret <= 0) {
		if (!ret && !notify) {
			ibv_req_notify_cq(tp->rcq, 0);
			notify = 1;
			goto retry;
		}
		return;
	} else 
		notify = 0;
	
	for (i = 0; i < ret; i++) {
		msg = (ib_cm_msg_t*) (uintptr_t) wc[i].wr_id;

		dapl_dbg_log(DAPL_DBG_TYPE_CM, 
			     " ucm_recv: stat=%d op=%s ln=%d id=%p sqp=%x\n",
			     wc[i].status, dapl_cm_op_str(ntohs(msg->op)),
			     wc[i].byte_len,
			     (void*)wc[i].wr_id, wc[i].src_qp);

		/* validate CM message, version */
		if (ntohs(msg->ver) < DCM_VER_MIN) {
			dapl_log(DAPL_DBG_TYPE_WARN,
				 " ucm_recv: UNKNOWN msg %p, ver %d\n", 
				 msg, msg->ver);
			ucm_post_rmsg(tp, msg);
			continue;
		}
		if (!(cm = ucm_cm_find(tp, msg))) {
			ucm_post_rmsg(tp, msg);
			continue;
		}
		
		/* match, process it */
		ucm_process_recv(tp, msg, cm);
		ucm_post_rmsg(tp, msg);
	}
	
	/* finished this batch of WC's, poll and rearm */
	goto retry;
}

/* ACTIVE/PASSIVE: build and send CM message out of CM object */
static int ucm_send(ib_hca_transport_t *tp, ib_cm_msg_t *msg, DAT_PVOID p_data, DAT_COUNT p_size)
{
	ib_cm_msg_t *smsg = NULL;
	struct ibv_send_wr wr, *bad_wr;
	struct ibv_sge sge;
	int len, ret = -1;
	uint16_t dlid = ntohs(msg->daddr.ib.lid);

	/* Get message from send queue, copy data, and send */
	dapl_os_lock(&tp->slock);
	if ((smsg = ucm_get_smsg(tp)) == NULL) {
		dapl_log(DAPL_DBG_TYPE_ERR,
			" ucm_send ERR: get_smsg(hd=%d,tl=%d) \n",
			tp->s_hd, tp->s_tl);
		goto bail;
	}

	len = (sizeof(*msg) - DCM_MAX_PDATA_SIZE);
	dapl_os_memcpy(smsg, msg, len);
	if (p_size) {
		smsg->p_size = ntohs(p_size);
		dapl_os_memcpy(&smsg->p_data, p_data, p_size);
	}

	wr.next = NULL;
        wr.sg_list = &sge;
        wr.num_sge = 1;
        wr.opcode = IBV_WR_SEND;
        wr.wr_id = (unsigned long)tp->s_hd;
	wr.send_flags = (wr.wr_id % tp->burst) ? 0 : IBV_SEND_SIGNALED;
	if (len <= tp->max_inline_send)
		wr.send_flags |= IBV_SEND_INLINE; 

        sge.length = len + p_size;
        sge.lkey = tp->mr_sbuf->lkey;
        sge.addr = (uintptr_t)smsg;

	dapl_dbg_log(DAPL_DBG_TYPE_CM, 
		" ucm_send: op %s ln %d lid %x c_qpn %x rport %x\n",
		dapl_cm_op_str(ntohs(smsg->op)), 
		sge.length, htons(smsg->daddr.ib.lid), 
		htonl(smsg->dqpn), htons(smsg->dport));

	/* empty slot, then create AH */
	if (!tp->ah[dlid]) {
		tp->ah[dlid] = 	
			dapls_create_ah(tp->hca, tp->pd, tp->qp, 
					htons(dlid), NULL);
		if (!tp->ah[dlid])
			goto bail;
	}
		
	wr.wr.ud.ah = tp->ah[dlid];
	wr.wr.ud.remote_qpn = ntohl(smsg->dqpn);
	wr.wr.ud.remote_qkey = DAT_UD_QKEY;

	ret = ibv_post_send(tp->qp, &wr, &bad_wr);
	if (ret) {
		dapl_log(DAPL_DBG_TYPE_ERR,
			 " ucm_send ERR: post_send() %s\n",
			 strerror(errno) );
	}

bail:
	dapl_os_unlock(&tp->slock);	
	return ret;
}

/* ACTIVE/PASSIVE: CM objects */
static void dapli_cm_dealloc(dp_ib_cm_handle_t cm) {

	dapl_os_assert(!cm->ref_count);
	dapl_os_lock_destroy(&cm->lock);
	dapl_os_wait_object_destroy(&cm->d_event);
	dapl_os_wait_object_destroy(&cm->f_event);
	dapl_os_free(cm, sizeof(*cm));
}

void dapls_cm_acquire(dp_ib_cm_handle_t cm)
{
	dapl_os_lock(&cm->lock);
	cm->ref_count++;
	dapl_os_unlock(&cm->lock);
}

void dapls_cm_release(dp_ib_cm_handle_t cm)
{
	dapl_os_lock(&cm->lock);
	cm->ref_count--;
	if (cm->ref_count) {
		if (cm->ref_count == 1)
			dapl_os_wait_object_wakeup(&cm->f_event);
                dapl_os_unlock(&cm->lock);
		return;
	}
	/* client, release local conn id port */
	if (!cm->sp && cm->msg.sport)
		ucm_free_port(&cm->hca->ib_trans, ntohs(cm->msg.sport));

	/* clean up any UD address handles */
	if (cm->ah) {
		ibv_destroy_ah(cm->ah);
		cm->ah = NULL;
	}
	dapl_os_unlock(&cm->lock);
	dapli_cm_dealloc(cm);
}

dp_ib_cm_handle_t dapls_ib_cm_create(DAPL_EP *ep)
{
	dp_ib_cm_handle_t cm;

	/* Allocate CM, init lock, and initialize */
	if ((cm = dapl_os_alloc(sizeof(*cm))) == NULL)
		return NULL;

	(void)dapl_os_memzero(cm, sizeof(*cm));
	if (dapl_os_lock_init(&cm->lock))
		goto bail;
	
	if (dapl_os_wait_object_init(&cm->f_event)) {
		dapl_os_lock_destroy(&cm->lock);
		goto bail;
	}
	if (dapl_os_wait_object_init(&cm->d_event)) {
		dapl_os_lock_destroy(&cm->lock);
		dapl_os_wait_object_destroy(&cm->f_event);
		goto bail;
	}
	dapls_cm_acquire(cm);

	cm->msg.ver = htons(DCM_VER);
	cm->msg.s_id = htonl(dapl_os_getpid()); /* process id for src id */
	
	/* ACTIVE: init source address QP info from local EP */
	if (ep) {
		DAPL_HCA *hca = ep->header.owner_ia->hca_ptr;

		cm->msg.sport = htons(ucm_get_port(&hca->ib_trans, 0));
		if (!cm->msg.sport) {
			dapl_os_wait_object_destroy(&cm->f_event);
			dapl_os_wait_object_destroy(&cm->d_event);
			dapl_os_lock_destroy(&cm->lock);
			goto bail;
		}
		/* link CM object to EP */
		dapl_ep_link_cm(ep, cm);
		cm->hca = hca;
		cm->ep = ep;

		/* IB info in network order */
		cm->msg.sqpn = htonl(hca->ib_trans.qp->qp_num); /* ucm */
		cm->msg.saddr.ib.qpn = htonl(ep->qp_handle->qp->qp_num); /* ep */
		cm->msg.saddr.ib.qp_type = ep->qp_handle->qp->qp_type;
                cm->msg.saddr.ib.lid = hca->ib_trans.addr.ib.lid; 
		dapl_os_memcpy(&cm->msg.saddr.ib.gid[0], 
			       &hca->ib_trans.addr.ib.gid, 16);
        }
	return cm;
bail:
	dapl_os_free(cm, sizeof(*cm));
	return NULL;
}

/* schedule destruction of CM object, clean UD CR */
void dapli_cm_free(dp_ib_cm_handle_t cm)
{
	DAPL_SP *sp_ptr = cm->sp;

	dapl_log(DAPL_DBG_TYPE_CM,
		 " dapli_cm_free: cm %p %s ep %p sp %p cr_cnt %d refs=%d\n",
		 cm, dapl_cm_state_str(cm->state),
		 cm->ep, sp_ptr, sp_ptr ? sp_ptr->cr_list_count:0, cm->ref_count);

	dapl_os_lock(&cm->lock);
	if (sp_ptr && cm->state == DCM_CONNECTED &&
	    cm->msg.daddr.ib.qp_type == IBV_QPT_UD) {
		dapl_os_lock(&sp_ptr->header.lock);
		cm->cr = dapl_sp_search_cr(sp_ptr, cm);
		dapl_log(DAPL_DBG_TYPE_CM, " dapli_cm_free: UD CR %p\n", cm->cr);

		if (cm->cr != NULL) {
			dapl_sp_remove_cr(sp_ptr, cm->cr);
			/* free CR at EP destroy */
		}
		dapl_os_unlock(&sp_ptr->header.lock);
	}
	cm->state = DCM_FREE;
	dapls_thread_signal(&cm->hca->ib_trans.signal);
	dapl_os_unlock(&cm->lock);
}

/* Blocking, ONLY called from dat_ep_free */
void dapls_cm_free(dp_ib_cm_handle_t cm)
{
	dapl_log(DAPL_DBG_TYPE_CM,
		 " dapl_cm_free: cm %p %s ep %p refs=%d\n", 
		 cm, dapl_cm_state_str(cm->state),
		 cm->ep, cm->ref_count);
	
	/* free from internal workq, wait until EP is last ref */
	dapl_os_lock(&cm->lock);
	if (cm->state != DCM_FREE) 
		cm->state = DCM_FREE;

	if (cm->cr)
		dapls_cr_free(cm->cr);

	if (cm->ref_count != 1) {
		dapls_thread_signal(&cm->hca->ib_trans.signal);
		dapl_os_unlock(&cm->lock);
		dapl_os_wait_object_wait(&cm->f_event, DAT_TIMEOUT_INFINITE);
		dapl_os_lock(&cm->lock);
	}
	dapl_os_unlock(&cm->lock);

	/* unlink, dequeue from EP. Final ref so release will destroy */
	dapl_ep_unlink_cm(cm->ep, cm);
}

/* ACTIVE/PASSIVE: queue up connection object on CM list */
static void dapli_queue_conn(dp_ib_cm_handle_t cm)
{
	/* add to work queue, list, for cm thread processing */
	dapl_llist_init_entry((DAPL_LLIST_ENTRY *)&cm->local_entry);
	dapl_os_lock(&cm->hca->ib_trans.lock);
	dapls_cm_acquire(cm);
	dapl_llist_add_tail(&cm->hca->ib_trans.list,
			    (DAPL_LLIST_ENTRY *)&cm->local_entry, cm);
	dapl_os_unlock(&cm->hca->ib_trans.lock);
	dapls_thread_signal(&cm->hca->ib_trans.signal);
}

/* PASSIVE: queue up listen object on listen list */
static void dapli_queue_listen(dp_ib_cm_handle_t cm)
{
	/* add to work queue, llist, for cm thread processing */
	dapl_llist_init_entry((DAPL_LLIST_ENTRY *)&cm->local_entry);
	dapl_os_lock(&cm->hca->ib_trans.llock);
	dapls_cm_acquire(cm);
	dapl_llist_add_tail(&cm->hca->ib_trans.llist,
			    (DAPL_LLIST_ENTRY *)&cm->local_entry, cm);
	dapl_os_unlock(&cm->hca->ib_trans.llock);
}

static void dapli_dequeue_listen(dp_ib_cm_handle_t cm) 
{
	DAPL_HCA *hca = cm->hca;

	dapl_os_lock(&hca->ib_trans.llock);
	dapl_llist_remove_entry(&hca->ib_trans.llist, 
				(DAPL_LLIST_ENTRY *)&cm->local_entry);
	dapls_cm_release(cm);
	dapl_os_unlock(&hca->ib_trans.llock);
}

/* called with local LIST and CM object lock */
static void dapli_cm_dequeue(dp_ib_cm_handle_t cm)
{
	/* Remove from work queue, cr thread processing */
	dapl_llist_remove_entry(&cm->hca->ib_trans.list,
				(DAPL_LLIST_ENTRY *)&cm->local_entry);
	dapls_cm_release(cm);
}

static void ucm_disconnect_final(dp_ib_cm_handle_t cm) 
{
	/* no EP attachment or not RC, nothing to process */
	if (cm->ep == NULL ||
	    cm->ep->param.ep_attr.service_type != DAT_SERVICE_TYPE_RC) 
		return;

	dapl_os_lock(&cm->lock);
	if ((cm->state == DCM_DISCONNECTED) || (cm->state == DCM_FREE)) {
		dapl_os_unlock(&cm->lock);
		return;
	}
		
	cm->state = DCM_DISCONNECTED;
	dapl_os_unlock(&cm->lock);

	if (cm->sp) 
		dapls_cr_callback(cm, IB_CME_DISCONNECTED, NULL, 0, cm->sp);
	else
		dapl_evd_connection_callback(cm, IB_CME_DISCONNECTED, NULL, 0, cm->ep);

	dapl_os_wait_object_wakeup(&cm->d_event);

}

/*
 * called from consumer thread via ep_disconnect/ep_free or 
 * from cm_thread when receiving DREQ
 */
DAT_RETURN dapli_cm_disconnect(dp_ib_cm_handle_t cm)
{
	int finalize = 1;
	int wakeup = 0;

	dapl_os_lock(&cm->lock);
	switch (cm->state) {
	case DCM_CONNECTED:
		/* CONSUMER: move to err state to flush, if not UD */
		if (cm->ep->qp_handle->qp->qp_type != IBV_QPT_UD)
			dapls_modify_qp_state(cm->ep->qp_handle->qp, IBV_QPS_ERR,0,0,0);

		/* send DREQ, event after DREP or DREQ timeout */
		cm->state = DCM_DISC_PENDING;
		cm->msg.op = htons(DCM_DREQ);
		finalize = 0; /* wait for DREP, wakeup timer after DREQ sent */
		wakeup = 1;
		DAPL_CNTR(((DAPL_IA *)dapl_llist_peek_head(&cm->hca->ia_list_head)), DCNT_IA_CM_DREQ_TX);
		break;
	case DCM_DISC_PENDING:
		/* DREQ timeout, resend until retries exhausted */
		cm->msg.op = htons(DCM_DREQ);
		if (cm->retries >= cm->hca->ib_trans.retries) {
			dapl_log(DAPL_DBG_TYPE_ERR, 
				" CM_DREQ: RETRIES EXHAUSTED:"
				" %x %x %x -> %x %x %x\n",
				htons(cm->msg.saddr.ib.lid), 
				htonl(cm->msg.saddr.ib.qpn), 
				htons(cm->msg.sport), 
				htons(cm->msg.daddr.ib.lid), 
				htonl(cm->msg.dqpn), 
				htons(cm->msg.dport));
			finalize = 1;
		}
		DAPL_CNTR(((DAPL_IA *)dapl_llist_peek_head(&cm->hca->ia_list_head)), DCNT_IA_CM_ERR_DREQ_RETRY);
		break;
	case DCM_DISC_RECV:
		/* CM_THREAD: move to err state to flush, if not UD */
		if (cm->ep->qp_handle->qp->qp_type != IBV_QPT_UD)
			dapls_modify_qp_state(cm->ep->qp_handle->qp, IBV_QPS_ERR,0,0,0);

		/* DREQ received, send DREP and schedule event, finalize */
		cm->msg.op = htons(DCM_DREP);
		DAPL_CNTR(((DAPL_IA *)dapl_llist_peek_head(&cm->hca->ia_list_head)), DCNT_IA_CM_DREP_TX);
		break;
	case DCM_DISCONNECTED:
		dapl_os_unlock(&cm->lock);
		return DAT_SUCCESS;
	default:
		dapl_log(DAPL_DBG_TYPE_EP, 
			"  disconnect UNKNOWN state: ep %p cm %p %s %s"
			"  %x %x %x %s %x %x %x r_id %x l_id %x\n",
			cm->ep, cm,
			cm->msg.saddr.ib.qp_type == IBV_QPT_RC ? "RC" : "UD",
			dapl_cm_state_str(cm->state),
			ntohs(cm->msg.saddr.ib.lid),
			ntohs(cm->msg.sport),
			ntohl(cm->msg.saddr.ib.qpn),	
			cm->sp ? "<-" : "->",
			ntohs(cm->msg.daddr.ib.lid),
			ntohs(cm->msg.dport),
			ntohl(cm->msg.daddr.ib.qpn),
			ntohl(cm->msg.d_id),
			ntohl(cm->msg.s_id));

		dapl_os_unlock(&cm->lock);
		return DAT_SUCCESS;
	}
	
	dapl_os_get_time(&cm->timer); /* reply expected */
	ucm_send(&cm->hca->ib_trans, &cm->msg, NULL, 0); 
	dapl_os_unlock(&cm->lock);
	
	if (wakeup)
		dapls_thread_signal(&cm->hca->ib_trans.signal);

	if (finalize) 
		ucm_disconnect_final(cm);
	
	return DAT_SUCCESS;
}

/*
 * ACTIVE: get remote CM SID server info from r_addr. 
 *         send, or resend CM msg via UD CM QP 
 */
DAT_RETURN
dapli_cm_connect(DAPL_EP *ep, dp_ib_cm_handle_t cm)
{
	dapl_log(DAPL_DBG_TYPE_EP, 
		 " connect: lid %x i_qpn %x lport %x p_sz=%d -> "
		 " lid %x c_qpn %x rport %x\n",
		 htons(cm->msg.saddr.ib.lid), htonl(cm->msg.saddr.ib.qpn),
		 htons(cm->msg.sport), htons(cm->msg.p_size),
		 htons(cm->msg.daddr.ib.lid), htonl(cm->msg.dqpn),
		 htons(cm->msg.dport));

	dapl_os_lock(&cm->lock);
	if (cm->state != DCM_INIT && cm->state != DCM_REP_PENDING) {
		dapl_os_unlock(&cm->lock);
		return DAT_INVALID_STATE;
	}
	
	if (cm->retries == cm->hca->ib_trans.retries) {
		dapl_log(DAPL_DBG_TYPE_ERR, 
			" CM_REQ: RETRIES EXHAUSTED:"
			 " 0x%x %x 0x%x -> 0x%x %x 0x%x\n",
			 htons(cm->msg.saddr.ib.lid), 
			 htonl(cm->msg.saddr.ib.qpn), 
			 htons(cm->msg.sport), 
			 htons(cm->msg.daddr.ib.lid), 
			 htonl(cm->msg.dqpn), 
			 htons(cm->msg.dport));

		dapl_os_unlock(&cm->lock);

#ifdef DAPL_COUNTERS
		/* called from check_timers in cm_thread, cm lock held */
		if (g_dapl_dbg_type & DAPL_DBG_TYPE_CM_LIST) {
			dapl_os_unlock(&cm->hca->ib_trans.lock);
			dapls_print_cm_list(ep->header.owner_ia);
			dapl_os_lock(&cm->hca->ib_trans.lock);
		}
#endif
		dapl_evd_connection_callback(cm, 
					     IB_CME_DESTINATION_UNREACHABLE,
					     NULL, 0, ep);
		
		return DAT_ERROR(DAT_INVALID_ADDRESS, 
				 DAT_INVALID_ADDRESS_UNREACHABLE);
	}

	cm->state = DCM_REP_PENDING;
	cm->msg.op = htons(DCM_REQ);
	dapl_os_get_time(&cm->timer); /* reset reply timer */
	if (ucm_send(&cm->hca->ib_trans, &cm->msg, 
		     &cm->msg.p_data, ntohs(cm->msg.p_size))) {
		dapl_os_unlock(&cm->lock);
		goto bail;
	}
	dapl_os_unlock(&cm->lock);
	DAPL_CNTR(((DAPL_IA *)dapl_llist_peek_head(&cm->hca->ia_list_head)),
		  ep->qp_handle->qp->qp_type == IBV_QPT_UD ? DCNT_IA_CM_AH_REQ_TX : DCNT_IA_CM_REQ_TX);

	return DAT_SUCCESS;

bail:
	DAPL_CNTR(((DAPL_IA *)dapl_llist_peek_head(&cm->hca->ia_list_head)), DCNT_IA_CM_ERR);
	dapl_log(DAPL_DBG_TYPE_WARN, 
		 " connect: snd ERR -> cm_lid %x cm_qpn %x r_psp %x p_sz=%d\n",
		 htons(cm->msg.daddr.ib.lid),
		 htonl(cm->msg.dqpn), htons(cm->msg.dport), 
		 htons(cm->msg.p_size));

	dapli_cm_free(cm);
	return DAT_INSUFFICIENT_RESOURCES;
}

/*
 * ACTIVE: exchange QP information, called from CR thread
 */
static void ucm_connect_rtu(dp_ib_cm_handle_t cm, ib_cm_msg_t *msg)
{
	DAPL_EP *ep = cm->ep;
	ib_cm_events_t event = IB_CME_CONNECTED;

	dapl_os_lock(&cm->lock);
	if (cm->state != DCM_REP_PENDING) {
		dapl_log(DAPL_DBG_TYPE_WARN, 
			 " CONN_RTU: UNEXPECTED state:"
			 " op %s, st %s <- lid %x sqpn %x sport %x\n", 
			 dapl_cm_op_str(ntohs(msg->op)), 
			 dapl_cm_state_str(cm->state), 
			 ntohs(msg->saddr.ib.lid), ntohl(msg->saddr.ib.qpn), 
			 ntohs(msg->sport));
		dapl_os_unlock(&cm->lock);
		return;
	}

	/* save remote address information to EP and CM */
	cm->msg.d_id = msg->s_id;
	dapl_os_memcpy(&ep->remote_ia_address,
		       &msg->saddr, sizeof(union dcm_addr));
	dapl_os_memcpy(&cm->msg.daddr, 
		       &msg->saddr, sizeof(union dcm_addr));

	/* validate private data size, and copy if necessary */
	if (msg->p_size) {
		if (ntohs(msg->p_size) > DCM_MAX_PDATA_SIZE) {
			dapl_log(DAPL_DBG_TYPE_WARN, 
				 " CONN_RTU: invalid p_size %d:"
				 " st %s <- lid %x sqpn %x spsp %x\n", 
				 ntohs(msg->p_size), 
				 dapl_cm_state_str(cm->state), 
				 ntohs(msg->saddr.ib.lid), 
				 ntohl(msg->saddr.ib.qpn), 
				 ntohs(msg->sport));
			dapl_os_unlock(&cm->lock);
			goto bail;
		}
		dapl_os_memcpy(cm->msg.p_data, 
			       msg->p_data, ntohs(msg->p_size));
	}
		
	dapl_dbg_log(DAPL_DBG_TYPE_CM,
		     " CONN_RTU: DST lid=%x,"
		     " iqp=%x, qp_type=%d, port=%x psize=%d\n",
		     ntohs(cm->msg.daddr.ib.lid),
		     ntohl(cm->msg.daddr.ib.qpn), cm->msg.daddr.ib.qp_type,
		     ntohs(msg->sport), ntohs(msg->p_size));

	if (ntohs(msg->op) == DCM_REP)
		event = IB_CME_CONNECTED;
	else if (ntohs(msg->op) == DCM_REJ_USER) 
		event = IB_CME_DESTINATION_REJECT_PRIVATE_DATA;
	else  {
		dapl_log(DAPL_DBG_TYPE_WARN,
			 " Warning, non-user CR REJECT:"
			 " cm %p op %s, st %s dlid %x iqp %x port %x <-"
			 " slid %x iqp %x port %x\n", cm,
			 dapl_cm_op_str(ntohs(msg->op)),
			 dapl_cm_state_str(cm->state),
			 ntohs(msg->daddr.ib.lid), ntohl(msg->daddr.ib.qpn),
			 ntohs(msg->dport), ntohs(msg->saddr.ib.lid),
			 ntohl(msg->saddr.ib.qpn), ntohs(msg->sport));
		DAPL_CNTR(((DAPL_IA *)dapl_llist_peek_head(&cm->hca->ia_list_head)), DCNT_IA_CM_ERR_REJ_RX);
		event = IB_CME_DESTINATION_REJECT;
	}
	if (event != IB_CME_CONNECTED) {
		dapl_log(DAPL_DBG_TYPE_CM, 
			 " ACTIVE: CM_REQ REJECTED:"
			 " cm %p op %s, st %s dlid %x iqp %x port %x <-"
			 " slid %x iqp %x port %x\n", cm,
			 dapl_cm_op_str(ntohs(msg->op)), 
			 dapl_cm_state_str(cm->state), 
			 ntohs(msg->daddr.ib.lid), ntohl(msg->daddr.ib.qpn), 
			 ntohs(msg->dport), ntohs(msg->saddr.ib.lid), 
			 ntohl(msg->saddr.ib.qpn), ntohs(msg->sport));

		cm->state = DCM_REJECTED;
		dapl_os_unlock(&cm->lock);

#ifdef DAT_EXTENSIONS
		if (cm->msg.daddr.ib.qp_type == IBV_QPT_UD) 
			goto ud_bail;
		else
#endif
		goto bail;
	}
	dapl_os_unlock(&cm->lock);

        /* rdma_out, initiator, cannot exceed remote rdma_in max */
	if (ntohs(cm->msg.ver) >= 7)
		cm->ep->param.ep_attr.max_rdma_read_out =
				DAPL_MIN(cm->ep->param.ep_attr.max_rdma_read_out,
					 cm->msg.rd_in);

	/* modify QP to RTR and then to RTS with remote info */
	dapl_os_lock(&cm->ep->header.lock);
	if (dapls_modify_qp_state(cm->ep->qp_handle->qp,
				  IBV_QPS_RTR, 
				  cm->msg.daddr.ib.qpn,
				  cm->msg.daddr.ib.lid,
				  (ib_gid_handle_t)cm->msg.daddr.ib.gid) != DAT_SUCCESS) {
		dapl_log(DAPL_DBG_TYPE_ERR,
			 " CONN_RTU: QPS_RTR ERR %s <- lid %x iqp %x\n",
			 strerror(errno), ntohs(cm->msg.daddr.ib.lid),
			 ntohl(cm->msg.daddr.ib.qpn));
		dapl_os_unlock(&cm->ep->header.lock);
		event = IB_CME_LOCAL_FAILURE;
		goto bail;
	}
	if (dapls_modify_qp_state(cm->ep->qp_handle->qp,
				  IBV_QPS_RTS, 
				  cm->msg.daddr.ib.qpn,
				  cm->msg.daddr.ib.lid,
				  NULL) != DAT_SUCCESS) {
		dapl_log(DAPL_DBG_TYPE_ERR,
			 " CONN_RTU: QPS_RTS ERR %s <- lid %x iqp %x\n",
			 strerror(errno), ntohs(cm->msg.daddr.ib.lid),
			 ntohl(cm->msg.daddr.ib.qpn));
		dapl_os_unlock(&cm->ep->header.lock);
		event = IB_CME_LOCAL_FAILURE;
		goto bail;
	}
	dapl_os_unlock(&cm->ep->header.lock);
	
	/* Send RTU, no private data */
	cm->msg.op = htons(DCM_RTU);
	
	dapl_os_lock(&cm->lock);
	cm->state = DCM_CONNECTED;
	if (ucm_send(&cm->hca->ib_trans, &cm->msg, NULL, 0)) {
		dapl_os_unlock(&cm->lock);
		goto bail;
	}
	dapl_os_unlock(&cm->lock);
	DAPL_CNTR(((DAPL_IA *)dapl_llist_peek_head(&cm->hca->ia_list_head)), DCNT_IA_CM_RTU_TX);

	/* init cm_handle and post the event with private data */
	dapl_dbg_log(DAPL_DBG_TYPE_EP, " ACTIVE: connected!\n");

#ifdef DAT_EXTENSIONS
ud_bail:
	if (cm->msg.daddr.ib.qp_type == IBV_QPT_UD) {
		DAT_IB_EXTENSION_EVENT_DATA xevent;
		uint16_t lid = ntohs(cm->msg.daddr.ib.lid);
		
		/* post EVENT, modify_qp, AH already created, ucm msg */
		xevent.status = 0;
		xevent.type = DAT_IB_UD_REMOTE_AH;
		xevent.remote_ah.qpn = ntohl(cm->msg.daddr.ib.qpn);
		xevent.remote_ah.ah = dapls_create_ah(cm->hca, 
						      cm->ep->qp_handle->qp->pd,
						      cm->ep->qp_handle->qp,
						      htons(lid), 
						      NULL);
		if (xevent.remote_ah.ah == NULL) {
			dapl_log(DAPL_DBG_TYPE_ERR,
				 " active UD RTU: ERR create_ah"
				 " for qpn 0x%x lid 0x%x\n",
				 xevent.remote_ah.qpn, lid);
			event = IB_CME_LOCAL_FAILURE;
			goto bail;
		}
		cm->ah = xevent.remote_ah.ah; /* keep ref to destroy */

		dapl_os_memcpy(&xevent.remote_ah.ia_addr,
			       &cm->msg.daddr,
			       sizeof(union dcm_addr));

		/* remote ia_addr reference includes ucm qpn, not IB qpn */
		((union dcm_addr*)
			&xevent.remote_ah.ia_addr)->ib.qpn = cm->msg.dqpn;

		dapl_dbg_log(DAPL_DBG_TYPE_EP,
			     " ACTIVE: UD xevent ah %p qpn %x lid %x\n",
			     xevent.remote_ah.ah, xevent.remote_ah.qpn, lid);
		dapl_dbg_log(DAPL_DBG_TYPE_EP,
		     	     " ACTIVE: UD xevent ia_addr qp_type %d"
			     " lid 0x%x qpn 0x%x gid 0x"F64x" 0x"F64x" \n",
			     ((union dcm_addr*)
				&xevent.remote_ah.ia_addr)->ib.qp_type,
			     ntohs(((union dcm_addr*)
				&xevent.remote_ah.ia_addr)->ib.lid),
			     ntohl(((union dcm_addr*)
				&xevent.remote_ah.ia_addr)->ib.qpn),
			     ntohll(*(uint64_t*)&cm->msg.daddr.ib.gid[0]),
			     ntohll(*(uint64_t*)&cm->msg.daddr.ib.gid[8]));

		if (event == IB_CME_CONNECTED)
			event = DAT_IB_UD_CONNECTION_EVENT_ESTABLISHED;
		else {
			xevent.type = DAT_IB_UD_CONNECT_REJECT;
			event = DAT_IB_UD_CONNECTION_REJECT_EVENT;
		}

		dapls_evd_post_connection_event_ext(
				(DAPL_EVD *)cm->ep->param.connect_evd_handle,
				event,
				(DAT_EP_HANDLE)ep,
				(DAT_COUNT)ntohs(cm->msg.p_size),
				(DAT_PVOID *)cm->msg.p_data,
				(DAT_PVOID *)&xevent);

		if (event != (ib_cm_events_t)DAT_IB_UD_CONNECTION_EVENT_ESTABLISHED)
			dapli_cm_free(cm);

		DAPL_CNTR(((DAPL_IA *)dapl_llist_peek_head(&cm->hca->ia_list_head)), DCNT_IA_CM_AH_RESOLVED);

	} else
#endif
	{
		DAPL_CNTR(((DAPL_IA *)dapl_llist_peek_head(&cm->hca->ia_list_head)), DCNT_IA_CM_ACTIVE_EST);
		dapl_evd_connection_callback(cm,
					     IB_CME_CONNECTED,
					     cm->msg.p_data, ntohs(cm->msg.p_size), cm->ep);
	}
	dapl_log(DAPL_DBG_TYPE_CM_EST,
		 " UCM_ACTIVE_CONN %p %d [lid port qpn] %x %x %x -> %x %x %x\n",
		 cm->hca, cm->retries, ntohs(cm->msg.saddr.ib.lid),
		 ntohs(cm->msg.sport), ntohl(cm->msg.saddr.ib.qpn),
		 ntohs(cm->msg.daddr.ib.lid), ntohs(cm->msg.dport),
		 ntohl(cm->msg.dqpn));
	return;
bail:
	dapl_evd_connection_callback(NULL, event, cm->msg.p_data, ntohs(cm->msg.p_size), cm->ep);
	dapli_cm_free(cm);
}

/*
 * PASSIVE: Accept on listen CM PSP.
 *          create new CM object for this CR, 
 *	    receive peer QP information, private data, 
 *	    and post cr_event 
 */
static void ucm_accept(ib_cm_srvc_handle_t cm, ib_cm_msg_t *msg)
{
	dp_ib_cm_handle_t acm;

	/* Allocate accept CM and setup passive references */
	if ((acm = dapls_ib_cm_create(NULL)) == NULL) {
		dapl_log(DAPL_DBG_TYPE_WARN, " accept: ERR cm_create\n");
		return;
	}

	/* dest CM info from CR msg, source CM info from listen */
	acm->sp = cm->sp;
	acm->hca = cm->hca;
	acm->msg.op = msg->op;
	acm->msg.dport = msg->sport;
	acm->msg.dqpn = msg->sqpn;
	acm->msg.sport = cm->msg.sport; 
	acm->msg.sqpn = cm->msg.sqpn;
	acm->msg.p_size = msg->p_size;
	acm->msg.d_id = msg->s_id;
	acm->msg.rd_in = msg->rd_in;

	/* CR saddr is CM daddr info, need EP for local saddr */
	dapl_os_memcpy(&acm->msg.daddr, &msg->saddr, sizeof(union dcm_addr));
	
	dapl_log(DAPL_DBG_TYPE_CM,
		 " accept: DST port=%x lid=%x, iqp=%x, psize=%d\n",
		 ntohs(acm->msg.dport), ntohs(acm->msg.daddr.ib.lid), 
		 htonl(acm->msg.daddr.ib.qpn), htons(acm->msg.p_size));

	/* validate private data size before reading */
	if (ntohs(msg->p_size) > DCM_MAX_PDATA_SIZE) {
		dapl_log(DAPL_DBG_TYPE_WARN, " accept: psize (%d) wrong\n",
			 ntohs(msg->p_size));
		goto bail;
	}

	/* read private data into cm_handle if any present */
	if (msg->p_size) 
		dapl_os_memcpy(acm->msg.p_data, 
			       msg->p_data, ntohs(msg->p_size));
		
	acm->state = DCM_ACCEPTING;
	dapli_queue_conn(acm);

#ifdef DAT_EXTENSIONS
	if (acm->msg.daddr.ib.qp_type == IBV_QPT_UD) {
		DAT_IB_EXTENSION_EVENT_DATA xevent;

		/* post EVENT, modify_qp created ah */
		xevent.status = 0;
		xevent.type = DAT_IB_UD_CONNECT_REQUEST;

		dapls_evd_post_cr_event_ext(acm->sp,
					    DAT_IB_UD_CONNECTION_REQUEST_EVENT,
					    acm,
					    (DAT_COUNT)ntohs(acm->msg.p_size),
					    (DAT_PVOID *)acm->msg.p_data,
					    (DAT_PVOID *)&xevent);
		DAPL_CNTR(((DAPL_IA *)dapl_llist_peek_head(&cm->hca->ia_list_head)), DCNT_IA_CM_AH_REQ_TX);
	} else
#endif
		/* trigger CR event and return SUCCESS */
		dapls_cr_callback(acm,
				  IB_CME_CONNECTION_REQUEST_PENDING,
				  acm->msg.p_data, ntohs(msg->p_size), acm->sp);
	return;

bail:
	/* schedule work thread cleanup */
	DAPL_CNTR(((DAPL_IA *)dapl_llist_peek_head(&cm->hca->ia_list_head)), DCNT_IA_CM_ERR);
	dapli_cm_free(acm);
	return;
}

/*
 * PASSIVE: read RTU from active peer, post CONN event
 */
static void ucm_accept_rtu(dp_ib_cm_handle_t cm, ib_cm_msg_t *msg)
{
	dapl_os_lock(&cm->lock);
	if ((ntohs(msg->op) != DCM_RTU) || (cm->state != DCM_RTU_PENDING)) {
		dapl_log(DAPL_DBG_TYPE_WARN, 
			 " accept_rtu: UNEXPECTED op, state:"
			 " op %s, st %s <- lid %x iqp %x sport %x\n", 
			 dapl_cm_op_str(ntohs(msg->op)), 
			 dapl_cm_state_str(cm->state), 
			 ntohs(msg->saddr.ib.lid), ntohl(msg->saddr.ib.qpn), 
			 ntohs(msg->sport));
		dapl_os_unlock(&cm->lock);
		goto bail;
	}
	cm->state = DCM_CONNECTED;
	dapl_os_unlock(&cm->lock);
	
	/* final data exchange if remote QP state is good to go */
	dapl_dbg_log(DAPL_DBG_TYPE_CM, " PASSIVE: connected!\n");

#ifdef DAT_EXTENSIONS
	if (cm->msg.saddr.ib.qp_type == IBV_QPT_UD) {
		DAT_IB_EXTENSION_EVENT_DATA xevent;
		uint16_t lid = ntohs(cm->msg.daddr.ib.lid);
		
		/* post EVENT, modify_qp, AH already created, ucm msg */
		xevent.status = 0;
		xevent.type = DAT_IB_UD_PASSIVE_REMOTE_AH;
		xevent.remote_ah.qpn = ntohl(cm->msg.daddr.ib.qpn);
		xevent.remote_ah.ah = dapls_create_ah(cm->hca, 
						      cm->ep->qp_handle->qp->pd,
						      cm->ep->qp_handle->qp,
						      htons(lid), 
						      NULL);
		if (xevent.remote_ah.ah == NULL) {
			dapl_log(DAPL_DBG_TYPE_ERR,
				 " passive UD RTU: ERR create_ah"
				 " for qpn 0x%x lid 0x%x\n",
				 xevent.remote_ah.qpn, lid);
			goto bail;
		}
		cm->ah = xevent.remote_ah.ah; /* keep ref to destroy */
		dapl_os_memcpy(&xevent.remote_ah.ia_addr,
			       &cm->msg.daddr,
			        sizeof(union dcm_addr));

		/* remote ia_addr reference includes ucm qpn, not IB qpn */
		((union dcm_addr*)&xevent.remote_ah.ia_addr)->ib.qpn = cm->msg.dqpn;

		dapl_dbg_log(DAPL_DBG_TYPE_EP,
			     " PASSIVE: UD xevent ah %p qpn %x lid %x\n",
			     xevent.remote_ah.ah, xevent.remote_ah.qpn, lid);
		dapl_dbg_log(DAPL_DBG_TYPE_EP,
		     	     " PASSIVE: UD xevent ia_addr qp_type %d"
			     " lid 0x%x qpn 0x%x gid 0x"F64x" 0x"F64x" \n",
			     ((union dcm_addr*)
				&xevent.remote_ah.ia_addr)->ib.qp_type,
			     ntohs(((union dcm_addr*)
				&xevent.remote_ah.ia_addr)->ib.lid),
			     ntohl(((union dcm_addr*)
				&xevent.remote_ah.ia_addr)->ib.qpn),
			     ntohll(*(uint64_t*)&cm->msg.daddr.ib.gid[0]),
			     ntohll(*(uint64_t*)&cm->msg.daddr.ib.gid[8]));

		dapls_evd_post_connection_event_ext(
				(DAPL_EVD *)cm->ep->param.connect_evd_handle,
				DAT_IB_UD_CONNECTION_EVENT_ESTABLISHED,
				(DAT_EP_HANDLE)cm->ep,
				(DAT_COUNT)ntohs(cm->msg.p_size),
				(DAT_PVOID *)cm->msg.p_data,
				(DAT_PVOID *)&xevent);

		DAPL_CNTR(((DAPL_IA *)dapl_llist_peek_head(&cm->hca->ia_list_head)), DCNT_IA_CM_AH_RESOLVED);
		dapli_cm_free(cm); /* still attached to EP */
	} else {
#endif
		DAPL_CNTR(((DAPL_IA *)dapl_llist_peek_head(&cm->hca->ia_list_head)), DCNT_IA_CM_PASSIVE_EST);
		dapls_cr_callback(cm, IB_CME_CONNECTED, NULL, 0, cm->sp);
	}
	dapl_log(DAPL_DBG_TYPE_CM_EST,
		 " UCM_PASSIVE_CONN %p %d [lid port qpn] %x %x %x <- %x %x %x\n",
		 cm->hca, cm->retries, ntohs(cm->msg.saddr.ib.lid),
		 ntohs(cm->msg.sport), ntohl(cm->msg.saddr.ib.qpn),
		 ntohs(cm->msg.daddr.ib.lid), ntohs(cm->msg.dport),
		 ntohl(cm->msg.dqpn));
	return;
bail:
	DAPL_CNTR(((DAPL_IA *)dapl_llist_peek_head(&cm->hca->ia_list_head)), DCNT_IA_CM_ERR);
	dapls_cr_callback(cm, IB_CME_LOCAL_FAILURE, NULL, 0, cm->sp);
	dapli_cm_free(cm);
}

/*
 * PASSIVE: user accepted, check and re-send reply message, called from cm_thread.
 */
static int ucm_reply(dp_ib_cm_handle_t cm)
{
	dapl_os_lock(&cm->lock);
	if (cm->state != DCM_RTU_PENDING) {
		dapl_log(DAPL_DBG_TYPE_ERR, 
			 " CM_REPLY: wrong state ep %p cm %p %s refs=%d"
			 " %x %x i_%x -> %x %x i_%x l_pid %x r_pid %x\n",
			 cm->ep, cm, dapl_cm_state_str(cm->state),
			 cm->ref_count,
			 htons(cm->msg.saddr.ib.lid),
			 htons(cm->msg.sport),
			 htonl(cm->msg.saddr.ib.qpn),
			 htons(cm->msg.daddr.ib.lid),
			 htons(cm->msg.dport),
			 htonl(cm->msg.daddr.ib.qpn),
			 ntohl(cm->msg.s_id),
			 ntohl(cm->msg.d_id));
		dapl_os_unlock(&cm->lock);
		return -1;
	}

	if (cm->retries == cm->hca->ib_trans.retries) {
		dapl_log(DAPL_DBG_TYPE_ERR, 
			" CM_REPLY: RETRIES EXHAUSTED (lid port qpn)"
			 " %x %x %x -> %x %x %x\n",
			 htons(cm->msg.saddr.ib.lid), 
			 htons(cm->msg.sport), 
			 htonl(cm->msg.saddr.ib.qpn), 
			 htons(cm->msg.daddr.ib.lid), 
			 htons(cm->msg.dport), 
			 htonl(cm->msg.daddr.ib.qpn));
			
		dapl_os_unlock(&cm->lock);
#ifdef DAPL_COUNTERS
		if (g_dapl_dbg_type & DAPL_DBG_TYPE_CM_LIST) {
			dapl_os_unlock(&cm->hca->ib_trans.lock);
			dapls_print_cm_list(dapl_llist_peek_head(&cm->hca->ia_list_head));
			dapl_os_lock(&cm->hca->ib_trans.lock);
		}
#endif
#ifdef DAT_EXTENSIONS
		if (cm->msg.saddr.ib.qp_type == IBV_QPT_UD) {
			DAT_IB_EXTENSION_EVENT_DATA xevent;
					
			/* post REJECT event with CONN_REQ p_data */
			xevent.status = 0;
			xevent.type = DAT_IB_UD_CONNECT_ERROR;
					
			dapls_evd_post_connection_event_ext(
				(DAPL_EVD *)cm->ep->param.connect_evd_handle,
				DAT_IB_UD_CONNECTION_ERROR_EVENT,
				(DAT_EP_HANDLE)cm->ep,
				(DAT_COUNT)ntohs(cm->msg.p_size),
				(DAT_PVOID *)cm->msg.p_data,
				(DAT_PVOID *)&xevent);
		} else 
#endif
			dapls_cr_callback(cm, IB_CME_LOCAL_FAILURE, 
					  NULL, 0, cm->sp);
		return -1;
	}

	dapl_os_get_time(&cm->timer); /* RTU expected */
	if (ucm_send(&cm->hca->ib_trans, &cm->msg, cm->p_data, cm->p_size)) {
		dapl_log(DAPL_DBG_TYPE_ERR," accept ERR: ucm reply send()\n");
		dapl_os_unlock(&cm->lock);
		return -1;
	}
	dapl_os_unlock(&cm->lock);
	return 0;
}


/*
 * PASSIVE: consumer accept, send local QP information, private data, 
 * queue on work thread to receive RTU information to avoid blocking
 * user thread. 
 */
DAT_RETURN
dapli_accept_usr(DAPL_EP *ep, DAPL_CR *cr, DAT_COUNT p_size, DAT_PVOID p_data)
{
	DAPL_IA *ia = ep->header.owner_ia;
	dp_ib_cm_handle_t cm = cr->ib_cm_handle;

	if (p_size > DCM_MAX_PDATA_SIZE)
		return DAT_LENGTH_ERROR;

	dapl_os_lock(&cm->lock);
	if (cm->state != DCM_ACCEPTING) {
		dapl_log(DAPL_DBG_TYPE_ERR,
			 " CM_ACCEPT_USR: wrong state ep %p cm %p %s refs=%d"
			 " %x %x i_%x -> %x %x i_%x l_pid %x r_pid %x\n",
			 cm->ep, cm, dapl_cm_state_str(cm->state),
			 cm->ref_count,
			 htons(cm->hca->ib_trans.addr.ib.lid),
			 htons(cm->msg.sport),
			 htonl(ep->qp_handle->qp->qp_num),
			 htons(cm->msg.daddr.ib.lid),
			 htons(cm->msg.dport),
			 htonl(cm->msg.daddr.ib.qpn),
			 ntohl(cm->msg.s_id),
			 ntohl(cm->msg.d_id));
		dapl_os_unlock(&cm->lock);
		return DAT_INVALID_STATE;
	}
	dapl_os_unlock(&cm->lock);

	dapl_dbg_log(DAPL_DBG_TYPE_CM,
		     " ACCEPT_USR: remote lid=%x"
		     " iqp=%x qp_type %d, psize=%d\n",
		     ntohs(cm->msg.daddr.ib.lid),
		     ntohl(cm->msg.daddr.ib.qpn), cm->msg.daddr.ib.qp_type, 
		     p_size);

	dapl_dbg_log(DAPL_DBG_TYPE_CM,
		     " ACCEPT_USR: remote GID subnet %016llx id %016llx\n",
		     (unsigned long long)
		     htonll(*(uint64_t*)&cm->msg.daddr.ib.gid[0]),
		     (unsigned long long)
		     htonll(*(uint64_t*)&cm->msg.daddr.ib.gid[8]));

#ifdef DAT_EXTENSIONS
	if (cm->msg.daddr.ib.qp_type == IBV_QPT_UD &&
	    ep->qp_handle->qp->qp_type != IBV_QPT_UD) {
		dapl_log(DAPL_DBG_TYPE_ERR,
			     " ACCEPT_USR: ERR remote QP is UD,"
			     ", but local QP is not\n");
		return (DAT_INVALID_HANDLE | DAT_INVALID_HANDLE_EP);
	}
#endif

        /* rdma_out, initiator, cannot exceed remote rdma_in max */
	if (ntohs(cm->msg.ver) >= 7)
		ep->param.ep_attr.max_rdma_read_out =
				DAPL_MIN(ep->param.ep_attr.max_rdma_read_out,
					 cm->msg.rd_in);

	/* modify QP to RTR and then to RTS with remote info already read */
	dapl_os_lock(&ep->header.lock);
	if (dapls_modify_qp_state(ep->qp_handle->qp,
				  IBV_QPS_RTR, 
				  cm->msg.daddr.ib.qpn,
				  cm->msg.daddr.ib.lid,
				  (ib_gid_handle_t)cm->msg.daddr.ib.gid) != DAT_SUCCESS) {
		dapl_log(DAPL_DBG_TYPE_ERR,
			 " ACCEPT_USR: QPS_RTR ERR %s -> lid %x qpn %x\n",
			 strerror(errno), ntohs(cm->msg.daddr.ib.lid),
			 ntohl(cm->msg.daddr.ib.qpn));
		dapl_os_unlock(&ep->header.lock);
		goto bail;
	}
	if (dapls_modify_qp_state(ep->qp_handle->qp,
				  IBV_QPS_RTS, 
				  cm->msg.daddr.ib.qpn,
				  cm->msg.daddr.ib.lid,
				  NULL) != DAT_SUCCESS) {
		dapl_log(DAPL_DBG_TYPE_ERR,
			 " ACCEPT_USR: QPS_RTS ERR %s -> lid %x qpn %x\n",
			 strerror(errno), ntohs(cm->msg.daddr.ib.lid),
			 ntohl(cm->msg.daddr.ib.qpn));
		dapl_os_unlock(&ep->header.lock);
		goto bail;
	}
	dapl_os_unlock(&ep->header.lock);

	/* save remote address information */
	dapl_os_memcpy(&ep->remote_ia_address,
		       &cm->msg.saddr, sizeof(union dcm_addr));

	/* setup local QP info and type from EP, copy pdata, for reply */
	cm->msg.op = htons(DCM_REP);
	cm->msg.rd_in = ep->param.ep_attr.max_rdma_read_in;
	cm->msg.saddr.ib.qpn = htonl(ep->qp_handle->qp->qp_num);
	cm->msg.saddr.ib.qp_type = ep->qp_handle->qp->qp_type;
	cm->msg.saddr.ib.lid = cm->hca->ib_trans.addr.ib.lid; 
	dapl_os_memcpy(&cm->msg.saddr.ib.gid[0],
		       &cm->hca->ib_trans.addr.ib.gid, 16); 

	/*
	 * UD: deliver p_data with REQ and EST event, keep REQ p_data in
	 * cm->msg.p_data and save REPLY accept data in cm->p_data for retries
	 */
	cm->p_size = p_size;
	dapl_os_memcpy(&cm->p_data, p_data, p_size);

	/* save state and setup valid reference to EP, HCA. !PSP !RSP */
	if (!cm->sp->ep_handle && !cm->sp->psp_flags)
		dapl_ep_link_cm(ep, cm);
	cm->ep = ep;
	cm->hca = ia->hca_ptr;

	/* Send RTU and change state under CM lock */
	dapl_os_lock(&cm->lock);
	cm->state = DCM_RTU_PENDING;
	dapl_os_get_time(&cm->timer); /* RTU expected */
	if (ucm_send(&cm->hca->ib_trans, &cm->msg, cm->p_data, cm->p_size)) {
		dapl_log(DAPL_DBG_TYPE_ERR," accept ERR: ucm reply send()\n");
		dapl_os_unlock(&cm->lock);
		dapl_ep_unlink_cm(ep, cm);
		goto bail;
	}
	dapl_os_unlock(&cm->lock);

	DAPL_CNTR(ia, DCNT_IA_CM_REP_TX);
	dapl_dbg_log(DAPL_DBG_TYPE_CM, " PASSIVE: accepted!\n");
	dapls_thread_signal(&cm->hca->ib_trans.signal);
	return DAT_SUCCESS;
bail:
	DAPL_CNTR(ia, DCNT_IA_CM_ERR);
	dapli_cm_free(cm);
	return DAT_INTERNAL_ERROR;
}


/*
 * dapls_ib_connect
 *
 * Initiate a connection with the passive listener on another node
 *
 * Input:
 *	ep_handle,
 *	remote_ia_address,
 *	remote_conn_qual,
 *	prd_size		size of private data and structure
 *	prd_prt			pointer to private data structure
 *
 * Output:
 * 	none
 *
 * Returns:
 * 	DAT_SUCCESS
 *	DAT_INSUFFICIENT_RESOURCES
 *	DAT_INVALID_PARAMETER
 *
 */
DAT_RETURN
dapls_ib_connect(IN DAT_EP_HANDLE ep_handle,
		 IN DAT_IA_ADDRESS_PTR r_addr,
		 IN DAT_CONN_QUAL r_psp,
		 IN DAT_COUNT p_size, IN void *p_data)
{
	DAPL_EP *ep = (DAPL_EP *)ep_handle;
	dp_ib_cm_handle_t cm;
	union dcm_addr *ucm_ia = (union dcm_addr *) r_addr;

	dapl_log(DAPL_DBG_TYPE_CM, " UCM connect -> AF %d LID 0x%x QPN 0x%x GID"
		 " 0x" F64x ":" F64x " port %d sl %d qt %d\n",
		 ucm_ia->ib.family, ntohs(ucm_ia->ib.lid), ntohl(ucm_ia->ib.qpn),
		 (unsigned long long)ntohll(*(uint64_t*)&ucm_ia->ib.gid[0]),
		 (unsigned long long)ntohll(*(uint64_t*)&ucm_ia->ib.gid[8]),
		 ntohs(ucm_ia->ib.port), ucm_ia->ib.sl, ucm_ia->ib.qp_type);
	
	/* create CM object, initialize SRC info from EP */
	cm = dapls_ib_cm_create(ep);
	if (cm == NULL)
		return DAT_INSUFFICIENT_RESOURCES;

	/* remote hca and port: lid, gid, network order */
	dapl_os_memcpy(&cm->msg.daddr, r_addr, sizeof(union dcm_addr));

	/* remote uCM information, comes from consumer provider r_addr */
	cm->msg.dport = htons((uint16_t)r_psp);
	cm->msg.dqpn = cm->msg.daddr.ib.qpn;
	cm->msg.daddr.ib.qpn = 0; /* don't have a remote qpn until reply */
	
        /* set max rdma inbound requests */
        cm->msg.rd_in = ep->param.ep_attr.max_rdma_read_in;

	if (p_size) {
		cm->msg.p_size = htons(p_size);
		dapl_os_memcpy(&cm->msg.p_data, p_data, p_size);
	}
	
	cm->state = DCM_INIT;

	/* link EP and CM, put on work queue */
	dapli_queue_conn(cm);

	/* build connect request, send to remote CM based on r_addr info */
	return (dapli_cm_connect(ep, cm));
}

/*
 * dapls_ib_disconnect
 *
 * Disconnect an EP
 *
 * Input:
 *	ep_handle,
 *	disconnect_flags
 *
 * Output:
 * 	none
 *
 * Returns:
 * 	DAT_SUCCESS
 */
DAT_RETURN
dapls_ib_disconnect(IN DAPL_EP *ep_ptr, IN DAT_CLOSE_FLAGS close_flags)
{
	dp_ib_cm_handle_t cm_ptr = dapl_get_cm_from_ep(ep_ptr);

	dapl_os_lock(&ep_ptr->header.lock);
	if (ep_ptr->param.ep_state == DAT_EP_STATE_DISCONNECTED ||
	    ep_ptr->param.ep_attr.service_type != DAT_SERVICE_TYPE_RC ||
	    cm_ptr == NULL) {
		dapl_os_unlock(&ep_ptr->header.lock);
		return DAT_SUCCESS;
	} 
	dapl_os_unlock(&ep_ptr->header.lock);
	
	dapli_cm_disconnect(cm_ptr);

        /* ABRUPT close, wait for callback and DISCONNECTED state */
        if (close_flags == DAT_CLOSE_ABRUPT_FLAG) {
                dapl_os_lock(&ep_ptr->header.lock);
                if (ep_ptr->param.ep_state != DAT_EP_STATE_DISCONNECTED) {
                	dapl_os_unlock(&ep_ptr->header.lock);
                	dapl_os_wait_object_wait(&cm_ptr->d_event, DAT_TIMEOUT_INFINITE);
                	dapl_os_lock(&ep_ptr->header.lock);
                }
                dapl_os_unlock(&ep_ptr->header.lock);
	}

	return DAT_SUCCESS;
}

/*
 * dapls_ib_disconnect_clean
 *
 * Clean up outstanding connection data. This routine is invoked
 * after the final disconnect callback has occurred. Only on the
 * ACTIVE side of a connection. It is also called if dat_ep_connect
 * times out using the consumer supplied timeout value.
 *
 * Input:
 *	ep_ptr		DAPL_EP
 *	active		Indicates active side of connection
 *
 * Output:
 * 	none
 *
 * Returns:
 * 	void
 *
 */
void
dapls_ib_disconnect_clean(IN DAPL_EP *ep,
			  IN DAT_BOOLEAN active,
			  IN const ib_cm_events_t ib_cm_event)
{
	if (ib_cm_event == IB_CME_TIMEOUT) {
		dp_ib_cm_handle_t cm_ptr;

		if ((cm_ptr = dapl_get_cm_from_ep(ep)) == NULL)
			return;

		dapl_log(DAPL_DBG_TYPE_WARN,
			"dapls_ib_disc_clean: CONN_TIMEOUT ep %p cm %p %s\n",
			ep, cm_ptr, dapl_cm_state_str(cm_ptr->state));
		
		/* schedule release of socket and local resources */
		dapli_cm_free(cm_ptr);
	}
}

/*
 * dapl_ib_setup_conn_listener
 *
 * Have the CM set up a connection listener.
 *
 * Input:
 *	ibm_hca_handle		HCA handle
 *	qp_handle			QP handle
 *
 * Output:
 * 	none
 *
 * Returns:
 * 	DAT_SUCCESS
 *	DAT_INSUFFICIENT_RESOURCES
 *	DAT_INTERNAL_ERROR
 *	DAT_CONN_QUAL_UNAVAILBLE
 *	DAT_CONN_QUAL_IN_USE
 *
 */
DAT_RETURN
dapls_ib_setup_conn_listener(IN DAPL_IA *ia, 
			     IN DAT_UINT64 sid, 
			     IN DAPL_SP *sp)
{
	ib_cm_srvc_handle_t cm = NULL;

	dapl_dbg_log(DAPL_DBG_TYPE_EP,
		     " listen(ia %p ServiceID %x sp %p)\n",
		     ia, sid, sp);

	/* reserve local port, then allocate CM object */
	if (!ucm_get_port(&ia->hca_ptr->ib_trans, (uint16_t)sid)) {
		dapl_dbg_log(DAPL_DBG_TYPE_WARN,
			     " listen: ERROR %s on conn_qual %x\n",
			     strerror(errno), sid);
		return DAT_CONN_QUAL_IN_USE;
	}

	/* cm_create will setup saddr for listen server */
	if ((cm = dapls_ib_cm_create(NULL)) == NULL)
		return DAT_INSUFFICIENT_RESOURCES;

	/* LISTEN: init DST address and QP info to local CM server info */
	cm->sp = sp;
	cm->hca = ia->hca_ptr;
	cm->msg.sport = htons((uint16_t)sid);
	cm->msg.sqpn = htonl(ia->hca_ptr->ib_trans.qp->qp_num);
	cm->msg.saddr.ib.qp_type = IBV_QPT_UD;
        cm->msg.saddr.ib.lid = ia->hca_ptr->ib_trans.addr.ib.lid; 
	dapl_os_memcpy(&cm->msg.saddr.ib.gid[0],
		       &cm->hca->ib_trans.addr.ib.gid, 16); 
	
	/* save cm_handle reference in service point */
	sp->cm_srvc_handle = cm;

	/* queue up listen socket to process inbound CR's */
	cm->state = DCM_LISTEN;
	dapli_queue_listen(cm);
	DAPL_CNTR(ia, DCNT_IA_CM_LISTEN);

	return DAT_SUCCESS;
}


/*
 * dapl_ib_remove_conn_listener
 *
 * Have the CM remove a connection listener.
 *
 * Input:
 *	ia_handle		IA handle
 *	ServiceID		IB Channel Service ID
 *
 * Output:
 * 	none
 *
 * Returns:
 * 	DAT_SUCCESS
 *	DAT_INVALID_STATE
 *
 */
DAT_RETURN
dapls_ib_remove_conn_listener(IN DAPL_IA *ia, IN DAPL_SP *sp)
{
	ib_cm_srvc_handle_t cm = sp->cm_srvc_handle;

	/* free cm_srvc_handle and port, and mark CM for cleanup */
	if (cm) {
		dapl_dbg_log(DAPL_DBG_TYPE_EP,
		     " remove_listener(ia %p sp %p cm %p psp=%x)\n",
		     ia, sp, cm, ntohs(cm->msg.dport));

		sp->cm_srvc_handle = NULL;
		dapli_dequeue_listen(cm);  
		ucm_free_port(&cm->hca->ib_trans, ntohs(cm->msg.sport));
		dapls_cm_release(cm);  /* last ref, dealloc */
	}
	return DAT_SUCCESS;
}

/*
 * dapls_ib_accept_connection
 *
 * Perform necessary steps to accept a connection
 *
 * Input:
 *	cr_handle
 *	ep_handle
 *	private_data_size
 *	private_data
 *
 * Output:
 * 	none
 *
 * Returns:
 * 	DAT_SUCCESS
 *	DAT_INSUFFICIENT_RESOURCES
 *	DAT_INTERNAL_ERROR
 *
 */
DAT_RETURN
dapls_ib_accept_connection(IN DAT_CR_HANDLE cr_handle,
			   IN DAT_EP_HANDLE ep_handle,
			   IN DAT_COUNT p_size, 
			   IN const DAT_PVOID p_data)
{
	DAPL_CR *cr = (DAPL_CR *)cr_handle;
	DAPL_EP *ep = (DAPL_EP *)ep_handle;

	dapl_dbg_log(DAPL_DBG_TYPE_EP,
		     " accept_connection(cr %p ep %p prd %p,%d)\n",
		     cr, ep, p_data, p_size);

	/* allocate and attach a QP if necessary */
	if (ep->qp_state == DAPL_QP_STATE_UNATTACHED) {
		DAT_RETURN status;
		status = dapls_ib_qp_alloc(ep->header.owner_ia,
					   ep, ep);
		if (status != DAT_SUCCESS)
			return status;
	}
	return (dapli_accept_usr(ep, cr, p_size, p_data));
}

/*
 * dapls_ib_reject_connection
 *
 * Reject a connection
 *
 * Input:
 *	cr_handle
 *
 * Output:
 * 	none
 *
 * Returns:
 * 	DAT_SUCCESS
 *	DAT_INTERNAL_ERROR
 *
 */
DAT_RETURN
dapls_ib_reject_connection(IN dp_ib_cm_handle_t cm,
			   IN int reason,
			   IN DAT_COUNT psize, IN const DAT_PVOID pdata)
{
	dapl_dbg_log(DAPL_DBG_TYPE_EP,
		     " reject(cm %p reason %x, pdata %p, psize %d)\n",
		     cm, reason, pdata, psize);

        if (psize > DCM_MAX_PDATA_SIZE)
                return DAT_LENGTH_ERROR;

	/* cr_thread will destroy CR, update saddr lid, gid, qp_type info */
	dapl_os_lock(&cm->lock);
	dapl_log(DAPL_DBG_TYPE_CM, 
		 " PASSIVE: REJECTING CM_REQ:"
		 " cm %p op %s, st %s slid %x iqp %x port %x ->"
		 " dlid %x iqp %x port %x\n", cm,
		 dapl_cm_op_str(ntohs(cm->msg.op)), 
		 dapl_cm_state_str(cm->state), 
		 ntohs(cm->hca->ib_trans.addr.ib.lid), 
		 ntohl(cm->msg.saddr.ib.qpn), 
		 ntohs(cm->msg.sport), ntohs(cm->msg.daddr.ib.lid), 
		 ntohl(cm->msg.daddr.ib.qpn), ntohs(cm->msg.dport));

	cm->state = DCM_REJECTED;
	cm->msg.saddr.ib.lid = cm->hca->ib_trans.addr.ib.lid; 
	cm->msg.saddr.ib.qp_type = cm->msg.daddr.ib.qp_type;
	dapl_os_memcpy(&cm->msg.saddr.ib.gid[0],
		       &cm->hca->ib_trans.addr.ib.gid, 16); 
	
	if (reason == IB_CM_REJ_REASON_CONSUMER_REJ)
		cm->msg.op = htons(DCM_REJ_USER);
	else
		cm->msg.op = htons(DCM_REJ_CM);

	DAPL_CNTR(((DAPL_IA *)dapl_llist_peek_head(&cm->hca->ia_list_head)),
		  reason == IB_CM_REJ_REASON_CONSUMER_REJ ?
		  DCNT_IA_CM_USER_REJ_TX : DCNT_IA_CM_ERR_REJ_TX);

	if (ucm_send(&cm->hca->ib_trans, &cm->msg, pdata, psize)) {
		dapl_log(DAPL_DBG_TYPE_WARN,
			 " cm_reject: send ERR: %s\n", strerror(errno));
		dapl_os_unlock(&cm->lock);
		return DAT_INTERNAL_ERROR;
	}
	dapl_os_unlock(&cm->lock);
	dapli_cm_free(cm);
	return DAT_SUCCESS;
}

/*
 * dapls_ib_cm_remote_addr
 *
 * Obtain the remote IP address given a connection
 *
 * Input:
 *	cr_handle
 *
 * Output:
 *	remote_ia_address: where to place the remote address
 *
 * Returns:
 * 	DAT_SUCCESS
 *	DAT_INVALID_HANDLE
 *
 */
DAT_RETURN
dapls_ib_cm_remote_addr(IN DAT_HANDLE dat_handle,
			OUT DAT_SOCK_ADDR6 * remote_ia_address)
{
	DAPL_HEADER *header;
	dp_ib_cm_handle_t cm;

	dapl_dbg_log(DAPL_DBG_TYPE_EP,
		     "dapls_ib_cm_remote_addr(dat_handle %p, ....)\n",
		     dat_handle);

	header = (DAPL_HEADER *) dat_handle;

	if (header->magic == DAPL_MAGIC_EP)
		cm = dapl_get_cm_from_ep((DAPL_EP *) dat_handle);
	else if (header->magic == DAPL_MAGIC_CR)
		cm = ((DAPL_CR *) dat_handle)->ib_cm_handle;
	else
		return DAT_INVALID_HANDLE;

	dapl_os_memcpy(remote_ia_address,
		       &cm->msg.daddr, sizeof(DAT_SOCK_ADDR6));

	return DAT_SUCCESS;
}

int dapls_ib_private_data_size(
	IN DAPL_HCA *hca_ptr)
{
	return DCM_MAX_PDATA_SIZE;
}

#if defined(_WIN32) || defined(_WIN64)

void cm_thread(void *arg)
{
	struct dapl_hca *hca = arg;
	dp_ib_cm_handle_t cm, next;
	DWORD time_ms;

	dapl_dbg_log(DAPL_DBG_TYPE_UTIL, " cm_thread: ENTER hca %p\n", hca);
	dapl_os_lock(&hca->ib_trans.lock);
	for (hca->ib_trans.cm_state = IB_THREAD_RUN;
	     hca->ib_trans.cm_state == IB_THREAD_RUN ||
	     !dapl_llist_is_empty(&hca->ib_trans.list);
	     dapl_os_lock(&hca->ib_trans.lock)) {

		time_ms = INFINITE;
		CompSetZero(&hca->ib_trans.signal.set);
		CompSetAdd(&hca->ib_hca_handle->channel, &hca->ib_trans.signal.set);
		CompSetAdd(&hca->ib_trans.rch->comp_channel, &hca->ib_trans.signal.set);
		CompSetAdd(&hca->ib_trans.ib_cq->comp_channel, &hca->ib_trans.signal.set);

		next = dapl_llist_is_empty(&hca->ib_trans.list) ? NULL :
			dapl_llist_peek_head(&hca->ib_trans.list);

		while (next) {
			cm = next;
			next = dapl_llist_next_entry(&hca->ib_trans.list,
						     (DAPL_LLIST_ENTRY *)&cm->local_entry);
			dapls_cm_acquire(cm); /* hold thread ref */
			dapl_os_lock(&cm->lock);
			if (cm->state == DCM_FREE || 
			    hca->ib_trans.cm_state != IB_THREAD_RUN) {
				dapl_os_unlock(&cm->lock);
				dapl_log(DAPL_DBG_TYPE_CM, 
					 " CM FREE: %p ep=%p st=%s refs=%d\n", 
					 cm, cm->ep, dapl_cm_state_str(cm->state), 
					 cm->ref_count);

				dapls_cm_release(cm); /* release alloc ref */
				dapli_cm_dequeue(cm); /* release workq ref */
				dapls_cm_release(cm); /* release thread ref */
				continue;
			}
			dapl_os_unlock(&cm->lock);
			ucm_check_timers(cm, &time_ms);
			dapls_cm_release(cm); /* release thread ref */
		}

		dapl_os_unlock(&hca->ib_trans.lock);

		hca->ib_hca_handle->channel.Milliseconds = time_ms;
		hca->ib_trans.rch->comp_channel.Milliseconds = time_ms;
		hca->ib_trans.ib_cq->comp_channel.Milliseconds = time_ms;
		CompSetPoll(&hca->ib_trans.signal.set, time_ms);

		hca->ib_hca_handle->channel.Milliseconds = 0;
		hca->ib_trans.rch->comp_channel.Milliseconds = 0;
		hca->ib_trans.ib_cq->comp_channel.Milliseconds = 0;

		ucm_recv(&hca->ib_trans);
		ucm_async_event(hca);
		dapli_cq_event_cb(&hca->ib_trans);
	}

	dapl_os_unlock(&hca->ib_trans.lock);
	hca->ib_trans.cm_state = IB_THREAD_EXIT;
	dapl_dbg_log(DAPL_DBG_TYPE_UTIL, " cm_thread(hca %p) exit\n", hca);
}

#else				// _WIN32 || _WIN64

void cm_thread(void *arg)
{
	struct dapl_hca *hca = arg;
	dp_ib_cm_handle_t cm, next;
	struct dapl_fd_set *set;
	char rbuf[2];
	int time_ms;

	dapl_dbg_log(DAPL_DBG_TYPE_UTIL, " cm_thread: ENTER hca %p\n", hca);
	set = dapl_alloc_fd_set();
	if (!set)
		goto out;

	dapl_os_lock(&hca->ib_trans.lock);
	hca->ib_trans.cm_state = IB_THREAD_RUN;

	while (1) {
		time_ms = -1; /* reset to blocking */
		dapl_fd_zero(set);
		dapl_fd_set(hca->ib_trans.signal.scm[0], set, DAPL_FD_READ);	
		dapl_fd_set(hca->ib_hca_handle->async_fd, set, DAPL_FD_READ);
		dapl_fd_set(hca->ib_trans.rch->fd, set, DAPL_FD_READ);
		dapl_fd_set(hca->ib_trans.ib_cq->fd, set, DAPL_FD_READ);
		
		if (!dapl_llist_is_empty(&hca->ib_trans.list))
			next = dapl_llist_peek_head(&hca->ib_trans.list);
		else
			next = NULL;

		while (next) {
			cm = next;
			next = dapl_llist_next_entry(
					&hca->ib_trans.list,
					(DAPL_LLIST_ENTRY *)&cm->local_entry);
			dapls_cm_acquire(cm); /* hold thread ref */
			dapl_os_lock(&cm->lock);
			if (cm->state == DCM_FREE || 
			    hca->ib_trans.cm_state != IB_THREAD_RUN) {
				dapl_os_unlock(&cm->lock);
				dapl_log(DAPL_DBG_TYPE_CM, 
					 " CM FREE: %p ep=%p st=%s refs=%d\n", 
					 cm, cm->ep, dapl_cm_state_str(cm->state), 
					 cm->ref_count);

				dapls_cm_release(cm); /* release alloc ref */
				dapli_cm_dequeue(cm); /* release workq ref */
				dapls_cm_release(cm); /* release thread ref */
				continue;
			}
			dapl_os_unlock(&cm->lock);
			ucm_check_timers(cm, &time_ms);
			dapls_cm_release(cm); /* release thread ref */
		}

		/* set to exit and all resources destroyed */
		if ((hca->ib_trans.cm_state != IB_THREAD_RUN) &&
		    (dapl_llist_is_empty(&hca->ib_trans.list)))
			break;

		dapl_os_unlock(&hca->ib_trans.lock);
		dapl_select(set, time_ms);

		/* Process events: CM, ASYNC, NOTIFY THREAD */
		if (dapl_poll(hca->ib_trans.rch->fd, 
			      DAPL_FD_READ) == DAPL_FD_READ) {
			ucm_recv(&hca->ib_trans);
		}
		if (dapl_poll(hca->ib_hca_handle->async_fd, 
			      DAPL_FD_READ) == DAPL_FD_READ) {
			dapli_async_event_cb(&hca->ib_trans);
		}
		if (dapl_poll(hca->ib_trans.ib_cq->fd, 
			      DAPL_FD_READ) == DAPL_FD_READ) {
			dapli_cq_event_cb(&hca->ib_trans);
		}
		while (dapl_poll(hca->ib_trans.signal.scm[0], 
				 DAPL_FD_READ) == DAPL_FD_READ) {
			recv(hca->ib_trans.signal.scm[0], rbuf, 2, 0);
		}
		dapl_os_lock(&hca->ib_trans.lock);
		
		/* set to exit and all resources destroyed */
		if ((hca->ib_trans.cm_state != IB_THREAD_RUN) &&
		    (dapl_llist_is_empty(&hca->ib_trans.list)))
			break;
	}

	dapl_os_unlock(&hca->ib_trans.lock);
	free(set);
out:
	hca->ib_trans.cm_state = IB_THREAD_EXIT;
	dapl_dbg_log(DAPL_DBG_TYPE_UTIL, " cm_thread(hca %p) exit\n", hca);
}
#endif

#ifdef DAPL_COUNTERS
static char _ctr_host_[128];
/* Debug aid: List all Connections in process and state */
void dapls_print_cm_list(IN DAPL_IA *ia_ptr)
{
	/* Print in process CM's for this IA, if debug type set */
	int i = 0;
	dp_ib_cm_handle_t cm, next_cm;
	struct dapl_llist_entry	**list;
	DAPL_OS_LOCK *lock;
	
	/* LISTEN LIST */
	list = &ia_ptr->hca_ptr->ib_trans.llist;
	lock = &ia_ptr->hca_ptr->ib_trans.llock;

	dapl_os_lock(lock);
	if (!dapl_llist_is_empty((DAPL_LLIST_HEAD*)list))
		next_cm = dapl_llist_peek_head((DAPL_LLIST_HEAD*)list);
 	else
		next_cm = NULL;

	gethostname(_ctr_host_, sizeof(_ctr_host_));
	printf("\n [%s:%x] DAPL IA LISTEN/CONNECTIONS IN PROCESS:\n", 
		_ctr_host_ , dapl_os_getpid());

	while (next_cm) {
		cm = next_cm;
		next_cm = dapl_llist_next_entry((DAPL_LLIST_HEAD*)list,
						(DAPL_LLIST_ENTRY*)&cm->local_entry);

		printf( "  LISTEN[%d]: sp %p %s uCM_QP: %x %x c_%x l_pid %x \n",
			i, cm->sp, dapl_cm_state_str(cm->state),
			ntohs(cm->msg.saddr.ib.lid), ntohs(cm->msg.sport),
			ntohl(cm->msg.sqpn),
			ntohl(cm->msg.s_id));
		i++;
	}
	dapl_os_unlock(lock);

	/* CONNECTION LIST */
	list = &ia_ptr->hca_ptr->ib_trans.list;
	lock = &ia_ptr->hca_ptr->ib_trans.lock;

	dapl_os_lock(lock);
	if (!dapl_llist_is_empty((DAPL_LLIST_HEAD*)list))
		next_cm = dapl_llist_peek_head((DAPL_LLIST_HEAD*)list);
 	else
		next_cm = NULL;

        while (next_cm) {
		cm = next_cm;
		next_cm = dapl_llist_next_entry((DAPL_LLIST_HEAD*)list,
						(DAPL_LLIST_ENTRY*)&cm->local_entry);

		printf( "  CONN[%d]: ep %p cm %p %s %s"
			"  %x %x c_%x i_%x %s %x %x c_%x i_%x r_pid %x\n",
			i, cm->ep, cm,
			cm->msg.saddr.ib.qp_type == IBV_QPT_RC ? "RC" : "UD",
			dapl_cm_state_str(cm->state),
			ntohs(cm->msg.saddr.ib.lid),
			ntohs(cm->msg.sport),
			ntohl(cm->msg.sqpn),
			ntohl(cm->msg.saddr.ib.qpn),	
			cm->sp ? "<-" : "->",
			ntohs(cm->msg.daddr.ib.lid),
			ntohs(cm->msg.dport),
			ntohl(cm->msg.dqpn),
			ntohl(cm->msg.daddr.ib.qpn),
			ntohl(cm->msg.d_id));
		i++;
	}
	printf("\n");
	dapl_os_unlock(lock);
}

void dapls_print_cm_free_list(IN DAPL_IA *ia_ptr)
{
	DAPL_EP	*ep, *next_ep;
	dp_ib_cm_handle_t cm, next_cm;
	int i = 0;

	gethostname(_ctr_host_, sizeof(_ctr_host_));
	printf("\n [%s:%x] DAPL EP CM FREE LIST:\n",
		_ctr_host_ , dapl_os_getpid());

	dapl_os_lock(&ia_ptr->header.lock);
	ep = (dapl_llist_is_empty(&ia_ptr->ep_list_head) ?
		NULL : dapl_llist_peek_head(&ia_ptr->ep_list_head));
	while (ep != NULL) {
		next_ep = dapl_llist_next_entry(&ia_ptr->ep_list_head,
					        &ep->header.ia_list_entry);
		dapl_os_lock(&ep->header.lock);
		cm = (dapl_llist_is_empty(&ep->cm_list_head) ?
			NULL : dapl_llist_peek_head(&ep->cm_list_head));
	 	while (cm) {
	 		dapl_os_lock(&cm->lock);
			next_cm = dapl_llist_next_entry(&ep->cm_list_head,
							&cm->list_entry);
			if (cm->state == DCM_FREE) {
				printf( "  CONN[%d]: ep %p cm %p %s %s"
					" %x %x c_%x i_%x l_pid %x %s"
					" %x %x c_%x i_%x r_pid %x\n",
					i, cm->ep, cm,
					cm->msg.saddr.ib.qp_type == IBV_QPT_RC ? "RC" : "UD",
					dapl_cm_state_str(cm->state),
					ntohs(cm->msg.saddr.ib.lid),
					ntohs(cm->msg.sport),
					ntohl(cm->msg.sqpn),
					ntohl(cm->msg.saddr.ib.qpn),
					ntohl(cm->msg.s_id),
					cm->sp ? "<-" : "->",
					ntohs(cm->msg.daddr.ib.lid),
					ntohs(cm->msg.dport),
					ntohl(cm->msg.dqpn),
					ntohl(cm->msg.daddr.ib.qpn),
					ntohl(cm->msg.d_id));
				i++;
			}
			dapl_os_unlock(&cm->lock);
			cm = next_cm;
		}
	 	dapl_os_unlock(&ep->header.lock);
		ep = next_ep;
	}
	dapl_os_unlock(&ia_ptr->header.lock);
}
#endif
