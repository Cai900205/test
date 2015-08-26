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

/* System headers */
#include <assert.h>
#include <inttypes.h>
#include <unistd.h>
#include <stdio.h>
#include <limits.h>
#include <sched.h>
#include <signal.h>
#include <stdbool.h>
#include <semaphore.h>
#include <readline.h>
#include <time.h>
#include <sys/eventfd.h>

/* IPC stuff */
#include <ipc/ipc/include/fsl_ipc_errorcodes.h>
#include <ipc/ipc/include/fsl_usmmgr.h>
#include <ipc/ipc/include/fsl_ipc_types.h>
#include <ipc/ipc/include/fsl_bsc913x_ipc.h>
#include <ipc/ipc/include/dsp_boot.h>
#include <ipc/ipc/include/fsl_heterogeneous_l1_defense.h>

/* USDPAA APIs */
#include <internal/compat.h>

#include <usdpaa/compat.h>
#include <usdpaa/of.h>
#include <usdpaa/fsl_usd.h>
#include <usdpaa/fsl_qman.h>
#include <usdpaa/fsl_bman.h>

#define add_cli_cmd(cmd) install_cli_cmd(#cmd, cmd)
static const char cmd_prompt[] = "dsp_dpa> ";
static const char const *dsp_images[] = {
					"sh0.bin",	/* Shared image */
					"c0.bin",	/* DSP0 image */
					"c1.bin",	/* DSP1 image */
					"c2.bin",	/* DSP2 image */
					"c3.bin",	/* DSP3 image */
					"c4.bin",	/* DSP4 image */
					"c5.bin"	/* DSP5 image */
					};

struct fq_pair_t {
	struct qman_fq pa_fq;
	uint32_t dsp_id;
	uint32_t dsp_fqid;
} ____cacheline_aligned;

/* Other constants */
#define WORKER_SLOWPOLL_BUSY		4
#define WORKER_SLOWPOLL_IDLE		400
#define WORKER_FASTPOLL_DQRR		16
#define WORKER_SLOWPOLL_BUSY		4
#define WORKER_FASTPOLL_DOIRQ		2000

#define CPU_SPIN_BACKOFF_CYCLES		512
#define IPC_MSG_RING_BUF_SZ		1024
#define IPC_MSG_RING_BUF_NUM		8

/* Defined for B4860 */
#define NUM_DSP			6

#define for_each_dsp(d, m, t) for (t = m, d = 0; t; t = (t & ~(1 << d++))) \
						if (t & (1 << d))

/* config vars initialised with defaults*/
static unsigned long num_active_dsp = NUM_DSP;
static uint32_t active_dsp_mask;
static unsigned long msg_per_iteration = 1;
static unsigned long msg_len = 0x100;
static unsigned long bpool_buff_cnt = 8192;

/* Match these as defined in DSP side app */
static const u32 pa_rcv_chan_id[] = { 2, 4, 6, 8, 10, 12 };
static const u32 dsp_rcv_chan_id[] = { 1, 3, 5, 7, 9, 11 };

static fsl_ipc_t ipc;
static sem_t dsp_handshake_sem;
static int l1d_eventfd;
static bool sigint_recv;

struct test_stats {
	uint64_t num_sent;
	uint64_t num_recv;
	uint64_t bad_recv;
};

struct dsp_assoc_t {
	struct usdpaa_raw_portal bportal;
	struct usdpaa_raw_portal qportal;

	uint32_t bpid;
	mem_range_t msg_ring;		/* Recv msg ring */
	uint32_t dsp_mbox;
	uint32_t pa_mbox;
	bool dsp_ready;
	struct bman_pool *bpool;
	struct fq_pair_t fq_pair;
	uint32_t expected_seq;
	uint64_t iter_start_time;	/* ATB clock */
	uint64_t iter_end_time;
	int event_fd;
	mem_range_t bpool_mem;
	struct test_stats stats;
} dsp_assoc[NUM_DSP];

struct dsp_msg_t {
	uint32_t bpid;
	uint32_t seq;
	uint32_t pattern[0];
};

struct worker {
	pthread_t id;
	int cpu;
	int init_done;
} ____cacheline_aligned;

static uint32_t sdqcr;
static uint32_t pchannel;
static struct qman_fq local_fq;
static fsl_usmmgr_t usmmgr;

/* Macro to move to the next cmd-line arg and returns the value of argc */
#define ARGINC() ({ argv++; --argc; })

static void *p2vcb(unsigned long phys_addr)
{
	/* Typecasing below to avoid warnings due to IPC interface issue */
	return fsl_usmmgr_p2v((uint64_t)phys_addr, usmmgr);
}

/* Transmit a frame */
static inline int send_frame(u32 fqid, const struct qm_fd *fd)
{
	int ret;
	local_fq.fqid = fqid;
retry:
	ret = qman_enqueue(&local_fq, fd, 0);
	if (ret) {
		if (-EBUSY != ret)
			return ret;
		cpu_spin(CPU_SPIN_BACKOFF_CYCLES);
		goto retry;
	}

	return ret;
}

static void l1d_cb(uint32_t wsrsr)
{
	uint32_t dsp;
	uint64_t dspmask = 0;

	/* Now convert MPIC WSRSR to dspmask.
	 * DSPs start from offset 16 in register WSRSR.
	 */
	wsrsr = wsrsr >> 16;

	for (dsp = 0; dsp < NUM_DSP; dsp++) {
		if (wsrsr & 3) {
			dspmask = dspmask | (1 << dsp);
			dsp_assoc[dsp].dsp_ready = false;
		}

		wsrsr = wsrsr >> 2;
	}

	if (sizeof(dspmask) != write(l1d_eventfd, &dspmask, sizeof(dspmask)))
		pr_err("Writing to l1d_eventfd failed\n");
}

static int ipc_init(void)
{
	int ret;
	mem_range_t sh_ctrl;
	mem_range_t dsp_ccsr;
	mem_range_t pa_ccsr;

	/* Initialise PA DSP shared mem manager */
	usmmgr = fsl_usmmgr_init();
	if (!usmmgr) {
		pr_err("Error initializing USMMGR\n");
		return -1;
	}

	ret = get_pa_ccsr_area(&pa_ccsr, usmmgr);
	if (ret) {
		pr_err("Error %#x obtaining PA CCSR Area info\n", ret);
		return -1;
	}

	ret = get_dsp_ccsr_area(&dsp_ccsr, usmmgr);
	if (ret) {
		pr_err("Error %#x obtaining DSP CCSR Area info\n", ret);
		return -1;
	}

	ret = get_shared_ctrl_area(&sh_ctrl, usmmgr);
	if (ret) {
		pr_err("Error %#x obtaining Shared Control Area info\n", ret);
		return -1;
	}

	/* Init the IPC libs */
#define UIO_INTERFACE   ((char *)"/dev/uio0")
	ipc = fsl_ipc_init_rat(0, p2vcb, sh_ctrl, dsp_ccsr,
			       pa_ccsr, UIO_INTERFACE);
	if (!ipc) {
		pr_err("Issue with fsl_ipc_init_rat\n");
		return -1;
	}

	fsl_L1_defense_register_cb(l1d_cb);

	return 0;
}

