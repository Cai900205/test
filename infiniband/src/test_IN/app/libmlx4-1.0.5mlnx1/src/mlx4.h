/*
 * Copyright (c) 2004, 2005 Topspin Communications.  All rights reserved.
 * Copyright (c) 2005, 2006, 2007 Cisco Systems.  All rights reserved.
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

#ifndef MLX4_H
#define MLX4_H

#include <stdio.h>
#include <stddef.h>
#include <sys/ioctl.h>

#include <infiniband/driver.h>
#include <infiniband/driver_exp.h>
#include <infiniband/arch.h>
#include <infiniband/verbs.h>
#include <infiniband/verbs_exp.h>

#define MLX4_MMAP_CMD_BITS 8
#define MLX4_MMAP_GET_CONTIGUOUS_PAGES_CMD 2
#define MLX4_IB_MMAP_GET_HW_CLOCK 3
#define MLX4_CQ_PREFIX "MLX_CQ"
#define MLX4_QP_PREFIX "MLX_QP"
#define MLX4_MR_PREFIX "MLX_MR"
#define MLX4_MAX_LOG2_CONTIG_BLOCK_SIZE 23
#define MLX4_MIN_LOG2_CONTIG_BLOCK_SIZE 12
#define MLX4_PORTS_NUM 2

#ifdef HAVE_VALGRIND_MEMCHECK_H

#  include <valgrind/memcheck.h>

#  if !defined(VALGRIND_MAKE_MEM_DEFINED) || !defined(VALGRIND_MAKE_MEM_UNDEFINED)
#    warning "Valgrind support requested, but VALGRIND_MAKE_MEM_(UN)DEFINED not available"
#  endif

#endif /* HAVE_VALGRIND_MEMCHECK_H */

#ifndef VALGRIND_MAKE_MEM_DEFINED
#  define VALGRIND_MAKE_MEM_DEFINED(addr,len)
#endif

#ifndef VALGRIND_MAKE_MEM_UNDEFINED
#  define VALGRIND_MAKE_MEM_UNDEFINED(addr,len)
#endif

#ifndef rmb
#  define rmb() mb()
#endif

#ifndef wmb
#  define wmb() mb()
#endif

#ifndef wc_wmb

#if defined(__i386__)
#define wc_wmb() asm volatile("lock; addl $0,0(%%esp) " ::: "memory")
#elif defined(__x86_64__)
#define wc_wmb() asm volatile("sfence" ::: "memory")
#elif defined(__ia64__)
#define wc_wmb() asm volatile("fwb" ::: "memory")
#else
#define wc_wmb() wmb()
#endif

#endif

#define HIDDEN		__attribute__((visibility ("hidden")))

#if GCC_VERSION >= (4,3)
#	define HOT              __attribute__((hot))
#else
#	define HOT
#endif

#define PFX		"mlx4: "

#ifndef max
#define max(a, b) \
	({ typeof (a) _a = (a); \
	   typeof (b) _b = (b); \
	   _a > _b ? _a : _b; })
#endif

#ifndef min
#define min(a, b) \
	({ typeof (a) _a = (a); \
	   typeof (b) _b = (b); \
	   _a < _b ? _a : _b; })
#endif

#ifndef likely
#ifdef __GNUC__
#define likely(x)       __builtin_expect(!!(x),1)
#else
#define likely(x)	(x)
#endif
#endif


#ifndef unlikely
#ifdef __GNUC__
#define unlikely(x)       __builtin_expect(!!(x), 0)
#else
#define unlikely(x)       (x)
#endif
#endif

#include "list.h"

/****************************************/
/* ioctl codes */
/****************************************/
#define MLX4_IOC_MAGIC 'm'
#define MLX4_IOCHWCLOCKOFFSET _IOR(MLX4_IOC_MAGIC, 1, int)

enum {
	MLX4_STAT_RATE_OFFSET		= 5
};

enum {
	MLX4_QP_TABLE_BITS		= 8,
	MLX4_QP_TABLE_SIZE		= 1 << MLX4_QP_TABLE_BITS,
	MLX4_QP_TABLE_MASK		= MLX4_QP_TABLE_SIZE - 1
};

#define MLX4_REMOTE_SRQN_FLAGS(wr) htonl(wr->qp_type.xrc.remote_srqn << 8)

