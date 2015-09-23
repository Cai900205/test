/*
 * Copyright (c) 2009-2014 Intel Corporation.  All rights reserved.
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
#include "dapl_osd.h"

/*
 * CM proxy services, MCM on MIC to MPXYD via SCIF
 *
 * MIX_IA_OPEN
 */
int dapli_mix_open(ib_hca_transport_t *tp, char *name, int port, int query_only)
{
	int ret, len;
	dat_mix_open_t msg;
	scif_epd_t listen_ep;
	int listen_port;
	int always_proxy;
	int scif_port_id;

	/* make MPXY connection even not running on MIC. good for debugging */
	always_proxy = dapl_os_get_env_val("DAPL_MCM_ALWAYS_PROXY", 0);
	scif_port_id = dapl_os_get_env_val("DAPL_MCM_PORT_ID", SCIF_OFED_PORT_8);

	ret = scif_get_nodeIDs(NULL, 0, &tp->self.node);
	if (ret < 0) {
		dapl_log(1, " scif_get_nodeIDs() failed with error %s\n", strerror(errno));
		return -1;
	}
	dapl_log(DAPL_DBG_TYPE_EXTENSION,
		 " SCIF node_id: %d client req_id 0x%x\n",
		 (uint16_t)tp->self.node, dapl_os_getpid());

	if (tp->self.node == 0)
		tp->addr.ep_map = HOST_SOCK_DEV;  /* non-MIC mapping */

	if (query_only || (tp->self.node == 0 && !always_proxy)){
		dapl_log(DAPL_DBG_TYPE_EXTENSION," Not running on MIC, no MPXY connect required\n");
		tp->scif_ep = 0;
		return 0;
	}
	dapl_log(DAPL_DBG_TYPE_EXTENSION," Running on MIC, MPXY connect required\n");

	/* Create an endpoint for MPXYD to connect back */
	listen_ep = scif_open();
	if (listen_ep < 0) {
		dapl_log(1, "scif_open() failed with error %s\n", strerror(errno));
		return -1;
	}

	listen_port = scif_bind(listen_ep, 0);
	if (listen_port < 0) {
		dapl_log(1, "scif_listen() failed with error %s\n", strerror(errno));
		return -1;
	}

	ret = scif_listen(listen_ep, 2);
	if (ret < 0) {
		dapl_log(1, "scif_listen() failed with error %s\n", strerror(errno));
		return -1;
	}

	/* MPXYD is running on node 0 and well-known OFED port */
	tp->peer.node = 0;
	tp->peer.port = scif_port_id;

	tp->scif_ep = scif_open();
	if (tp->scif_ep < 0) {
		dapl_log(1, "scif_open() failed with error %s\n", strerror(errno));
		return -1;
	}
	ret = scif_connect(tp->scif_ep, &tp->peer);
	if (ret < 0) {
		dapl_log(1, "scif_connect() to port %d, failed with error %s\n",
			    scif_port_id, strerror(errno));
		return -1;
	}
	dapl_log(DAPL_DBG_TYPE_EXTENSION, "Connected to node 0 for operations, ep=%d\n", tp->scif_ep);

	len = sizeof(listen_port);
	ret = scif_send(tp->scif_ep, &listen_port, len, SCIF_SEND_BLOCK);
	if (ret != len) {
		dapl_log(1, " ERR: OPEN EP's send on %d, ret %d, exp %d, error %s\n",
			 tp->scif_ep, ret, len, strerror(errno));
		return -1;
	}
	dapl_log(DAPL_DBG_TYPE_EXTENSION," Sent listen port number (%d) on SCIF EP\n", listen_port);

	ret = scif_accept(listen_ep, &tp->peer_ev, &tp->scif_ev_ep, SCIF_ACCEPT_SYNC);
	if (ret < 0) {
		dapl_log(1, "scif_accept() for ev_ep failed with error %s\n", strerror(errno));
		return -1;
	}
	dapl_log(DAPL_DBG_TYPE_EXTENSION," Accepted Event EP (%d)\n", tp->scif_ev_ep);
	ret = scif_accept(listen_ep, &tp->peer_tx, &tp->scif_tx_ep, SCIF_ACCEPT_SYNC);
	if (ret < 0) {
		dapl_log(1, "scif_accept() for tx_ep failed with error %s\n", strerror(errno));
		return -1;
	}
	dapl_log(DAPL_DBG_TYPE_EXTENSION," Accepted TX EP (%d)\n", tp->scif_tx_ep);
	ret = scif_close(listen_ep);
	if (ret < 0) {
		dapl_log(1, "scif_close() failed with error %d\n", strerror(errno));
		return -1;
	}
	dapl_log(DAPL_DBG_TYPE_EXTENSION, "Connected to node 0 for DATA, tx_ep=%d \n", tp->scif_tx_ep);

	/* MIX_IA_OPEN: device name and port */
	msg.hdr.ver = DAT_MIX_VER;
	msg.hdr.op = MIX_IA_OPEN;
	msg.hdr.status = 0;
	msg.hdr.flags = MIX_OP_REQ;
	msg.hdr.req_id = dapl_os_getpid();
	msg.port = port;
	strcpy((char*)&msg.name, name);
	memcpy(&msg.dev_attr, (void*)&tp->ack_timer, sizeof(dat_mix_dev_attr_t));

	len = sizeof(dat_mix_open_t);
	ret = scif_send(tp->scif_ep, &msg, len, SCIF_SEND_BLOCK);
	if (ret != len) {
		dapl_log(1, " ERR: %s send on %d, ret %d, exp %d, error %s\n",
			 mix_op_str(msg.hdr.op),tp->scif_ep, ret, len, strerror(errno));
		return -1;
	}
	dapl_log(DAPL_DBG_TYPE_EXTENSION," Sent %s request on SCIF EP %d, req_id 0x%x\n",
			mix_op_str(msg.hdr.op), tp->scif_ep, ntohl(msg.hdr.req_id));

	/* MIX_IA_OPEN: reply includes addr info */
	ret = scif_recv(tp->scif_ep, &msg, len, SCIF_RECV_BLOCK);
	if (ret != len) {
		dapl_log(1, " ERR: dev_open reply ep %d, ret %d, exp %d, error %s\n",
			    tp->scif_ep, ret, len, strerror(errno));
		return -1;
	}
	dapl_log(DAPL_DBG_TYPE_EXTENSION," Recv'd %s reply on SCIF EP %d, dev_id %d\n",
				mix_op_str(msg.hdr.op), tp->scif_ep, msg.hdr.req_id);

	if (msg.hdr.ver != DAT_MIX_VER || msg.hdr.op != MIX_IA_OPEN ||
	    msg.hdr.flags != MIX_OP_RSP || msg.hdr.status != MIX_SUCCESS) {
		dapl_log(1, " ERR: dev_open ver (exp %d rcv %d), op %s, flgs %d, st %d dev_id %d\n",
			     DAT_MIX_VER, msg.hdr.ver, mix_op_str(msg.hdr.op),
			     msg.hdr.flags, msg.hdr.status, msg.hdr.req_id);
		return -1;
	}
	/* save address to transport object, keeps IA queries local */
	memcpy((void*)&tp->addr, (void*)&msg.dev_addr, sizeof(dat_mcm_addr_t));
	tp->dev_id = msg.hdr.req_id;
	dapl_log(DAPL_DBG_TYPE_EXTENSION,
		 " mix_open reply (msg %p, ln %d) EPs %d %d %d - dev_id %d\n",
		 &msg, len, tp->scif_ep, tp->scif_ev_ep,
		 tp->scif_tx_ep, tp->dev_id);
	return 0;
}