static int dsp_ipc_mem_alloc(uint32_t dsp)
{
	int ret;
	struct dsp_assoc_t *assoc = &dsp_assoc[dsp];

	/* Open a receiver IPC channel for self */
	assoc->msg_ring.size = IPC_MSG_RING_BUF_SZ * IPC_MSG_RING_BUF_NUM;
	ret = fsl_usmmgr_alloc(&assoc->msg_ring, usmmgr);
	if (ret) {
		pr_err("Problem with fsl_usmmgr_alloc\n");
		return -1;
	}

	/* Allocate memory for bpool */
	assoc->bpool_mem.size = bpool_buff_cnt * msg_len;

	ret = fsl_usmmgr_alloc(&assoc->bpool_mem, usmmgr);
	if (ret) {
		fprintf(stderr, "Error allocating bpool mem\n");
		return -1;
	}

	/* Open up event_fd */
	assoc->event_fd = eventfd(0, 0);
	if (-1 == assoc->event_fd) {
		pr_err("eventfd create for dsp: %u\n", dsp);
		return -1;
	}

	return 0;
}


static int dsp_ipc_chan_init(uint32_t dsp_id)
{
	int ret;
	struct dsp_assoc_t *assoc = &dsp_assoc[dsp_id];

	ret = fsl_ipc_configure_channel(pa_rcv_chan_id[dsp_id],
					IPC_MSG_RING_BUF_NUM,
					IPC_MSG_CH,
					assoc->msg_ring.phys_addr,
					IPC_MSG_RING_BUF_SZ,
					NULL, ipc);
	if (ret) {
		pr_err("Error %d opening PA mbox\n", ret);
		return -1;
	}

	assoc->pa_mbox = pa_rcv_chan_id[dsp_id];

	ret = fsl_ipc_open_prod_ch(dsp_rcv_chan_id[dsp_id], ipc);
	if (ret) {
		pr_err("Error %d opening dsp mbox\n", ret);
		return -1;
	}

	assoc->dsp_mbox = dsp_rcv_chan_id[dsp_id];

	return 0;
}

static int pa_dpa_init(void)
{
	int ret;
	cpu_set_t cpuset;

	/* Load the Qman/Bman drivers */
	ret = qman_global_init();
	if (ret) {
		fprintf(stderr, "Fail: %s: %d\n", "qman_global_init()", ret);
		return -1;
	}
	ret = bman_global_init();
	if (ret) {
		fprintf(stderr, "Fail: %s: %d\n", "bman_global_init()", ret);
		return -1;
	}

	/* Set CPU affinity. This is required since main thread requires
	 * bportal and qportals.
	 */
	CPU_ZERO(&cpuset);
	CPU_SET(0, &cpuset);
	ret = pthread_setaffinity_np(pthread_self(), sizeof(cpuset), &cpuset);
	if (ret) {
		pr_err("pthread_setaffinity_np() failed for main thread\n");
		return -1;
	}

	/* Get hold of a BMAN portal for seeding bpools*/
	ret = bman_thread_init();
	if (ret) {
		pr_err("Allocating bman portal for main thread failed\n");
		return -1;
	}

	/* Get hold of a QMAN portal for starting frame enqueue to DSPs*/
	ret = qman_thread_init();
	if (ret) {
		pr_err("Allocating qman portal for main thread failed\n");
		return -1;
	}

	/* Allocate pool channel if required */
	ret = qman_alloc_pool_range(&pchannel, 1, 1, 0);
	if (ret != 1) {
		pr_err("qman_alloc_pool_range() failed\n");
		return -1;
	}

	/* Initialise enqueue object */
	ret = qman_create_fq(1, QMAN_FQ_FLAG_NO_MODIFY, &local_fq);
	if (ret) {
		pr_err("qman_create_fq() failed for local_fq\n");
		return -1;
	}

	return 0;
}

static void *ccsr_addr;

static int map_ccsr(void)
{
	int fd = open("/dev/mem", O_RDWR);
	if (-1 == fd) {
		perror("open failed\n");
		return -1;
	}

	ccsr_addr = mmap(NULL, 0x1000000, PROT_READ | PROT_WRITE, MAP_SHARED,
			 fd, 0xffe000000);
	if (!ccsr_addr) {
		perror("mmap failed\n");
		return -1;
	}

	printf("CCSRBAR = %#llx at %p\n", *(uint64_t *)ccsr_addr, ccsr_addr);

	return 0;
}

#ifdef DEBUG
static int cme_query(uint8_t dsp_id,
	      uint64_t phys_addr)	/*Physical address */
{
#define QCR	0xE014
#define QCC	0xE020
#define QCA	0xE024
#define QU1	0xE028
#define QU2	0xE02C
#define CR	0xE03C

	uint32_t cme_qcc, cme_qca, cme_cr, cme_qcr;
	uint32_t cluster = dsp_id >> 1;
	uintptr_t cme_addr = (uintptr_t)ccsr_addr +
				0xc40000 + (cluster * 0x40000);
	uint32_t qu1;
	uint32_t qu2;

	cme_qcc = ((phys_addr >> 32) << 6) | 0x11;
	cme_qca = phys_addr & 0xFFFFFFFF;

	cme_cr = *(uint32_t *)(cme_addr + CR);
	if (cme_cr & 1)
		return -1;

	/* issued the cme ext query command*/
	*(uint32_t *)(cme_addr + QCC) = cme_qcc;
	hwsync();
	*(uint32_t *)(cme_addr + QCA) = cme_qca;
	hwsync();

	/*poling to verified that external query or barrier was executed */
	do {
		cme_qcr = *(uint32_t *)(cme_addr + QCR);
		hwsync();
	} while (cme_qcr & 1);

	if (cme_qcr & 0x2)
		return -1;

	qu1 = *(uint32_t *)(cme_addr + QU1);
	qu2 = *(uint32_t *)(cme_addr + QU2);

	pr_info("QCC: %#x QCA: %#x\n", cme_qcc, cme_qca);
	pr_info("qu1 = %#x, qu2 = %#x\n", qu1, qu2);

	return 0;
}
#endif

static int verify_dsp_msg(const struct qm_fd *fd, uint32_t dsp_id)
{
	int i;
	uint16_t bpid;
	uint32_t words;
	const mem_range_t *bpool_mem = &dsp_assoc[dsp_id].bpool_mem;
	uint64_t phys_addr = qm_fd_addr(fd);
	struct dsp_msg_t *msg;

	if (phys_addr < bpool_mem->phys_addr ||
	    phys_addr > bpool_mem->phys_addr + bpool_mem->size ||
	    phys_addr & (msg_len - 1)) {
		pr_err("Invalid buffer in FD: %#llx\n", phys_addr);
		return -1;
	}
	hwsync();
	msg = fsl_usmmgr_p2v(phys_addr + fd->offset, usmmgr);

	bpid = bman_get_params(dsp_assoc[dsp_id].bpool)->bpid;
	if ((fd->bpid != bpid) || (msg->bpid != bpid)) {
		pr_err("BPID mismatch in msg from dsp[%u]\n", dsp_id);
		pr_err("Real: %u, FD: %u, Msg: %u\n",
		       bpid, fd->bpid, msg->bpid);
		return -1;
	}

	if (dsp_assoc[dsp_id].expected_seq != msg->seq) {
		pr_err("Seq mismatch in msg from dsp[%u]\n ", dsp_id);
		pr_err("Expected: %u, Received: %u\n",
			   dsp_assoc[dsp_id].expected_seq, msg->seq);
		return -1;
	}

	/* First two words for BPID, Seq */
	words = fd->length20/sizeof(uint32_t) - 2;

	/* Now check pattern */
	for (i = 0; i < words; i++) {
		uint32_t expected = ((i + 1) << 16) + 1;
		if (msg->pattern[i] != expected) {
			pr_err("Mismatch at word[%u] from dsp[%u]. ",
				   i, dsp_id);
			pr_err("Expected: %#x, Received: %#x\n",
				   expected, msg->pattern[i]);

#ifdef DEBUG
			phys_addr = phys_addr + fd->offset + ((i + 2) * 4);
			pr_info("virt_addr: %p, phys_addr = %#llx\n",
				&msg->pattern[i], phys_addr);
			if (cme_query(dsp_id, phys_addr))
				pr_err("cme_query failed\n");
#endif
			return -1;
		}

		/*msg->pattern[i] = 0;*/
	}

	msg->bpid = 0;
	msg->seq = 0;

	return 0;
}


