/*
 *Date:09/11/2014
 *
 *Fuction:Rapidio Send/Receive Packets
 *
 *Author:ctx
 */
#include <usdpaa/compat.h>
#include <usdpaa/of.h>
#include <usdpaa/dma_mem.h>
#include <usdpaa/fsl_dma.h>
#include <usdpaa/fsl_srio.h>
#include <error.h>
#include <atb_clock.h>
#include <readline.h>
#include <assert.h>
#include <inttypes.h>
#include <unistd.h>
#include <sys/time.h>

#define SEND_THREAD_NUM 2
#define RECEIVE_THREAD_NUM SEND_THREAD_NUM	/*ctx add*/
#define SRIO_SYS_ADDR		0x10000000	/* used for srio system addr */
#define SRIO_SYS_ADDR1		0x20000000	/* used for srio system addr */
#define SRIO_WIN_SIZE		0x1000000
#define THREAD_WIN_SIZE		(SRIO_WIN_SIZE/SEND_THREAD_NUM) /*ctx add*/
#define SRIO_POOL_PORT_SECT_NUM 4
#define SRIO_CMD_MIN_NUM	3
#define SRIO_POOL_PORT_OFFSET\
	(SRIO_WIN_SIZE * SRIO_POOL_PORT_SECT_NUM)
#define SRIO_POOL_SECT_SIZE	SRIO_WIN_SIZE /*POOLSIZE*/
#define SRIO_POOL_SIZE	0x8000000  /*dma pool size 64M*/
#define TEST_CMD_NUM		5
/*ctx add*/
#define SEND_NUM_OFFSET 0x7ffb00 /*reserve */
#define PACKET_LENGTH 0x100000	/*packet length*/
#define BUFFER_NUM 200		/*buffer number*/
#define SEND_TOTAL_NUM 100	/*SEND_NUMber*/
/*ctx end*/

struct srio_ctl{
   uint32_t number;
   uint32_t info;
   uint64_t rev[31];
};
struct srio_pool_org {
	uint8_t write_recv_data_t[SEND_THREAD_NUM][THREAD_WIN_SIZE]; /* space mapped to
							 other port win */
	uint8_t read_recv_data_t[SEND_THREAD_NUM][THREAD_WIN_SIZE]; /* port read data space */
	uint8_t write_data_prep_t[SEND_THREAD_NUM][THREAD_WIN_SIZE]; /* port DMA write data
							 prepare space */
	uint8_t res_t[SEND_THREAD_NUM][THREAD_WIN_SIZE];
};
struct srio_pool_org_thread {
	uint8_t *write_recv_data; /* space mapped to
							 other port win */
	uint8_t *read_recv_data; /* port read data space */
	uint8_t *write_data_prep; /* port DMA write data
							 prepare space */
	uint8_t *res;
};
struct srio_pool_org_phys {
	dma_addr_t write_recv_data;
	dma_addr_t read_recv_data;
	dma_addr_t write_data_prep;
	dma_addr_t res;
};
struct srio_port_data {
	struct srio_pool_org_phys phys;
	struct srio_pool_org *virt;
	struct srio_port_info port_info;
	void *range_virt;
};

struct srio_port_data_thread {
	struct srio_pool_org_phys phys;
	struct srio_pool_org_thread virt;
	struct srio_port_info port_info;
	/*void *range_virt;*//*1.6*/
};


enum srio_io_op {
	SRIO_DIR_WRITE,
	SRIO_DIR_READ,
	SRIO_DIR_SET_MEM,
	SRIO_DIR_PRI_MEM,
};

struct dma_pool {
	dma_addr_t dma_phys_base;
	void *dma_virt_base;
};


static const uint32_t srio_test_win_attrv[] = {3, 4, 5, 4, 0};
static const char * const test_param[5] = {"swrite", "nwrite", "nwrite_r", "nread"} ;
static const char * const perf_str[] = {"length(byte):", "time(us):",
					"avg Gb/s:", "max Gb/s:"};


/*ctx add*/
struct srio_packet{
	uint32_t length;
	unsigned char data[PACKET_LENGTH-4];	
};
/*ctx end*/
struct cmd_param_type {
	uint8_t curr_port_id;
	uint8_t test_type;/*test type*/
	uint8_t test_srio_type;
	uint8_t start_cpu;
        uint32_t passes;
};


struct task_arg_type {
	struct dma_ch *dmadev;
	struct srio_port_data_thread port_data_thread;/*ctx add*/
	uint8_t srio_type;
	uint32_t port;
        uint8_t test_type;
	int cpu;/*ctx add*/
        uint32_t passes;
};

enum srio_write_type {
	FLUSH,
	SWRITE,
	NWRITE,
	NWRITE_R,
	MAINTW,
};