/* MIX_IA_CLOSE - no operation, just shutdown endpoint(s) */
void dapli_mix_close(ib_hca_transport_t *tp)
{
	dapl_log(DAPL_DBG_TYPE_EXTENSION,
		 " MIX_IA_CLOSE: tp %p scif EP's %d,%d,%d dev_id %d\n",
		 tp, tp->scif_ep, tp->scif_tx_ep, tp->scif_ev_ep, tp->dev_id);

	if (tp->scif_ep) {
		scif_close(tp->scif_ep);
		tp->scif_ep = 0;
	}
	if (tp->scif_tx_ep) {
		scif_close(tp->scif_tx_ep);
		tp->scif_tx_ep = 0;
	}
	if (tp->scif_ev_ep) {
		scif_close(tp->scif_ev_ep);
		tp->scif_ev_ep = 0;
	}
}

/* MIX_LISTEN */
int dapli_mix_listen(dp_ib_cm_handle_t cm, uint16_t sid)
{
	dat_mix_listen_t msg;
	scif_epd_t mix_ep = cm->hca->ib_trans.scif_ep;
	int ret, len;

	dapl_log(DAPL_DBG_TYPE_EXTENSION,
		 " MIX_LISTEN port 0x%x htons(0x%x), %d - client req_id 0x%x\n",
		 sid, htons(sid), sid, htonl(dapl_os_getpid()));

	/* listen request: sid and backlog */
	msg.hdr.ver = DAT_MIX_VER;
	msg.hdr.op = MIX_LISTEN;
	msg.hdr.status = 0;
	msg.hdr.flags = MIX_OP_REQ;
	msg.hdr.req_id = cm->hca->ib_trans.dev_id;
	msg.sp_ctx = (uint64_t)cm->sp;
	msg.sid = sid;
	msg.backlog = 64;

	len = sizeof(dat_mix_listen_t);
	ret = scif_send(mix_ep, &msg, len, SCIF_SEND_BLOCK);
	if (ret != len) {
		dapl_log(1, " ERR: %s msg %p send on %d, ret %d, exp %d, error %s\n",
			 mix_op_str(msg.hdr.op), &msg, mix_ep, ret, len, strerror(errno));
		return -1;
	}
	dapl_log(DAPL_DBG_TYPE_EXTENSION," Sent %s request on SCIF EP %d\n", mix_op_str(msg.hdr.op), mix_ep);

	/* listen response */
	ret = scif_recv(mix_ep, &msg, len, SCIF_RECV_BLOCK);
	if (ret != len) {
		dapl_log(1, " ERR: rcv on new_ep %d, ret %d, exp %d, error %s\n", mix_ep, ret, len, strerror(errno));
		return -1;
	}
	dapl_log(DAPL_DBG_TYPE_EXTENSION," Recv'd %s reply on SCIF EP %d for dev_id %d\n",
		 mix_op_str(msg.hdr.op), mix_ep, msg.hdr.req_id);

	if (msg.hdr.ver != DAT_MIX_VER || msg.hdr.op != MIX_LISTEN ||
	    msg.hdr.flags != MIX_OP_RSP || msg.hdr.status != MIX_SUCCESS) {
		dapl_log(1, " ERR: MIX_LISTEN ver %d, op %s, flgs %d, st %d dev_id %d\n",
			     msg.hdr.ver, mix_op_str(msg.hdr.op),
			     msg.hdr.flags, msg.hdr.status, msg.hdr.req_id);
		if (msg.hdr.status != MIX_SUCCESS)
			return msg.hdr.status;
		else
			return -1;
	}

	dapl_log(DAPL_DBG_TYPE_EXTENSION," MIX_LISTEN successful on SCIF EP %d\n", mix_ep);
	return 0;
}

/* MIX_LISTEN_FREE */
int dapli_mix_listen_free(dp_ib_cm_handle_t cm)
{
	dat_mix_hdr_t msg;
	scif_epd_t mix_ep = cm->hca->ib_trans.scif_ep;
	int ret, len;

	dapl_log(DAPL_DBG_TYPE_EXTENSION," mix_listen_free port 0x%x htons(0x%x), %d\n",
		 (uint16_t)cm->sp->conn_qual, htons((uint16_t)cm->sp->conn_qual),
		 (uint16_t)cm->sp->conn_qual);

	/* listen free request */
	msg.ver = DAT_MIX_VER;
	msg.op = MIX_LISTEN_FREE;
	msg.status = 0;
	msg.flags = MIX_OP_REQ;
	msg.req_id = (uint16_t)cm->sp->conn_qual;

	len = sizeof(dat_mix_hdr_t);
	ret = scif_send(mix_ep, &msg, len, SCIF_SEND_BLOCK);
	if (ret != len) {
		dapl_log(1, " ERR: %s send on %d, ret %d, exp %d, error %s\n",
			 mix_op_str(msg.op), mix_ep, ret, len, strerror(errno));
	}
	dapl_log(DAPL_DBG_TYPE_EXTENSION," Sent %s request on SCIF EP\n", mix_op_str(msg.op));

	/* listen free response */
	ret = scif_recv(mix_ep, &msg, len, SCIF_RECV_BLOCK);
	if (ret != len) {
		dapl_log(1, " ERR: rcv on new_ep %d, ret %d, exp %d, error %s\n", mix_ep, ret, len, strerror(errno));
		return -1;
	}
	if (msg.ver != DAT_MIX_VER || msg.op != MIX_LISTEN_FREE ||
	    msg.flags != MIX_OP_RSP || msg.status != MIX_SUCCESS) {
		dapl_log(1, " MIX_LISTEN_FREE: sid 0x%x, ver %d, op %d, flags %d, or stat %d ERR \n",
			 (uint16_t)cm->sp->conn_qual, msg.ver, msg.op, msg.flags, msg.status);
		return -1;
	}
	dapl_log(DAPL_DBG_TYPE_EXTENSION," received successful reply on SCIF EP\n");
	return 0;
}

/*  MIX_LMR_CREATE */
int dapli_mix_mr_create(ib_hca_transport_t *tp, DAPL_LMR * lmr)
{
	dat_mix_mr_t msg;
	scif_epd_t mix_ep = tp->scif_ep;
	int ret, len;

	dapl_log(DAPL_DBG_TYPE_EXTENSION," lmr create %p, addr %p %p rmr_context %x mr->rkey %x\n",
		 lmr, lmr->mr_handle->addr, lmr->param.registered_address,
		 lmr->param.rmr_context, lmr->mr_handle->rkey );

	/* request: */
	msg.hdr.ver = DAT_MIX_VER;
	msg.hdr.op = MIX_MR_CREATE;
	msg.hdr.status = 0;
	msg.hdr.flags = MIX_OP_REQ;
	msg.hdr.req_id = tp->dev_id;
	msg.mr_id = 0;
	msg.mr_len = lmr->param.registered_size;
	msg.sci_addr = lmr->sci_addr;
	msg.sci_off = lmr->sci_off;
	msg.ib_addr = (uint64_t) lmr->mr_handle->addr;
	msg.ib_rkey = lmr->param.rmr_context;
	msg.ctx = (uint64_t)lmr;


	len = sizeof(dat_mix_mr_t);
	ret = scif_send(mix_ep, &msg, len, SCIF_SEND_BLOCK);
	if (ret != len) {
		dapl_log(1, " ERR: %s send on %d, ret %d, exp %d, error %s\n",
			 mix_op_str(msg.hdr.op), mix_ep, ret, len, strerror(errno));
	}
	dapl_log(DAPL_DBG_TYPE_EXTENSION," Sent %s request on SCIF EP\n", mix_op_str(msg.hdr.op));

	/* response, status and mr_id */
	len = sizeof(dat_mix_mr_t);
	ret = scif_recv(mix_ep, &msg, len, SCIF_RECV_BLOCK);
	if (ret != len) {
		dapl_log(1, " ERR: rcv on new_ep %d, ret %d, exp %d, error %s\n",
			    mix_ep, ret, len, strerror(errno));
		return -1;
	}
	if (msg.hdr.ver != DAT_MIX_VER || msg.hdr.op != MIX_MR_CREATE ||
	    msg.hdr.flags != MIX_OP_RSP || msg.hdr.status != MIX_SUCCESS) {
		dapl_log(1, " MIX msg ver %d, op %d, flags %d, or stat %d ERR \n",
			 msg.hdr.ver, msg.hdr.op, msg.hdr.flags, msg.hdr.status);
		return -1;
	}

	/* save the MPXYD mr_id */
	lmr->mr_id = msg.mr_id;

	dapl_log(DAPL_DBG_TYPE_EXTENSION," lmr_created %p id = %d\n", lmr, lmr->mr_id);
	return 0;
}

