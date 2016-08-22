/* Copyright (c) 2014 Freescale Semiconductor, Inc.
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

#define __USE_GNU
#include <sched.h>
#include <signal.h>
#include <usdpaa/usdpaa_netcfg.h>
#include "helper_api.h"
#define POOL_NUM_IN_DTS 3
#define ARGINC() ({ argv++; --argc; })
#define __stringify_1(x) #x
#define __stringify(x)	__stringify_1(x)

struct worker {
	int cpu, do_global_init;
	int idx, total_cpus;
	pthread_t id;
	struct list_head node;
};

static pthread_barrier_t barr;
static u32 ncpus;
static bool app_running = true;
static LIST_HEAD(workers);

char *app_cfg_path = (char *)__stringify(DEF_CFG_PATH);
char *app_pcd_path = (char *)__stringify(DEF_PCD_PATH);
int app_pool_cnt[POOL_NUM_IN_DTS] = {0x400, 0x400, 0x400};
struct usdpaa_netcfg_info *app_net_cfg = NULL;
struct packet_pool_cfg app_pool_cfg[HLP_BPOOL_MAX + 1] = {
	{-1, 0, 0}
};

/* interface param table, defined by usdpaa_config_*.xml file */
static struct interface_param app_if_param_table[] = {
	/* port, type, fman, mac id, name      */
	{0x00, fman_mac_1g, 0, 0, "fm0-dtsec0"},
	{0x01, fman_mac_1g, 0, 1, "fm0-dtsec1"},
	{0x02, fman_mac_1g, 0, 2, "fm0-dtsec2"},
	{0x03, fman_mac_1g, 0, 3, "fm0-dtsec3"},
	{0x04, fman_mac_1g, 0, 4, "fm0-dtsec4"},
	{0x05, fman_mac_1g, 0, 5, "fm0-dtsec5"},
	{0x06, fman_mac_10g, 0, 0, "fm0-tgec0"},
	{0x07, fman_mac_10g, 0, 1, "fm0-tgec1"},

	{0x08, fman_mac_1g, 1, 0, "fm1-dtsec0"},
	{0x09, fman_mac_1g, 1, 1, "fm1-dtsec1"},
	{0x0a, fman_mac_1g, 1, 2, "fm1-dtsec2"},
	{0x0b, fman_mac_1g, 1, 3, "fm1-dtsec3"},
	{0x0c, fman_mac_1g, 1, 4, "fm1-dtsec4"},
	{0x0d, fman_mac_1g, 1, 5, "fm1-dtsec5"},
	{0x0e, fman_mac_10g, 1, 0, "fm1-tgec0"},
	{0x0f, fman_mac_10g, 1, 1, "fm1-tgec1"},

	{0x10, fman_mac_10g, 0, 2, "fm0-tgec2"},
	{0x11, fman_mac_10g, 0, 3, "fm0-tgec3"},
	{0x12, fman_mac_10g, 1, 2, "fm1-tgec2"},
	{0x13, fman_mac_10g, 1, 3, "fm1-tgec3"},

	/* the end of the table */
	{-1, -1, -1, -1, NULL}
};

static int parse_cpu(const char *str, const char **legit, int legitlen)
{
	char *endptr;
	int ret = -EINVAL;
	/* Extract a ulong */
	unsigned long tmp = strtoul(str, &endptr, 0);
	if ((tmp == ULONG_MAX) || (endptr == str))
		goto out;
	/* Check next char */
	while (legitlen--) {
		if (**legit == *endptr) {
			/* validate range */
			if (tmp >= ncpus) {
				ret = -ERANGE;
				goto out;
			}
			*legit = endptr;
			return (int)tmp;
		}
		(*legit)++;
	}
out:
	fprintf(stderr, "error: invalid cpu '%s'\n", str);
	return ret;
}

/* Parse a cpu range (eg. "3"=="3..3"). Return 0 for valid parse. */
static int parse_cpus(const char *str, int *start, int *end)
{
	/* NB: arrays of chars, not strings. Also sizeof(), not strlen()! */
	static const char PARSE_STR1[] = { ' ', '.', '\0' };
	static const char PARSE_STR2[] = { ' ', '\0' };
	const char *p = &PARSE_STR1[0];
	int ret;
	ret = parse_cpu(str, &p, sizeof(PARSE_STR1));
	if (ret < 0)
		return ret;
	*start = ret;
	if ((p[0] == '.') && (p[1] == '.')) {
		const char *p2 = &PARSE_STR2[0];
		ret = parse_cpu(p + 2, &p2, sizeof(PARSE_STR2));
		if (ret < 0)
			return ret;
	}
	*end = ret;
	return 0;
}