enum srio_read_type {
	IO_READ_HOME,
	NREAD,
	MAINTR,
	ATOMIC_INC,
	ATOMIC_DEC,
	ATOMIC_SET,
	ATOMIC_CLR,
};

static struct cmd_param_type cmd_param;
static int port_num;

void usage()
{
      printf("[option]: --port          The number of the first port\n");
      printf("          --test_type     The type of the test\n");
      printf("          --type          The type of the data packet\n");
      printf("          --startcpu      The begin of the CPU \n");
      printf("          --help          The help of the program\n");
      printf("          --version       The version of the program\n");

}

/*ctx add*/
static int cmd_translate(int32_t cmd_num, char **cmd_in,struct cmd_param_type *cmd_param)
{
	int i=0,k=0,err = 0;
	for ( i = 1; i < cmd_num; i++)
	{
		char* arg = cmd_in[i];
		if (!strcmp(arg, "--port") && i + 1 < cmd_num)
		{
			cmd_param->curr_port_id = atoi(cmd_in[++i]) ;
		}
		else if (!strcmp(arg, "--passes") && i + 1 < cmd_num)
		{
			cmd_param->passes = strtoul(cmd_in[++i],NULL,10);
		}
		else if (!strcmp(arg, "--test_type") && i + 1 < cmd_num)
		{
			cmd_param->test_type = atoi(cmd_in[++i]);
		}
		else if (!strcmp(arg, "--startcpu") && i + 1 < cmd_num)
		{
			cmd_param->start_cpu = atoi(cmd_in[++i]);
		}
		else if (!strcmp(arg, "--type") && i + 1 < cmd_num) 
		{
			i++;
			for (k = 0;(k < 4) && test_param[k];k++)
			{
				if (!strcmp(cmd_in[i], test_param[k]))
					break;
 			}
			if(k==4)
			{
				printf("srio test type error!\n");
                                fflush(stdout);
				return -1;
			}		
			cmd_param->test_srio_type = k;
		}
		else if (!strcmp(arg, "--help")) 
		{
			usage();
			fflush(stdout);
                        return 2;
		}
		else if (!strcmp(arg, "--version")) 
		{
			printf("SRIO TEST VERSION:POWERPC-SRIO_TEST-V1.00\n");
			fflush(stdout);
			return 2;
		}else
		{
			printf("Unknown option: %s\n", arg);
			usage();
                        fflush(stdout);
	                return -1;
		}
	}
	return 0;
}


static void *t_srio_send(void *arg)
{
	struct task_arg_type *args = arg;
	struct dma_ch *dmadev = args->dmadev;
	struct srio_port_data_thread send_data = args->port_data_thread;
	uint32_t size = PACKET_LENGTH;
	uint32_t port = args->port;
	int err = 0,val=0;
	uint8_t srio_type = args->srio_type;
	uint8_t test_type = args->test_type;
	uint64_t src_phys,dest_phys;
	uint32_t buf_number=0,usebuf_number=0;
	uint32_t total_buf=BUFFER_NUM;
        uint64_t total_count=0;
	uint32_t send_num =0;
	struct atb_clock *atb_clock=NULL;
	uint64_t atb_multiplier=0;
	int atb_flag=0;
	cpu_set_t cpuset;
        double speed=0.0;
	struct timeval tm_start,tm_end;	
	CPU_ZERO(&cpuset);
	CPU_SET(args->cpu,&cpuset);
	err = pthread_setaffinity_np(pthread_self(),sizeof(cpu_set_t),&cpuset);
	uint8_t data=1;
        uint32_t passes=args->passes;
        uint32_t pi;
        gettimeofday(&tm_start,NULL);
        volatile struct srio_ctl *pcnt=NULL; 
        struct srio_ctl ctl_info;
	
        uint32_t offset= 0;
	if(srio_type!=3){
		src_phys = send_data.phys.write_data_prep+offset;	
		dest_phys = send_data.port_info.range_start+offset;
	}else{
		src_phys = send_data.port_info.range_start+offset;			
		dest_phys = send_data.phys.read_recv_data+offset;
	}
	while (1) 
	{	
		fsl_dma_direct_start(dmadev, src_phys, dest_phys,size);
		err = fsl_dma_wait(dmadev);
		if (err < 0) {
			printf("port %d: dma task error!\n", port + 1);
			fflush(stdout);
		}
                total_count++;
                gettimeofday(&tm_end,NULL);
		float diff=(tm_end.tv_sec-tm_start.tv_sec)+(tm_end.tv_usec-tm_start.tv_usec)/1000000.0;
                if(diff>5)
                {
                    double da_lu=total_count/diff;
                    printf("CPU:%d  port:%d  length(byte): %-15u time(s): %-15f  avg MB/s: %-15f total_count:%lld \n",args->cpu,port,size,diff,da_lu,total_count);
                    total_count=0;
                    gettimeofday(&tm_start,NULL);	
                }
       }
//err_dma: printf("Send error!\n");
//	 fflush(stdout);
	 pthread_exit(NULL);
}