/* MIX_LMR_FREE */
int dapli_mix_mr_free(ib_hca_transport_t *tp, DAPL_LMR * lmr)
{
	dat_mix_mr_t msg;
	scif_epd_t mix_ep = tp->scif_ep;
	int ret, len;

	dapl_log(DAPL_DBG_TYPE_EXTENSION," lmr free %p, id=%d\n", lmr, lmr->mr_id);

	/* request */
	msg.hdr.ver = DAT_MIX_VER;
	msg.hdr.op = MIX_MR_FREE;
	msg.hdr.status = 0;
	msg.hdr.flags = MIX_OP_REQ;
	msg.mr_id = lmr->mr_id;

	len = sizeof(dat_mix_mr_t);
	ret = scif_send(mix_ep, &msg, len, SCIF_SEND_BLOCK);
	if (ret != len) {
		dapl_log(1, " ERR: %s send on %d, ret %d, exp %d, error %s\n",
			 mix_op_str(msg.hdr.op), mix_ep, ret, len, strerror(errno));
	}
	dapl_log(DAPL_DBG_TYPE_EXTENSION," Sent %s request on SCIF EP\n", mix_op_str(msg.hdr.op));

	/* response, status only */
	len = sizeof(dat_mix_hdr_t);
	ret = scif_recv(mix_ep, &msg, len, SCIF_RECV_BLOCK);
	if (ret != len) {
		dapl_log(1, " ERR: rcv on new_ep %d, ret %d, exp %d, error %s\n",
			    mix_ep, ret, len, strerror(errno));
		return -1;
	}
	if (msg.hdr.ver != DAT_MIX_VER || msg.hdr.op != MIX_MR_FREE ||
	    msg.hdr.flags != MIX_OP_RSP || msg.hdr.status != MIX_SUCCESS) {
		dapl_log(1, " MIX msg ver %d, op %d, flags %d, or stat %d ERR \n",
			 msg.hdr.ver, msg.hdr.op, msg.hdr.flags, msg.hdr.status);
		return -1;
	}
	dapl_log(DAPL_DBG_TYPE_EXTENSION," removed lmr %p, id %d\n", lmr, lmr->mr_id);
	return 0;
}


/*  MIX_QP_CREATE */
int dapli_mix_qp_create(ib_qp_handle_t m_qp, struct ibv_qp_init_attr *attr,
			ib_cq_handle_t req_cq, ib_cq_handle_t rcv_cq)
{
	dat_mix_qp_t msg;
	scif_epd_t mix_ep = m_qp->tp->scif_ep;
	int ret, len;

	/* request: QP_r local or shadowed, QP_t shadowed */
	msg.hdr.ver = DAT_MIX_VER;
	msg.hdr.op = MIX_QP_CREATE;
	msg.hdr.status = 0;
	msg.hdr.flags = MIX_OP_REQ;
	msg.hdr.req_id = m_qp->tp->dev_id;

	if (m_qp->qp) { 	/* QP_r local */
		msg.qp_r.qp_num = m_qp->qp->qp_num;
		msg.qp_r.qp_type = m_qp->qp->qp_type;
		msg.qp_r.state = m_qp->qp->state;
	} else { 		/* QP_r shadowed on proxy */
		msg.qp_r.qp_num = 0;
		msg.qp_r.qp_type = 0;
		msg.qp_r.state = 0;
	}
	msg.qp_r.rcq_id = rcv_cq->cq_id;
	msg.qp_r.ctx = (uint64_t)m_qp;
	msg.qp_r.qp_id = 0; /* for now */
	msg.qp_r.qp_type = attr->qp_type;
	msg.qp_r.max_recv_wr = attr->cap.max_recv_wr;
	msg.qp_r.max_recv_sge = attr->cap.max_recv_sge;
	msg.qp_r.max_send_wr = attr->cap.max_send_wr;
	msg.qp_r.max_send_sge = attr->cap.max_send_sge;

	msg.qp_t.qp_type = attr->qp_type;
	msg.qp_t.max_inline_data = attr->cap.max_inline_data;
	msg.qp_t.max_send_wr = attr->cap.max_send_wr;
	msg.qp_t.max_send_sge = attr->cap.max_send_sge;
	msg.qp_t.max_recv_wr = attr->cap.max_recv_wr;
	msg.qp_t.max_recv_sge = attr->cap.max_recv_sge;
	msg.qp_t.scq_id = req_cq->cq_id;  /* QP_t always shadowed on proxy */

	dapl_log(DAPL_DBG_TYPE_EXTENSION,
		" MIX_QP_CREATE: QP_r - qpn 0x%x, ctx %p, rq %d,%d sq %d,%d rcq_id %d,%p\n",
		msg.qp_r.qp_num, msg.qp_r.ctx, msg.qp_r.max_recv_wr,
		msg.qp_r.max_recv_sge, msg.qp_r.max_send_wr,
		msg.qp_r.max_send_sge, msg.qp_r.rcq_id, rcv_cq);

	dapl_log(DAPL_DBG_TYPE_EXTENSION,
		" MIX_QP_CREATE: QP_t - wr %d sge %d inline %d scq_id %d,%p\n",
		msg.qp_t.max_send_wr, msg.qp_t.max_send_sge,
		msg.qp_t.max_inline_data, msg.qp_t.scq_id, req_cq);

	len = sizeof(dat_mix_qp_t);
	ret = scif_send(mix_ep, &msg, len, SCIF_SEND_BLOCK);
	if (ret != len) {
		dapl_log(1, " ERR: %s send on %d, ret %d, exp %d, error %s\n",
			 mix_op_str(msg.hdr.op), mix_ep, ret, len, strerror(errno));
		return EFAULT;
	}
	dapl_log(DAPL_DBG_TYPE_EXTENSION," Sent %s request on SCIF EP\n", mix_op_str(msg.hdr.op));

	/* wait for response */
	ret = scif_recv(mix_ep, &msg, len, SCIF_RECV_BLOCK);
	if (ret != len) {
		dapl_log(1, " ERR: rcv on new_ep %d, ret %d, exp %d, error %s\n", mix_ep, ret, len, strerror(errno));
		return EFAULT;
	}
	if (msg.hdr.ver != DAT_MIX_VER || msg.hdr.op != MIX_QP_CREATE ||
	    msg.hdr.flags != MIX_OP_RSP || msg.hdr.status != MIX_SUCCESS) {
		dapl_log(1, " MIX msg ver %d, op %d, flags %d, or stat %d ERR \n",
			 msg.hdr.ver, msg.hdr.op, msg.hdr.flags, msg.hdr.status);
		if (msg.hdr.status)
			return msg.hdr.status;
		else
			return EINVAL;
	}

	/* save QP_t id, QP is shadowed TX */
	m_qp->qp_id = msg.qp_t.qp_id;
	m_qp->m_inline = msg.m_inline;

	dapl_log(DAPL_DBG_TYPE_EXTENSION,
		 " MIX_QP_CREATE: reply, proxy qp_id 0x%x\n", m_qp->qp_id);

	return 0;
}

