/* Copyright (c) 2011 Freescale Semiconductor, Inc.
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

/* System headers */
#include <inttypes.h>
#include <unistd.h>
#include <stdio.h>
#include <limits.h>
#include <sched.h>
#include <signal.h>
#include <netinet/if_ether.h>
#include <netinet/ip.h>
#include <stdbool.h>

/* USDPAA APIs */
#include <usdpaa/fsl_usd.h>
#include <usdpaa/fsl_qman.h>
#include <usdpaa/fsl_bman.h>
#include <usdpaa/of.h>
#include <usdpaa/usdpaa_netcfg.h>
#include <usdpaa/dma_mem.h>

/* Define "stringify" macro to avoid needing internal/compat.h */
#define __stringify_1(x) #x
#define __stringify(x)	__stringify_1(x)

/* Alignment to use for cpu-local structs to avoid coherency problems. */
#define MAX_CACHELINE 64

/* Other constants */
#define WORKER_SLOWPOLL_BUSY 4
#define WORKER_SLOWPOLL_IDLE 400
#define WORKER_FASTPOLL_DQRR 16
#define NET_IF_NUM_TX 2
#define NET_IF_TX_PRIORITY 3
#define NET_IF_ADMIN_PRIORITY 4
#define NET_IF_RX_PRIORITY 4
#define NET_IF_RX_ANNOTATION_STASH 0
#define NET_IF_RX_DATA_STASH 1
#define NET_IF_RX_CONTEXT_STASH 0
#define CPU_SPIN_BACKOFF_CYCLES 512
#define NUM_POOL_CHANNELS 4
#define DMA_MAP_SIZE 0x1000000
static const char __PCD_PATH[] = __stringify(DEF_PCD_PATH);
static const char __CFG_PATH[] = __stringify(DEF_CFG_PATH);
static const char *PCD_PATH = __PCD_PATH;
static const char *CFG_PATH = __CFG_PATH;
/* Flag to determine if the user wants to run hello_reflector in
 * short circuit mode where core task is not performed.
 */
static bool short_circuit_mode;

/* Each thread is represented by a "worker" struct. It will exit when 'quit' is
 * set non-zero. The thread for 'cpu==0' will perform global init and set
 * 'init_done' once completed. */
struct worker {
	pthread_t id;
	volatile int quit;
	int cpu;
	int init_done;
} __attribute__((aligned(MAX_CACHELINE)));

/* Each "admin" FQ is represented by one of these */
#define ADMIN_FQ_RX_ERROR   0
#define ADMIN_FQ_RX_DEFAULT 1
#define ADMIN_FQ_TX_ERROR   2
#define ADMIN_FQ_TX_CONFIRM 3
#define ADMIN_FQ_NUM        4 /* Upper limit for loops */
struct net_if_admin {
	struct qman_fq fq;
	int idx; /* ADMIN_FQ_<x> */
};

/* Each "rx_hash" (PCD) FQ is represented by one of these */
struct net_if_rx {
	struct qman_fq fq;
	/* Each Rx FQ is "pre-mapped" to a Tx FQ. Eg. if there are 32 Rx FQs and
	 * 2 Tx FQs for each interface, then each Tx FQ will be reflecting
	 * frames from 16 Rx FQs. */
	uint32_t tx_fqid;
} __attribute__((aligned(MAX_CACHELINE)));

/* Each PCD FQ-range within an interface is represented by one of these */
struct net_if_rx_fqrange {
	struct net_if_rx *rx; /* array size ::rx_count */
	unsigned int rx_count;
	struct list_head list;
};

/* Each network interface is represented by one of these */
struct net_if {
	const struct fm_eth_port_cfg *cfg;
	struct qman_fq *tx_fqs; /* array size NET_IF_NUM_TX */
	struct net_if_admin admin[ADMIN_FQ_NUM];
	struct list_head rx_list; /* list of "struct net_if_rx_fqrange" */
};

static uint32_t sdqcr;
static uint32_t pchannels[NUM_POOL_CHANNELS];
static int received_sigint;
static struct net_if *interfaces;
static struct usdpaa_netcfg_info *netcfg;
static struct bman_pool *pool[64];
__thread struct qman_fq local_fq;
static unsigned int bpool_cnt[3] = { 0, 0, 1728 };

static void handle_sigint(int s)
{
	received_sigint = 1;
}

/* pre-declare the worker function, required by main() for thread-creation */
static void *worker_fn(void *__worker);