static void parse_args(int argc, char *argv[], int *first, int *last)
{
	int ret;
	char *endptr;
	int pool_cnt[POOL_NUM_IN_DTS];

	/* parse cpu as traditional style "m..n" */
	ncpus = (unsigned long)sysconf(_SC_NPROCESSORS_ONLN);
	if (ncpus == 1)
		*first = *last = 0;
	else {
		*first = 1;
		*last = ncpus - 1;
	}
	if (argc == 1)
		return;
	else {
		ARGINC();
		ret = parse_cpus(*argv, first, last);
		if (ret) {
			fprintf(stderr, "Wrong cpu argument (m..n)\n");
			exit(EXIT_FAILURE);
		}
	}

	/* parse the other arguments */
	while (ARGINC() > 0) {
		if (!strcmp(*argv, "-p")) {
			if (!ARGINC()) {
				fprintf(stderr, "Missing argument to -p\n");
				exit(EXIT_FAILURE);
			}
			app_pcd_path = *argv;
		} else if (!strcmp(*argv, "-c")) {
			if (!ARGINC()) {
				fprintf(stderr, "Missing argument to -c\n");
				exit(EXIT_FAILURE);
			}
			app_cfg_path = *argv;
		} else if (!strcmp(*argv, "-b")) {
			if (!ARGINC()) {
				fprintf(stderr, "Missing argument to -b\n");
				exit(EXIT_FAILURE);
			}
			pool_cnt[0] = strtoul(*argv, &endptr, 0);
			if ((pool_cnt[0] == ULONG_MAX) || (*endptr != ':'))
				goto b_err;
			pool_cnt[1] = strtoul(endptr + 1, &endptr, 0);
			if ((pool_cnt[1] == ULONG_MAX) || (*endptr != ':'))
				goto b_err;
			pool_cnt[2] = strtoul(endptr + 1, &endptr, 0);
			if ((pool_cnt[2] == ULONG_MAX) || (*endptr != '\0'))
				goto b_err;

			memcpy(app_pool_cnt, pool_cnt, sizeof(pool_cnt));

			DEBUG("pool count: %d:%d:%d\n", pool_cnt[0],
				pool_cnt[1], pool_cnt[2]);
			continue;
b_err:
			fprintf(stderr, "Invalid argument to -b (%s)\n", *argv);
			exit(EXIT_FAILURE);

		} else {
			fprintf(stderr, "Unknown argument '%s'\n", *argv);
			exit(EXIT_FAILURE);
		}
	}
}

static void parse_buffer_pool(void)
{
	int j;
	int num_of_pools;
	struct packet_pool_cfg *pool_cfg = app_pool_cfg;
	struct fman_if *pfif;

	/* check the configured pool in code */
	num_of_pools = 0;
	for (j = 0; j < (HLP_BPOOL_MAX + 1); j++) {
		if (pool_cfg[j].bpid == -1)
			break;
		num_of_pools++;
	}

	/* parse the DTS pool */
	list_for_each_entry(pfif, fman_if_list, node) {
		int idx = 0;
		struct fman_if_bpool *bp;
		list_for_each_entry(bp, &pfif->bpool_list, node) {
			/* scan and overwrite the prev pool cfg */
			for (j = 0; j < num_of_pools; j++) {
				if (bp->bpid == pool_cfg[j].bpid) {
					pool_cfg[j].size = bp->size;
					pool_cfg[j].num = app_pool_cnt[idx++];
					break;
				}
			}

			/* add to the end of pool cfg array */
			if (j == num_of_pools) {
				pool_cfg[j].bpid = bp->bpid;
				pool_cfg[j].size = bp->size;
				pool_cfg[j].num = app_pool_cnt[idx++];
				num_of_pools++;
				pool_cfg[num_of_pools].bpid = -1;

				DEBUG("Pool[%d]: size %d, num %d\n",
					pool_cfg[j].bpid,
					pool_cfg[j].size,
					pool_cfg[j].num);
			}
		}
	}
}