enum {
	MLX4_XSRQ_TABLE_BITS = 8,
	MLX4_XSRQ_TABLE_SIZE = 1 << MLX4_XSRQ_TABLE_BITS,
	MLX4_XSRQ_TABLE_MASK = MLX4_XSRQ_TABLE_SIZE - 1
};

struct mlx4_xsrq_table {
	struct {
		struct mlx4_srq **table;
		int		  refcnt;
	} xsrq_table[MLX4_XSRQ_TABLE_SIZE];

	pthread_mutex_t		  mutex;
	int			  num_xsrq;
	int			  shift;
	int			  mask;
};

void mlx4_init_xsrq_table(struct mlx4_xsrq_table *xsrq_table, int size);
struct mlx4_srq *mlx4_find_xsrq(struct mlx4_xsrq_table *xsrq_table, uint32_t srqn);
int mlx4_store_xsrq(struct mlx4_xsrq_table *xsrq_table, uint32_t srqn,
		    struct mlx4_srq *srq);
void mlx4_clear_xsrq(struct mlx4_xsrq_table *xsrq_table, uint32_t srqn);

enum {
	MLX4_XRC_QPN_BIT     = (1 << 23)
};

enum mlx4_db_type {
	MLX4_DB_TYPE_CQ,
	MLX4_DB_TYPE_RQ,
	MLX4_NUM_DB_TYPE
};

enum mlx4_alloc_type {
	MLX4_ALLOC_TYPE_ANON,
	MLX4_ALLOC_TYPE_HUGE,
	MLX4_ALLOC_TYPE_CONTIG,
	MLX4_ALLOC_TYPE_PREFER_HUGE,
	MLX4_ALLOC_TYPE_PREFER_CONTIG,
	MLX4_ALLOC_TYPE_ALL
};

enum {
	MLX4_OPCODE_NOP			= 0x00,
	MLX4_OPCODE_SEND_INVAL		= 0x01,
	MLX4_OPCODE_RDMA_WRITE		= 0x08,
	MLX4_OPCODE_RDMA_WRITE_IMM	= 0x09,
	MLX4_OPCODE_SEND		= 0x0a,
	MLX4_OPCODE_SEND_IMM		= 0x0b,
	MLX4_OPCODE_LSO			= 0x0e,
	MLX4_OPCODE_RDMA_READ		= 0x10,
	MLX4_OPCODE_ATOMIC_CS		= 0x11,
	MLX4_OPCODE_ATOMIC_FA		= 0x12,
	MLX4_OPCODE_ATOMIC_MASK_CS	= 0x14,
	MLX4_OPCODE_ATOMIC_MASK_FA	= 0x15,
	MLX4_OPCODE_BIND_MW		= 0x18,
	MLX4_OPCODE_FMR			= 0x19,
	MLX4_OPCODE_LOCAL_INVAL		= 0x1b,
	MLX4_OPCODE_CONFIG_CMD		= 0x1f,

	MLX4_OPCODE_SEND_ENABLE		= 0x17,
	MLX4_OPCODE_RECV_ENABLE		= 0x16,
	MLX4_OPCODE_CQE_WAIT		= 0x0f,
	MLX4_OPCODE_CALC_SEND		= 0x1e,
	MLX4_OPCODE_CALC_RDMA_WRITE_IMM	= 0x1f,

	MLX4_RECV_OPCODE_RDMA_WRITE_IMM	= 0x00,
	MLX4_RECV_OPCODE_SEND		= 0x01,
	MLX4_RECV_OPCODE_SEND_IMM	= 0x02,
	MLX4_RECV_OPCODE_SEND_INVAL	= 0x03,

	MLX4_CQE_OPCODE_ERROR		= 0x1e,
	MLX4_CQE_OPCODE_RESIZE		= 0x16,
};

extern int mlx4_stall_num_loop;
extern int mlx4_trace;
extern int mlx4_single_threaded;

enum {
	MLX4_MAX_WQE_SIZE = 1008
};

struct mlx4_device {
	struct verbs_device		verbs_dev;
	int				page_size;

	struct {
		unsigned id;
		unsigned short rev;
	} devid;
	int	driver_abi_ver;
};

struct mlx4_db_page;

struct mlx4_spinlock {
	pthread_spinlock_t		lock;
	int				in_use;
};