/* MIX_EP_FREE, fits in header */
int dapli_mix_qp_free(ib_qp_handle_t m_qp)
{
	dat_mix_hdr_t msg;
	scif_epd_t mix_ep = m_qp->tp->scif_ep;
	int ret, len;

	/* request */
	msg.ver = DAT_MIX_VER;
	msg.op = MIX_QP_FREE;
	msg.status = 0;
	msg.flags = MIX_OP_REQ;
	msg.req_id = m_qp->qp_id;  /* shadowed QP */

	len = sizeof(dat_mix_hdr_t);
	ret = scif_send(mix_ep, &msg, len, SCIF_SEND_BLOCK);
	if (ret != len) {
		dapl_log(1, " ERR: %s send on %d, ret %d, exp %d, error %s\n",
			 mix_op_str(msg.op), mix_ep, ret, len, strerror(errno));
	}
	dapl_log(DAPL_DBG_TYPE_EXTENSION," Sent %s request on SCIF EP\n", mix_op_str(msg.op));

	/* response */
	ret = scif_recv(mix_ep, &msg, len, SCIF_RECV_BLOCK);
	if (ret != len) {
		dapl_log(1, " ERR: rcv on new_ep %d, ret %d, exp %d, error %s\n",
			    mix_ep, ret, len, strerror(errno));
		return -1;
	}
	if (msg.ver != DAT_MIX_VER || msg.op != MIX_QP_FREE ||
	    msg.flags != MIX_OP_RSP || msg.status != MIX_SUCCESS) {
		dapl_log(1, " MIX_QP_FREE ERR: ver %d, op %d, flags %d, or stat %d len %d\n",
			 msg.ver, msg.op, msg.flags, msg.status, ret);
		return -1;
	}
	dapl_log(DAPL_DBG_TYPE_EXTENSION," received reply on SCIF EP\n");
	return 0;
}

/*  MIX_CQ_CREATE */
int dapli_mix_cq_create(ib_cq_handle_t m_cq, int cq_len)
{
	dat_mix_cq_t msg;
	scif_epd_t mix_ep = m_cq->tp->scif_ep;
	int ret, len;

	/* request: QP_r local, QP_t shadowed */
	msg.hdr.ver = DAT_MIX_VER;
	msg.hdr.op = MIX_CQ_CREATE;
	msg.hdr.status = 0;
	msg.hdr.flags = MIX_OP_REQ;
	msg.hdr.req_id = m_cq->tp->dev_id;
	msg.cq_len = cq_len;
	msg.cq_ctx = (uint64_t)m_cq;
	msg.cq_id = 0;

	len = sizeof(dat_mix_cq_t);
	ret = scif_send(mix_ep, &msg, len, SCIF_SEND_BLOCK);
	if (ret != len) {
		dapl_log(1, " ERR: %s snd on %d, ret %d, exp %d, err %s\n",
			 mix_op_str(msg.hdr.op), mix_ep, ret, len,
			 strerror(errno));
		return -1;
	}
	dapl_log(DAPL_DBG_TYPE_EXTENSION," Sent %s request on SCIF EP\n",
		 mix_op_str(msg.hdr.op));

	/* wait for response */
	ret = scif_recv(mix_ep, &msg, len, SCIF_RECV_BLOCK);
	if (ret != len) {
		dapl_log(1, " ERR: rcv on ep %d, ret %d, exp %d, err %s\n",
			    mix_ep, ret, len, strerror(errno));
		return -1;
	}
	if (msg.hdr.ver != DAT_MIX_VER || msg.hdr.op != MIX_CQ_CREATE ||
	    msg.hdr.flags != MIX_OP_RSP || msg.hdr.status != MIX_SUCCESS) {
		dapl_log(1, " ERR: %s %p ver %d, op %d, flags %d, stat %d\n",
			    mix_op_str(msg.hdr.op), m_cq, msg.hdr.ver,
			    msg.hdr.op, msg.hdr.flags, msg.hdr.status);
		return -1;
	}

	/* save id from proxy CQ create */
	m_cq->cq_id = msg.cq_id;

	dapl_log(DAPL_DBG_TYPE_EXTENSION,
		 " MIX_CQ_CREATE: reply, proxy cq_id 0x%x\n", m_cq->cq_id);
	return 0;
}

/* MIX_CQ_FREE, fits in header */
int dapli_mix_cq_free(ib_cq_handle_t m_cq)
{
	dat_mix_hdr_t msg;
	scif_epd_t mix_ep = m_cq->tp->scif_ep;
	int ret, len;

	/* request */
	msg.ver = DAT_MIX_VER;
	msg.op = MIX_CQ_FREE;
	msg.status = 0;
	msg.flags = MIX_OP_REQ;
	msg.req_id = m_cq->cq_id;

	len = sizeof(dat_mix_hdr_t);
	ret = scif_send(mix_ep, &msg, len, SCIF_SEND_BLOCK);
	if (ret != len) {
		dapl_log(1, " ERR: %s send on %d, ret %d, exp %d, error %s\n",
			 mix_op_str(msg.op), mix_ep, ret, len, strerror(errno));
	}
	dapl_log(DAPL_DBG_TYPE_EXTENSION," Sent %s request on SCIF EP\n",
		 mix_op_str(msg.op));

	/* response */
	ret = scif_recv(mix_ep, &msg, len, SCIF_RECV_BLOCK);
	if (ret != len) {
		dapl_log(1, " ERR: rcv on new_ep %d, ret %d, exp %d, error %s\n",
			    mix_ep, ret, len, strerror(errno));
		return -1;
	}
	if (msg.ver != DAT_MIX_VER || msg.op != MIX_CQ_FREE ||
	    msg.flags != MIX_OP_RSP || msg.status != MIX_SUCCESS) {
		dapl_log(1, " MIX_CQ_FREE ERR: ver %d, op %d, flags %d, or stat %d ln %d\n",
			 msg.ver, msg.op, msg.flags, msg.status, ret);
		return -1;
	}
	dapl_log(DAPL_DBG_TYPE_EXTENSION,
		 " MIX_CQ_FREE: reply, proxy cq_id 0x%x\n", m_cq->cq_id);
	return 0;
}

int dapli_mix_cq_poll(ib_cq_handle_t m_cq, struct ibv_wc *wc)
{
	/* MPXYD will send event and update EVD, return empty to avoid unnecessary SCIF traffic */
	return 0;
}

/* SCIF DMA outbound writes and inbound msg receives; translate to scif_off via LMR */
/* TODO: faster translation for post_send? */
static inline int mix_proxy_data(ib_qp_handle_t m_qp, dat_mix_sr_t *msg, struct ibv_sge *sglist, int txlen, int mix_ep)
{
	off_t l_off;
	uint64_t addr;
	struct dapl_lmr *lmr = NULL;
	int i, len;

	for (i=0; i < msg->wr.num_sge ; i++) {
		dapl_log(DAPL_DBG_TYPE_EXTENSION, " mix_proxy_data: post_%s: sge[%d] addr %p, len %d\n",
			 msg->wr.opcode == OP_RECEIVE ? "recv":"send",
			 i, sglist[i].addr, sglist[i].length);

		/* find LMR with lkey to get scif_off for scif_read_from */
		l_off = 0;

		if (!lmr || (lmr && (lmr->mr_handle->lkey != sglist[i].lkey)))
			lmr = dapl_llist_peek_head(&m_qp->ep->header.owner_ia->lmr_list_head);

		while (lmr) {
			if (lmr->mr_handle->lkey == sglist[i].lkey) {
				len = sglist[i].length;
				addr = sglist[i].addr;
				l_off = lmr->sci_addr + lmr->sci_off + (addr - lmr->param.registered_address);
				dapl_log(DAPL_DBG_TYPE_EXTENSION,
					 " mix_proxy_data: LMR (%p) lkey %x sci_addr %p off %x l_off %p addr %p len %d\n",
					 lmr, lmr->mr_handle->lkey, lmr->sci_addr, lmr->sci_off, l_off, addr, len);
				break;
			}
			lmr = dapl_llist_next_entry(&lmr->header.owner_ia->lmr_list_head,
						    &lmr->header.ia_list_entry);
		}
		if (l_off) {
			msg->sge[i].length = len;
			msg->sge[i].addr = l_off;
			msg->sge[i].lkey = 0;
		} else
			return -1; /* no translation */
	}
	dapl_log(DAPL_DBG_TYPE_EXTENSION," mix_proxy_data: return \n");
	return 0;
}