static struct usdpaa_netcfg_info *parse_pcd_file(void)
{
	/* parse the xml files */
	struct usdpaa_netcfg_info *net_cfg =
		usdpaa_netcfg_acquire(app_pcd_path, app_cfg_path);
	if (!net_cfg) {
		fprintf(stderr, "Fail: usdpaa_netcfg_acquire(%s,%s)\n",
			app_pcd_path, app_cfg_path);
		exit(EXIT_FAILURE);
	}

	DEBUG("PCD file(%s,%s)\n", app_pcd_path, app_cfg_path);
	DEBUG("Eth port number %d\n", net_cfg->num_ethports);
	if (!net_cfg->num_ethports) {
		fprintf(stderr, "Fail: no network interfaces available\n");
		exit(EXIT_FAILURE);
	}

	return net_cfg;
}

static struct interface_pcd_param *parse_pcd_param(
	struct usdpaa_netcfg_info *net_cfg,
	struct interface_param *if_param)
{
	int loop;
	int num_of_fqrs;
	struct fm_eth_port_cfg *pcfg;
	struct fm_eth_port_fqrange *eth_fqr;
	struct interface_fq_range *if_fqr;
	struct interface_pcd_param *pcd_param;

	for (loop = 0; loop < net_cfg->num_ethports; loop++) {
		pcfg = &net_cfg->port_cfg[loop];
		if (pcfg->fman_if->mac_type == if_param->type &&
			pcfg->fman_if->mac_idx == if_param->id &&
			pcfg->fman_if->fman_idx == if_param->fman) {
			/* count the fqr number */
			num_of_fqrs = 0;
			list_for_each_entry(eth_fqr, pcfg->list, list)
				num_of_fqrs++;

			/* allocate fqr structure */
			pcd_param = DMA_MEM_ALLOC(L1_CACHE_BYTES,
				sizeof(struct interface_pcd_param) +
				sizeof(struct interface_fq_range)*num_of_fqrs);

			DEBUG("FQ ranges %d\n", num_of_fqrs);
			if_fqr = pcd_param->rx_fq_range;
			list_for_each_entry(eth_fqr, pcfg->list, list) {
				DEBUG(" FQR start 0x%x\n", eth_fqr->start);
				DEBUG(" FQR count 0x%x\n", eth_fqr->count);
				if_fqr->count = eth_fqr->count;
				if_fqr->start = eth_fqr->start;
				if_fqr->fq_type = 0;
				if_fqr->channel = 0;
				if_fqr++;
			}
			pcd_param->port = if_param->port;
			pcd_param->num_rx_fq_ranges = num_of_fqrs;

			return pcd_param;
		}
	}

	return NULL;
}

static inline void sync_all(void)
{
	pthread_barrier_wait(&barr);
}

static inline void calm_down(void)
{
	qman_poll_dqrr(16);
	qman_poll_slow();
	bman_poll();
	sync_all();
}

static inline void ether_header_swap(u8 *eth_hdr)
{
	register u32 a, b, c;
	u32 *overlay = (u32 *) eth_hdr;
	a = overlay[0];
	b = overlay[1];
	c = overlay[2];
	overlay[0] = (b << 16) | (c >> 16);
	overlay[1] = (c << 16) | (a >> 16);
	overlay[2] = (a << 16) | (b >> 16);
}

static inline void packet_reflector(u8 *eth_hdr)
{
	struct iphdr *iphdr;
	u32 tmp;

	ether_header_swap(eth_hdr);

	/* switch ipv4 src/dst addresses */
	iphdr = (struct iphdr *)(eth_hdr + 14);
	tmp = iphdr->daddr;
	iphdr->daddr = iphdr->saddr;
	iphdr->saddr = tmp;
}

static inline void reflector_process(void)
{
	struct packet_desc *pdesc;
	int ret;

	if (fsl_pkt_recv(&pdesc) == OK) {
		packet_reflector((u8 *) pdesc->payload_addr);
		ret = fsl_pkt_send(pdesc);
		if (ret < 0)
			fsl_pkt_drop(pdesc);
	}
}

static void reflector_init(int global)
{
	/* init buffer pool at first */
	parse_buffer_pool();
	fsl_buffer_pool_init(global, app_pool_cfg);

	if (global) {
		struct interface_param *if_param;
		struct interface_pcd_param *pcd_param;
		struct usdpaa_netcfg_info *net_cfg = parse_pcd_file();

		/* init the interfaces based on if table */
		for (if_param = app_if_param_table; if_param->port != -1;
			 if_param++) {
			fsl_interface_init(if_param);

			/* init pcd if based on pcd files */
			pcd_param = parse_pcd_param(net_cfg, if_param);
			if (pcd_param) {
				fsl_interface_pcd_init(pcd_param);
				DMA_MEM_FREE(pcd_param);
			}
		}

		/* release the net_cfg */
		usdpaa_netcfg_release(net_cfg);
	}

	sync_all();
}

