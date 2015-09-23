/*
 * Copyright (c) 2012 Mellanox Technologies, Inc.  All rights reserved.
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
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <sys/mman.h>
#include <pthread.h>
#include <string.h>
#include <sched.h>
#include <sys/param.h>

#ifndef HAVE_IBV_REGISTER_DRIVER
#include <sysfs/libsysfs.h>
#endif

#include "mlx5.h"
#include "mlx5-abi.h"

#ifndef PCI_VENDOR_ID_MELLANOX
#define PCI_VENDOR_ID_MELLANOX			0x15b3
#endif

#ifndef CPU_AND
#define CPU_AND(x, y, z) do {} while (0)
#endif

#ifndef CPU_EQUAL
#define CPU_EQUAL(x, y) 1
#endif

#ifndef CPU_COUNT
#define CPU_COUNT(x) 0
#endif

#define HCA(v, d) \
	{ .vendor = PCI_VENDOR_ID_##v,			\
	  .device = d }

struct {
	unsigned		vendor;
	unsigned		device;
} hca_table[] = {
	HCA(MELLANOX, 4113),	/* MT27600 Connect-IB */
	HCA(MELLANOX, 4114),	/* MT27600 Connect-IB virtual function */
};

uint32_t mlx5_debug_mask = 0;
int mlx5_freeze_on_error_cqe;

static struct ibv_context_ops mlx5_ctx_ops = {
	.query_device  = mlx5_query_device,
	.query_port    = mlx5_query_port,
	.alloc_pd      = mlx5_alloc_pd,
	.dealloc_pd    = mlx5_free_pd,
	.reg_mr	       = mlx5_reg_mr,
	.dereg_mr      = mlx5_dereg_mr,
	.create_cq     = mlx5_create_cq,
	.poll_cq       = mlx5_poll_cq,
	.req_notify_cq = mlx5_arm_cq,
	.cq_event      = mlx5_cq_event,
	.resize_cq     = mlx5_resize_cq,
	.destroy_cq    = mlx5_destroy_cq,
	.create_srq    = mlx5_create_srq,
	.modify_srq    = mlx5_modify_srq,
	.query_srq     = mlx5_query_srq,
	.destroy_srq   = mlx5_destroy_srq,
	.post_srq_recv = mlx5_post_srq_recv,
	.create_qp     = mlx5_create_qp,
	.query_qp      = mlx5_query_qp,
	.modify_qp     = mlx5_modify_qp,
	.destroy_qp    = mlx5_destroy_qp,
	.post_send     = mlx5_post_send,
	.post_recv     = mlx5_post_recv,
	.create_ah     = mlx5_create_ah,
	.destroy_ah    = mlx5_destroy_ah,
	.attach_mcast  = mlx5_attach_mcast,
	.detach_mcast  = mlx5_detach_mcast
};

static int read_number_from_line(const char *line, int *value)
{
	const char *ptr;

	ptr = strchr(line, ':');
	if (!ptr)
		return 1;

	++ptr;

	*value = atoi(ptr);
	return 0;
}

static int mlx5_is_sandy_bridge(int *num_cores)
{
	char line[128];
	FILE *fd;
	int rc = 0;
	int cur_cpu_family = -1;
	int cur_cpu_model = -1;

	fd = fopen("/proc/cpuinfo", "r");
	if (!fd)
		return 0;

	*num_cores = 0;

	while (fgets(line, 128, fd)) {
		int value;

		/* if this is information on new processor */
		if (!strncmp(line, "processor", 9)) {
			++*num_cores;

			cur_cpu_family = -1;
			cur_cpu_model  = -1;
		} else if (!strncmp(line, "cpu family", 10)) {
			if ((cur_cpu_family < 0) && (!read_number_from_line(line, &value)))
				cur_cpu_family = value;
		} else if (!strncmp(line, "model", 5)) {
			if ((cur_cpu_model < 0) && (!read_number_from_line(line, &value)))
				cur_cpu_model = value;
		}

		/* if this is a Sandy Bridge CPU */
		if ((cur_cpu_family == 6) &&
		    (cur_cpu_model == 0x2A || (cur_cpu_model == 0x2D) ))
			rc = 1;
	}

	fclose(fd);
	return rc;
}