/**** speed path ****/
int dapli_mix_post_send(ib_qp_handle_t m_qp, int txlen, struct ibv_send_wr *wr, struct ibv_send_wr **bad_wr)
{
	char cmd[DAT_MIX_MSG_MAX + DAT_MIX_INLINE_MAX];
	dat_mix_sr_t *msg = (dat_mix_sr_t *)cmd;
	scif_epd_t mix_ep = m_qp->tp->scif_ep;
	int ret, i, offset = sizeof(dat_mix_sr_t);

	dapl_log(DAPL_DBG_TYPE_EXTENSION,
		" mix_post_send: msg=%p sge=%d len=%d op=%d off=%d (%p)raddr %Lx rkey 0x%x, wr_id %LX\n",
		 msg, wr->num_sge, txlen, wr->opcode, offset, &wr->wr.rdma.remote_addr,
		 wr->wr.rdma.remote_addr, wr->wr.rdma.rkey, wr->wr_id);

	if (wr->opcode != IBV_WR_SEND &&
	    wr->opcode != IBV_WR_RDMA_WRITE &&
	    wr->opcode != IBV_WR_RDMA_WRITE_WITH_IMM)
		return EINVAL;

	msg->hdr.ver = DAT_MIX_VER;
	msg->hdr.op = MIX_SEND;
	msg->hdr.status = 0;
	msg->hdr.flags = MIX_OP_REQ;
	msg->hdr.req_id = m_qp->tp->dev_id;
	msg->len = txlen;
	msg->qp_id = m_qp->qp_id;
	mcm_const_mix_wr(&msg->wr, wr);

	if (txlen > m_qp->m_inline) {
		if (mix_proxy_data(m_qp, msg, wr->sg_list, txlen, mix_ep))
			return EINVAL;
	} else {
		msg->hdr.flags |= MIX_OP_INLINE;
		for (i=0; i < wr->num_sge; i++) {
			memcpy(&cmd[offset], (void*)wr->sg_list[i].addr, wr->sg_list[i].length);
			offset += wr->sg_list[i].length;
		}
	}

	ret = scif_send(mix_ep, msg, offset, SCIF_SEND_BLOCK);
	if (ret != offset) {
		dapl_log(1, " ERR: %s on %d, ret %d, exp %d, error %s\n",
			 mix_op_str(msg->hdr.op), mix_ep, ret,
			 offset, strerror(errno));
		return -1;
	}

	dapl_log(DAPL_DBG_TYPE_EXTENSION," Sent MIX_SEND on SCIF EP %d, mlen=%d\n", mix_ep, offset);
	return 0;
}

int dapli_mix_post_recv(ib_qp_handle_t m_qp, int len, struct ibv_recv_wr *wr, struct ibv_recv_wr **bad_wr)
{
	char cmd[DAT_MIX_MSG_MAX + DAT_MIX_INLINE_MAX];
	dat_mix_sr_t *msg = (dat_mix_sr_t *)cmd;
	scif_epd_t mix_ep = m_qp->tp->scif_ep;
	int ret;

	dapl_log(DAPL_DBG_TYPE_EXTENSION,
		" mix_post_recv: msg=%p sge=%d len=%d wr_id %LX, addr %p lkey 0x%x\n",
		 msg, wr->num_sge, len, wr->wr_id, wr->sg_list[0].addr, wr->sg_list[0].lkey);

	if (wr->num_sge > DAT_MIX_SGE_MAX)
		return EINVAL;

	msg->hdr.ver = DAT_MIX_VER;
	msg->hdr.op = MIX_RECV;
	msg->hdr.status = 0;
	msg->hdr.flags = MIX_OP_REQ;
	msg->hdr.req_id = m_qp->tp->dev_id;
	msg->len = len;
	msg->qp_id = m_qp->qp_id; /* shadowed RX */

	/* setup work request */
	memset((void*)&msg->wr, 0, sizeof(dat_mix_wr_t));
	msg->wr.opcode = OP_RECEIVE;
	msg->wr.wr_id = wr->wr_id;
	msg->wr.num_sge = wr->num_sge;

	if (mix_proxy_data(m_qp, msg, wr->sg_list, len, mix_ep))
		return EINVAL;

	ret = scif_send(mix_ep, msg, sizeof(dat_mix_sr_t), SCIF_SEND_BLOCK);
	if (ret != sizeof(dat_mix_sr_t)) {
		dapl_log(1, " ERR: %s on %d, ret %d, exp %d, error %s\n",
			 mix_op_str(msg->hdr.op), mix_ep, ret,
			 sizeof(dat_mix_sr_t), strerror(errno));
		return -1;
	}

	dapl_log(DAPL_DBG_TYPE_EXTENSION," Sent MIX_RECV on SCIF EP %d, mlen=%d\n", mix_ep, sizeof(dat_mix_sr_t));
	return 0;
}


/* MIX CM operations:
 *
 *  Event/CM channel (scif_ev_ep) for events and CM messages
 *  	This channel is used via CM Thread context, separate from user thread context for OPs
 *      Separate EP's per thread too avoid locking overhead on SCIF streams
 */

dp_ib_cm_handle_t dapli_mix_get_cm(ib_hca_transport_t *tp, uint64_t cm_ctx)
{
	dp_ib_cm_handle_t cm = NULL;

	dapl_os_lock(&tp->lock);
	if (!dapl_llist_is_empty(&tp->list))
		cm = dapl_llist_peek_head(&tp->list);

	while (cm) {
		if (cm == (void*)cm_ctx)
			break;

		cm = dapl_llist_next_entry(&tp->list, &cm->local_entry);
	}
	dapl_os_unlock(&tp->lock);
	return cm;
}

/* CM_REP operation, user context, op channel */
int dapli_mix_cm_rep_out(dp_ib_cm_handle_t m_cm, int p_size, void *p_data)
{
	dat_mix_cm_t msg;
	scif_epd_t mix_ep = m_cm->tp->scif_ep; /* op channel */
	int ret, len;

	/* request: QP_r local, QP_t shadowed */
	msg.hdr.ver = DAT_MIX_VER;
	msg.hdr.op = MIX_CM_ACCEPT;
	msg.hdr.status = 0;
	msg.hdr.flags = MIX_OP_REQ;
	msg.hdr.req_id = m_cm->tp->dev_id;
	msg.qp_id = m_cm->ep->qp_handle->qp_id; /* QP2 shadowed TX */
	msg.cm_id = m_cm->cm_id;
	msg.cm_ctx = (uint64_t)m_cm->cm_ctx;
	msg.sp_ctx = (uint64_t)m_cm; /* send back my cm_ctx */
	memcpy(&msg.msg, &m_cm->msg, sizeof(dat_mcm_msg_t));
	memcpy(msg.msg.p_data, p_data, p_size);
	msg.msg.p_size = htons(p_size);

	dapl_log(DAPL_DBG_TYPE_EXTENSION,
		 " ACCEPT -> dport %x cqpn %x iqpn %x lid %x, psize %d pdata[0]=%x\n",
		 ntohs(msg.msg.dport), ntohl(msg.msg.dqpn),
		 ntohl(msg.msg.daddr1.qpn), ntohs(msg.msg.daddr1.lid),
		 p_size, msg.msg.p_data[0]);

	len = sizeof(dat_mix_cm_t);
	ret = scif_send(mix_ep, &msg, len, SCIF_SEND_BLOCK);
	if (ret != len) {
		dapl_log(1, " ERR: %s send on %d, ret %d, exp %d, error %s\n",
			 mix_op_str(msg.hdr.op), mix_ep, ret, len, strerror(errno));
		return -1;
	}
	dapl_log(DAPL_DBG_TYPE_EXTENSION," Sent %s request on SCIF EP\n", mix_op_str(msg.hdr.op));

	/* no reply */
	return 0;
}