/* Macro to move to the next cmd-line arg and returns the value of argc */
#define ARGINC() ({ argv++; --argc; })

/* The main() function/thread creates the worker threads and then waits for
 * threads to exit or ctrl-c. All code outside the main() function is used by
 * the worker threads. */
int main(int argc, char *argv[])
{
	struct worker *workers;
	char *endptr;
	size_t sz = DMA_MAP_SIZE;
	/* Determine number of cores (==number of threads) */
	long ncpus = sysconf(_SC_NPROCESSORS_ONLN);
	/* Load the device-tree driver */
	int loop, tmpret, teardown, ret = of_init();

	while (ARGINC() > 0) {
		if (!strcmp(*argv, "-n")) {
			unsigned long val;
			if (!ARGINC()) {
				fprintf(stderr, "Missing argument to -n\n");
				exit(EXIT_FAILURE);
			}
			val = strtoul(*argv, &endptr, 0);
			if ((val == ULONG_MAX) || (*endptr != '\0')) {
				fprintf(stderr, "Invalid argument to -n (%s)\n",
					*argv);
				exit(EXIT_FAILURE);
			}
			if (!val || (val > ncpus)) {
				fprintf(stderr, "Out of range (-n %lu)\n", val);
				exit(EXIT_FAILURE);
			}
			ncpus = val;
		} else if (!strcmp(*argv, "-p")) {
			if (!ARGINC()) {
				fprintf(stderr, "Missing argument to -p\n");
				exit(EXIT_FAILURE);
			}
			PCD_PATH = *argv;
		} else if (!strcmp(*argv, "-c")) {
			if (!ARGINC()) {
				fprintf(stderr, "Missing argument to -c\n");
				exit(EXIT_FAILURE);
			}
			CFG_PATH = *argv;
		} else if (!strcmp(*argv, "-s")) {
			unsigned long val;
			if (!ARGINC()) {
				fprintf(stderr, "Missing argument to -s\n");
				exit(EXIT_FAILURE);
			}
			val = strtoul(*argv, &endptr, 0);
			if ((val == ULONG_MAX) || (*endptr != '\0') || !val) {
				fprintf(stderr, "Invalid argument to -s (%s)\n",
					*argv);
				exit(EXIT_FAILURE);
			}
			sz = (size_t)val;
		} else if (!strcmp(*argv, "-sc")) {
			short_circuit_mode = 1;
		} else if (!strcmp(*argv, "-b")) {
			unsigned long val;
			if (!ARGINC()) {
				fprintf(stderr, "Missing argument to -b\n");
				exit(EXIT_FAILURE);
			}
			val = strtoul(*argv, &endptr, 0);
			if ((val == ULONG_MAX) || (*endptr != ':'))
				goto b_err;
			bpool_cnt[0] = val;
			val = strtoul(endptr + 1, &endptr, 0);
			if ((val == ULONG_MAX) || (*endptr != ':'))
				goto b_err;
			bpool_cnt[1] = val;
			val = strtoul(endptr + 1, &endptr, 0);
			if ((val == ULONG_MAX) || (*endptr != '\0'))
				goto b_err;
			bpool_cnt[2] = val;
			continue;
b_err:
			fprintf(stderr, "Invalid argument to -b (%s)\n", *argv);
			exit(EXIT_FAILURE);
		} else {
			fprintf(stderr, "Unknown argument '%s'\n", *argv);
			exit(EXIT_FAILURE);
		}
	}

	printf("Starting hello_reflector, ncpus=%ld\n", ncpus);

	if (ncpus < 1) {
		fprintf(stderr, "Fail: # processors: %ld\n", ncpus);
		exit(EXIT_FAILURE);
	}
	if (ret) {
		fprintf(stderr, "Fail: %s: %d\n", "of_init()", ret);
		exit(EXIT_FAILURE);
	}
	/* Parse FMC policy and configuration files for the network
	 * configuration. This also "extracts" other settings into 'netcfg' that
	 * are not necessarily from the XML files, such as the pool channels
	 * that the application is allowed to use (these are currently
	 * hard-coded into the netcfg code). */
	netcfg = usdpaa_netcfg_acquire(PCD_PATH, CFG_PATH);
	if (!netcfg) {
		fprintf(stderr, "Fail: usdpaa_netcfg_acquire(%s,%s)\n",
			PCD_PATH, CFG_PATH);
		exit(EXIT_FAILURE);
	}
	if (!netcfg->num_ethports) {
		fprintf(stderr, "Fail: no network interfaces available\n");
		exit(EXIT_FAILURE);
	}
	/* Install ctrl-c handler */
	if (signal(SIGINT, handle_sigint) == SIG_ERR) {
		fprintf(stderr, "Fail: %s\n", "signal(SIGINT)");
		exit(EXIT_FAILURE);
	}
	/* Allocate the worker structs */
	ret = posix_memalign((void **)&workers, MAX_CACHELINE,
			     ncpus * sizeof(*workers));
	if (ret) {
		fprintf(stderr, "Fail: %s: %d\n", "posix_memalign()", ret);
		exit(EXIT_FAILURE);
	}
	/* Load the Qman/Bman drivers */
	ret = qman_global_init();
	if (ret) {
		fprintf(stderr, "Fail: %s: %d\n", "qman_global_init()", ret);
		exit(EXIT_FAILURE);
	}
	ret = bman_global_init();
	if (ret) {
		fprintf(stderr, "Fail: %s: %d\n", "bman_global_init()", ret);
		exit(EXIT_FAILURE);
	}
	ret = qman_alloc_pool_range(&pchannels[0], NUM_POOL_CHANNELS, 1, 0);
	if (ret != NUM_POOL_CHANNELS) {
		fprintf(stderr, "Fail: no pool channels available\n");
		exit(EXIT_FAILURE);
	}
	/* Compute SDQCR */
	for (loop = 0; loop < NUM_POOL_CHANNELS; loop++)
		sdqcr |= QM_SDQCR_CHANNELS_POOL_CONV(pchannels[loop]);
	/* Load dma_mem driver */
	dma_mem_generic = dma_mem_create(DMA_MAP_FLAG_ALLOC, NULL, sz);
	if (!dma_mem_generic) {
		fprintf(stderr, "Fail: %s:\n", "dma_mem_create()");
		exit(EXIT_FAILURE);
	}
	printf("DMA region created of size %zu (0x%zx)\n", sz, sz);
	/* Start up the threads */
	for (loop = 0; loop < ncpus; loop++) {
		struct worker *worker = &workers[loop];
		worker->quit = worker->init_done = 0;
		worker->cpu = loop;
		ret = pthread_create(&worker->id, NULL, worker_fn, worker);
		if (ret) {
			fprintf(stderr, "Fail: %s(%d): %d\n", "pthread_create",
				loop, ret);
			while (--loop >= 0) {
				(--worker)->quit = 1;
				tmpret = pthread_join(worker->id, NULL);
				if (tmpret)
					fprintf(stderr, "Fail: %s(%d): %d\n",
						"pthread_join", loop, tmpret);
			}
			exit(EXIT_FAILURE);
		}
		/* Wait for thread init to complete (the first thread will
		 * complete global init as part of that) */
		while (!worker->init_done) {
			pthread_yield();
			if (!pthread_tryjoin_np(worker->id, NULL)) {
				fprintf(stderr, "Fail: primary thread init\n");
				exit(EXIT_FAILURE);
			}
		}
	}

	/* Threads are created, now manage them (and catch ctrl-c) */
	printf("Hit Ctrl-C (or send SIGINT) to terminate.\n");
	teardown = 0;
	while (ncpus) {
		if (!teardown) {
			if (received_sigint) {
				/* Ctrl-c signal triggers teardown */
				teardown = 1;
				printf("Ctrl-C, ending...\n");
				/* Non-primary threads should quit first */
				for (loop = 1; loop < ncpus; loop++)
					workers[loop].quit = 1;
			} else
				/* No teardown, no signal, this is where we can
				 * pause */
				sleep(1);
		} else {
			/* Once the primary thread is the only thread, it can
			 * quit too (global cleanup) */
			if (ncpus == 1)
				workers[0].quit = 1;
		}
		/* Reap loop */
		loop = 0;
		while (loop < ncpus) {
			struct worker *worker = &workers[loop];
			if (!pthread_tryjoin_np(worker->id, NULL)) {
				fprintf(stderr, "Exit: thread %d\n",
					worker->cpu);
				if (--ncpus > loop)
					memmove(worker, worker + 1,
						(ncpus - loop) *
						sizeof(*worker));
			} else
				loop++;
		}
	}
	qman_release_pool_range(pchannels[0], NUM_POOL_CHANNELS);

	printf("Finished hello_reflector\n");
	return 0;
}

