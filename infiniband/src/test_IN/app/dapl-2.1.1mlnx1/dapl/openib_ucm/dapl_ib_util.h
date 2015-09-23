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

#ifndef _DAPL_IB_UTIL_H_
#define _DAPL_IB_UTIL_H_
#define _OPENIB_UCM_

#include <infiniband/verbs.h>
#include "openib_osd.h"
#include "dapl_ib_common.h"

/* DAPL CM objects MUST include list_entry, ref_count, event for EP linking */
struct ib_cm_handle
{ 
	struct dapl_llist_entry	list_entry;
	struct dapl_llist_entry	local_entry;
	DAPL_OS_WAIT_OBJECT	d_event;
	DAPL_OS_WAIT_OBJECT	f_event;
	DAPL_OS_LOCK		lock;
	DAPL_OS_TIMEVAL		timer;
        int			ref_count;
	int			state;
	int			retries;
	struct dapl_hca		*hca;
	struct dapl_sp		*sp;	
	struct dapl_ep 		*ep;
	struct dapl_cr 		*cr;
	struct ibv_ah		*ah;
	uint16_t		p_size; /* accept p_data, for retries */
	uint8_t			p_data[DCM_MAX_PDATA_SIZE];
	ib_cm_msg_t		msg;
};

typedef struct ib_cm_handle	*dp_ib_cm_handle_t;
typedef dp_ib_cm_handle_t	ib_cm_srvc_handle_t;

/* Definitions */
#define IB_INVALID_HANDLE	NULL

/* ib_hca_transport_t, specific to this implementation */
typedef struct _ib_hca_transport
{ 
	struct	ibv_device	*ib_dev;
	struct	dapl_hca	*hca;
        struct  ibv_context     *ib_ctx;
        struct ibv_comp_channel *ib_cq;
        ib_cq_handle_t          ib_cq_empty;
	int			destroy;
	int			cm_state;
	DAPL_OS_THREAD		thread;
	DAPL_OS_LOCK		lock;	/* connect list */
	struct dapl_llist_entry	*list;	
	DAPL_OS_LOCK		llock;	/* listen list */
	struct dapl_llist_entry	*llist;	
	ib_async_handler_t	async_unafiliated;
	void			*async_un_ctx;
	ib_async_cq_handler_t	async_cq_error;
	ib_async_dto_handler_t	async_cq;
	ib_async_qp_handler_t	async_qp_error;
	union dcm_addr		addr;	/* lid, port, qp_num, gid */
	int			max_inline_send;
	int			rd_atom_in;
	int			rd_atom_out;
	uint8_t			ack_timer;
	uint8_t			ack_retry;
	uint8_t			rnr_timer;
	uint8_t			rnr_retry;
	uint8_t			global;
	uint8_t			hop_limit;
	uint8_t			tclass;
	uint8_t			mtu;
	struct dapl_thread_signal signal;
	int			cqe;
	int			qpe;
	int			burst;
	int			retries;
	int			cm_timer;
	int			rep_time;
	int			rtu_time;
	DAPL_OS_LOCK		slock;	
	int			s_hd;
	int			s_tl;
	struct ibv_pd		*pd; 
	struct ibv_cq		*scq;
	struct ibv_cq		*rcq;
	struct ibv_qp		*qp;
	struct ibv_mr		*mr_rbuf;
	struct ibv_mr		*mr_sbuf;
	ib_cm_msg_t		*sbuf;
	ib_cm_msg_t		*rbuf;
	struct ibv_comp_channel *rch;
	struct ibv_ah		**ah;  
	DAPL_OS_LOCK		plock;
	uint16_t		lid;
	uint8_t			*sid;  /* Sevice IDs, port space, bitarray? */
	uint8_t			sl;
	uint16_t		pkey;
	int			pkey_idx;
	uint64_t		guid;
	char 			guid_str[32];
	ib_named_attr_t		na;
#ifdef DAT_IB_COLLECTIVES
	/* Collective member device and address information */
	ib_thread_state_t 	coll_thread_state;
	DAPL_OS_THREAD 		coll_thread;
	DAPL_OS_LOCK 		coll_lock;
	DAPL_OS_WAIT_OBJECT 	coll_event;
	struct dapl_llist_entry *grp_list;
	user_progress_func_t 	*user_func;
	int 			l_sock;
	struct sockaddr_in	m_addr;
	void 			*m_ctx;
	void			*m_info;
	void			*f_info;
	int			m_size;
	int			f_size;
	int			t_id;
#endif

} ib_hca_transport_t;

/* prototypes */
void cm_thread(void *arg);
void dapli_async_event_cb(struct _ib_hca_transport *tp);
void dapli_cq_event_cb(struct _ib_hca_transport *tp);
void dapls_cm_acquire(dp_ib_cm_handle_t cm_ptr);
void dapls_cm_release(dp_ib_cm_handle_t cm_ptr);
void dapls_cm_free(dp_ib_cm_handle_t cm_ptr);

#ifdef DAPL_COUNTERS
void dapls_print_cm_list(IN DAPL_IA *ia_ptr);
#endif

#endif /*  _DAPL_IB_UTIL_H_ */