/*
man cpuset

  This format displays each 32-bit word in hexadecimal (using ASCII characters "0" - "9" and "a" - "f"); words
  are filled with leading zeros, if required. For masks longer than one word, a comma separator is used between
  words. Words are displayed in big-endian order, which has the most significant bit first. The hex digits
  within a word are also in big-endian order.

  The number of 32-bit words displayed is the minimum number needed to display all bits of the bitmask, based on
  the size of the bitmask.

  Examples of the Mask Format:

     00000001                        # just bit 0 set
     40000000,00000000,00000000      # just bit 94 set
     000000ff,00000000               # bits 32-39 set
     00000000,000E3862               # 1,5,6,11-13,17-19 set

  A mask with bits 0, 1, 2, 4, 8, 16, 32, and 64 set displays as:

     00000001,00000001,00010117

  The first "1" is for bit 64, the second for bit 32, the third for bit 16, the fourth for bit 8, the fifth for
  bit 4, and the "7" is for bits 2, 1, and 0.
*/
static void mlx5_local_cpu_set(struct ibv_device *ibdev, cpu_set_t *cpu_set)
{
	char *p, buf[1024];
	char *env_value;
	uint32_t word;
	int i, k;

	env_value = getenv("MLX5_LOCAL_CPUS");
	if (env_value)
		strncpy(buf, env_value, sizeof(buf));
	else {
		char fname[MAXPATHLEN];
		FILE *fp;

		snprintf(fname, MAXPATHLEN, "/sys/class/infiniband/%s/device/local_cpus",
			 ibv_get_device_name(ibdev));

		fp = fopen(fname, "r");
		if (!fp) {
			fprintf(stderr, PFX "Warning: can not get local cpu set: failed to open %s\n", fname);
			return;
		}
		if (!fgets(buf, sizeof(buf), fp)) {
			fprintf(stderr, PFX "Warning: can not get local cpu set: failed to read cpu mask\n");
			fclose(fp);
			return;
		}
		fclose(fp);
	}

	p = strrchr(buf, ',');
	if (!p)
		p = buf;

	i = 0;
	do {
		if (*p == ',') {
			*p = 0;
			p ++;
		}

		word = strtoul(p, 0, 16);

		for (k = 0; word; ++k, word >>= 1)
			if (word & 1)
				CPU_SET(k+i, cpu_set);

		if (p == buf)
			break;

		p = strrchr(buf, ',');
		if (!p)
			p = buf;

		i += 32;
	} while (i < CPU_SETSIZE);
}

static int mlx5_enable_sandy_bridge_fix(struct ibv_device *ibdev)
{
	cpu_set_t my_cpus, dev_local_cpus, result_set;
	int stall_enable;
	int ret;
	int num_cores;

	if (!mlx5_is_sandy_bridge(&num_cores))
		return 0;

	/* by default disable stall on sandy bridge arch */
	stall_enable = 0;

	/*
	 * check if app is bound to cpu set that is inside
	 * of device local cpu set. Disable stalling if true
	 */

	/* use static cpu set - up to CPU_SETSIZE (1024) cpus/node */
	CPU_ZERO(&my_cpus);
	CPU_ZERO(&dev_local_cpus);
	CPU_ZERO(&result_set);
	ret = sched_getaffinity(0, sizeof(my_cpus), &my_cpus);
	if (ret == -1) {
		if (errno == EINVAL)
			fprintf(stderr, PFX "Warning: my cpu set is too small\n");
		else
			fprintf(stderr, PFX "Warning: failed to get my cpu set\n");
		goto out;
	}

	/* get device local cpu set */
	mlx5_local_cpu_set(ibdev, &dev_local_cpus);

	/* make sure result_set is not init to all 0 */
	CPU_SET(0, &result_set);
	/* Set stall_enable if my cpu set and dev cpu set are disjoint sets */
	CPU_AND(&result_set, &my_cpus, &dev_local_cpus);
	stall_enable = CPU_COUNT(&result_set) ? 0 : 1;

out:
	return stall_enable;
}