/* All the following code is used by the worker threads */

/* Drop a frame (releases buffers to Bman) */
static inline void drop_frame(const struct qm_fd *fd)
{
	struct bm_buffer buf;
	int ret;

	BUG_ON(fd->format != qm_fd_contig);
	bm_buffer_set64(&buf, qm_fd_addr(fd));
retry:
	ret = bman_release(pool[fd->bpid], &buf, 1, 0);
	if (ret) {
		cpu_spin(CPU_SPIN_BACKOFF_CYCLES);
		goto retry;
	}
}

/* Transmit a frame */
static inline void send_frame(u32 fqid, const struct qm_fd *fd)
{
	int ret;
	local_fq.fqid = fqid;
retry:
	ret = qman_enqueue(&local_fq, fd, 0);
	if (ret) {
		cpu_spin(CPU_SPIN_BACKOFF_CYCLES);
		goto retry;
	}
}

/* DQRR callback used by Tx FQs (used when retiring and draining) as well as
 * admin FQs ([rt]x_error, rx_default, tx_confirm). */
static enum qman_cb_dqrr_result cb_drop(struct qman_portal *qm __always_unused,
				      struct qman_fq *fq __always_unused,
				      const struct qm_dqrr_entry *dqrr)
{
	drop_frame(&dqrr->fd);
	return qman_cb_dqrr_consume;
}