/* CM_REJ message, user or cm_thread context, locking required */
int dapli_mix_cm_rej_out(dp_ib_cm_handle_t m_cm, int p_size, void *p_data, int reason)
{
	dat_mix_cm_t msg;
	scif_epd_t mix_ep = m_cm->tp->scif_ev_ep; /* CM,EV channel */
	int ret, len;

	/* request: QP_r local, QP_t shadowed */
	msg.hdr.ver = DAT_MIX_VER;
	msg.hdr.op = MIX_CM_REJECT;
	msg.hdr.status = 0;
	msg.hdr.flags = MIX_OP_REQ;
	msg.hdr.req_id = m_cm->tp->dev_id;
	msg.cm_id = m_cm->cm_id;
	msg.cm_ctx = (uint64_t)m_cm->cm_ctx;
	memcpy(&msg.msg, &m_cm->msg, sizeof(dat_mcm_msg_t));
	memcpy(msg.msg.p_data, p_data, p_size);
	msg.msg.p_size = htons(p_size);

	if (reason == IB_CM_REJ_REASON_CONSUMER_REJ)  /* setup op in CM message */
		msg.msg.op = htons(MCM_REJ_USER);
	else
		msg.msg.op = htons(MCM_REJ_CM);

	msg.msg.saddr1.lid = m_cm->hca->ib_trans.addr.lid;
	msg.msg.saddr1.qp_type = m_cm->msg.daddr1.qp_type;
	dapl_os_memcpy(&msg.msg.saddr1.gid[0], &m_cm->hca->ib_trans.addr.gid, 16);

	dapl_log(DAPL_DBG_TYPE_EXTENSION," REJECT -> dport 0x%x, dqpn 0x%x dlid 0x%x, reason %d, psize %d\n",
		 ntohs(msg.msg.dport), ntohl(msg.msg.dqpn), ntohs(msg.msg.daddr1.lid), reason, p_size );

	len = sizeof(dat_mix_cm_t);
	dapl_os_lock(&m_cm->tp->lock);
	ret = scif_send(mix_ep, &msg, len, SCIF_SEND_BLOCK);
	dapl_os_unlock(&m_cm->tp->lock);
	if (ret != len) {
		dapl_log(1, " ERR: %s send on %d, ret %d, exp %d, error %s\n",
			 mix_op_str(msg.hdr.op), mix_ep, ret, len, strerror(errno));
		return -1;
	}
	dapl_log(DAPL_DBG_TYPE_EXTENSION," Sent %s request on SCIF EP\n", mix_op_str(msg.hdr.op));

	/* no reply */
	return 0;
}

/*  MIX_CM_REQ operation, user context, op channel */
int dapli_mix_cm_req_out(dp_ib_cm_handle_t m_cm, ib_qp_handle_t m_qp)
{
	dat_mix_cm_t msg;
	scif_epd_t mix_ep = m_cm->tp->scif_ep; /* use operation channel */
	int ret, len;

	/* request: QP_r local, QP_t shadowed */
	msg.hdr.ver = DAT_MIX_VER;
	msg.hdr.op = MIX_CM_REQ;
	msg.hdr.status = 0;
	msg.hdr.flags = MIX_OP_REQ;
	msg.hdr.req_id = m_cm->tp->dev_id;
	msg.qp_id = m_qp->qp_id; /* shadowed TX */
	msg.cm_id = m_cm->cm_id;
	msg.cm_ctx = (uint64_t)m_cm;
	memcpy(&msg.msg, &m_cm->msg, sizeof(dat_mcm_msg_t));

	dapl_log(DAPL_DBG_TYPE_EXTENSION," -> dport 0x%x, dqpn 0x%x dlid 0x%x ep_map %s\n",
		 ntohs(msg.msg.dport), ntohl(msg.msg.dqpn), ntohs(msg.msg.daddr1.lid),
		 mcm_map_str(msg.msg.daddr1.ep_map));

	len = sizeof(dat_mix_cm_t);
	ret = scif_send(mix_ep, &msg, len, SCIF_SEND_BLOCK);
	if (ret != len) {
		dapl_log(1, " ERR: %s send on %d, ret %d, exp %d, error %s\n",
			 mix_op_str(msg.hdr.op), mix_ep, ret, len, strerror(errno));
	}
	dapl_log(DAPL_DBG_TYPE_EXTENSION," Sent %s request on SCIF EP\n", mix_op_str(msg.hdr.op));

	/* wait for response */
	ret = scif_recv(mix_ep, &msg, len, SCIF_RECV_BLOCK);
	if (ret != len) {
		dapl_log(1, " ERR: req_out rcv ep %d, ret %d, exp %d, err %s\n",
			    mix_ep, ret, len, strerror(errno));
		return -1;
	}
	if (msg.hdr.ver != DAT_MIX_VER || msg.hdr.op != MIX_CM_REQ ||
	    msg.hdr.flags != MIX_OP_RSP || msg.hdr.status != MIX_SUCCESS) {
		dapl_log(1, " MIX msg ver %d, op %s, flags %d, or stat %d ERR \n",
			 msg.hdr.ver, mix_op_str(msg.hdr.op), msg.hdr.flags, msg.hdr.status);
		return -1;
	}

	/* CM object linking:  MIC to MPXYD */
	m_cm->scm_id = msg.cm_id;
	m_cm->scm_ctx = msg.cm_ctx;

	dapl_log(DAPL_DBG_TYPE_EXTENSION," reply on SCIF EP -> cm_id 0x%x, ctx %p\n",
		m_cm->scm_id, (void*)m_cm->scm_ctx );

	return 0;
}

/*  MIX_CM_RTU message, cm_thread context, use EV/CM channel, lock snd channel */
int dapli_mix_cm_rtu_out(dp_ib_cm_handle_t m_cm)
{
	dat_mix_cm_t msg;
	scif_epd_t mix_ep = m_cm->tp->scif_ev_ep;
	int ret, len;

	/* connect RTU: QP_r local, QP_t shadowed */
	msg.hdr.ver = DAT_MIX_VER;
	msg.hdr.op = MIX_CM_RTU;
	msg.hdr.status = 0;
	msg.hdr.flags = MIX_OP_REQ;
	msg.hdr.req_id = m_cm->tp->dev_id;
	msg.cm_id = m_cm->scm_id;
	msg.cm_ctx = (uint64_t)m_cm;

	dapl_log(DAPL_DBG_TYPE_EXTENSION," RTU -> id 0x%x dport 0x%x, dqpn 0x%x dlid 0x%x\n",
		 msg.cm_id, ntohs(msg.msg.dport), ntohl(msg.msg.dqpn), ntohs(msg.msg.daddr1.lid));

	len = sizeof(dat_mix_cm_t);
	dapl_os_lock(&m_cm->tp->lock);
	ret = scif_send(mix_ep, &msg, len, SCIF_SEND_BLOCK);
	dapl_os_unlock(&m_cm->tp->lock);
	if (ret != len) {
		dapl_log(1, " ERR: %s send on %d, ret %d, exp %d, error %s\n",
			 mix_op_str(msg.hdr.op), mix_ep, ret, len, strerror(errno));
		return -1;
	}
	dapl_log(DAPL_DBG_TYPE_EXTENSION," Sent %s request on SCIF EP\n", mix_op_str(msg.hdr.op));
	return 0;
}