static enum qman_cb_dqrr_result cb_rx(struct qman_portal *qm __always_unused,
				      struct qman_fq *fq,
				      const struct qm_dqrr_entry *dqrr)
{
	int ret;
	struct bm_buffer bufs;
	const uint32_t dsp_id = ((struct fq_pair_t *)fq)->dsp_id;
	const struct qm_fd *fd = &dqrr->fd;

	dsp_assoc[dsp_id].stats.num_recv++;

	if (verify_dsp_msg(fd, dsp_id)) {
		void *msg = fsl_usmmgr_p2v(qm_fd_addr(fd) + fd->offset,
					   usmmgr);

		pr_err("DSP[%u] message verify failed, fd_addr: %#llx\n",
			   dsp_id, qm_fd_addr(fd));
		hexdump(msg, fd->length20);
		dsp_assoc[dsp_id].stats.bad_recv++;
	}

	/* Since rcv FQ is in HOLDACTIVE, we do not need atomic_inc */
	dsp_assoc[dsp_id].expected_seq++;

	bufs.addr = qm_fd_addr(fd);
	ret = bman_release(dsp_assoc[dsp_id].bpool, &bufs, 1, 0);
	if (ret)
		pr_err("bman_release failed\n");

	if (dsp_assoc[dsp_id].expected_seq > msg_per_iteration) {
		uint64_t word = msg_per_iteration;
		pr_debug("DSP: %d iteration done\n", dsp_id);

		dsp_assoc[dsp_id].iter_end_time = mftb();
		/* Declare iteration complete ..*/
		ret = write(dsp_assoc[dsp_id].event_fd, &word, sizeof(word));
		if (sizeof(word) != ret) {
			pr_err("Iterarion wake up failed for dsp: %u\n",
			       dsp_id);
		}
	}
	return qman_cb_dqrr_consume;
}

static int create_fq_pair(struct fq_pair_t *fq_pair)
{
	int ret;
	uint32_t flags;
	struct qm_mcc_initfq opts;

	/* Allocate a dynamic FQ which PA side listens */
	fq_pair->pa_fq.cb.dqrr = cb_rx;
	fq_pair->pa_fq.cb.ern = NULL;
	fq_pair->pa_fq.cb.fqs = NULL;

	flags = QMAN_FQ_FLAG_NO_ENQUEUE | QMAN_FQ_FLAG_DYNAMIC_FQID;
	ret = qman_create_fq(0, flags, &fq_pair->pa_fq);
	if (ret) {
		pr_err("Fail: %s: %d\n", "qman_create_fq()", ret);
		return -1;
	}

	/* Now initialise the FQ */
	flags = QMAN_INITFQ_FLAG_SCHED;
	opts.we_mask = QM_INITFQ_WE_DESTWQ | QM_INITFQ_WE_FQCTRL |
			QM_INITFQ_WE_CONTEXTA;
	opts.fqd.dest.wq = 3;
	opts.fqd.fq_ctrl = QM_FQCTRL_PREFERINCACHE | QM_FQCTRL_HOLDACTIVE |
			QM_FQCTRL_CTXASTASHING;

	opts.fqd.context_a.stashing.exclusive = 0;
	opts.fqd.context_a.stashing.annotation_cl = 0;
	opts.fqd.context_a.stashing.data_cl = 1;
	opts.fqd.context_a.stashing.context_cl = 0;

	opts.fqd.dest.channel = pchannel;

	ret = qman_init_fq(&fq_pair->pa_fq, flags, &opts);
	if (ret) {
		pr_err("Failed qman_init_fq() :%d\n", ret);
		return -1;
	}

	/*Allocate a dynamic FQID on which DSP listens */
	ret = qman_alloc_fqid(&fq_pair->dsp_fqid);
	if (ret) {
		pr_err("Failed qman_alloc_fqid(): %d\n", ret);
		return -1;
	}

	return 0;
}
static int seed_dsp_bpool(uint32_t dsp_id, uint64_t count, uint64_t sz)
{
	u32 bpid;
	struct bm_buffer bufs[8];
	unsigned int num_bufs = 0;
	uint64_t phys_addr;

	struct dsp_assoc_t *assoc = &dsp_assoc[dsp_id];
	int ret = 0;

	bpid = bman_get_params(assoc->bpool)->bpid;

	/* Drain the pool of anything already in it. */
	do {
		/* Acquire is all-or-nothing, so we drain in 8s, then in 1s for
		 * the remainder. */
		if (ret != 1)
			ret = bman_acquire(assoc->bpool, bufs, 8, 0);
		if (ret < 8)
			ret = bman_acquire(assoc->bpool, bufs, 1, 0);
		if (ret > 0)
			num_bufs += ret;
	} while (ret > 0);

	if (num_bufs)
		fprintf(stderr, "Warn: drained %u bufs from BPID %d\n",
			num_bufs, bpid);

	/* Fill the pool */
	phys_addr = assoc->bpool_mem.phys_addr;
	for (num_bufs = 0; num_bufs < count; ) {
		unsigned int loop, rel = (count - num_bufs) > 8 ? 8 :
					(count - num_bufs);
		for (loop = 0; loop < rel; loop++) {
			bm_buffer_set64(&bufs[loop], phys_addr);
			phys_addr = phys_addr + sz;
		}
		do {
			ret = bman_release(assoc->bpool, bufs, rel, 0);
		} while (ret == -EBUSY);
		if (ret)
			fprintf(stderr, "Fail: %s\n", "bman_release()");
		num_bufs += rel;
	}

	pr_info("Released %u bufs (%#llx - %#llx) to BPID %d\n",
		num_bufs, assoc->bpool_mem.phys_addr, phys_addr, bpid);
	return 0;
}