/* Swap 6-byte MAC headers "efficiently" (hopefully) */
static inline void ether_header_swap(struct ether_header *prot_eth)
{
	register u32 a, b, c;
	u32 *overlay = (u32 *)prot_eth;
	a = overlay[0];
	b = overlay[1];
	c = overlay[2];
	overlay[0] = (b << 16) | (c >> 16);
	overlay[1] = (c << 16) | (a >> 16);
	overlay[2] = (a << 16) | (b >> 16);
}

/* DQRR callback for Rx FQs, this is the essential "reflector" logic (together
 * with the ether_header_swap() helper above that it uses). */
static enum qman_cb_dqrr_result cb_rx(struct qman_portal *qm __always_unused,
				      struct qman_fq *fq,
				      const struct qm_dqrr_entry *dqrr)
{
	struct ether_header *prot_eth;
	const struct qm_fd *fd = &dqrr->fd;
	struct net_if_rx *rx = container_of(fq, struct net_if_rx, fq);

	BUG_ON(fd->format != qm_fd_contig);
	prot_eth = __dma_mem_ptov(qm_fd_addr(fd)) + fd->offset;
	/* Broadcasts and non-IP packets are not reflected. */
	if (likely(!(prot_eth->ether_dhost[0] & 0x01) &&
			(prot_eth->ether_type == ETHERTYPE_IP))) {
		struct iphdr *iphdr = (typeof(iphdr))(prot_eth + 1);
		__be32 tmp;
		/* switch ipv4 src/dst addresses */
		tmp = iphdr->daddr;
		iphdr->daddr = iphdr->saddr;
		iphdr->saddr = tmp;
		/* switch ethernet src/dest MAC addresses */
		ether_header_swap(prot_eth);
		/* transmit */
		send_frame(rx->tx_fqid, fd);
	} else
		/* drop */
		drop_frame(fd);
	return qman_cb_dqrr_consume;
}

/* Initialise a Tx FQ */
static int net_if_tx_init(struct qman_fq *fq, const struct fman_if *fif)
{
	struct qm_mcc_initfq opts;
	int ret;

	fq->cb.dqrr = cb_drop;
	ret = qman_create_fq(0, QMAN_FQ_FLAG_DYNAMIC_FQID |
			     QMAN_FQ_FLAG_TO_DCPORTAL, fq);
	if (ret)
		return ret;
	opts.we_mask = QM_INITFQ_WE_DESTWQ | QM_INITFQ_WE_FQCTRL |
		       QM_INITFQ_WE_CONTEXTB | QM_INITFQ_WE_CONTEXTA;
	opts.fqd.dest.channel = fif->tx_channel_id;
	opts.fqd.dest.wq = NET_IF_TX_PRIORITY;
	opts.fqd.fq_ctrl = QM_FQCTRL_PREFERINCACHE;
	opts.fqd.context_b = 0;
	/* no tx-confirmation */
	opts.fqd.context_a.hi = 0x80000000 | fman_dealloc_bufs_mask_hi;
	opts.fqd.context_a.lo = 0 | fman_dealloc_bufs_mask_lo;
	return qman_init_fq(fq, QMAN_INITFQ_FLAG_SCHED, &opts);
}