/* Init DMA pool */
static int dma_usmem_init(struct dma_pool *pool)
{
	int err;

	dma_mem_generic = dma_mem_create(DMA_MAP_FLAG_ALLOC,
					NULL, SRIO_POOL_SIZE);
	if (!dma_mem_generic) {
		err = -EINVAL;
		error(0, -err, "%s(): dma_mem_create()", __func__);
		return err;
	}

	pool->dma_virt_base = __dma_mem_memalign(4096, SRIO_POOL_PORT_OFFSET);
	if (!pool->dma_virt_base) {
		err = -EINVAL;
		error(0, -err, "%s(): __dma_mem_memalign()", __func__);
		return err;
	}
	pool->dma_phys_base = __dma_mem_vtop(pool->dma_virt_base);

	return 0;
}

static int dma_pool_init(struct dma_pool **pool)
{
	struct dma_pool *dma_pool;
	int err;

	dma_pool = malloc(sizeof(*dma_pool));
	if (!dma_pool) {
		error(0, errno, "%s(): DMA pool", __func__);
		return -errno;
	}
	memset(dma_pool, 0, sizeof(*dma_pool));
	*pool = dma_pool;

	err = dma_usmem_init(dma_pool);
	if (err < 0) {
		error(0, -err, "%s(): DMA pool", __func__);
		free(dma_pool);
		return err;
	}

	return 0;
}

static void dma_pool_finish(struct dma_pool *pool)
{
	free(pool);
}

static void cmd_format_print(void)
{
	printf("-----------------SRIO APP CMD FORMAT-----------------\n");
	printf("srio_test --port --type --test_type --startcpu \n");
	printf("-----------------------------------------------------\n");
        fflush(stdout);
}

/* dma link data input */