static void mlx5_read_env(struct ibv_device *ibdev, struct mlx5_context *ctx)
{
	char *env_value;

	env_value = getenv("MLX5_STALL_CQ_POLL");
	if (env_value && !strcmp(env_value, "0"))
		/* check if cq stall is overrided by user */
		ctx->stall_enable = 0;
	else
		/* autodetect if we need to do cq polling */
		ctx->stall_enable = mlx5_enable_sandy_bridge_fix(ibdev);

	env_value = getenv("MLX5_STALL_NUM_LOOP");
	if (env_value)
		mlx5_stall_num_loop = atoi(env_value);

	env_value = getenv("MLX5_STALL_CQ_POLL_MIN");
	if (env_value)
		mlx5_stall_cq_poll_min = atoi(env_value);

	env_value = getenv("MLX5_STALL_CQ_POLL_MAX");
	if (env_value)
		mlx5_stall_cq_poll_max = atoi(env_value);

	env_value = getenv("MLX5_STALL_CQ_INC_STEP");
	if (env_value)
		mlx5_stall_cq_inc_step = atoi(env_value);

	env_value = getenv("MLX5_STALL_CQ_DEC_STEP");
	if (env_value)
		mlx5_stall_cq_dec_step = atoi(env_value);

	ctx->stall_adaptive_enable = 0;
	ctx->stall_cycles = 0;

	if (mlx5_stall_num_loop < 0) {
		ctx->stall_adaptive_enable = 1;
		ctx->stall_cycles = mlx5_stall_cq_poll_min;
	}

}

static int get_total_uuars(void)
{
	char *env;
	int size = MLX5_DEF_TOT_UUARS;

	env = getenv("MLX5_TOTAL_UUARS");
	if (env)
		size = atoi(env);

	if (size < 1)
		return -EINVAL;

	size = align(size, MLX5_NUM_UUARS_PER_PAGE);
	if (size > MLX5_MAX_UUARS)
		return -ENOMEM;

	return size;
}

static void open_debug_file(struct mlx5_context *ctx)
{
	char *env;

	env = getenv("MLX5_DEBUG_FILE");
	if (!env) {
		ctx->dbg_fp = stderr;
		return;
	}

	ctx->dbg_fp = fopen(env, "aw+");
	if (!ctx->dbg_fp) {
		fprintf(stderr, "Failed opening debug file %s, using stderr\n", env);
		ctx->dbg_fp = stderr;
		return;
	}
}

static void close_debug_file(struct mlx5_context *ctx)
{
	if (ctx->dbg_fp && ctx->dbg_fp != stderr)
		fclose(ctx->dbg_fp);
}

static void set_debug_mask(void)
{
	char *env;

	env = getenv("MLX5_DEBUG_MASK");
	if (env)
		mlx5_debug_mask = strtol(env, NULL, 0);
}

static void set_freeze_on_error(void)
{
	char *env;

	env = getenv("MLX5_FREEZE_ON_ERROR_CQE");
	if (env)
		mlx5_freeze_on_error_cqe = strtol(env, NULL, 0);
}

static int get_always_bf(void)
{
	char *env;

	env = getenv("MLX5_POST_SEND_PREFER_BF");
	if (!env)
		return 1;

	return strcmp(env, "0") ? 1 : 0;
}

static int get_shut_up_bf(void)
{
	char *env;

	env = getenv("MLX5_SHUT_UP_BF");
	if (!env)
		return 0;

	return strcmp(env, "0") ? 1 : 0;
}

static int get_num_low_lat_uuars(void)
{
	char *env;
	int num = 4;

	env = getenv("MLX5_NUM_LOW_LAT_UUARS");
	if (env)
		num = atoi(env);

	if (num < 0)
		return -EINVAL;

	return num;
}

static int need_uuar_lock(struct mlx5_context *ctx, int uuarn)
{
	if (uuarn == 0)
		return 0;

	if (uuarn >= (ctx->tot_uuars - ctx->low_lat_uuars) * 2)
		return 0;

	return 1;
}

static int single_threaded_app(void)
{

	char *env;

	env = getenv("MLX5_SINGLE_THREADED");
	if (env)
		return strcmp(env, "1") ? 0 : 1;

	return 0;
}