/*  MIX_CM_DREQ operation, user context, op channel */
void dapli_mix_cm_dreq_out(dp_ib_cm_handle_t m_cm) {

	dat_mix_cm_t msg;
	scif_epd_t mix_ep = m_cm->tp->scif_ep; /* operation channel */
	int ret, len;

	/* disconnect request out */
	msg.hdr.ver = DAT_MIX_VER;
	msg.hdr.op = MIX_CM_DISC;
	msg.hdr.status = 0;
	msg.hdr.flags = MIX_OP_REQ;
	msg.hdr.req_id = m_cm->tp->dev_id;
	msg.cm_id = m_cm->scm_id;
	msg.cm_ctx = (uint64_t)m_cm;

	dapl_log(DAPL_DBG_TYPE_EXTENSION," DREQ -> id 0x%x dport 0x%x, dqpn 0x%x dlid 0x%x\n",
		 msg.cm_id, ntohs(msg.msg.dport), ntohl(msg.msg.dqpn), ntohs(msg.msg.daddr1.lid) );

	len = sizeof(dat_mix_cm_t);
	ret = scif_send(mix_ep, &msg, len, SCIF_SEND_BLOCK);
	if (ret != len) {
		dapl_log(DAPL_DBG_TYPE_CM_WARN,
			 " ERR: %s send on %d, ret %d, exp %d, error %s\n",
			 mix_op_str(msg.hdr.op), mix_ep, ret, len, strerror(errno));
	}
	dapl_log(DAPL_DBG_TYPE_EXTENSION," Sent %s request on SCIF EP\n", mix_op_str(msg.hdr.op));
}

/* unsolicited CM event, scif_ep channel */
int dapli_mix_cm_event_in(ib_hca_transport_t *tp, scif_epd_t scif_ep, dat_mix_cm_event_t *pmsg)
{
	int len, ret;
	dp_ib_cm_handle_t cm;

	/* hdr already read, get operation data */
	len = sizeof(dat_mix_cm_event_t) - sizeof(dat_mix_hdr_t);
	ret = scif_recv(scif_ep, ((char*)pmsg + sizeof(dat_mix_hdr_t)), len, SCIF_RECV_BLOCK);
	if (ret != len) {
		dapl_log(DAPL_DBG_TYPE_ERR, " ERR: ret %d, exp %d, error %s\n", ret, len, strerror(errno));
		return ret;
	}
	dapl_log(DAPL_DBG_TYPE_EXTENSION,
		 " MIX_CM_EVENT <-: id %d ctx %p event 0x%x\n",
		 pmsg->cm_id, pmsg->cm_ctx, pmsg->event);

	/* Find the CM and EP for event processing */
	cm = dapli_mix_get_cm(tp, pmsg->cm_ctx);
	if (!cm) {
		dapl_log(DAPL_DBG_TYPE_EXTENSION, " mcm_get_cm, ctx %p, not found\n", pmsg->cm_ctx);
		return 0;
	}

	switch (pmsg->event) {
	case DAT_CONNECTION_EVENT_TIMED_OUT:
		if (cm->sp)
			dapls_cr_callback(cm, IB_CME_LOCAL_FAILURE, NULL, 0, cm->sp);
		else
			dapl_evd_connection_callback(cm, IB_CME_DESTINATION_UNREACHABLE, NULL, 0, cm->ep);

		break;

	case DAT_CONNECTION_EVENT_ESTABLISHED:
	case DAT_CONNECTION_REQUEST_EVENT:
	case DAT_DTO_COMPLETION_EVENT:
	case DAT_CONNECTION_EVENT_PEER_REJECTED:
	case DAT_CONNECTION_EVENT_NON_PEER_REJECTED:
	case DAT_CONNECTION_EVENT_ACCEPT_COMPLETION_ERROR:
	case DAT_CONNECTION_EVENT_DISCONNECTED:
		mcm_disconnect_final(cm);
		break;
	case DAT_CONNECTION_EVENT_BROKEN:
	case DAT_CONNECTION_EVENT_UNREACHABLE:

	default:
		break;
	}

	return 0;
}

/* unsolicited DTO event, op channel */
int dapli_mix_dto_event_in(ib_hca_transport_t *tp, scif_epd_t scif_ep, dat_mix_dto_comp_t *pmsg)
{
	int len, ret, i;
	struct dcm_ib_cq  *m_cq;
	DAPL_COOKIE *cookie;

	/* hdr already read, get operation data */
	len = sizeof(dat_mix_dto_comp_t) - sizeof(dat_mix_hdr_t);
	ret = scif_recv(scif_ep, ((char*)pmsg + sizeof(dat_mix_hdr_t)), len, SCIF_RECV_BLOCK);
	if (ret != len) {
		dapl_log(DAPL_DBG_TYPE_ERR, " ERR: ret %d, exp %d, error %s\n", ret, len, strerror(errno));
		return ret;
	}

	/* Get cq and post DTO event with this WC entry */
	m_cq = (void*)pmsg->cq_ctx;

	for (i=0; i<pmsg->wc_cnt; i++) {
		struct ibv_wc ib_wc;
		/* possible segmentation on mpxyd side, update length if success */
		if (pmsg->wc[i].status == 0) {
			cookie = (DAPL_COOKIE *) (uintptr_t) pmsg->wc[i].wr_id;
			if (!cookie) {
				dapl_log(DAPL_DBG_TYPE_ERR,
					 " ERR: mix_dto_event_in: wr_id=0 wc[%d] cnt %d\n",
					 i, pmsg->wc_cnt);
				return 0;
			}
			pmsg->wc[i].byte_len = cookie->val.dto.size;
			dapl_log(DAPL_DBG_TYPE_EP,
				 " mix_dto_event: MCM evd %p ep %p wr_id=%Lx ln=%d\n",
				 m_cq->evd, cookie->ep, pmsg->wc[i].wr_id,
				 cookie->val.dto.size);
		}
		mcm_const_ib_wc(&ib_wc, &pmsg->wc[i], 1);
		dapl_os_lock(&m_cq->evd->header.lock);
		dapls_evd_cqe_to_event(m_cq->evd, &ib_wc);
		dapl_os_unlock(&m_cq->evd->header.lock);
	}

	return 0;
}

int dapli_mix_cm_rep_in(ib_hca_transport_t *tp, scif_epd_t scif_ep, dat_mix_cm_t *pmsg)
{
	int len, ret;
	dp_ib_cm_handle_t cm;

	/* hdr already read, get operation data */
	len = sizeof(dat_mix_cm_t) - sizeof(dat_mix_hdr_t);
	ret = scif_recv(scif_ep, ((char*)pmsg + sizeof(dat_mix_hdr_t)), len, SCIF_RECV_BLOCK);
	if (ret != len) {
		dapl_log(DAPL_DBG_TYPE_ERR, " ERR: ret %d, exp %d, error %s\n", ret, len, strerror(errno));
		return ret;
	}
	dapl_log(DAPL_DBG_TYPE_EXTENSION,
		 " MIX_CM_REP_IN <-: id %d ctx %p \n", pmsg->cm_id, pmsg->cm_ctx);

	/* Find the CM and EP for event processing */
	cm = dapli_mix_get_cm(tp, pmsg->cm_ctx);
	if (!cm) {
		dapl_log(DAPL_DBG_TYPE_ERR, " ERR: mcm_get_cm, ctx %p, not found\n", pmsg->cm_ctx);
		return -1;
	}

	mcm_connect_rtu(cm, &pmsg->msg);
	return 0;
}