int main(int argc, char *argv[])
{
	struct srio_dev *sriodev;
	struct dma_ch *send_dmadev[SEND_THREAD_NUM];
	struct dma_ch *receive_dmadev[RECEIVE_THREAD_NUM];
	struct dma_pool *dmapool = NULL;
	int i, err;
	struct srio_port_data *port_data;
	uint32_t attr_read, attr_write;
	struct task_arg_type task_arg_send[SEND_THREAD_NUM];
	struct task_arg_type task_arg_receive[RECEIVE_THREAD_NUM];
	pthread_t send_id[SEND_THREAD_NUM];
	pthread_t receive_id[RECEIVE_THREAD_NUM];
	of_init();
	err = fsl_srio_uio_init(&sriodev);
	if (err < 0)
		error(EXIT_FAILURE, -err, "%s(): srio_uio_init()", __func__);

	port_num = fsl_srio_get_port_num(sriodev);

	memset(&cmd_param, 0, sizeof(cmd_param));
/*ctx add*/
	cmd_param.curr_port_id=0;
	cmd_param.start_cpu=1;
	cmd_param.passes=10;
	cmd_param.test_type=0;
	cmd_param.test_srio_type=0;
	if (argc<3)
	{
		cmd_format_print();
		return -1;
	}
	err = cmd_translate(argc, argv,&cmd_param);
	if(err==2)
	{
		return 0;
	}
	if ((err < 0) ||(argc<3))
	{
		cmd_format_print();
		return -1;
	}
	
	

	port_data = malloc(sizeof(struct srio_port_data) * port_num);
	if (!port_data) {
		error(0, errno, "%s(): port_data", __func__);
		goto err_cmd_malloc;
	}

	for (i = 0; i < port_num; i++) {
		fsl_srio_connection(sriodev, i);
		fsl_srio_get_port_info(sriodev, i + 1, &port_data[i].port_info,
				       &port_data[i].range_virt);
	}

	err = fsl_srio_port_connected(sriodev);
	if (err <= 0) {
		error(0, -err, "%s(): fsl_srio_port_connected", __func__);
		goto err_srio_connected;
	}
	uint8_t flag=0;
	if(cmd_param.test_type==1)
	{
		for (i = 0; i < port_num; i++) {
			if(srio_link(sriodev,i))
			{
		  	     printf("port %d sucess!\n",i);
                             fflush(stdout);
			}
			else
			{
			     printf("port %d failed!\n",i);
                             fflush(stdout);
			     flag++;
			}
		}
		if(flag != 0)
			return -1;
		return 0;
	}
	err = dma_pool_init(&dmapool);
	uint8_t port = cmd_param.curr_port_id;
	attr_read = srio_test_win_attrv[3];
	attr_write = srio_test_win_attrv[cmd_param.test_srio_type];

	for (i = 0; i < port_num; i++) {
		dma_addr_t port_phys_base =
			dmapool->dma_phys_base + SRIO_POOL_PORT_OFFSET * i;
		port_data[i].phys.write_recv_data = port_phys_base;
		port_data[i].phys.read_recv_data =
			port_phys_base + SRIO_POOL_SECT_SIZE;
		port_data[i].phys.write_data_prep =
			port_phys_base + SRIO_POOL_SECT_SIZE * 2;
		port_data[i].phys.res =
			port_phys_base + SRIO_POOL_SECT_SIZE * 3;
		
		port_data[i].virt = (typeof(port_data[i].virt))
			(dmapool->dma_virt_base + i * SRIO_POOL_PORT_OFFSET);
		fsl_srio_set_ibwin(sriodev, i, 1,
				   port_data[i].phys.write_recv_data,
				   SRIO_SYS_ADDR, LAWAR_SIZE_16M);	
		if (fsl_srio_port_connected(sriodev) & (0x1 << i)) {
			fsl_srio_set_obwin(sriodev, i, 1,
				   port_data[i].port_info.range_start,
				   SRIO_SYS_ADDR, LAWAR_SIZE_16M);
			fsl_srio_set_obwin_attr(sriodev, i, 1,
					attr_read, attr_write);
		} else {
			printf("SRIO port %d error!\n", i + 1);
			fflush(stdout);
			return -errno;
		}
		memset(port_data[i].virt,0,SRIO_POOL_PORT_OFFSET);
	}
        err = fsl_srio_set_targetid(sriodev,0,1,0x11);
	if(err!=0)
        {
		printf("sro set targetid  failed!\n");
                fflush(stdout);
	}
        err = fsl_srio_set_targetid(sriodev,1,1,0x14);
	if(err!=0)
        {
		printf("sro set targetid  failed!\n");
                fflush(stdout);
	}
/*ctx add*/
	for(i=0;i<2;i++)
	{
		
		err = fsl_dma_chan_init(&send_dmadev[i], i, 1);	
		if (err < 0) {
			error(0, -err, "%s(): fsl_dma_chan_init()", __func__);
			goto err_srio_connected;
		}
		fsl_dma_chan_basic_direct_init(send_dmadev[i]);
		fsl_dma_chan_bwc(send_dmadev[i], DMA_BWC_1024);
		task_arg_send[i].dmadev = send_dmadev[i];
	        port =i;	
		/* ctx add*/
		task_arg_send[i].port_data_thread.phys.write_recv_data=port_data[port].phys.write_recv_data+THREAD_WIN_SIZE*1;
		task_arg_send[i].port_data_thread.phys.read_recv_data=port_data[port].phys.read_recv_data+THREAD_WIN_SIZE*0;
		task_arg_send[i].port_data_thread.phys.write_data_prep=port_data[port].phys.write_data_prep+THREAD_WIN_SIZE*0;
		task_arg_send[i].port_data_thread.phys.res=port_data[port].phys.res+THREAD_WIN_SIZE*0;
		task_arg_send[i].port_data_thread.virt.write_recv_data = &port_data[port].virt->write_recv_data_t[1][0];
		task_arg_send[i].port_data_thread.virt.read_recv_data = &port_data[port].virt->read_recv_data_t[0][0];
		task_arg_send[i].port_data_thread.virt.write_data_prep = &port_data[port].virt->write_data_prep_t[0][0];
		task_arg_send[i].port_data_thread.virt.res = &port_data[port].virt->res_t[0][0];
		task_arg_send[i].port_data_thread.port_info.range_start = port_data[port].port_info.range_start+THREAD_WIN_SIZE*0; 
		/* cta end*/
		task_arg_send[i].port = port;
		task_arg_send[i].srio_type = cmd_param.test_srio_type;
		task_arg_send[i].cpu = cmd_param.start_cpu+1;/*bind cpu*/
		task_arg_send[i].passes = cmd_param.passes;/*bind cpu*/
		err = pthread_create(&send_id[i], NULL,t_srio_send, &task_arg_send[i]);
		if (err) {
			printf("Port %d : Send thread failed!\n",port + 1);
			fflush(stdout);
			return -errno;
		}
	}

	for(i=0;i<2;i++)
	{
		pthread_join(send_id[i],NULL);
	}
/*ctx end*/

        free(port_data);
	for(i=0;i<SEND_THREAD_NUM;i++)
	{
		fsl_dma_chan_finish(send_dmadev[i]);
	}
	dma_pool_finish(dmapool);
	fsl_srio_uio_finish(sriodev);

	of_finish();
	return EXIT_SUCCESS;

err_srio_connected:
    free(port_data);
err_cmd_malloc:
	fsl_srio_uio_finish(sriodev);
	of_finish();

	return err;
}