static void set_extended(struct verbs_context *verbs_ctx)
{
	int off_create_qp_ex = offsetof(struct verbs_context, create_qp_ex);
	int off_open_xrcd = offsetof(struct verbs_context, open_xrcd);
	int off_create_srq = offsetof(struct verbs_context, create_srq_ex);
	int off_get_srq_num = offsetof(struct verbs_context, get_srq_num);
	int off_open_qp = offsetof(struct verbs_context, open_qp);
	int off_mlx5_close_xrcd = offsetof(struct verbs_context, close_xrcd);

	if (sizeof(*verbs_ctx) - off_create_qp_ex <= verbs_ctx->sz)
		verbs_ctx->create_qp_ex = mlx5_drv_create_qp;

	if (sizeof(*verbs_ctx) - off_open_xrcd <= verbs_ctx->sz)
		verbs_ctx->open_xrcd = mlx5_open_xrcd;

	if (sizeof(*verbs_ctx) - off_create_srq <= verbs_ctx->sz)
		verbs_ctx->create_srq_ex = mlx5_create_srq_ex;

	if (sizeof(*verbs_ctx) - off_get_srq_num <= verbs_ctx->sz)
		verbs_ctx->get_srq_num = mlx5_get_srq_num;

	if (sizeof(*verbs_ctx) - off_open_qp <= verbs_ctx->sz)
		verbs_ctx->open_qp = mlx5_open_qp;

	if (sizeof(*verbs_ctx) - off_mlx5_close_xrcd <= verbs_ctx->sz)
		verbs_ctx->close_xrcd = mlx5_close_xrcd;

}

static void set_experimental(struct ibv_context *ctx)
{
	struct verbs_context_exp *verbs_exp_ctx = verbs_get_exp_ctx(ctx);

	verbs_set_exp_ctx_op(verbs_exp_ctx, create_dct, mlx5_create_dct);
	verbs_set_exp_ctx_op(verbs_exp_ctx, destroy_dct, mlx5_destroy_dct);
	verbs_set_exp_ctx_op(verbs_exp_ctx, query_dct, mlx5_query_dct);
	verbs_set_exp_ctx_op(verbs_exp_ctx, drv_exp_arm_dct, mlx5_arm_dct);
	verbs_set_exp_ctx_op(verbs_exp_ctx, drv_exp_query_device, mlx5_query_device_ex);
	verbs_set_exp_ctx_op(verbs_exp_ctx, drv_exp_create_qp, mlx5_exp_create_qp);
	verbs_set_exp_ctx_op(verbs_exp_ctx, drv_exp_modify_qp, mlx5_modify_qp_ex);
	verbs_set_exp_ctx_op(verbs_exp_ctx, drv_exp_get_legacy_xrc, mlx5_get_legacy_xrc);
	verbs_set_exp_ctx_op(verbs_exp_ctx, drv_exp_set_legacy_xrc, mlx5_set_legacy_xrc);
	verbs_set_exp_ctx_op(verbs_exp_ctx, drv_exp_modify_cq, mlx5_modify_cq);
	verbs_set_exp_ctx_op(verbs_exp_ctx, exp_create_cq, mlx5_create_cq_ex);
	verbs_set_exp_ctx_op(verbs_exp_ctx, drv_exp_ibv_poll_cq, mlx5_poll_cq_ex);
	verbs_set_exp_ctx_op(verbs_exp_ctx, drv_exp_post_task, mlx5_post_task);
	verbs_set_exp_ctx_op(verbs_exp_ctx, drv_exp_reg_mr, mlx5_exp_reg_mr);
	verbs_set_exp_ctx_op(verbs_exp_ctx, drv_exp_post_send, mlx5_exp_post_send);
	verbs_set_exp_ctx_op(verbs_exp_ctx, drv_exp_alloc_mkey_list_memory, mlx5_alloc_mkey_mem);
	verbs_set_exp_ctx_op(verbs_exp_ctx, drv_exp_dealloc_mkey_list_memory, mlx5_free_mkey_mem);
	verbs_set_exp_ctx_op(verbs_exp_ctx, drv_exp_query_mkey, mlx5_query_mkey);
	verbs_set_exp_ctx_op(verbs_exp_ctx, drv_exp_create_mr, mlx5_create_mr);
	verbs_set_exp_ctx_op(verbs_exp_ctx, drv_exp_prefetch_mr,
			     mlx5_prefetch_mr);
	verbs_set_exp_ctx_op(verbs_exp_ctx, drv_exp_dereg_mr, mlx5_exp_dereg_mr);
}