static int dsp_dpa_init(uint32_t dsp_id)
{
	int ret;
	struct dsp_assoc_t *assoc = &dsp_assoc[dsp_id];

	struct bman_pool_params params = {
		/* No stockpile as bpool is shared among workers */
		.flags = BMAN_POOL_FLAG_DYNAMIC_BPID
	};

	params.bpid = assoc->bpid;
	assoc->bpool = bman_new_pool(&params);
	if (!assoc->bpool) {
		pr_err("bman_new_pool() failed for dsp: %u\n", dsp_id);
		return -1;
	}

	assoc->bpid = bman_get_params(assoc->bpool)->bpid;

	ret = create_fq_pair(&assoc->fq_pair);
	if (ret) {
		pr_err("create_fq_pair() failed for dsp: %u\n", dsp_id);
		return -1;
	}

	assoc->fq_pair.dsp_id = dsp_id;

	ret = seed_dsp_bpool(dsp_id, bpool_buff_cnt, msg_len);
	if (ret) {
		pr_err("seed_dsp_bpool() failed for dsp: %u\n", dsp_id);
		return -1;
	}

	/* Allocate BMAN portals for DSP */
	assoc->bportal.index = QBMAN_ANY_PORTAL_IDX;
	ret = bman_allocate_raw_portal(&assoc->bportal);
	if (ret) {
		pr_err("Allocating BMAN portal failed for dsp: %u\n", dsp_id);
		return -1;
	}

	/* Allocate QMAN portals for DSP */
	assoc->qportal.index = QBMAN_ANY_PORTAL_IDX;
	assoc->qportal.cpu = dsp_id;
	assoc->qportal.cache = 4;		/* DSP L2 */
	assoc->qportal.window = 0xffffffff;
	if (getenv("disable_stash"))
		assoc->qportal.enable_stash = 0;
	else
		assoc->qportal.enable_stash = 1;

	/* Each DSP cluster has two DSPs.
	 * For stash flow control, each target cache must receive
	 * stash transactions from only one SRQ. QMAN driver internally
	 * converts CPU_ID to SRQ index.
	 */
	assoc->qportal.sdest = dsp_id;
	if (assoc->qportal.enable_stash)
		pr_info("DSP: %u, SDEST: %d\n", dsp_id, assoc->qportal.sdest);

	ret = qman_allocate_raw_portal(&assoc->qportal);
	if (ret) {
		pr_err("Allocating QMAN portal failed for dsp: %u\n", dsp_id);
		return -1;
	}

	return 0;
}

static uint64_t build_dsp_msg(int dsp, size_t len, uint32_t seq)
{
	int i, ret;
	struct bm_buffer buf;
	struct dsp_msg_t *msg;
	struct bman_pool *bpool = dsp_assoc[dsp].bpool;
	uint32_t words = (len - offsetof(struct dsp_msg_t, pattern))/
			  sizeof(uint32_t);

	ret = bman_acquire(bpool, &buf, 1, 0);
	if (ret < 0) {
		pr_err("Buffer alloc failed for dsp: %d\n", dsp);
		return 0;
	}

	msg = fsl_usmmgr_p2v(buf.addr, usmmgr);

	msg->bpid = dsp_assoc[dsp].bpid;
	msg->seq = seq;
	for (i = 0; i < words; i++)
		msg->pattern[i] = ((i + 1) << 16);

	return buf.addr;
}

static int send_dsp_msgs(const int dsp, const int num_msgs, struct qm_fd *fds)
{
	uint64_t msg;
	int i;
	uint32_t fqid = dsp_assoc[dsp].fq_pair.dsp_fqid;

	dsp_assoc[dsp].expected_seq = 1;

	for (i = 0; i < num_msgs; i++) {
		msg = build_dsp_msg(dsp, msg_len, i + 1);
		if (!msg) {
			pr_err("build_msg() failed for dsp: %d\n", dsp);
			return -1;
		}

		fds[i].cmd = 0;
		fds[i].format = qm_fd_contig;
		fds[i].offset = 0;
		fds[i].addr = msg;
		fds[i].length20 = msg_len;
		fds[i].bpid = dsp_assoc[dsp].bpid;
	}

	/* Note down start time */
	dsp_assoc[dsp].iter_start_time = mftb();

	for (i = 0; i < num_msgs; i++) {
		if (send_frame(fqid, &fds[i])) {
			pr_err("send_frame failed for dsp: %d\n", dsp);
			return -1;
		}

		dsp_assoc[dsp].stats.num_sent++;
	}

	return 0;
}

enum dsp_status_t {
	DPAA_INIT_SUCCESS	= 0x900d,
	DPAA_INIT_FAIL		= 0xbad,
};

static int proc_dsp_ipc_msg(uint32_t dsp_id)
{
	struct dsp_assoc_t *assoc = &dsp_assoc[dsp_id];
	uint8_t msg[IPC_MSG_RING_BUF_SZ];
	uint32_t len;
	enum dsp_status_t *status = (enum dsp_status_t *)msg;

	if (fsl_ipc_recv_msg(dsp_assoc[dsp_id].pa_mbox, msg, &len, ipc)) {
		pr_err("fsl_ipc_recv_msg returned error\n");
		return -1;
	}

	if (*status != DPAA_INIT_SUCCESS) {
		pr_err("DSP ID: %u returned failure\n", dsp_id);
		return -1;
	}

	assoc->dsp_ready = true;
	sem_post(&dsp_handshake_sem);
	return 0;
}

struct bman_pool_t {
	uint32_t    bpid;	/*buffer pool ID*/
	uint32_t    len;	/*in bytes*/
};

/* The following message structure is common with DSP code */
struct dpaa_params_t {
	uint32_t	bportal;
	uint32_t	qportal;
	uint64_t	bportal_ce; /*cache enabled*/
	uint64_t	bportal_ci; /*cache inhibited*/
	uint64_t	qportal_ce; /*cache enabled*/
	uint64_t	qportal_ci; /*cache inhibited*/
	uint32_t	num_bpool;
#define MAX_BP_NUM	10
	struct bman_pool_t	bpool[MAX_BP_NUM];
	uint32_t	num_fq;
#define MAX_FQ_NUM	10
	uint32_t	dsp_fqid[MAX_FQ_NUM];
	uint32_t	pa_fqid[MAX_FQ_NUM];
};

static void print_dsp_assoc(unsigned long dsp_id)
{
	const char *status;
	const struct dsp_assoc_t *assoc = &dsp_assoc[dsp_id];

	if (true == assoc->dsp_ready)
		status = "READY";
	else
		status = "NOT READY";

	printf("--------------------------------------\n");
	printf("DSP: %lu, status: %s\n", dsp_id, status);
	printf("--------------------------------------\n");

	printf("Bportal: %u Qportal: %u\n",
	       assoc->bportal.index, assoc->qportal.index);
	printf("bportal_ce: %#llx bportal_ci: %#llx\n",
	       assoc->bportal.cena, assoc->bportal.cinh);
	printf("qportal_ce: %#llx qportal_ci: %#llx\n",
	       assoc->qportal.cena, assoc->qportal.cinh);
	printf("BPID: %u, Count: %u\n",
	       assoc->bpid, bman_query_free_buffers(assoc->bpool));
	printf("PA FQ: %u, DSP FQ: %u, DSP: %u\n",
	       assoc->fq_pair.pa_fq.fqid,
	       assoc->fq_pair.dsp_fqid,
	       assoc->fq_pair.dsp_id);
	printf("Bpool phys_addr: %#llx\n", assoc->bpool_mem.phys_addr);
	printf("pa_mbox: %u, dsp_mbox: %u\n", assoc->pa_mbox, assoc->dsp_mbox);
}