/* Utility to select one of the available pool channels in a round-robin manner.
 * As software-consumed FQs are initialised, this function is called each time
 * in order to spread the FQs around the pool-channels. */
static u16 get_next_rx_channel(void)
{
	static uint32_t pchannel_idx;
	u16 ret = pchannels[pchannel_idx];
	pchannel_idx = (pchannel_idx + 1) % NUM_POOL_CHANNELS;
	return ret;
}

/* Helper to determine whether an admin FQ is used on the given interface */
static int net_if_admin_is_used(struct net_if *interface, int idx)
{
	if ((idx < 0) || (idx >= ADMIN_FQ_NUM))
		return 0;
	/* Offline ports don't support tx_error nor tx_confirm */
	if ((idx <= ADMIN_FQ_RX_DEFAULT) ||
			(interface->cfg->fman_if->mac_type != fman_offline))
		return 1;
	return 0;
}

/* Initialise a admin FQ ([rt]x_error, rx_default, tx_confirm). */
static int net_if_admin_init(struct net_if_admin *a, uint32_t fqid, int idx)
{
	struct qm_mcc_initfq opts;
	int ret;

	ret = qman_reserve_fqid(fqid);
	if (ret)
		return -EINVAL;
	a->idx = idx;
	a->fq.cb.dqrr = cb_drop;
	ret = qman_create_fq(fqid, QMAN_FQ_FLAG_NO_ENQUEUE, &a->fq);
	if (ret)
		return ret;
	opts.we_mask = QM_INITFQ_WE_DESTWQ;
	opts.fqd.dest.channel = get_next_rx_channel();
	opts.fqd.dest.wq = NET_IF_ADMIN_PRIORITY;
	return qman_init_fq(&a->fq, QMAN_INITFQ_FLAG_SCHED, &opts);
}

/* Initialise an Rx FQ */
static int net_if_rx_init(struct net_if *interface,
			  struct net_if_rx_fqrange *fqrange,
			  int offset, int overall,
			  uint32_t fqid)
{
	struct net_if_rx *rx = &fqrange->rx[offset];
	struct qm_mcc_initfq opts;
	int ret;
	uint32_t flags = QMAN_FQ_FLAG_NO_ENQUEUE;

	ret = qman_reserve_fqid(fqid);
	if (ret)
		return -EINVAL;
	/* "map" this Rx FQ to one of the interfaces Tx FQID */
	if (!short_circuit_mode)
		rx->tx_fqid = interface->tx_fqs[overall % NET_IF_NUM_TX].fqid;
	else
		flags = flags | QMAN_FQ_FLAG_TO_DCPORTAL;

	rx->fq.cb.dqrr = cb_rx;
	ret = qman_create_fq(fqid, flags, &rx->fq);
	if (ret)
		return ret;
	/* User may want to run the application in short circuit mode.
	 * Here packets are dequeued from the interface and received on
	 * Rx FQs. These packets are then sent out from the QMAN channel
	 * on which Tx FQs are scheduled. We can be sure of the sanity
	 * of the hardware path for the packet flow by this way without
	 * any processing by the core on the received packets.
	 */
	if (!short_circuit_mode) {
		opts.we_mask = QM_INITFQ_WE_DESTWQ | QM_INITFQ_WE_FQCTRL |
				QM_INITFQ_WE_CONTEXTA;
		opts.fqd.dest.channel = get_next_rx_channel();
		opts.fqd.dest.wq = NET_IF_RX_PRIORITY;
		opts.fqd.fq_ctrl =
			QM_FQCTRL_AVOIDBLOCK | QM_FQCTRL_CTXASTASHING |
			QM_FQCTRL_PREFERINCACHE;
		opts.fqd.context_a.stashing.exclusive = 0;
		opts.fqd.context_a.stashing.annotation_cl =
			NET_IF_RX_ANNOTATION_STASH;
		opts.fqd.context_a.stashing.data_cl = NET_IF_RX_DATA_STASH;
		opts.fqd.context_a.stashing.context_cl =
			NET_IF_RX_CONTEXT_STASH;
	} else {
		opts.we_mask = QM_INITFQ_WE_DESTWQ | QM_INITFQ_WE_FQCTRL |
				QM_INITFQ_WE_CONTEXTB | QM_INITFQ_WE_CONTEXTA;
		opts.fqd.dest.channel = interface->cfg->fman_if->tx_channel_id;
		opts.fqd.dest.wq = NET_IF_TX_PRIORITY;
		opts.fqd.fq_ctrl = QM_FQCTRL_PREFERINCACHE;
		opts.fqd.context_b = 0;
		/* no tx-confirmation */
		opts.fqd.context_a.hi = 0x80000000 | fman_dealloc_bufs_mask_hi;
		opts.fqd.context_a.lo = 0 | fman_dealloc_bufs_mask_lo;
	}
	return qman_init_fq(&rx->fq, QMAN_INITFQ_FLAG_SCHED, &opts);
}