static int mlx5_alloc_context(struct verbs_device *vdev,
			      struct ibv_context *ctx, int cmd_fd)
{
	struct mlx5_context	       *context;
	struct mlx5_alloc_ucontext	req;
	struct mlx5_alloc_ucontext_resp resp;
	struct ibv_device		*ibdev = &vdev->device;
	struct verbs_context *verbs_ctx = verbs_get_ctx(ctx);
	int	i;
	int	page_size = to_mdev(ibdev)->page_size;
	int	tot_uuars;
	int	low_lat_uuars;
	int 	gross_uuars;
	int	j;
	off_t	offset;

	mlx5_single_threaded = single_threaded_app();

	context = to_mctx(ctx);
	context->ibv_ctx.cmd_fd = cmd_fd;

	memset(&resp, 0, sizeof(resp));
	open_debug_file(context);
	set_debug_mask();
	set_freeze_on_error();
	if (gethostname(context->hostname, sizeof(context->hostname)))
		strcpy(context->hostname, "host_unknown");

	tot_uuars = get_total_uuars();
	if (tot_uuars <= 0) {
		if (tot_uuars == 0)
			errno = EINVAL;
		else
			errno = -tot_uuars;
		goto err_free;
	}

	gross_uuars = tot_uuars / MLX5_NUM_UUARS_PER_PAGE * 4;
	context->bfs = calloc(gross_uuars, sizeof *context->bfs);
	if (!context->bfs) {
		errno = ENOMEM;
		goto err_free;
	}

	low_lat_uuars = get_num_low_lat_uuars();
	if (low_lat_uuars < 0) {
		errno = ENOMEM;
		goto err_free_bf;
	}

	if (low_lat_uuars > tot_uuars - 1) {
		errno = ENOMEM;
		goto err_free_bf;
	}

	memset(&req, 0, sizeof(req));
	req.total_num_uuars = tot_uuars;
	req.num_low_latency_uuars = low_lat_uuars;
	if (ibv_cmd_get_context(&context->ibv_ctx, &req.ibv_req, sizeof req,
				&resp.ibv_resp, sizeof resp))
		goto err_free_bf;

	context->max_num_qps		= resp.qp_tab_size;
	context->bf_reg_size		= resp.bf_reg_size;
	context->tot_uuars		= resp.tot_uuars;
	context->low_lat_uuars		= low_lat_uuars;
	context->cache_line_size	= resp.cache_line_size;
	context->max_sq_desc_sz = resp.max_sq_desc_sz;
	context->max_rq_desc_sz = resp.max_rq_desc_sz;
	context->max_send_wqebb	= resp.max_send_wqebb;
	context->num_ports	= resp.num_ports;
	context->max_recv_wr	= resp.max_recv_wr;
	context->max_srq_recv_wr = resp.max_srq_recv_wr;
	context->max_desc_sz_sq_dc = resp.max_desc_sz_sq_dc;
	context->atomic_sizes_dc = resp.atomic_sizes_dc;
	pthread_mutex_init(&context->rsc_table_mutex, NULL);
	pthread_mutex_init(&context->srq_table_mutex, NULL);
	for (i = 0; i < MLX5_QP_TABLE_SIZE; ++i)
		context->rsc_table[i].refcnt = 0;

	context->db_list = NULL;

	pthread_mutex_init(&context->db_list_mutex, NULL);


	for (i = 0; i < resp.tot_uuars / MLX5_NUM_UUARS_PER_PAGE; ++i) {
		offset = 0;
		set_command(MLX5_MMAP_GET_REGULAR_PAGES_CMD, &offset);
		set_index(i, &offset);
		context->uar[i] = mmap(NULL, to_mdev(ibdev)->page_size, PROT_WRITE,
				       MAP_SHARED, cmd_fd,
				       page_size * offset);
		if (context->uar[i] == MAP_FAILED) {
			context->uar[i] = NULL;
			goto err_free_bf;
		}
	}

	for (j = 0; j < gross_uuars; ++j) {
		context->bfs[j].reg = context->uar[j / 4] +
			MLX5_BF_OFFSET + (j % 4) * context->bf_reg_size;
		context->bfs[j].need_lock = need_uuar_lock(context, j);
		mlx5_spinlock_init(&context->bfs[j].lock);
		context->bfs[j].offset = 0;
		if (j)
			context->bfs[j].buf_size = context->bf_reg_size / 2;

		context->bfs[j].uuarn = j;
	}

