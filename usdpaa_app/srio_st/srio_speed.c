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

#define SEND_THREAD_NUM 2
#define RECEIVE_THREAD_NUM SEND_THREAD_NUM	/*ctx add*/
#define SRIO_SYS_ADDR		0x10000000	/* used for srio system addr */
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
#define SEND_NUM_OFFSET 0x7ffff0 /*reserve */
#define PACKET_LENGTH 32768	/*packet length*/
#define BUFFER_NUM 200		/*buffer number*/
#define SEND_TOTAL_NUM 100	/*SEND_NUMber*/
/*ctx end*/

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
static const char * const srio_test_win_attrc[] = {"SWRITE", "NWRITE",
					"NWRITE_R", "NREAD", "DMA/CORE"};
/*static const char * const cmd_name[] = {"-attr", "-op", "-test","-send"};cta add send*/
static const char * const cmd_name[] = {"-send"};
static const char * const test_param[][5] = {{"srio", "dma_chain", "show_perf",
				"show_task", "free_task"},
				{"port1", "port2"},
				{"dma", "core"},
				{"swrite", "nwrite", "nwrite_r", "nread"} };
static const char * const perf_str[] = {"length(byte):", "time(us):",
					"avg Gb/s:", "max Gb/s:"};

enum srio_cmd {
	SRIO_SEND
};/*ctx add*/

struct cmd_port_param {
	uint32_t attr_cmd3_id;
	uint32_t attr_cmd4;
	uint32_t attr_tdid;
	uint32_t attr_read;
	uint32_t attr_write;
	uint8_t op_type;
	uint8_t op_win_id;
	uint8_t op_seg_id;
	uint8_t op_subseg_id;
	size_t op_len;
};
/*ctx add*/
struct srio_packet{
	uint32_t length;
	unsigned char data[PACKET_LENGTH-4];	
};
/*ctx end*/
struct cmd_param_type {
	struct cmd_port_param *port;
	uint8_t curr_cmd;
	uint8_t curr_port_id;
	uint8_t test_type;
	uint8_t test_bwc;
	uint8_t test_task_type;
	uint8_t test_srio_type;
	uint8_t test_srio_priority;
	uint8_t test_data;
	uint32_t test_num;
};


