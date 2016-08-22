
#include <fra_common.h>
#include <usdpaa/fsl_usd.h>
#include <usdpaa/of.h>
#include <fra_fq_interface.h>
#include <fra_cfg_parser.h>
#include <fra.h>
#include <test_speed.h>
#include <readline/readline.h>  /* libedit */
#include <readline/history.h>
#include <argp.h>

/* Seed buffer pools according to the configuration symbols */
const struct bpool_config  bpool_config[] = {
	{ DMA_MEM_BP3_BPID, DMA_MEM_BP3_NUM, DMA_MEM_BP3_SIZE},
	{ DMA_MEM_BP4_BPID, DMA_MEM_BP4_NUM, DMA_MEM_BP4_SIZE},
	{ DMA_MEM_BP5_BPID, DMA_MEM_BP5_NUM, DMA_MEM_BP5_SIZE},
	{ DMA_MEM_BP6_BPID, DMA_MEM_BP6_NUM, DMA_MEM_BP6_SIZE}
};
/* Configuration */
static struct fra_cfg *fra_cfg;
/******************/
/* Worker threads */
/******************/
struct worker_msg {
	/* The CLI thread sets ::msg!=worker_msg_none then waits on the barrier.
	 * The worker thread checks for this in its polling loop, and if set it
	 * will perform the desired function, set ::msg=worker_msg_none, then go
	 * into the barrier (releasing itself and the CLI thread). */
	volatile enum worker_msg_type {
		worker_msg_none = 0,
		worker_msg_list,
		worker_msg_quit,
		worker_msg_do_global_init,
		worker_msg_do_global_finish,
		worker_msg_do_test_speed,
		worker_msg_do_test_pw,
		worker_msg_do_reset,
	} msg;
} ____cacheline_aligned;

struct worker {
	struct worker_msg *msg;
	int cpu;
	uint32_t uid;
	pthread_t id;
	int result;
	struct list_head node;
} ____cacheline_aligned;

static uint32_t next_worker_uid;

/* -------------------------------- */
/* msg-processing within the worker */

static void do_global_finish(void)
{
	fra_finish();
	bpools_finish();
}

static void do_global_init(void)
{
	int err;

	bpools_init(bpool_config, ARRAY_SIZE(bpool_config));
	err = fra_init(NULL, fra_cfg);

	if (unlikely(err < 0)) {
		error(0, -err, "fra_init()");
		do_global_finish();
		return;
	}
}

static int process_msg(struct worker *worker, struct worker_msg *msg)
{
	int ret = 1;

	/* List */
	if (msg->msg == worker_msg_list)
		fprintf(stderr, "Thread uid:%u alive (on cpu %d)\n",
			worker->uid, worker->cpu);
	/* Quit */
	else if (msg->msg == worker_msg_quit)
		ret = 0;
	/* Do global init */
	else if (msg->msg == worker_msg_do_global_init)
	{
		printf("do global init!\n");
		do_global_init();
    }
	/* Do global finish */
	else if (msg->msg == worker_msg_do_global_finish)
		do_global_finish();
	/* Do test speed */
	else if (msg->msg == worker_msg_do_test_speed)
	{	
		printf("test speed send msg!\n");
		test_speed_send_msg();
	}
	/* Do global reset */
	else if (msg->msg == worker_msg_do_reset)
		fra_reset();

	/* What did you want? */
	else
		panic("bad message type");

	msg->msg = worker_msg_none;
	return ret;
}

/* the worker's polling loop calls this function to drive the message pump */
static inline int check_msg(struct worker *worker)
{
	struct worker_msg *msg = worker->msg;
	if (likely(msg->msg == worker_msg_none))
		return 1;
	return process_msg(worker, msg);
}

/* ---------------------- */
/* worker thread function */