int dapli_mix_cm_req_in(ib_hca_transport_t *tp, scif_epd_t scif_ep, dat_mix_cm_t *pmsg)
{
	int len, ret;
	dp_ib_cm_handle_t acm;

	/* hdr already read, get operation data */
	len = sizeof(dat_mix_cm_t) - sizeof(dat_mix_hdr_t);
	ret = scif_recv(scif_ep, ((char*)pmsg + sizeof(dat_mix_hdr_t)), len, SCIF_RECV_BLOCK);
	if (ret != len) {
		dapl_log(DAPL_DBG_TYPE_ERR, " ERR: ret %d, exp %d, error %s\n", ret, len, strerror(errno));
		return ret;
	}

	/* Allocate client CM and setup passive references */
	if ((acm = dapls_cm_create(tp->hca, NULL)) == NULL) {
		dapl_log(DAPL_DBG_TYPE_WARN, " mix_cm_req_in: ERR cm_create\n");
		return -1;
	}

	acm->sp = (DAPL_SP *)pmsg->sp_ctx;
	acm->cm_id = pmsg->cm_id;
	acm->cm_ctx = pmsg->cm_ctx;
	memcpy(&acm->msg, &pmsg->msg, sizeof(dat_mcm_msg_t));

	dapl_log(DAPL_DBG_TYPE_EXTENSION,
		 " MIX_CM_REQ_IN <- DST port=%x lid=%x, QPr=%x, QPt=%x, psize=%d r_guid=%Lx\n",
		 ntohs(acm->msg.dport), ntohs(acm->msg.daddr1.lid),
		 htonl(acm->msg.daddr1.qpn), htonl(acm->msg.daddr2.qpn),
		 htons(acm->msg.p_size), htonll(acm->msg.sys_guid));

	dapl_log(DAPL_DBG_TYPE_EXTENSION,
		 " MIX_CM_REQ_IN <-: sp %p new_cm %p pxy_id %d pxy_ctx %p psize %d pdata[0]=0x%x\n",
		 acm->sp, acm, pmsg->cm_id, pmsg->cm_ctx, ntohs(acm->msg.p_size),
		 acm->msg.p_data[0]);

	/* trigger CR event */
	acm->state = MCM_ACCEPTING;
	dapli_queue_conn(acm);
	dapls_cr_callback(acm, IB_CME_CONNECTION_REQUEST_PENDING,
			  acm->msg.p_data, ntohs(acm->msg.p_size), acm->sp);
	return 0;
}

int dapli_mix_cm_rtu_in(ib_hca_transport_t *tp, scif_epd_t scif_ep, dat_mix_cm_t *pmsg)
{
	int len, ret;
	dp_ib_cm_handle_t cm;

	/* hdr already read, get operation data */
	len = sizeof(dat_mix_cm_t) - sizeof(dat_mix_hdr_t);
	ret = scif_recv(scif_ep, ((char*)pmsg + sizeof(dat_mix_hdr_t)), len, SCIF_RECV_BLOCK);
	if (ret != len) {
		dapl_log(DAPL_DBG_TYPE_ERR, " ERR: ret %d, exp %d, error %s\n", ret, len, strerror(errno));
		return ret;
	}
	dapl_log(DAPL_DBG_TYPE_EXTENSION,
		 " MIX_CM_RTU_IN <-: id %d ctx %p \n", pmsg->cm_id, pmsg->cm_ctx);

	/* Find the CM and EP for event processing */
	cm = dapli_mix_get_cm(tp, pmsg->cm_ctx);
	if (!cm) {
		dapl_log(DAPL_DBG_TYPE_ERR, " ERR: mcm_get_cm, ctx %p, not found\n", pmsg->cm_ctx);
		return -1;
	}

	dapl_os_lock(&cm->lock);
	cm->state = MCM_CONNECTED;
	dapl_os_unlock(&cm->lock);

	dapls_cr_callback(cm, IB_CME_CONNECTED, NULL, 0, cm->sp);
	return 0;
}

int dapli_mix_cm_rej_in(ib_hca_transport_t *tp, scif_epd_t scif_ep, dat_mix_cm_t *pmsg)
{
	int len, ret, event;
	dp_ib_cm_handle_t cm;

	/* hdr already read, get operation data */
	len = sizeof(dat_mix_cm_t) - sizeof(dat_mix_hdr_t);
	ret = scif_recv(scif_ep, ((char*)pmsg + sizeof(dat_mix_hdr_t)), len, SCIF_RECV_BLOCK);
	if (ret != len) {
		dapl_log(DAPL_DBG_TYPE_ERR, " MCM_REJ_IN ERR: %d exp %d, scif_recv err %s\n",
			 ret, len, strerror(errno));
		return ret;
	}

	/* Find the CM and EP for event processing */
	cm = dapli_mix_get_cm(tp, pmsg->cm_ctx);
	if (!cm) {
		dapl_log(1, " ERR: mcm_get_cm, ctx %p, not found\n", pmsg->cm_ctx);
		return -1;
	}
	memcpy(&cm->msg, &pmsg->msg, sizeof(dat_mcm_msg_t));

	dapl_log(DAPL_DBG_TYPE_CM_WARN,
		 " MCM_REJ_IN%s <- p_msg %p id %d cm %p (%d) ep %p %s\n",
		 pmsg->hdr.op == MIX_CM_REJECT_USER ? "_USER":"",
		 pmsg, pmsg->cm_id, pmsg->cm_ctx, cm->ref_count, cm->ep,
		 dapl_cm_state_str(cm->state));

	if (pmsg->hdr.op == MIX_CM_REJECT_USER)
		event = IB_CME_DESTINATION_REJECT_PRIVATE_DATA;
	else
		event = IB_CME_DESTINATION_REJECT;

	dapl_os_lock(&cm->lock);
	cm->state = MCM_REJECTED;
	dapl_os_unlock(&cm->lock);

	if (cm->sp)
		dapls_cr_callback(cm, event, NULL, 0, cm->sp);
	else
		dapl_evd_connection_callback(cm, event,
					     cm->msg.p_data,
					     ntohs(cm->msg.p_size), cm->ep);
	return 0;
}


/*
 * MIX recv, unsolicited messages from MPXYD, via scif_ev_ep - CM/EV endpoint
 *
 */
int dapli_mix_recv(DAPL_HCA *hca, int scif_ep)
{
	char cmd[DAT_MIX_MSG_MAX + DAT_MIX_INLINE_MAX];
	dat_mix_hdr_t *phdr = (dat_mix_hdr_t *)cmd;
	ib_hca_transport_t *tp = &hca->ib_trans;
	int ret, len;

	len = sizeof(dat_mix_hdr_t);
	ret = scif_recv(scif_ep, phdr, len, SCIF_RECV_BLOCK);
	if ((ret != len) || (phdr->ver != DAT_MIX_VER) || phdr->flags != MIX_OP_REQ) {
		dapl_log(DAPL_DBG_TYPE_EXCEPTION,
			 " ERR: rcv on scif_ep %d, ret %d, exp %d, VER=%d flgs=%d, error %s\n",
			 scif_ep, ret, len, phdr->ver, phdr->flags, strerror(errno));
		return -1;
	}
	dapl_log(DAPL_DBG_TYPE_EXTENSION, " ver %d, op %d, flags %d\n", phdr->ver, phdr->op, phdr->flags);

	switch (phdr->op) {
	case MIX_DTO_EVENT:
		ret = dapli_mix_dto_event_in(tp, scif_ep, (dat_mix_dto_comp_t*)phdr);
		break;
	case MIX_CM_EVENT:
		ret = dapli_mix_cm_event_in(tp, scif_ep, (dat_mix_cm_event_t*)phdr);
		break;
	case MIX_CM_REQ:
		ret = dapli_mix_cm_req_in(tp, scif_ep, (dat_mix_cm_t*)phdr);
		break;
	case MIX_CM_REP:
		ret = dapli_mix_cm_rep_in(tp, scif_ep, (dat_mix_cm_t*)phdr);
		break;
	case MIX_CM_REJECT:
	case MIX_CM_REJECT_USER:
		ret = dapli_mix_cm_rej_in(tp, scif_ep, (dat_mix_cm_t*)phdr);
		break;
	case MIX_CM_RTU:
		ret = dapli_mix_cm_rtu_in(tp, scif_ep, (dat_mix_cm_t*)phdr);
		break;
	case MIX_CM_EST:
	case MIX_CM_DISC:
	case MIX_CM_DREP:
		break;
	default:
		dapl_log(DAPL_DBG_TYPE_ERR, " ERROR!!! unknown MIX operation: %d\n", phdr->op);
		return -1;
	}
	return ret;
}