/* Initialise a network interface */
static int net_if_init(struct net_if *interface,
		       const struct fm_eth_port_cfg *cfg)
{
	const struct fman_if *fif = cfg->fman_if;
	struct fm_eth_port_fqrange *fq_range;
	int ret = 0, loop;

	interface->cfg = cfg;

	/* Initialise Tx FQs */
	if (!short_circuit_mode) {
		interface->tx_fqs = calloc(NET_IF_NUM_TX,
					sizeof(interface->tx_fqs[0]));
		if (!interface->tx_fqs)
			return -ENOMEM;
		for (loop = 0; loop < NET_IF_NUM_TX; loop++) {
			ret = net_if_tx_init(&interface->tx_fqs[loop], fif);
			if (ret)
				return ret;
		}
	}

	/* Initialise admin FQs */
	if (!ret && net_if_admin_is_used(interface, ADMIN_FQ_RX_ERROR))
		ret = net_if_admin_init(&interface->admin[ADMIN_FQ_RX_ERROR],
					fif->fqid_rx_err,
					ADMIN_FQ_RX_ERROR);
	if (!ret && net_if_admin_is_used(interface, ADMIN_FQ_RX_DEFAULT))
		ret = net_if_admin_init(&interface->admin[ADMIN_FQ_RX_DEFAULT],
					cfg->rx_def,
					ADMIN_FQ_RX_DEFAULT);
	if (!ret && net_if_admin_is_used(interface, ADMIN_FQ_TX_ERROR))
		ret = net_if_admin_init(&interface->admin[ADMIN_FQ_TX_ERROR],
					fif->fqid_tx_err,
					ADMIN_FQ_TX_ERROR);
	if (!ret && net_if_admin_is_used(interface, ADMIN_FQ_TX_CONFIRM))
		ret = net_if_admin_init(&interface->admin[ADMIN_FQ_TX_CONFIRM],
					fif->fqid_tx_confirm,
					ADMIN_FQ_TX_CONFIRM);
	if (ret)
		return ret;

	/* Initialise each Rx FQ-range for the interface */
	INIT_LIST_HEAD(&interface->rx_list);
	loop = 0;
	list_for_each_entry(fq_range, cfg->list, list) {
		int tmp;
		struct net_if_rx_fqrange *newrange = malloc(sizeof(*newrange));
		if (!newrange)
			return -ENOMEM;
		newrange->rx_count = fq_range->count;
		newrange->rx = __dma_mem_memalign(MAX_CACHELINE,
				newrange->rx_count * sizeof(newrange->rx[0]));
		if (!newrange->rx)
			return -ENOMEM;
		memset(newrange->rx, 0,
		       newrange->rx_count * sizeof(newrange->rx[0]));
		/* Initialise each Rx FQ within the range */
		for (tmp = 0; tmp < fq_range->count; tmp++, loop++) {
			ret = net_if_rx_init(interface, newrange, tmp, loop,
					     fq_range->start + tmp);
			if (ret)
				return ret;
		}
		/* Range initialised, at it to the interface's rx-list */
		list_add_tail(&newrange->list, &interface->rx_list);
	}

	/* Enable RX and TX */
	fman_if_enable_rx(fif);
	return 0;
}