#define WORKER_SELECT_TIMEOUT_us 10000
#define WORKER_SLOWPOLL_BUSY 4
#define WORKER_SLOWPOLL_IDLE 400
#define WORKER_FASTPOLL_DQRR 16
#define WORKER_FASTPOLL_DOIRQ 2000
#ifdef FRA_IDLE_IRQ
static void drain_4_bytes(int fd, fd_set *fdset)
{
	if (FD_ISSET(fd, fdset)) {
		uint32_t junk;
		ssize_t sjunk = read(fd, &junk, sizeof(junk));
		if (sjunk != sizeof(junk))
			error(0, errno, "UIO irq read error");
	}
}
#endif
static void *worker_fn(void *__worker)
{
	struct worker *worker = __worker;
	cpu_set_t cpuset;
	int s;
	int calm_down = 16, slowpoll = 0;
#ifdef FRA_IDLE_IRQ
	int fd_qman, fd_bman, nfds;
	int irq_mode = 0, fastpoll = 0;
	fd_set readset;
#endif

	FRA_DBG("This is the thread on cpu %d", worker->cpu);

	/* Set this cpu-affinity */
	CPU_ZERO(&cpuset);
	CPU_SET(worker->cpu, &cpuset);
	s = pthread_setaffinity_np(worker->id, sizeof(cpu_set_t), &cpuset);
	if (s != 0) {
		error(0, -s, "pthread_setaffinity_np(%d)",
		      worker->cpu);
		goto err;
	}

	/* Initialise bman/qman portals */
	s = bman_thread_init();
	if (s) {
		error(0, -s,
		      "No available Bman portals for cpu %d",
		      worker->cpu);
		goto err;
	}
	s = qman_thread_init();
	if (s) {
		error(0, -s,
		      "No available Qman portals for cpu %d",
		      worker->cpu);
		bman_thread_finish();
		goto err;
	}

#ifdef FRA_IDLE_IRQ
	fd_qman = qman_thread_fd();
	fd_bman = bman_thread_fd();
	if (fd_qman > fd_bman)
		nfds = fd_qman + 1;
	else
		nfds = fd_bman + 1;
#endif

	local_fq_init();

	/* Global init is triggered by having the message preset to
	 * "do_global_init" before the thread even runs. Thi */
	if (worker->msg->msg == worker_msg_do_global_init) {
		s = process_msg(worker, worker->msg);
		if (s <= 0)
			goto global_init_fail;
	}

	/* Run! */
	FRA_DBG("Starting poll loop on cpu %d", worker->cpu);
	while (check_msg(worker)) {
#ifdef FRA_IDLE_IRQ
		/* IRQ mode */
		if (irq_mode) {
			/* Go into (and back out of) IRQ mode for each select,
			 * it simplifies exit-path considerations and other
			 * potential nastiness. */
			struct timeval tv = {
				.tv_sec = WORKER_SELECT_TIMEOUT_us / 1000000,
				.tv_usec = WORKER_SELECT_TIMEOUT_us % 1000000
			};
			FD_ZERO(&readset);
			FD_SET(fd_qman, &readset);
			FD_SET(fd_bman, &readset);
			bman_irqsource_add(BM_PIRQ_RCRI | BM_PIRQ_BSCN);
			qman_irqsource_add(QM_PIRQ_SLOW | QM_PIRQ_DQRI);
			s = select(nfds, &readset, NULL, NULL, &tv);
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
			if (s < 0) {
				error(0, 0, "QBMAN select error");
				break;
			}
			if (!s)
				/* timeout, stay in IRQ mode */
				continue;
			drain_4_bytes(fd_bman, &readset);
			drain_4_bytes(fd_qman, &readset);
			/* Transition out of IRQ mode */
			irq_mode = 0;
			fastpoll = 0;
			slowpoll = 0;
		}
#endif
		/* non-IRQ mode */
		if (!(slowpoll--)) {
			if (qman_poll_slow() || bman_poll_slow()) {
				slowpoll = WORKER_SLOWPOLL_BUSY;
#ifdef FRA_IDLE_IRQ
				fastpoll = 0;
#endif
			} else
				slowpoll = WORKER_SLOWPOLL_IDLE;
		}
#ifdef FRA_IDLE_IRQ
		if (qman_poll_dqrr(WORKER_FASTPOLL_DQRR))
			fastpoll = 0;
		else
			/* No fast-path work, do we transition to IRQ mode? */
			if (++fastpoll > WORKER_FASTPOLL_DOIRQ)
				irq_mode = 1;
#else
		qman_poll_dqrr(WORKER_FASTPOLL_DQRR);
#endif
	}

global_init_fail:
	qman_static_dequeue_del(~(uint32_t)0);
	while (calm_down--) {
		qman_poll_slow();
		qman_poll_dqrr(16);
	}
	qman_thread_finish();
	bman_thread_finish();
err:
	FRA_DBG("Leaving thread on cpu %d", worker->cpu);
	pthread_exit(NULL);
}