struct mlx4_context {
	union {
		struct ibv_context      ibv_ctx;
	};

	void			       *uar;
	struct mlx4_spinlock		uar_lock;

	void			       *bf_page;
	int				bf_buf_size;
	int				bf_offset;
	struct mlx4_spinlock		bf_lock;

	struct {
		struct mlx4_qp	      **table;
		int			refcnt;
	}				qp_table[MLX4_QP_TABLE_SIZE];
	pthread_mutex_t			qp_table_mutex;
	int				num_qps;
	int				qp_table_shift;
	int				qp_table_mask;
	int				max_qp_wr;
	int				max_sge;
	int				max_cqe;
	struct {
		int				offset;
		int				mult;
		int				shift;
		uint64_t			mask;
	} core_clk;
	void			       *hca_core_clock;

	struct mlx4_xsrq_table		xsrq_table;

	struct mlx4_db_page	       *db_list[MLX4_NUM_DB_TYPE];
	pthread_mutex_t			db_list_mutex;
	int				cqe_size;
	int				prefer_bf;
	struct mlx4_spinlock			hugetlb_lock;
	struct list_head			hugetlb_list;
	int				stall_enable;
	pthread_mutex_t			task_mutex;
	struct {
		uint8_t			valid;
		uint8_t			link_layer;
		enum ibv_port_cap_flags	caps;
	} port_query_cache[MLX4_PORTS_NUM];
};

struct mlx4_buf {
	void			       *buf;
	size_t				length;
	int				base;
	void				*hmem;
};

struct mlx4_pd {
	struct ibv_pd			ibv_pd;
	uint32_t			pdn;
};

struct mlx4_cq {
	struct ibv_cq			ibv_cq;
	struct mlx4_buf			buf;
	struct mlx4_buf			resize_buf;
	struct mlx4_spinlock		lock;
	uint32_t			cqn;
	uint32_t			cons_index;
	uint32_t                        wait_index;
	uint32_t                        wait_count;
	uint32_t		       *set_ci_db;
	uint32_t		       *arm_db;
	int				arm_sn;
	int 		       stall_next_poll;
	int				stall_enable;
	int                             cqe_size;
	int				creation_flags;
};

struct mlx4_srq {
	struct verbs_srq		verbs_srq;
	struct mlx4_buf			buf;
	struct mlx4_spinlock		lock;
	uint64_t		       *wrid;
	uint32_t			srqn;
	int				max;
	int				max_gs;
	int				wqe_shift;
	int				head;
	int				tail;
	uint32_t		       *db;
	uint16_t			counter;
	uint8_t				ext_srq;
	struct ibv_srq_legacy *ibv_srq_legacy;
};

struct mlx4_wq {
	uint64_t		       *wrid;
	struct mlx4_spinlock		lock;
	int				wqe_cnt;
	int				max_post;
	unsigned			head;
	unsigned			head_en_index;
	unsigned			head_en_count;
	unsigned			tail;
	int				max_gs;
	int				wqe_shift;
	int				offset;
};

/* enclosing ibv_mr adding some extra managing information */
struct mlx4_mr {
	struct ibv_mr ibv_mr;
	struct mlx4_buf buf;
	uint64_t allocation_flags;
	int shared_mr;
};


struct mlx4_inlr_rbuff {
	void *rbuff;
	int rlen;
};

struct mlx4_inlr_sg_list {
	struct mlx4_inlr_rbuff *sg_list;
	int list_len;
};

struct mlx4_inlr_buff {
	struct mlx4_inlr_sg_list *buff;
	int len;
};

struct mlx4_qp {
	struct verbs_qp			verbs_qp;
	struct mlx4_buf			buf;
	int				max_inline_data;
	int				buf_size;

	uint32_t			doorbell_qpn;
	uint32_t			sq_signal_bits;
	int				sq_spare_wqes;
	struct mlx4_wq			sq;

	uint32_t		       *db;
	struct mlx4_wq			rq;

	uint8_t				link_layer;
	int				create_flags;
	uint32_t			max_inlr_sg;
	struct mlx4_inlr_buff		inlr_buff;
	int				is_masked_atomic;
};