static int do_dsp_handshake(uint32_t dsp)
{
	int ret;
	const char *status;
	struct dpaa_params_t msg;
	struct dsp_assoc_t *assoc = &dsp_assoc[dsp];

	memset(&msg, 0, sizeof(msg));

	/* Portal assignments */
	msg.qportal = assoc->qportal.index;
	msg.bportal = assoc->bportal.index;

	msg.qportal_ce = assoc->qportal.cena;
	msg.qportal_ci = assoc->qportal.cinh;
	msg.bportal_ce = assoc->bportal.cena;
	msg.bportal_ci = assoc->bportal.cinh;

	/* Presently we are supporting single BPID and FQID per DSP */
	msg.num_bpool = 1;
	msg.bpool[0].bpid = assoc->bpid;
	msg.bpool[0].len = msg_len;

	msg.num_fq = 1;
	msg.dsp_fqid[0] = assoc->fq_pair.dsp_fqid;
	msg.pa_fqid[0] = assoc->fq_pair.pa_fq.fqid;

	ret = fsl_ipc_send_msg(assoc->dsp_mbox, &msg, sizeof(msg), ipc);
	if (ret) {
		pr_err("fsl_ipc_send_msg failed, chan: %u, ecode: %d\n",
		       assoc->dsp_mbox, ret);
		return -1;
	}

	pr_info("Sent handshake req to DSP: %u\n", dsp);

	/* Wait for DSP to complete handshake */
	while (1) {
		struct timespec timeout;

		clock_gettime(CLOCK_REALTIME, &timeout);
		timeout.tv_sec = timeout.tv_sec + 1;

		ret = sem_timedwait(&dsp_handshake_sem, &timeout);
		if (ret) {
			if (ETIMEDOUT == errno) {
				pr_info("Response awaited from DSP:%u\n", dsp);
				continue;
			}
			pr_err("sem_timedwait fail: %d\n", errno);
		}
		break;
	}

	if (false == dsp_assoc[dsp].dsp_ready)
		status = "FAILED";
	else
		status = "SUCCESS";

	pr_info("Handshake status : %s\n", status);
	return 0;
}

static void *ipc_poll_thread(void *arg __always_unused)
{
	uint64_t bmask;
	uint32_t pa_mbox;
	uint32_t dsp_id;
	int ret;

	while (1) {
		pa_mbox = 0;

		ret = fsl_ipc_chk_recv_status(&bmask, ipc);
		if (ERR_SUCCESS != ret) {
			pr_err("fsl_ipc_chk_recv_status failed: %d\n", ret);
			return NULL;
		}

		while (bmask) {
			if (bmask & (1ULL << (63 - pa_mbox))) {
				assert(pa_mbox > 1);
				dsp_id = pa_mbox/2 - 1;
				proc_dsp_ipc_msg(dsp_id);

				/* Clear channel bit*/
				bmask = bmask & ~(1ULL << (63 - pa_mbox));
			}
			pa_mbox++;
		}
		usleep(100000);	/* add some delay */
	}

	return NULL;
}

typedef int (*cli_handle_t)(int argc, char *argv[]);

struct cli_table_entry {
	const char *cmd;
	cli_handle_t handler;
} cli_table[16];

static int install_cli_cmd(const char *cmd, cli_handle_t handler)
{
	static int cli_index;

	if (cli_index == ARRAY_SIZE(cli_table)) {
		pr_err("No space for CLI command: %s\n", cmd);
		return -1;
	}

	cli_table[cli_index].cmd = cmd;
	cli_table[cli_index].handler = handler;

	cli_index++;
	return 0;
}

static void sigint_handler(int sig)
{
	sigint_recv = true;
}

void process_cmd(char *cli)
{
	int ret, cli_argc, index;
	char **cli_argv;
	const struct cli_table_entry *cli_cmd;
	struct sigaction oldact;
	struct sigaction newact = {
		.sa_handler = sigint_handler,
		.sa_flags = 0,
	};

	if (cli[0] == 0) {
		free(cli);
		return;
	}

	sigemptyset(&newact.sa_mask);

	cli_argv = history_tokenize(cli);
	if (unlikely(cli_argv == NULL)) {
		pr_err("Out of memory while parsing: %s\n", cli);
		free(cli);
		return;
	}

	for (cli_argc = 0; cli_argv[cli_argc] != NULL; cli_argc++)
		;

	for (index = 0; index < ARRAY_SIZE(cli_table); index++) {
		cli_cmd = &cli_table[index];
		if (!cli_cmd->cmd) {
			index = ARRAY_SIZE(cli_table);
			break;
		}

		if (strcmp(cli_argv[0], cli_cmd->cmd) == 0) {
			/* Install a ctrl+c handler for cmd duration */
			ret = sigaction(SIGINT, &newact, &oldact);
			assert(0 == ret);
			cli_cmd->handler(cli_argc, cli_argv);
			sigint_recv = false;
			ret = sigaction(SIGINT, &oldact, NULL);
			assert(0 == ret);
			add_history(cli);
			break;
		}
	}

	if (index == ARRAY_SIZE(cli_table))
		printf("Unknown command: %s\n", cli);

	for (cli_argc = 0; cli_argv[cli_argc] != NULL; cli_argc++)
		free(cli_argv[cli_argc]);

	free(cli_argv);
	free(cli);
}

static void cli_loop(void)
{
	fd_set fds;

	while (1) {
		rl_callback_handler_install(cmd_prompt, process_cmd);

		FD_ZERO(&fds);
		FD_SET(fileno(stdin), &fds);
		FD_SET(l1d_eventfd, &fds);

		if (select(FD_SETSIZE, &fds, NULL, NULL, NULL) < 0) {
			perror("cli_loop select err");
			exit(1);
		}

		if (FD_ISSET(l1d_eventfd, &fds)) {
			rl_callback_handler_remove();
			return;
		}

		if (FD_ISSET(fileno(stdin), &fds))
			rl_callback_read_char();
	}
}

static int wait_for_iter_finish(uint32_t *run_mask,
				uint64_t iter)
{
	unsigned long dsp;
	uint32_t mask, dsp_mask = *run_mask;
	uint64_t word, cycles;
	fd_set fds, readfds;
	int nfds;
	int ret;

	nfds = 0;
	FD_ZERO(&readfds);
	for (dsp = 0, mask = *run_mask; mask; mask = (mask & ~(1 << dsp++))) {
		if (!(mask & (1 << dsp)))
			continue;

		FD_SET(dsp_assoc[dsp].event_fd, &readfds);
		if (nfds < dsp_assoc[dsp].event_fd + 1)
			nfds = dsp_assoc[dsp].event_fd + 1;
	}

	while (dsp_mask) {
		struct timeval timeout = { 1, 0};
		fds = readfds;
		ret = select(nfds, &fds, NULL, NULL, &timeout);
		if (!ret) {
			printf("Waiting for iteration: %llu completion\n",
			       iter);
			continue;
		}

		if (-1 == ret) {
			if (errno == EINTR) {
				printf("Waiting for iteration to finish\n");
				*run_mask = 0;
			} else {
				pr_err("select() failed: %d\n", ret);
				return -1;
			}
		}

		/* One or more DSP completed */
		for (dsp = 0, mask = dsp_mask; mask;
		     mask = (mask & ~(1 << dsp++))) {
			if (!(mask & (1 << dsp)))
				continue;

			if (!FD_ISSET(dsp_assoc[dsp].event_fd, &fds))
				continue;

			FD_CLR(dsp_assoc[dsp].event_fd, &readfds);
			dsp_mask = dsp_mask & ~(1 << dsp);
			ret = read(dsp_assoc[dsp].event_fd,
				   &word, sizeof(word));
			if (sizeof(word) != ret) {
				pr_err("read err for dsp[%lu]\n", dsp);
				return -1;
			}

			/* Store average iteration time */
			cycles = dsp_assoc[dsp].iter_end_time -
			       dsp_assoc[dsp].iter_start_time;
			printf("DSP[%lu] iteration cycles: %llu\n",
			       dsp, cycles);
		}
	}

	return 0;
}