/* ------------------------------ */
/* msg-processing from main()/CLI */

/* This is implemented in the worker-management code lower down, but we need to
 * use it from msg_post() */
static int worker_reap(struct worker *worker);

static int msg_post(struct worker *worker, enum worker_msg_type m)
{
	worker->msg->msg = m;
	while (worker->msg->msg != worker_msg_none) {
		if (!worker_reap(worker))
			/* The worker is already gone */
			return -EIO;
		pthread_yield();
	}
	return 0;
}

static int msg_list(struct worker *worker)
{
	return msg_post(worker, worker_msg_list);
}

static int msg_quit(struct worker *worker)
{
	return msg_post(worker, worker_msg_quit);
}

static int msg_do_global_finish(struct worker *worker)
{
	return msg_post(worker, worker_msg_do_global_finish);
}

int msg_do_test_speed(struct worker *worker)
{
	return msg_post(worker, worker_msg_do_test_speed);
}

/**********************/
/* worker thread mgmt */
/**********************/

static LIST_HEAD(workers);
static unsigned long ncpus;

/* This worker is the first one created, must not be deleted, and must be the
 * last one to exit. (The buffer pools objects are initialised against its
 * portal.) */
static struct worker *primary;

int msg_do_reset(void)
{
	if (!primary)
		return -EINVAL;

	return msg_post(primary, worker_msg_do_reset);
}

static struct worker *worker_new(int cpu, int is_primary)
{
	struct worker *ret;
	int err = posix_memalign((void **)&ret, L1_CACHE_BYTES, sizeof(*ret));
	if (err)
		goto out;
	err = posix_memalign((void **)&ret->msg, L1_CACHE_BYTES,
			     sizeof(*ret->msg));
	if (err) {
		free(ret);
		goto out;
	}
	ret->cpu = cpu;
	ret->uid = next_worker_uid++;
	ret->msg->msg = is_primary ? worker_msg_do_global_init :
		worker_msg_none;
	INIT_LIST_HEAD(&ret->node);
	err = pthread_create(&ret->id, NULL, worker_fn, ret);
	if (err) {
		free(ret->msg);
		free(ret);
		goto out;
	}
	/* If is_primary, global init is processed on thread startup, so we poll
	 * for the message queue to be idle before proceeding. Note, the reason
	 * for doing this is to ensure global-init happens before the regular
	 * message processing loop. */
	while (ret->msg->msg != worker_msg_none) {
		if (!pthread_tryjoin_np(ret->id, NULL)) {
			/* The worker is already gone */
			free(ret->msg);
			free(ret);
			goto out;
		}
		pthread_yield();
	}
	/* Block until the worker is in its polling loop (by sending a "list"
	 * command and waiting for it to get processed). This ensures any
	 * start-up logging is produced before the CLI prints another prompt. */
	if (!msg_list(ret))
		return ret;
out:
	error(0, 0,
	      "error: failed to create worker for cpu %d", cpu);
	return NULL;
}

static void worker_add(struct worker *worker)
{
	struct worker *i;
	/* Keep workers ordered by cpu */
	list_for_each_entry(i, &workers, node) {
		if (i->cpu > worker->cpu) {
			list_add_tail(&worker->node, &i->node);
			return;
		}
	}
	list_add_tail(&worker->node, &workers);
}
static void worker_free(struct worker *worker)
{
	int err, cpu = worker->cpu;
	uint32_t uid = worker->uid;
	BUG_ON(worker == primary);
	msg_quit(worker);
	err = pthread_join(worker->id, NULL);
	if (err) {
		/* Leak, but warn */
		error(0, 0,
		      "Failed to join thread uid:%u (cpu %d)",
		      worker->uid, worker->cpu);
		return;
	}
	list_del(&worker->node);
	free(worker->msg);
	free(worker);
	fprintf(stderr, "Thread uid:%u killed (cpu %d)\n", uid, cpu);
}

static int worker_reap(struct worker *worker)
{
	if (pthread_tryjoin_np(worker->id, NULL))
		return -EBUSY;
	if (worker == primary) {
		error(0, 0, "Primary thread died!");
		abort();
	}
	if (!list_empty(&worker->node))
		list_del(&worker->node);
	free(worker->msg);
	free(worker);
	return 0;
}
/**************************************/
/* CLI and command-line parsing utils */
/**************************************/