struct mlx4_av {
	uint32_t			port_pd;
	uint8_t				reserved1;
	uint8_t				g_slid;
	uint16_t			dlid;
	uint8_t				reserved2;
	uint8_t				gid_index;
	uint8_t				stat_rate;
	uint8_t				hop_limit;
	uint32_t			sl_tclass_flowlabel;
	uint8_t				dgid[16];
};

struct mlx4_ah {
	struct ibv_ah			ibv_ah;
	struct mlx4_av			av;
	uint16_t			vlan;
	uint8_t				mac[6];
};

static inline unsigned long align(unsigned long val, unsigned long align)
{
	return (val + align - 1) & ~(align - 1);
}
int align_queue_size(int req);

#define to_mxxx(xxx, type)						\
	((struct mlx4_##type *)					\
	 ((void *) ib##xxx - offsetof(struct mlx4_##type, ibv_##xxx)))

static inline struct mlx4_device *to_mdev(struct ibv_device *ibdev)
{
	/* ibv_device is first field of verbs_device
	  * see try_driver in libibverbs.
	*/
	return container_of(ibdev, struct mlx4_device, verbs_dev);
}

static inline struct mlx4_context *to_mctx(struct ibv_context *ibctx)
{
	return to_mxxx(ctx, context);
}

static inline struct mlx4_pd *to_mpd(struct ibv_pd *ibpd)
{
	return to_mxxx(pd, pd);
}

static inline struct mlx4_cq *to_mcq(struct ibv_cq *ibcq)
{
	return to_mxxx(cq, cq);
}

static inline struct mlx4_srq *to_msrq(struct ibv_srq *ibsrq)
{
	return container_of(container_of(ibsrq, struct verbs_srq, srq),
			    struct mlx4_srq, verbs_srq);
}

static inline struct mlx4_qp *to_mqp(struct ibv_qp *ibqp)
{
	return container_of(container_of(ibqp, struct verbs_qp, qp),
			    struct mlx4_qp, verbs_qp);
}

static inline struct mlx4_mr *to_mmr(struct ibv_mr *ibmr)
{
	return to_mxxx(mr, mr);
}
static inline struct mlx4_ah *to_mah(struct ibv_ah *ibah)
{
	return to_mxxx(ah, ah);
}

int mlx4_alloc_buf(struct mlx4_buf *buf, size_t size, int page_size);
void mlx4_free_buf(struct mlx4_buf *buf);
int mlx4_alloc_buf_huge(struct mlx4_context *mctx, struct mlx4_buf *buf,
			size_t size, int page_size);
int mlx4_alloc_buf_contig(struct mlx4_context *mctx, struct mlx4_buf *buf,
			size_t size, int page_size, const char *component, void *req_addr);
int mlx4_alloc_prefered_buf(struct mlx4_context *mctx,
				struct mlx4_buf *buf,
				size_t size, int page_size,
				enum mlx4_alloc_type alloc_type,
				const char *component);
void mlx4_get_alloc_type(const char *component,
				enum mlx4_alloc_type *alloc_type,
				enum mlx4_alloc_type default_alloc_type);
void mlx4_free_buf_huge(struct mlx4_context *mctx, struct mlx4_buf *buf);
int mlx4_use_huge(const char *key);

uint32_t *mlx4_alloc_db(struct mlx4_context *context, enum mlx4_db_type type);
void mlx4_free_db(struct mlx4_context *context, enum mlx4_db_type type, uint32_t *db);

int __mlx4_query_device(uint64_t raw_fw_ver,
			struct ibv_device_attr *attr);
int mlx4_query_device(struct ibv_context *context,
		       struct ibv_device_attr *attr);
int mlx4_query_port(struct ibv_context *context, uint8_t port,
		     struct ibv_port_attr *attr);

struct ibv_pd *mlx4_alloc_pd(struct ibv_context *context);
int mlx4_free_pd(struct ibv_pd *pd);
struct ibv_xrcd *mlx4_open_xrcd(struct ibv_context *context,
				struct ibv_xrcd_init_attr *attr);
int mlx4_close_xrcd(struct ibv_xrcd *xrcd);

struct ibv_mr *mlx4_reg_mr(struct ibv_pd *pd, void *addr,
			   size_t length, int access);
struct ibv_mr *mlx4_exp_reg_mr(struct ibv_exp_reg_mr_in *in);
int mlx4_exp_post_send(struct ibv_qp *ibqp, struct ibv_exp_send_wr *wr,
		       struct ibv_exp_send_wr **bad_wr);
struct ibv_mr *mlx4_reg_shared_mr(struct ibv_exp_reg_shared_mr_in *in);
int mlx4_dereg_mr(struct ibv_mr *mr);

struct ibv_mw *mlx4_alloc_mw(struct ibv_pd *pd, enum ibv_mw_type type);
int mlx4_dealloc_mw(struct ibv_mw *mw);
int mlx4_bind_mw(struct ibv_qp *qp, struct ibv_mw *mw,
		 struct ibv_mw_bind *mw_bind);
int mlx4_exp_bind_mw(struct ibv_exp_mw_bind *mw_bind);

struct ibv_cq *mlx4_create_cq(struct ibv_context *context, int cqe,
			       struct ibv_comp_channel *channel,
			       int comp_vector);
int mlx4_alloc_cq_buf(struct mlx4_context *mctx, struct mlx4_buf *buf, int nent,
		      int entry_size);
int mlx4_resize_cq(struct ibv_cq *cq, int cqe);
int mlx4_destroy_cq(struct ibv_cq *cq);
int mlx4_poll_ibv_cq(struct ibv_cq *cq, int ne, struct ibv_wc *wc);
int mlx4_exp_poll_cq(struct ibv_cq *ibcq, int num_entries,
		     struct ibv_exp_wc *wc, uint32_t wc_size) HOT;
int mlx4_arm_cq(struct ibv_cq *cq, int solicited);
void mlx4_cq_event(struct ibv_cq *cq);
void __mlx4_cq_clean(struct mlx4_cq *cq, uint32_t qpn, struct mlx4_srq *srq);
void mlx4_cq_clean(struct mlx4_cq *cq, uint32_t qpn, struct mlx4_srq *srq);
int mlx4_get_outstanding_cqes(struct mlx4_cq *cq);
void mlx4_cq_resize_copy_cqes(struct mlx4_cq *cq, void *buf, int new_cqe);

struct ibv_srq *mlx4_create_srq(struct ibv_pd *pd,
				 struct ibv_srq_init_attr *attr);
struct ibv_srq *mlx4_create_srq_ex(struct ibv_context *context,
				   struct ibv_srq_init_attr_ex *attr_ex);
struct ibv_srq *mlx4_create_xrc_srq(struct ibv_context *context,
				    struct ibv_srq_init_attr_ex *attr_ex);
int mlx4_modify_srq(struct ibv_srq *srq,
		     struct ibv_srq_attr *attr,
		     int mask);
int mlx4_query_srq(struct ibv_srq *srq,
			   struct ibv_srq_attr *attr);
int mlx4_destroy_srq(struct ibv_srq *srq);
int mlx4_destroy_xrc_srq(struct ibv_srq *srq);
int mlx4_alloc_srq_buf(struct ibv_pd *pd, struct ibv_srq_attr *attr,
			struct mlx4_srq *srq);
void mlx4_init_xsrq_table(struct mlx4_xsrq_table *xsrq_table, int size);
struct mlx4_srq *mlx4_find_xsrq(struct mlx4_xsrq_table *xsrq_table, uint32_t srqn);
int mlx4_store_xsrq(struct mlx4_xsrq_table *xsrq_table, uint32_t srqn,
		    struct mlx4_srq *srq);
void mlx4_clear_xsrq(struct mlx4_xsrq_table *xsrq_table, uint32_t srqn);
void mlx4_free_srq_wqe(struct mlx4_srq *srq, int ind);
int mlx4_post_srq_recv(struct ibv_srq *ibsrq,
		       struct ibv_recv_wr *wr,
		       struct ibv_recv_wr **bad_wr);

struct ibv_qp *mlx4_create_qp(struct ibv_pd *pd, struct ibv_qp_init_attr *attr);
struct ibv_qp *mlx4_create_qp_ex(struct ibv_context *context,
				 struct ibv_qp_init_attr_ex *attr);
int mlx4_modify_cq(struct ibv_cq *cq, struct ibv_exp_cq_attr *attr, int attr_mask);
int mlx4_post_task(struct ibv_context *context,
			struct ibv_exp_task *task_list,
			struct ibv_exp_task **bad_task);
struct ibv_qp *mlx4_open_qp(struct ibv_context *context, struct ibv_qp_open_attr *attr);
int mlx4_query_qp(struct ibv_qp *qp, struct ibv_qp_attr *attr,
		   int attr_mask,
		   struct ibv_qp_init_attr *init_attr);
int mlx4_modify_qp(struct ibv_qp *qp, struct ibv_qp_attr *attr,
		    int attr_mask);
int mlx4_exp_modify_qp(struct ibv_qp *qp, struct ibv_exp_qp_attr *attr,
		       uint64_t attr_mask);
int mlx4_destroy_qp(struct ibv_qp *qp);
void *mlx4_get_recv_wqe(struct mlx4_qp *qp, int n);
void mlx4_init_qp_indices(struct mlx4_qp *qp);
void mlx4_qp_init_sq_ownership(struct mlx4_qp *qp);
int mlx4_post_send(struct ibv_qp *ibqp, struct ibv_send_wr *wr,
		   struct ibv_send_wr **bad_wr) HOT;
int mlx4_post_recv(struct ibv_qp *ibqp, struct ibv_recv_wr *wr,
		   struct ibv_recv_wr **bad_wr) HOT;
void mlx4_calc_sq_wqe_size(struct ibv_qp_cap *cap, enum ibv_qp_type type,
			   struct mlx4_qp *qp);
int num_inline_segs(int data, enum ibv_qp_type type);
void mlx4_dealloc_qp_buf(struct ibv_context *context, struct mlx4_qp *qp);
void mlx4_set_sq_sizes(struct mlx4_qp *qp, struct ibv_qp_cap *cap,
		       enum ibv_qp_type type);
struct mlx4_qp *mlx4_find_qp(struct mlx4_context *ctx, uint32_t qpn);
int mlx4_store_qp(struct mlx4_context *ctx, uint32_t qpn, struct mlx4_qp *qp);
void mlx4_clear_qp(struct mlx4_context *ctx, uint32_t qpn);
struct ibv_ah *mlx4_create_ah_common(struct ibv_pd *pd,
				     struct ibv_ah_attr *attr,
				     uint8_t link_layer);
struct ibv_ah *mlx4_create_ah(struct ibv_pd *pd, struct ibv_ah_attr *attr);
struct ibv_ah *mlx4_exp_create_ah(struct ibv_pd *pd,
				  struct ibv_exp_ah_attr *attr_ex);
int mlx4_destroy_ah(struct ibv_ah *ah);
int mlx4_alloc_av(struct mlx4_pd *pd, struct ibv_ah_attr *attr,
		   struct mlx4_ah *ah);
void mlx4_free_av(struct mlx4_ah *ah);
struct ibv_cq *mlx4_create_cq_ex(struct ibv_context *context,
				 int cqe,
				 struct ibv_comp_channel *channel,
				 int comp_vector,
				 struct ibv_exp_cq_init_attr *attr);
int mlx4_query_values(struct ibv_context *context, int q_values,
		      struct ibv_exp_values *values);
void *mlx4_get_legacy_xrc(struct ibv_srq *srq);
void mlx4_set_legacy_xrc(struct ibv_srq *srq, void *legacy_xrc_srq);

static inline int mlx4_spin_lock(struct mlx4_spinlock *lock)
{
	if (!mlx4_single_threaded)
		return pthread_spin_lock(&lock->lock);

	if (unlikely(lock->in_use)) {
		fprintf(stderr, "*** ERROR: multithreading violation ***\n"
			"You are running a multithreaded application but\n"
			"you set MLX4_SINGLE_THREADED=1. Please unset it.\n");
		abort();
	} else {
		lock->in_use = 1;
		wmb();
	}

	return 0;
}

static inline int mlx4_spin_unlock(struct mlx4_spinlock *lock)
{
	if (!mlx4_single_threaded)
		return pthread_spin_unlock(&lock->lock);

	lock->in_use = 0;

	return 0;
}

static inline int mlx4_spinlock_init(struct mlx4_spinlock *lock)
{
	lock->in_use = 0;
	return pthread_spin_init(&lock->lock, PTHREAD_PROCESS_PRIVATE);
}

static inline int mlx4_spinlock_destroy(struct mlx4_spinlock *lock)
{
	return pthread_spin_destroy(&lock->lock);
}


#endif /* MLX4_H */