static int run_test(int argc, char *argv[])
{
	int ret;
	uint32_t tmpmask, run_mask;
	unsigned long dsp;
	uint64_t num_iterations, iter = 0;
	char *endptr;
	struct qm_fd *qmfds;

	if (argc < 2) {
		fprintf(stderr,
			"run_test <dsp_mask> [num_iterations] [msg_per_iteration]\n");
		return -1;
	}

	run_mask = strtoul(argv[1], &endptr, 0);
	if ((run_mask == ULONG_MAX) || (*endptr != '\0')) {
		fprintf(stderr, "Invalid dsp_mask: %s\n", argv[1]);
		return -1;
	}

	/* Mask of bits for DSPs which are not active */
	run_mask = run_mask & (0xFFFFFFFF >> (32 - num_active_dsp));

	if (argc > 2) {
		num_iterations = strtoull(argv[2], &endptr, 0);
		if ((num_iterations == ULLONG_MAX) || (*endptr != '\0')) {
			fprintf(stderr, "Invalid num_iterations: %s\n",
				argv[2]);
			return -1;
		}
	} else {
		num_iterations = 1;
	}

	if (argc > 3) {
		msg_per_iteration = strtoul(argv[3], &endptr, 0);
		if ((msg_per_iteration == ULONG_MAX) || (*endptr != '\0')) {
			fprintf(stderr, "Invalid msg_per_iteration: %s\n",
				argv[3]);
			return -1;
		}

		if (msg_per_iteration > bpool_buff_cnt/2) {
			fprintf(stderr, "Max allowed msg_per_iteration: %lu\n",
				bpool_buff_cnt/2);
			return -1;
		}
	}

	qmfds = calloc(msg_per_iteration, sizeof(struct qm_fd));
	if (!qmfds) {
		pr_err("Memory allocation failed for QM fd\n");
		return -1;
	}

	printf("DSP mask: %#x, Num iteration: %llu, Msg per iteration: %lu\n",
	       run_mask, num_iterations, msg_per_iteration);

	/* Erase stats for all DSPs */
	for (dsp = 0; dsp < num_active_dsp; dsp++)
		memset(&dsp_assoc[dsp].stats, 0, sizeof(struct test_stats));

next_iteration:
	iter++;

	for_each_dsp(dsp, run_mask, tmpmask) {
		pr_debug("DSP[%lu]: Starting iteration: %llu\n", dsp, iter);
		if (false == dsp_assoc[dsp].dsp_ready) {
			pr_err("DSP: %lu not ready\n", dsp);
			continue;
		}

		ret = send_dsp_msgs(dsp, msg_per_iteration, qmfds);
		if (ret) {
			pr_err("send_dsp_msgs() failed for dsp: %lu\n", dsp);
			return -1;
		}
	}

	/* Wait for iteration to complete.
	 * We need to wait for all DSPs to declare iteration completion.
	 */
	ret = wait_for_iter_finish(&run_mask, iter);
	if (ret) {
		pr_err("wait_for_iter_finish failed\n");
		return -1;
	}

	printf("Iteration: %llu complete\n", iter);
	if (sigint_recv)
		goto end;

	if (!num_iterations || iter < num_iterations)
		goto next_iteration;

end:
	/* Print stats for all DSPs */
	for (dsp = 0; dsp < num_active_dsp; dsp++) {
		printf("DSP: %lu, Sent: %llu, Recv: %llu, Bad: %llu\n", dsp,
		       dsp_assoc[dsp].stats.num_sent,
		       dsp_assoc[dsp].stats.num_recv,
		       dsp_assoc[dsp].stats.bad_recv);
	}

	free(qmfds);
	return 0;
}


static int show_dsp(int argc, char *argv[])
{
	unsigned long dsp;

	for (dsp = 0; dsp < num_active_dsp; dsp++) {
		print_dsp_assoc(dsp);
		printf("\n");
	}

	return 0;
}

static int kill_dsp(uint32_t dsp)
{
	int ret;
	struct bm_buffer buf;
	struct dsp_msg_t *msg;
	struct qm_fd fd;
	struct bman_pool *bpool = dsp_assoc[dsp].bpool;

	ret = bman_acquire(bpool, &buf, 1, 0);
	if (ret < 0) {
		pr_err("Buffer alloc failed for dsp: %d\n", dsp);
		return 0;
	}

	msg = fsl_usmmgr_p2v(buf.addr, usmmgr);

	msg->bpid = 0xdeaddead;		/* Magic keyword to crash DSP */

	fd.cmd = 0;
	fd.format = qm_fd_contig;
	fd.offset = 0;
	fd.addr = buf.addr;
	fd.length20 = msg_len;
	fd.bpid = dsp_assoc[dsp].bpid;

	if (send_frame(dsp_assoc[dsp].fq_pair.dsp_fqid, &fd)) {
		pr_err("send_frame failed for dsp: %d\n", dsp);
		return -1;
	}

	return 0;
}

static int kill_dspmask(int argc, char *argv[])
{
	int ret;
	uint32_t dsp, dspmask, tmpmask;

	dspmask = strtoul(argv[1], NULL, 0);
	if (dspmask == ULONG_MAX) {
		fprintf(stderr, "Invalid argument to -n (%s)\n", *argv);
		return -1;
	}

	/* Remove those DSPs from mask which are not active */
	dspmask = dspmask & active_dsp_mask;

	for_each_dsp(dsp, dspmask, tmpmask) {
		printf("Sending kill cmd to DSP[%u].. ", dsp);
		ret = kill_dsp(dsp);
		if (ret) {
			pr_err("kill_dsp failed for DSP[%u]\n", dsp);
			return -1;
		}

		printf("done\n");
	}

	return 0;
}