static void reflector_exit(int global)
{
	calm_down();
	if (global) {
		struct interface_param *if_param;
		for (if_param = app_if_param_table; if_param->port != -1;
			 if_param++) {
			fsl_interface_pcd_clean(if_param->port);
			fsl_interface_clean(if_param->port);
		}
	}

	fsl_buffer_pool_clean();
	sync_all();
}

static int worker_main(struct worker *wker)
{
	int cpu = wker->cpu;
	int global = wker->do_global_init;

	/* init driver and application */
	fsl_cpu_init(cpu, global, sync_all);
	reflector_init(global);

	TRACE("entering processing loop on cpu %d\n", get_cpu_id());
	while (likely(app_running))
		reflector_process();

	TRACE("exiting processing loop on cpu %d\n", get_cpu_id());
	reflector_exit(global);
	fsl_cpu_exit(global);

	return OK;
}

static void *worker_fn(void *__worker)
{
	int err;
	cpu_set_t cpuset;
	struct worker *worker = __worker;

	/* Set cpu affinity */
	/*CPU_ZERO(&cpuset);
	CPU_SET(worker->cpu, &cpuset);
	err = pthread_setaffinity_np(worker->id, sizeof(cpu_set_t), &cpuset);
	if (err != 0) {
		fprintf(stderr, "pthread_setaffinity_np(%d) failed, ret=%d\n",
			worker->cpu, err);
		exit(EXIT_FAILURE);
	}*/

	worker_main(worker);

	TRACE("Worker %d exiting\n", worker->cpu);
	return 0;
}

static struct worker *worker_new(int cpu, int do_global_init,
				 int idx, int total)
{
	struct worker *ret;
	int err = posix_memalign((void **)&ret, L1_CACHE_BYTES, sizeof(*ret));
	ret->cpu = cpu;
	ret->do_global_init = do_global_init;
	ret->idx = idx;
	ret->total_cpus = total;
	err = pthread_create(&ret->id, NULL, worker_fn, ret);
	if (err) {
		free(ret);
		goto out;
	}
	return ret;
out:
	fprintf(stderr, "error: failed to create thread for %d\n", cpu);
	return NULL;
}

/* Keep "workers" ordered by cpu on insert */
static void worker_add(struct worker *worker)
{
	struct worker *i;
	list_for_each_entry(i, &workers, node) {
		if (i->cpu >= worker->cpu) {
			list_add_tail(&worker->node, &i->node);
			return;
		}
	}
	list_add_tail(&worker->node, &workers);
}

static void worker_free(struct worker *worker)
{
	int err;
	list_del(&worker->node);
	err = pthread_join(worker->id, NULL);
	if (err) {
		/* Leak, but warn */
		fprintf(stderr, "Failed to join thread %d\n", worker->cpu);
		return;
	}
	free(worker);
}

static void worker_exit(int sig)
{
	TRACE("captured the exit signal %d\n", sig);
	app_running = false;
}

int main(int argc, char *argv[])
{
	struct worker *worker, *tmpw;
	int ret, first, last, loop;

	/* parse cli parameters */
	parse_args(argc, argv, &first, &last);

	/* Create the barrier used by sync_all() */
	ret = pthread_barrier_init(&barr, NULL, last - first + 1);
	if (ret != 0) {
		fprintf(stderr, "Failed to init barrier\n");
		exit(EXIT_FAILURE);
	}

	/* capture exit alarm signal for graceful shutdown */
	//signal(SIGTERM, (sighandler_t) worker_exit);
	//signal(SIGINT, (sighandler_t) worker_exit);

	/* Create the threads */
	for (loop = first; loop <= last; loop++) {
		worker = worker_new(loop, (loop == first), loop - first,
					last - first + 1);
		if (!worker)
			panic("worker_new() failed\n");
		worker_add(worker);
	}

	/* Catch their exit */
	list_for_each_entry_safe(worker, tmpw, &workers, node)
		worker_free(worker);

	return 0;
}