	mlx5_spinlock_init(&context->lock32);

	context->prefer_bf = get_always_bf();
	context->shut_up_bf = get_shut_up_bf();
	mlx5_read_env(ibdev, context);

	mlx5_spinlock_init(&context->hugetlb_lock);
	INIT_LIST_HEAD(&context->hugetlb_list);

	pthread_mutex_init(&context->task_mutex, NULL);

	ctx->ops = mlx5_ctx_ops;
	set_extended(verbs_ctx);
	set_experimental(ctx);

	return 0;

err_free_bf:
	free(context->bfs);

err_free:
	for (i = 0; i < MLX5_MAX_UAR_PAGES; ++i) {
		if (context->uar[i])
			munmap(context->uar[i], page_size);
	}
	close_debug_file(context);
	return errno;
}

static void mlx5_free_context(struct verbs_device *device,
			      struct ibv_context *ibctx)
{
	struct mlx5_context *context = to_mctx(ibctx);
	int page_size = to_mdev(ibctx->device)->page_size;
	int i;

	free(context->bfs);
	for (i = 0; i < MLX5_MAX_UAR_PAGES; ++i) {
		if (context->uar[i])
			munmap(context->uar[i], page_size);
	}
	close_debug_file(context);
}

static struct verbs_device *mlx5_driver_init(const char *uverbs_sys_path,
					     int abi_version)
{
	char			value[8];
	struct mlx5_device     *dev;
	unsigned		vendor, device;
	int			i;

	if (ibv_read_sysfs_file(uverbs_sys_path, "device/vendor",
				value, sizeof value) < 0)
		return NULL;
	sscanf(value, "%i", &vendor);

	if (ibv_read_sysfs_file(uverbs_sys_path, "device/device",
				value, sizeof value) < 0)
		return NULL;
	sscanf(value, "%i", &device);

	for (i = 0; i < sizeof hca_table / sizeof hca_table[0]; ++i)
		if (vendor == hca_table[i].vendor &&
		    device == hca_table[i].device)
			goto found;

	return NULL;

found:
	if (abi_version < MLX5_UVERBS_MIN_ABI_VERSION ||
	    abi_version > MLX5_UVERBS_MAX_ABI_VERSION) {
		fprintf(stderr, PFX "Fatal: ABI version %d of %s is not supported "
			"(min supported %d, max supported %d)\n",
			abi_version, uverbs_sys_path,
			MLX5_UVERBS_MIN_ABI_VERSION,
			MLX5_UVERBS_MAX_ABI_VERSION);
		return NULL;
	}

	dev = malloc(sizeof *dev);
	if (!dev) {
		fprintf(stderr, PFX "Fatal: couldn't allocate device for %s\n",
			uverbs_sys_path);
		return NULL;
	}

	dev->page_size = sysconf(_SC_PAGESIZE);

	dev->devid.id = device;
	dev->driver_abi_ver = abi_version;

	dev->verbs_dev.sz = sizeof(dev->verbs_dev);
	dev->verbs_dev.size_of_context =
		sizeof(struct mlx5_context) - sizeof(struct ibv_context);

	/*
	 * mlx5_init_context will initialize provider calls
	 */
	dev->verbs_dev.init_context = mlx5_alloc_context;
	dev->verbs_dev.uninit_context = mlx5_free_context;

	return &dev->verbs_dev;
}

#ifdef HAVE_IBV_REGISTER_DRIVER
static __attribute__((constructor)) void mlx5_register_driver(void)
{
	verbs_register_driver("mlx5", mlx5_driver_init);
}
#else
/*
 * Export the old libsysfs sysfs_class_device-based driver entry point
 * if libibverbs does not export an ibv_register_driver() function.
 */
struct ibv_device *openib_driver_init(struct sysfs_class_device *sysdev)
{
	int abi_ver = 0;
	char value[8];

	if (ibv_read_sysfs_file(sysdev->path, "abi_version",
				value, sizeof value) > 0)
		abi_ver = strtol(value, NULL, 10);

	return mlx5_driver_init(sysdev->path, abi_ver);
}
#endif /* HAVE_IBV_REGISTER_DRIVER */