struct task_arg_type {
	struct dma_ch *dmadev;
	struct srio_port_data_thread port_data_thread;/*ctx add*/
	uint8_t srio_type;
	uint32_t port;
	int cpu;/*ctx add*/
	/*ctx add*/
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

static int send_param_trans(int32_t cmd_num, char **cmd_in)
{
	int32_t i;

	if (cmd_num > TEST_CMD_NUM)
		return -EINVAL;

	for (i = 0;
		(i < 2)&& test_param[1][i]; i++)
		if (!strcmp(cmd_in[2], test_param[1][i]))
			break;

		if ((i == 2 )|| !test_param[1][i])
			return -EINVAL;
		cmd_param.curr_port_id = i;

	for (i = 0;
		(i < 4) && test_param[3][i];i++)
		if (!strcmp(cmd_in[3], test_param[3][i]))
			break;

		if ((i == 4) || !test_param[3][i])
			return -EINVAL;
		cmd_param.test_srio_type = i;
		cmd_param.test_num = strtoul(cmd_in[4],NULL,0);

	return 0;
}
/*ctx add*/
static int cmd_translate(int32_t cmd_num, char **cmd_in)
{
	int i, err = 0;

	if (cmd_num < SRIO_CMD_MIN_NUM)
		return -EINVAL;

	for (i = 0; (i < 1) && cmd_name[i]; i++)
		if (!strcmp(cmd_in[1], cmd_name[i]))
			break;

	if ((i == 1) || !cmd_name[i])
		return -EINVAL;

	cmd_param.curr_cmd = i;

	switch (i) {
	case SRIO_SEND:
		err = send_param_trans(cmd_num, cmd_in);
		break;
	default:
		return -EINVAL;
	}

	return err;
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
	uint64_t src_phys,dest_phys;
	uint32_t buf_number=0,usebuf_number=0;
	uint32_t total_buf=BUFFER_NUM;
	uint32_t send_num =0;
	struct atb_clock *atb_clock=NULL;
	uint64_t atb_multiplier=0;
	int atb_flag=0;
	uint32_t test_packets_num = cmd_param.test_num;
	cpu_set_t cpuset;
        double speed=0.0;	
	CPU_ZERO(&cpuset);
	CPU_SET(args->cpu,&cpuset);
	err = pthread_setaffinity_np(pthread_self(),sizeof(cpu_set_t),&cpuset);


	while (1) {
		buf_number = *(volatile uint32_t *)send_data.virt.write_recv_data;
		/*for(;usebuf_number<buf_number;)*/
		{
/*ctx add send packet */
			uint32_t offset=0; /*(usebuf_number%total_buf)*sizeof(struct srio_packet);*/
			if(srio_type!=3){
				src_phys = send_data.phys.write_data_prep+offset;	
				
				dest_phys = send_data.port_info.range_start+offset;
			}else{
				src_phys = send_data.port_info.range_start+offset;			
				dest_phys = send_data.phys.read_recv_data+offset;
			}

			memcpy(send_data.virt.write_data_prep+offset,&size,sizeof(uint32_t));
			memset((send_data.virt.write_data_prep+offset+4),usebuf_number+1,(size-4));
/*ctx send*/
			if (test_packets_num && (!atb_flag)) {
				atb_flag = 1;
				atb_clock = malloc(sizeof(struct atb_clock));
				if(!atb_clock)
				{
					printf("show performance error!\n");
					fflush(stdout);
					exit(1);
				}
				atb_multiplier = atb_get_multiplier();
				atb_clock_init(atb_clock);
				atb_clock_reset(atb_clock);
				
			}
			if (test_packets_num && atb_flag)
				atb_clock_start(atb_clock);
			fsl_dma_direct_start(dmadev, src_phys, dest_phys,size);
			err = fsl_dma_wait(dmadev);
			if (err < 0) {
				printf("port %d: dma task error!\n", port + 1);
				fflush(stdout);
				goto err_dma;
			}
			if (test_packets_num && atb_flag)
				atb_clock_stop(atb_clock);
			test_packets_num--;
			if (!test_packets_num && cmd_param.test_num) {
				speed=(size * 8.0 * cmd_param.test_num /
				(atb_to_seconds(atb_clock_total(atb_clock),
					atb_multiplier) * 1000000000.0));
				printf("CPU:%d PORT:%d %s %-15u %s %-15f %s %-15f %s %-15f\n",args->cpu,port,
				perf_str[0], size, perf_str[1],
				atb_to_seconds(atb_clock_total(atb_clock),atb_multiplier) /
				cmd_param.test_num* ATB_MHZ,
				perf_str[2],
				speed,
				perf_str[3],
				size * 8.0 /
				(atb_to_seconds(atb_clock_min(atb_clock),
					atb_multiplier) * 1000000000.0));
				fflush(stdout);
				if(speed<13)
				{
					printf("speed error:%15f\n",speed);
				        fflush(stdout);
				}
				atb_clock_finish(atb_clock);
				test_packets_num = cmd_param.test_num;
				atb_flag = 0;
			}
			/*send success*/
			usebuf_number++;
			memset((send_data.virt.write_data_prep+offset),0,sizeof(struct srio_packet));
			send_num++;
		/*	if(send_num == SEND_TOTAL_NUM)
			{
				memcpy((send_data.virt.write_data_prep+SEND_NUM_OFFSET),&usebuf_number,sizeof(uint32_t));
				src_phys = send_data.phys.write_data_prep+SEND_NUM_OFFSET;	
				dest_phys = send_data.port_info.range_start+SEND_NUM_OFFSET;
				fsl_dma_direct_start(dmadev, src_phys, dest_phys,sizeof(uint32_t));
				err = fsl_dma_wait(dmadev);
				if (err < 0) {
					printf("port %d: dma task error!\n", port + 1);
				        fflush(stdout);
					goto err_dma;
				}
				send_num=0;
			}*/ 
			
/*end*/
		}


	}
err_dma: printf("Send error!\n");
	 fflush(stdout);
	 pthread_exit(NULL);
}

static void *t_srio_receive(void *arg)
{
	struct task_arg_type *args = arg;
	struct dma_ch *dmadev = args->dmadev;
	
	struct srio_port_data_thread receive_data = args->port_data_thread;
	uint32_t port = args->port;
	int err = 0,i=0;
	uint8_t srio_type = args->srio_type;
	uint64_t src_phys,dest_phys;

	uint32_t count=0,total_buf=BUFFER_NUM,buf_number=BUFFER_NUM;
	cpu_set_t cpuset;
	uint32_t receive_num=0,use_num=0,packet_num=0;
	
	CPU_ZERO(&cpuset);
	CPU_SET(args->cpu,&cpuset);
	err = pthread_setaffinity_np(pthread_self(),sizeof(cpu_set_t),&cpuset);

	if(err){
		printf("(%d)fail:pthread_setaffinity_np()\n",args->cpu);
		fflush(stdout);
		return NULL;
	}

	if(srio_type!=3){
		src_phys = receive_data.phys.write_data_prep;			
		dest_phys = receive_data.port_info.range_start;
	}else{
		src_phys = receive_data.port_info.range_start;			
		dest_phys = receive_data.phys.read_recv_data;
	}
	memcpy(receive_data.virt.write_data_prep,&buf_number,sizeof(uint32_t));
	
/*ctx send*/
	fsl_dma_direct_start(dmadev, src_phys, dest_phys, sizeof(uint32_t));
	err = fsl_dma_wait(dmadev);
	if (err < 0) {
		printf("port %d: dma task error!\n", port + 1);
		fflush(stdout);
		return NULL;
	}

	while (1) 
	{
/*ctx add receive packet */
		uint32_t offset=(count%buf_number)*sizeof(struct srio_packet);
		receive_num = *(volatile uint32_t *)(receive_data.virt.write_recv_data+SEND_NUM_OFFSET);
		if(receive_num>use_num)
		{
			packet_num = receive_num-use_num;
			if(use_num == 0 || use_num == BUFFER_NUM)
			{
				printf("CPU:%d PORT:%d\n",args->cpu,port);
				fflush(stdout);
				char *p=receive_data.virt.write_recv_data+offset+4;
				int error_count=0;
				for(i=0;i<10;i++)
				{
					if(*p!=(use_num+1))
					{
						error_count++;
					}
					p++;
				}
				if(error_count!=0)
				{
					printf("Receive Data:%02x  Test Data:%02x error Number:%d\n",*p,use_num+1,error_count);
				        fflush(stdout);
				}
				else
				{
					printf("Data Right!\n");
				}
			}
		
			memset((receive_data.virt.write_recv_data+offset),0,sizeof(struct srio_packet)*packet_num);
			
			count=count+packet_num;
			total_buf=total_buf+packet_num;
			memcpy(receive_data.virt.write_data_prep,&total_buf,sizeof(uint32_t));
/*ctx send*/
			fsl_dma_direct_start(dmadev, src_phys, dest_phys, sizeof(uint32_t));
			err = fsl_dma_wait(dmadev);
			if (err < 0) {
				printf("port %d: dma task error!\n", port + 1);
				fflush(stdout);
				break;
			}
			use_num=receive_num;
			
		}	
/*end*/
		
	}
	printf("Receive error!\n");
	fflush(stdout);
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

	pool->dma_virt_base = __dma_mem_memalign(64, SRIO_POOL_PORT_OFFSET);
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
	printf("sra -send port1 swrite start_CPU  test_number \n");
	printf("-----------------------------------------------------\n");
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
	cmd_param.port = malloc(sizeof(struct cmd_port_param) * port_num);
	if (!cmd_param.port) {
		error(0, errno, "%s(): command port", __func__);
		goto err_cmd_malloc;
	}
	memset(cmd_param.port, 0, sizeof(struct cmd_port_param));
/*ctx add*/
	err = cmd_translate(argc, argv);
	if (err < 0)
		cmd_format_print();

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
	}
        err = fsl_srio_set_targetid(sriodev,1,1,0x14);
	if(err!=0)
        {
		printf("sro set targetid  failed!\n");
	}
/*ctx add*/
        sleep(2);
	for(i=0;i<1;i++)
	{
		
		err = fsl_dma_chan_init(&send_dmadev[i], 0, i);	
		if (err < 0) {
			error(0, -err, "%s(): fsl_dma_chan_init()", __func__);
			goto err_srio_connected;
		}
		err = fsl_dma_chan_init(&receive_dmadev[i], 1, i);
		if (err < 0) {
			error(0, -err, "%s(): fsl_dma_chan_init()", __func__);
			goto err_srio_connected;
		}
		fsl_dma_chan_basic_direct_init(send_dmadev[i]);
		fsl_dma_chan_bwc(send_dmadev[i], DMA_BWC_1024);
		task_arg_send[i].dmadev = send_dmadev[i];
		
		fsl_dma_chan_basic_direct_init(receive_dmadev[i]);
		fsl_dma_chan_bwc(receive_dmadev[i], DMA_BWC_1024);
		task_arg_receive[i].dmadev = receive_dmadev[i];
		
		
		task_arg_receive[i].port_data_thread.phys.write_recv_data=port_data[1-port].phys.write_recv_data+THREAD_WIN_SIZE*i;
		task_arg_receive[i].port_data_thread.phys.read_recv_data=port_data[1-port].phys.read_recv_data+THREAD_WIN_SIZE*i;
		task_arg_receive[i].port_data_thread.phys.write_data_prep=port_data[1-port].phys.write_data_prep+THREAD_WIN_SIZE*i;
		task_arg_receive[i].port_data_thread.phys.res=port_data[1-port].phys.res+THREAD_WIN_SIZE*i;
		
		task_arg_receive[i].port_data_thread.virt.write_recv_data = &port_data[1-port].virt->write_recv_data_t[i][0];
		
		task_arg_receive[i].port_data_thread.virt.read_recv_data = &port_data[1-port].virt->read_recv_data_t[i][0];
		
		task_arg_receive[i].port_data_thread.virt.write_data_prep = &port_data[1-port].virt->write_data_prep_t[i][0];
		
		task_arg_receive[i].port_data_thread.virt.res = &port_data[1-port].virt->res_t[i][0];
		
		task_arg_receive[i].port_data_thread.port_info.range_start = port_data[1-port].port_info.range_start+THREAD_WIN_SIZE*i; 
		
		task_arg_receive[i].port = 1-port;
		task_arg_receive[i].srio_type = cmd_param.test_srio_type;
		task_arg_receive[i].cpu = i+SEND_THREAD_NUM+2;/*bind cpu*/
		
		err = pthread_create(&receive_id[i],NULL,t_srio_receive,&task_arg_receive[i]);
		if (err) {
			printf("Port %d : Receive thread failed!\n",2-port);
			fflush(stdout);
			return -errno;
		} 
		
		/* ctx add*/
		task_arg_send[i].port_data_thread.phys.write_recv_data=port_data[port].phys.write_recv_data+THREAD_WIN_SIZE*i;
		task_arg_send[i].port_data_thread.phys.read_recv_data=port_data[port].phys.read_recv_data+THREAD_WIN_SIZE*i;
		task_arg_send[i].port_data_thread.phys.write_data_prep=port_data[port].phys.write_data_prep+THREAD_WIN_SIZE*i;
		task_arg_send[i].port_data_thread.phys.res=port_data[port].phys.res+THREAD_WIN_SIZE*i;
		task_arg_send[i].port_data_thread.virt.write_recv_data = &port_data[port].virt->write_recv_data_t[i][0];
		task_arg_send[i].port_data_thread.virt.read_recv_data = &port_data[port].virt->read_recv_data_t[i][0];
		task_arg_send[i].port_data_thread.virt.write_data_prep = &port_data[port].virt->write_data_prep_t[i][0];
		task_arg_send[i].port_data_thread.virt.res = &port_data[port].virt->res_t[i][0];
		task_arg_send[i].port_data_thread.port_info.range_start = port_data[port].port_info.range_start+THREAD_WIN_SIZE*i; 
		/* cta end*/
		task_arg_send[i].port = port;
		task_arg_send[i].srio_type = cmd_param.test_srio_type;
		task_arg_send[i].cpu = i+2;/*bind cpu*/

		err = pthread_create(&send_id[i], NULL,t_srio_send, &task_arg_send[i]);
		if (err) {
			printf("Port %d : Send thread failed!\n",port + 1);
			fflush(stdout);
			return -errno;
		} 	
	}
/*multiple*/
	for(i=1;i<SEND_THREAD_NUM;i++)
	{
		
		err = fsl_dma_chan_init(&send_dmadev[i], 0, i);	
		if (err < 0) {
			error(0, -err, "%s(): fsl_dma_chan_init()", __func__);
			goto err_srio_connected;
		}
		err = fsl_dma_chan_init(&receive_dmadev[i], 1, i);
		if (err < 0) {
			error(0, -err, "%s(): fsl_dma_chan_init()", __func__);
			goto err_srio_connected;
		}
		fsl_dma_chan_basic_direct_init(send_dmadev[i]);
		fsl_dma_chan_bwc(send_dmadev[i], DMA_BWC_1024);
		task_arg_send[i].dmadev = send_dmadev[i];
		
		fsl_dma_chan_basic_direct_init(receive_dmadev[i]);
		fsl_dma_chan_bwc(receive_dmadev[i], DMA_BWC_1024);
		task_arg_receive[i].dmadev = receive_dmadev[i];
		
		/* ctx add*/
		task_arg_send[i].port_data_thread.phys.write_recv_data=port_data[port].phys.write_recv_data+THREAD_WIN_SIZE*i;
		task_arg_send[i].port_data_thread.phys.read_recv_data=port_data[port].phys.read_recv_data+THREAD_WIN_SIZE*i;
		task_arg_send[i].port_data_thread.phys.write_data_prep=port_data[port].phys.write_data_prep+THREAD_WIN_SIZE*i;
		task_arg_send[i].port_data_thread.phys.res=port_data[port].phys.res+THREAD_WIN_SIZE*i;
		task_arg_send[i].port_data_thread.virt.write_recv_data = &port_data[port].virt->write_recv_data_t[i][0];
		task_arg_send[i].port_data_thread.virt.read_recv_data = &port_data[port].virt->read_recv_data_t[i][0];
		task_arg_send[i].port_data_thread.virt.write_data_prep = &port_data[port].virt->write_data_prep_t[i][0];
		task_arg_send[i].port_data_thread.virt.res = &port_data[port].virt->res_t[i][0];
		task_arg_send[i].port_data_thread.port_info.range_start = port_data[port].port_info.range_start+THREAD_WIN_SIZE*i; 
		/* cta end*/
		task_arg_send[i].port = port;
		task_arg_send[i].srio_type = cmd_param.test_srio_type;
		task_arg_send[i].cpu = i+2;/*bind cpu*/

		err = pthread_create(&send_id[i], NULL,t_srio_receive, &task_arg_send[i]);
		if (err) {
			printf("Port %d : Send thread failed!\n",port + 1);
			fflush(stdout);
			return -errno;
		} 
		
		task_arg_receive[i].port_data_thread.phys.write_recv_data=port_data[1-port].phys.write_recv_data+THREAD_WIN_SIZE*i;
		task_arg_receive[i].port_data_thread.phys.read_recv_data=port_data[1-port].phys.read_recv_data+THREAD_WIN_SIZE*i;
		task_arg_receive[i].port_data_thread.phys.write_data_prep=port_data[1-port].phys.write_data_prep+THREAD_WIN_SIZE*i;
		task_arg_receive[i].port_data_thread.phys.res=port_data[1-port].phys.res+THREAD_WIN_SIZE*i;
		
		task_arg_receive[i].port_data_thread.virt.write_recv_data = &port_data[1-port].virt->write_recv_data_t[i][0];
		
		task_arg_receive[i].port_data_thread.virt.read_recv_data = &port_data[1-port].virt->read_recv_data_t[i][0];
		
		task_arg_receive[i].port_data_thread.virt.write_data_prep = &port_data[1-port].virt->write_data_prep_t[i][0];
		
		task_arg_receive[i].port_data_thread.virt.res = &port_data[1-port].virt->res_t[i][0];
		
		task_arg_receive[i].port_data_thread.port_info.range_start = port_data[1-port].port_info.range_start+THREAD_WIN_SIZE*i; 
		
		task_arg_receive[i].port = 1-port;
		task_arg_receive[i].srio_type = cmd_param.test_srio_type;
		task_arg_receive[i].cpu = i+SEND_THREAD_NUM+2;/*bind cpu*/
		
		err = pthread_create(&receive_id[i],NULL,t_srio_send,&task_arg_receive[i]);
		if (err) {
			printf("Port %d : Receive thread failed!\n",2-port);
			fflush(stdout);
			return -errno;
		} 
	
	}

	for(i=0;i<SEND_THREAD_NUM;i++)
	{
		pthread_join(send_id[i],NULL);
		pthread_join(receive_id[i],NULL);
	}
/*ctx end*/

	free(port_data);
	free(cmd_param.port);
	for(i=0;i<SEND_THREAD_NUM;i++)
	{
		fsl_dma_chan_finish(send_dmadev[i]);
		fsl_dma_chan_finish(receive_dmadev[i]);
	}
	dma_pool_finish(dmapool);
	fsl_srio_uio_finish(sriodev);

	of_finish();
	return EXIT_SUCCESS;

err_srio_connected:
	free(port_data);
	free(cmd_param.port);
err_cmd_malloc:
	fsl_srio_uio_finish(sriodev);
	of_finish();

	return err;
}