/* Retire, drain, OOS a FQ. (Dynamically-allocated FQIDs will be released.) */
static void teardown_fq(struct qman_fq *fq)
{
	u32 flags;
	int s = qman_retire_fq(fq, &flags);
	if (s == 1) {
		/* Retire is non-blocking, poll for completion */
		enum qman_fq_state state;
		do {
			qman_poll();
			qman_fq_state(fq, &state, &flags);
		} while (state != qman_fq_state_retired);
		if (flags & QMAN_FQ_STATE_NE) {
			/* FQ isn't empty, drain it */
			s = qman_volatile_dequeue(fq, 0,
				QM_VDQCR_NUMFRAMES_TILLEMPTY);
			if (s) {
				fprintf(stderr, "Fail: %s: %d\n",
					"qman_volatile_dequeue()", s);
				return;
			}
			/* Poll for completion */
			do {
				qman_poll();
				qman_fq_state(fq, &state, &flags);
			} while (flags & QMAN_FQ_STATE_VDQCR);
		}
	}
	s = qman_oos_fq(fq);
	if (!(fq->flags & QMAN_FQ_FLAG_DYNAMIC_FQID))
		qman_release_fqid(fq->fqid);
	if (s)
		fprintf(stderr, "Fail: %s: %d\n", "qman_oos_fq()", s);
	else
		qman_destroy_fq(fq, 0);
}

/* Tear down a network interface */
static void net_if_finish(struct net_if *interface,
			  const struct fm_eth_port_cfg *cfg)
{
	const struct fman_if *fif = cfg->fman_if;
	struct net_if_rx_fqrange *rx_fqrange;
	int loop;

	/* Disable Rx */
	fman_if_disable_rx(fif);

	/* Cleanup Rx FQs */
	list_for_each_entry(rx_fqrange, &interface->rx_list, list)
		for (loop = 0; loop < rx_fqrange->rx_count; loop++)
			teardown_fq(&rx_fqrange->rx[loop].fq);

	/* Cleanup admin FQs */
	if (net_if_admin_is_used(interface, ADMIN_FQ_RX_ERROR))
		teardown_fq(&interface->admin[ADMIN_FQ_RX_ERROR].fq);
	if (net_if_admin_is_used(interface, ADMIN_FQ_RX_DEFAULT))
		teardown_fq(&interface->admin[ADMIN_FQ_RX_DEFAULT].fq);
	if (net_if_admin_is_used(interface, ADMIN_FQ_TX_ERROR))
		teardown_fq(&interface->admin[ADMIN_FQ_TX_ERROR].fq);
	if (net_if_admin_is_used(interface, ADMIN_FQ_TX_CONFIRM))
		teardown_fq(&interface->admin[ADMIN_FQ_TX_CONFIRM].fq);

	/* Cleanup Tx FQs */
	if (!short_circuit_mode) {
		for (loop = 0; loop < NET_IF_NUM_TX; loop++)
			teardown_fq(&interface->tx_fqs[loop]);
	}
}

static void init_bpid(int bpid, uint64_t count, uint64_t sz)
{
	struct bm_buffer bufs[8];
	unsigned int num_bufs = 0;
	int ret = 0;

	if (pool[bpid])
		/* Already initialised */
		return;
	/* Drain (if necessary) then seed buffer pools */
	if (!pool[bpid]) {
		struct bman_pool_params params = {
			.bpid = bpid
		};
		pool[bpid] = bman_new_pool(&params);
		if (!pool[bpid]) {
			fprintf(stderr, "error: bman_new_pool() failed\n");
			abort();
		}
	}
	/* Drain the pool of anything already in it. */
	do {
		/* Acquire is all-or-nothing, so we drain in 8s, then in 1s for
		 * the remainder. */
		if (ret != 1)
			ret = bman_acquire(pool[bpid], bufs, 8, 0);
		if (ret < 8)
			ret = bman_acquire(pool[bpid], bufs, 1, 0);
		if (ret > 0)
			num_bufs += ret;
	} while (ret > 0);
	if (num_bufs)
		fprintf(stderr, "Warn: drained %u bufs from BPID %d\n",
			num_bufs, bpid);
	/* Fill the pool */
	for (num_bufs = 0; num_bufs < count; ) {
		unsigned int loop, rel = (count - num_bufs) > 8 ? 8 :
					(count - num_bufs);
		for (loop = 0; loop < rel; loop++) {
			void *ptr = __dma_mem_memalign(64, sz);
			if (!ptr) {
				fprintf(stderr, "error: no buffer space\n");
				abort();
			}
			bm_buffer_set64(&bufs[loop], __dma_mem_vtop(ptr));
		}
		do {
			ret = bman_release(pool[bpid], bufs, rel, 0);
		} while (ret == -EBUSY);
		if (ret)
			fprintf(stderr, "Fail: %s\n", "bman_release()");
		num_bufs += rel;
	}
	printf("Released %u bufs to BPID %d\n", num_bufs, bpid);
}