/* Parse a cpu id. On entry legit/len contain acceptable "next char" values, on
 * exit legit points to the "next char" we found. Return -1 for bad parse. */
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
	error(0, 0, "error: invalid cpu '%s'", str);
	return ret;
}

/* Parse a cpu range (eg. "3"=="3..3"). Return 0 for valid parse. */
static int parse_cpus(const char *str, int *start, int *end)
{
	/* Note: arrays of chars, not strings. Also sizeof(), not strlen()! */
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

struct fra_arguments {
	const char *fm_cfg;
	const char *fm_pcd;
	const char *fra_cfg;
	int first, last;
	int noninteractive;
	int only_rman;
};

//const char *argp_program_version = PACKAGE_VERSION;
const char *argp_program_bug_address = "<usdpa-devel@gforge.freescale.net>";

static const char argp_doc[] = "\nUSDPAA fra-based application";
static const char _fra_args[] = "[cpu-range]";

static const struct argp_option argp_opts[] = {
	{"fm-config", 'c', "FILE", 0, "FMC configuration XML file"},
	{"fm-pcd", 'p', "FILE", 0, "FMC PCD XML file"},
	{"fm-interfaces", 'i', "interface names", 0, "FMAN interfaces"},
	{"fra-config", 'f', "FILE", 0, "FRA configuration XML file"},
	{"non-interactive", 'n', 0, 0, "Ignore stdin"},
	{"rman only", 'r', 0, 0, "Only initialize RMan without FMan"},
	{"cpu-range", 0, 0, OPTION_DOC, "'index' or 'first'..'last'"},
	{}
};

static error_t fra_parse(int key, char *arg, struct argp_state *state)
{
	int _errno;
	struct fra_arguments *args;

	args = (typeof(args))state->input;
	switch (key) {
	case 'c':
		args->fm_cfg = arg;
		break;
	case 'p':
		args->fm_pcd = arg;
		break;
	case 'f':
		args->fra_cfg = arg;
		break;
	case 'n':
		args->noninteractive = 1;
		break;
	case 'r':
		args->only_rman = 1;
		break;
	case ARGP_KEY_ARGS:
		if (state->argc - state->next != 1)
			argp_usage(state);
		_errno = parse_cpus(state->argv[state->next],
				    &args->first, &args->last);
		if (unlikely(_errno < 0))
			argp_usage(state);
		break;
	default:
		return ARGP_ERR_UNKNOWN;
	}
	return 0;
}

static const struct argp fra_argp = {argp_opts, fra_parse, _fra_args,
				      argp_doc, NULL};

static struct fra_arguments fra_args;

/***************/
/* CLI support */
/***************/

extern const struct cli_table_entry cli_table_start[], cli_table_end[];

#define foreach_cli_table_entry(cli_cmd)				\
	for (cli_cmd = cli_table_start; cli_cmd < cli_table_end; cli_cmd++)

static int fra_cli_help(int argc, char *argv[])
{
	const struct cli_table_entry *cli_cmd;

	puts("Available commands:");
	foreach_cli_table_entry(cli_cmd) {
		error(0, 0, "%s ", cli_cmd->cmd);
	}
	puts("");

	return argc != 1 ? -EINVAL : 0;
}

void test_speed_to_send(void)
{
	struct worker *worker;
	int loop;

	for (loop = 0; loop < test_speed.total_loop; loop++) {
		memset(send_time, 0, sizeof(send_time));
		memset(receive_time, 0, sizeof(receive_time));
		list_for_each_entry(worker, &workers, node) {
			msg_do_test_speed(worker);
			break;
		}
	}
}

cli_cmd(help, fra_cli_help);

const char fra_prompt[] = "fra> ";

int main(int argc, char *argv[])
{
	struct worker *worker, *tmpworker;
	const char *fra_cfg_path = NULL;
	const char *envp;
	int loop;
	int rcode, cli_argc;
	char *cli, **cli_argv;
	const struct cli_table_entry *cli_cmd;

	rcode = of_init();
	if (rcode) {
		pr_err("of_init() failed");
		exit(EXIT_FAILURE);
	}

	ncpus = (unsigned long)sysconf(_SC_NPROCESSORS_ONLN);
	if (ncpus > 1) {
		fra_args.first = 1;
		fra_args.last = 1;
	}

	fra_args.noninteractive = 0;

	rcode = argp_parse(&fra_argp, argc, argv, 0, NULL, &fra_args);
	if (unlikely(rcode != 0))
		return -rcode;

	/* Do global init that doesn't require portal access; */
	/* - load the config (includes discovery and mapping of MAC devices) */
	FRA_DBG("Loading configuration");
	if (fra_args.fra_cfg != NULL)
		fra_cfg_path = fra_args.fra_cfg;
	else {
		envp = getenv("DEF_FRA_CFG_PATH");
		if (envp != NULL)
			fra_cfg_path = envp;
	}

	/* Parse FMC policy and configuration files for the network
	 * configuration. This also "extracts" other settings into 'netcfg' that
	 * are not necessarily from the XML files, such as the pool channels
	 * that the application is allowed to use (these are currently
	 * hard-coded into the netcfg code). */
	fra_cfg = fra_parse_cfgfile(fra_cfg_path);
	if (!fra_cfg) {
		error(0, 0,
		      "failed to load fra configuration");
		return -EINVAL;
	}
	/* - initialise DPAA */
	rcode = qman_global_init();
	if (rcode)
		error(0, 0,
		      "qman global init, continuing");
	rcode = bman_global_init();
	if (rcode)
		error(0, 0,
		      "bman global init, continuing");
	rcode = init_pool_channels();
	if (rcode)
		error(0, 0,
			"no pool channels available\n");
	/* - map DMA mem */
	FRA_DBG("Initialising DMA mem");
	dma_mem_generic = dma_mem_create(DMA_MAP_FLAG_ALLOC, NULL,
						 FRA_DMA_MAP_SIZE);
	if (!dma_mem_generic)
		fprintf(stderr, "error: dma_mem init, continuing\n");

	/* Create the threads */
	FRA_DBG("Starting %d threads for cpu-range '%d..%d'",
		fra_args.last - fra_args.first + 1,
		fra_args.first, fra_args.last);
	for (loop = fra_args.first; loop <= fra_args.last; loop++) {
		worker = worker_new(loop, !primary);
		if (!worker) {
			rcode = -EINVAL;
			goto leave;
		}
		if (!primary)
			primary = worker;
		worker_add(worker);
	}

	/* Run the CLI loop */
	while (1) {
		/* Reap any dead threads */
		list_for_each_entry_safe(worker, tmpworker, &workers, node)
			if (!worker_reap(worker))
				error(0, 0,
				      "Caught dead thread uid:%u (cpu %d)",
				      worker->uid, worker->cpu);

		/* If non-interactive, have the CLI thread twiddle its thumbs
		 * between (infrequent) checks for dead threads. */
		if (fra_args.noninteractive) {
			sleep(1);
			continue;
		}
		/* Get CLI input */
		cli = readline(fra_prompt);
		if (unlikely((cli == NULL) || strncmp(cli, "q", 1) == 0))
			break;
		if (cli[0] == 0) {
			free(cli);
			continue;
		}

		cli_argv = history_tokenize(cli);
		if (unlikely(cli_argv == NULL)) {
			error(0, 0,
			      "Out of memory while parsing: %s", cli);
			free(cli);
			continue;
		}
		for (cli_argc = 0; cli_argv[cli_argc] != NULL; cli_argc++)
			;
		foreach_cli_table_entry(cli_cmd) {
			if (strcmp(cli_argv[0], cli_cmd->cmd) == 0) {
				rcode = cli_cmd->handle(cli_argc, cli_argv);
				if (unlikely(rcode < 0))
					error(0, 0, "%s: %s",
					      cli_cmd->cmd, strerror(-rcode));
				add_history(cli);
				break;
			}
		}

		if (cli_cmd == cli_table_end)
			fprintf(stderr, "Unknown command: %s\n", cli);

		for (cli_argc = 0; cli_argv[cli_argc] != NULL; cli_argc++)
			free(cli_argv[cli_argc]);
		free(cli_argv);
		free(cli);
	}
	/* success */
	rcode = 0;

leave:
	/* Do datapath dependent cleanup before removing the primary worker */
    printf("****finish_pool_channels\n");
    msg_do_global_finish(primary);
	worker = primary;
	primary = NULL;
	worker_free(worker);
	finish_pool_channels();

	fra_cfg_release(fra_cfg);
	of_finish();
	return rcode;
}