static int download_dsp_images(uint32_t dspmask, os_het_l1d_mode_t l1d_mode)
{
	dsp_core_info info;
	uint32_t dsp, tmpmask;
	int ret;

	info.hw_sem_num = 1;
	info.reset_mode = l1d_mode;
	info.maple_reset_mode = 0;
	info.debug_print = 0;

	/* Reset the whole info structure */
	for (dsp = 0; dsp < num_active_dsp; dsp++) {
		info.reDspCoreInfo[dsp].reset_core_flag = 0;
		info.shDspCoreInfo[dsp].reset_core_flag = 0;
		info.reDspCoreInfo[dsp].core_id = dsp;
		info.reDspCoreInfo[dsp].dsp_filename =
						(char *)dsp_images[dsp + 1];
		info.shDspCoreInfo[dsp].dsp_filename = (char *)dsp_images[0];
	}

	if ((MODE_3_ACTIVE == l1d_mode) || (MODE_2_ACTIVE == l1d_mode)) {
		for (dsp = 0; dsp < num_active_dsp; dsp++)
			info.reDspCoreInfo[dsp].reset_core_flag = 1;

		if (MODE_3_ACTIVE == l1d_mode)
			info.shDspCoreInfo[0].reset_core_flag = 1;
	} else {
		for_each_dsp(dsp, dspmask, tmpmask)
			info.reDspCoreInfo[dsp].reset_core_flag = 1;
	}

	ret = fsl_start_L1_defense(ipc, &info);
	if (ret) {
		pr_err("fsl_start_L1_defense failed\n");
		return -1;
	}

	return 0;
}

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
			BUG_ON(s);
			/* Poll for completion */
			do {
				qman_poll();
				qman_fq_state(fq, &state, &flags);
			} while (flags & QMAN_FQ_STATE_VDQCR);
		}
	}
	s = qman_oos_fq(fq);
	BUG_ON(s);
	if (!(fq->flags & QMAN_FQ_FLAG_DYNAMIC_FQID))
		qman_release_fqid(fq->fqid);
	qman_destroy_fq(fq, 0);
}

static int remove_fq_pair(struct fq_pair_t *fq_pair)
{
	teardown_fq(&fq_pair->pa_fq);
	qman_release_fqid(fq_pair->dsp_fqid);

	return 0;
}
static int cleanup_dsp(uint32_t dsp)
{
	int ret;
	struct dsp_assoc_t *assoc;

	assoc = &dsp_assoc[dsp];

	ret = remove_fq_pair(&assoc->fq_pair);
	if (ret) {
		pr_err("remove_fq_pair failed for DSP[%u]\n", dsp);
		return -1;
	}

	/* Release buffer pool */
	bman_free_pool(assoc->bpool);

	/* Release QBMan portals */
	ret = qman_free_raw_portal(&assoc->qportal);
	if (ret) {
		pr_err("qman_free_raw_portal failed for DSP[%u]\n", dsp);
		return -1;
	}

	ret = bman_free_raw_portal(&assoc->bportal);
	if (ret) {
		pr_err("bman_free_raw_portal failed for DSP[%u]\n", dsp);
		return -1;
	}

	return 0;
}

static int cleanup_dspmask(uint32_t dspmask)
{
	uint32_t dsp, tmpmask;

	for_each_dsp(dsp, dspmask, tmpmask) {
		if (cleanup_dsp(dsp)) {
			pr_err("DSP[%u] cleanup failed\n", dsp);
			return -1;
		}

		pr_info("DSP[%u] cleaned up\n", dsp);
	}
	return 0;
}

static void drain_4_bytes(int fd, fd_set *fdset)
{
	if (FD_ISSET(fd, fdset)) {
		uint32_t junk;
		ssize_t sjunk = read(fd, &junk, sizeof(junk));
		if (sjunk != sizeof(junk))
			perror("UIO irq read error");
	}
}

/* This is the worker thread function. It sets up thread-affinity, sets up
 * thread-local portal resources, doing "global init" if it is the first/primary
 * thread then enters the run-to-completion loop. As/when the quit message is
 * seen, it exits the run-to-completion loop and tears down. */
static void *worker_fn(void *__worker)
{
	cpu_set_t cpuset;
	struct worker *worker = __worker;
	int ret, slowpoll = 0, irq_mode = 0, fastpoll = 0;
	int fd_qman, fd_bman, nfds;

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
		pr_err("bman_thread_init() failed for: %d\n", worker->cpu);
		return NULL;
	}
	ret = qman_thread_init();
	if (ret) {
		pr_err("qman_thread_init() failed for: %d\n", worker->cpu);
		return NULL;
	}

	/* Set the qman portal's SDQCR mask */
	qman_static_dequeue_add(sdqcr);

	/* Indicate that our thread-init is complete. */
	worker->init_done = 1;

	fd_qman = qman_thread_fd();
	fd_bman = bman_thread_fd();
	if (fd_qman > fd_bman)
		nfds = fd_qman + 1;
	else
		nfds = fd_bman + 1;

	/* The run-to-completion loop */
	while (1) {
		/* IRQ mode */
		if (irq_mode) {
			fd_set readset;

			/* Go into (and back out of) IRQ mode for each select,
			 * it simplifies exit-path considerations and other
			 * potential nastiness. */
			FD_ZERO(&readset);
			FD_SET(fd_qman, &readset);
			FD_SET(fd_bman, &readset);
			bman_irqsource_add(BM_PIRQ_RCRI | BM_PIRQ_BSCN);
			qman_irqsource_add(QM_PIRQ_SLOW | QM_PIRQ_DQRI);
			ret = select(nfds, &readset, NULL, NULL, NULL);
			/* Calling irqsource_remove() prior to thread_irq()
			 * means thread_irq() will not process whatever caused
			 * the interrupts, however it does ensure that, once
			 * thread_irq() re-enables interrupts, they won't fire
			 * again immediately. The calls to poll_slow() force
			 * handling of whatever triggered the interrupts. */
			bman_irqsource_remove(~0);
			qman_irqsource_remove(~0);
			bman_thread_irq();
			qman_thread_irq();
			bman_poll_slow();
			qman_poll_slow();
			if (ret < 0) {
				pr_err("QBMAN select error");
				break;
			}
			drain_4_bytes(fd_bman, &readset);
			drain_4_bytes(fd_qman, &readset);
			/* Transition out of IRQ mode */
			irq_mode = 0;
			fastpoll = 0;
			slowpoll = 0;
		}

		/* non-IRQ mode */
		if (!(slowpoll--)) {
			if (qman_poll_slow() || bman_poll_slow()) {
				slowpoll = WORKER_SLOWPOLL_BUSY;
				fastpoll = 0;
			} else {
				slowpoll = WORKER_SLOWPOLL_IDLE;
			}
		}

		if (qman_poll_dqrr(WORKER_FASTPOLL_DQRR)) {
			fastpoll = 0;
		} else {
			/* No fast-path work, do we transition to IRQ mode? */
			if (++fastpoll > WORKER_FASTPOLL_DOIRQ)
				irq_mode = 1;
		}
	}

	return NULL;
}

/*
 * The main() function/thread creates the worker threads, ipc thread and then
 * enters into CLI loop. This function is never expected to return.
 */