/* Do all global-initialisation that is dependent on a portal-enabled USDPAA
 * thread. The first worker thread will call this. Driver initialisation happens
 * in main() prior to any worker threads being created so this stuff can assume
 * the drivers are already initialised. */
static int do_global_init(void)
{
	unsigned int loop;

	/* Create and initialise the network interfaces */
	interfaces = calloc(netcfg->num_ethports, sizeof(*interfaces));
	if (!interfaces)
		return -ENOMEM;
	for (loop = 0; loop < netcfg->num_ethports; loop++) {
		struct fman_if_bpool *bp;
		struct fm_eth_port_cfg *pcfg = &netcfg->port_cfg[loop];
		int bp_idx = 0, ret = net_if_init(&interfaces[loop], pcfg);
		if (ret) {
			fprintf(stderr, "Fail: net_if_init(%d)\n", loop);
			return ret;
		}
		/* initialise the associated pools */
		list_for_each_entry(bp, &pcfg->fman_if->bpool_list, node)
			init_bpid(bp->bpid, bpool_cnt[bp_idx++], bp->size);
	}
	return 0;
}

/* Do the opposite of do_globl_init(), this gets called as the last worker
 * thread is being torn-down. */
static void do_global_finish(void)
{
	unsigned int loop;
	/* Tear down the network interfaces */
	for (loop = 0; loop < netcfg->num_ethports; loop++)
		net_if_finish(&interfaces[loop], &netcfg->port_cfg[loop]);
}

/* This is the worker thread function. It sets up thread-affinity, sets up
 * thread-local portal resources, doing "global init" if it is the first/primary
 * thread then enters the run-to-completion loop. As/when the quit message is
 * seen, it exits the run-to-completion loop and tears down. */
static void *worker_fn(void *__worker)
{
	struct worker *worker = __worker;
	cpu_set_t cpuset;
	int ret, slowpoll = 0, calm_down = 16;

	printf("(%d): Starting\n", worker->cpu);

	/* Set CPU affinity */
	CPU_ZERO(&cpuset);
	CPU_SET(worker->cpu, &cpuset);
	ret = pthread_setaffinity_np(worker->id, sizeof(cpu_set_t), &cpuset);
	if (ret) {
		fprintf(stderr, "(%d): Fail: %s\n", worker->cpu,
			"pthread_setaffinity_np()");
		return NULL;
	}

	/* Initialise thread/cpu-local portals */
	ret = bman_thread_init();
	if (ret) {
		fprintf(stderr, "(%d): Fail: %s\n", worker->cpu,
			"bman_thread_init()");
		goto fail_bman;
	}
	ret = qman_thread_init();
	if (ret) {
		fprintf(stderr, "(%d): Fail: %s\n", worker->cpu,
			"qman_thread_init()");
		goto fail_qman;
	}

	/* Initialise thread/cpu-local enqueue object */
	ret = qman_create_fq(1, QMAN_FQ_FLAG_NO_MODIFY, &local_fq);
	if (ret) {
		fprintf(stderr, "(%d): Fail: %s\n", worker->cpu,
			"qman_create_fq()");
		goto fail_eq;
	}

	/* Set the qman portal's SDQCR mask */
	qman_static_dequeue_add(sdqcr);

	/* If we're the primary thread, do global init */
	if (!worker->cpu) {
		ret = do_global_init();
		if (ret)
			goto fail_global_init;
	}

	/* Indicate that our thread-init is complete. */
	worker->init_done = 1;

	/* The run-to-completion loop */
	while (!worker->quit) {
		if (!(slowpoll--)) {
			if (qman_poll_slow() || bman_poll_slow())
				slowpoll = WORKER_SLOWPOLL_BUSY;
			else
				slowpoll = WORKER_SLOWPOLL_IDLE;
		}
		qman_poll_dqrr(WORKER_FASTPOLL_DQRR);
	}

	/* If we're the primary thread, do global cleanup */
	if (!worker->cpu)
		do_global_finish();

fail_global_init:
	qman_static_dequeue_del(~(uint32_t)0);
fail_eq:
	while (calm_down--) {
		qman_poll_slow();
		qman_poll_dqrr(16);
	}
	qman_thread_finish();
fail_qman:
	bman_thread_finish();
fail_bman:
	printf("(%d): Finished\n", worker->cpu);
	pthread_exit(NULL);
}