int main(int argc, char *argv[])
{
	struct worker *workers;
	char *endptr;
	uint32_t dsp_id;
	pthread_t ipc_thread;
	int l1d_fd;
	uint32_t dspmask, tmpmask;
	uint64_t word;
	os_het_l1d_mode_t l1d_mode;

	/* Determine number of cores (==number of threads) */
	long ncpus = sysconf(_SC_NPROCESSORS_ONLN);
	int loop, ret = of_init();

	if (ret) {
		fprintf(stderr, "Fail: %s: %d\n", "of_init()", ret);
		return -1;
	}

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
		} else if (!strcmp(*argv, "-b")) {
			unsigned long val;
			if (!ARGINC()) {
				fprintf(stderr, "Missing argument to -b\n");
				exit(EXIT_FAILURE);
			}
			val = strtoul(*argv, &endptr, 0);
			if ((val == ULONG_MAX) || (*endptr != '\0')) {
				fprintf(stderr, "Invalid argument to -b (%s)\n",
					*argv);
				exit(EXIT_FAILURE);
			}
			if (!val) {
				fprintf(stderr, "Out of range (-b %lu)\n", val);
				exit(EXIT_FAILURE);
			}
			bpool_buff_cnt = val;
			continue;
		} else if (!strcmp(*argv, "-d")) {
			unsigned long val;
			if (!ARGINC()) {
				fprintf(stderr, "Missing argument to -d\n");
				exit(EXIT_FAILURE);
			}
			val = strtoul(*argv, &endptr, 0);
			if ((val == ULONG_MAX) || (*endptr != '\0')) {
				fprintf(stderr, "Invalid argument to -d (%s)\n",
					*argv);
				exit(EXIT_FAILURE);
			}
			if (val > NUM_DSP) {
				fprintf(stderr, "Out of range (-d %lu)\n", val);
				exit(EXIT_FAILURE);
			}
			num_active_dsp = val;
		} else if (!strcmp(*argv, "-l")) {
			unsigned long val;
			if (!ARGINC()) {
				fprintf(stderr, "Missing argument to -l\n");
				exit(EXIT_FAILURE);
			}
			val = strtoul(*argv, &endptr, 0);
			if ((val == ULONG_MAX) || (*endptr != '\0')) {
				fprintf(stderr, "Invalid argument to -l (%s)\n",
					*argv);
				exit(EXIT_FAILURE);
			}
			if (val & 0x3) {
				fprintf(stderr, "Len must be multiple of 4\n");
				exit(EXIT_FAILURE);
			}
			msg_len = val;
		} else {
			fprintf(stderr, "Unknown argument '%s'\n", *argv);
			exit(EXIT_FAILURE);
		}
	}

	if (ncpus < 1) {
		pr_err("Fail: # processors: %ld\n", ncpus);
		exit(EXIT_FAILURE);
	}

	if (map_ccsr()) {
		pr_err("map_ccsr failed\n");
		return -1;
	}

	printf("Starting DSP DPA demo, NUM_PA = %ld, NUM_DSP = %lu\n",
	       ncpus, num_active_dsp);

	/* Allocate the worker structs */
	ret = posix_memalign((void **)&workers, L1_CACHE_BYTES,
			     ncpus * sizeof(*workers));
	if (ret) {
		pr_err("Fail: %s: %d\n", "posix_memalign()", ret);
		exit(EXIT_FAILURE);
	}

	ret = sem_init(&dsp_handshake_sem, 0, 0);
	if (ret) {
		pr_err("sem_init failed\n");
		exit(EXIT_FAILURE);
	}

	l1d_eventfd = eventfd(0, 0);
	if (-1 == l1d_eventfd) {
		pr_err("l1d_eventfd create failed\n");
		exit(EXIT_FAILURE);
	}

	if (ipc_init()) {
		pr_err("ipc_init() failure\n");
		exit(EXIT_FAILURE);
	}

	if (pa_dpa_init()) {
		pr_err("DPA init failure\n");
		exit(EXIT_FAILURE);
	}

	/* Do memory allocations for bpool and IPC channels */
	for (dsp_id = 0; dsp_id < num_active_dsp; dsp_id++) {
		if (dsp_ipc_mem_alloc(dsp_id)) {
			pr_err("dsp_ipc_mem_alloc failed dsp: %u\n", dsp_id);
			exit(EXIT_FAILURE);
		}
		active_dsp_mask = active_dsp_mask | (1 << dsp_id);
	}

	/* Now create a thread for polling on IPC channels */
	ret = pthread_create(&ipc_thread, NULL, ipc_poll_thread, NULL);
	if (ret) {
		pr_err("Err: %d creating IPC thread", ret);
		exit(EXIT_FAILURE);
	}

	/* Compute SDQCR */
	sdqcr = QM_SDQCR_CHANNELS_POOL_CONV(pchannel);

	/* Start up the threads */
	for (loop = 0; loop < ncpus; loop++) {
		struct worker *worker = &workers[loop];
		worker->init_done = 0;
		worker->cpu = loop;
		ret = pthread_create(&worker->id, NULL, worker_fn, worker);
		if (ret) {
			pr_err("pthread_create failed for worker thread");
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

	l1d_fd = open("/dev/fsl_l1d", O_RDWR);
	if (-1 == l1d_fd) {
		perror("Open L1 defense device failed\n");
		exit(-1);
	}

	/* add cli commands here */
	add_cli_cmd(run_test);
	add_cli_cmd(show_dsp);

	/* This command is to simulate DSP crash */
	add_cli_cmd(kill_dspmask);

	dspmask = active_dsp_mask;

reconfigure_dsps_ipc:
	for_each_dsp(dsp_id, dspmask, tmpmask) {
		if (dsp_ipc_chan_init(dsp_id)) {
			pr_err("IPC init failed for dsp: %u\n", dsp_id);
			exit(EXIT_FAILURE);
		}
	}

reconfigure_dsps:
	/* Start IPC association with DSP */
	for_each_dsp(dsp_id, dspmask, tmpmask) {
		if (dsp_dpa_init(dsp_id)) {
			pr_err("DSP DPA init failed for dsp: %u\n", dsp_id);
			exit(EXIT_FAILURE);
		}

		if (do_dsp_handshake(dsp_id)) {
			pr_err("do_dsp_handshake failed for dsp: %u\n",
			       dsp_id);
			exit(EXIT_FAILURE);
		}
	}

	cli_loop();

	/* If we are here, it means that one or more DSPs are down.
	 * Find out what DSP mask is down.
	 */
	if (-1 == read(l1d_eventfd, &word, sizeof(word))) {
		perror("read failed on l1d_eventfd\n");
		exit(EXIT_FAILURE);
	}

	dspmask = word;
	pr_info("DSP mask = %#x down\n", dspmask);

retry_l1d_mode:
	printf("Enter L1Defense mode (mode1=%d, mode2=%d, mode3=%d): ",
	       MODE_1_ACTIVE, MODE_2_ACTIVE, MODE_3_ACTIVE);
	scanf("%1d", (int *)&l1d_mode);

	if ((l1d_mode != MODE_1_ACTIVE) &&
	    (l1d_mode != MODE_2_ACTIVE) &&
	    (l1d_mode != MODE_3_ACTIVE)) {
		printf("Invalid mode = %d.\n", l1d_mode);
		goto retry_l1d_mode;
	}

	pr_info("Invoking L1Defense mode: %d\n", l1d_mode);

	if (MODE_3_ACTIVE == l1d_mode || MODE_2_ACTIVE == l1d_mode)
		dspmask = active_dsp_mask;

	ret = download_dsp_images(dspmask, l1d_mode);
	if (ret) {
		pr_err("download_dsp_images failed\n");
		exit(-1);
	}

	ret = cleanup_dspmask(dspmask);
	if (ret) {
		pr_err("cleanup_dsps failed\n");
		exit(-1);
	}

	/* IPC channels need to be re-init as they get reset after mode3 L1D */
	if (MODE_3_ACTIVE == l1d_mode)
		goto reconfigure_dsps_ipc;
	else
		goto reconfigure_dsps;
}
