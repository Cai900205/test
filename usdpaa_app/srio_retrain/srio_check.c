/*
 *Date:09/11/2014
 *
 *Fuction:Rapidio Send/Receive Packets
 *
 *Author:ctx
 */
#include "help.h"
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
	uint8_t *write_recv_data; /* space mapped to*/
	uint8_t *read_recv_data; /* port read data space */
	uint8_t *write_data_prep; /* port DMA write data  prepare space */
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
        uint8_t bind;
        uint8_t workers;
        uint8_t interval;
        uint32_t time;
	uint32_t passes;
};


struct task_arg_type {
	struct dma_ch *dmadev;
	struct srio_dev *sriodev;
	struct srio_port_data_thread port_data_thread;/*ctx add*/
	uint8_t srio_type;
	uint32_t port;
        uint8_t test_type;
	int bind;/*ctx add*/
        uint32_t time;
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

static int cmd_translate(int32_t cmd_num, char **cmd_in,struct cmd_param_type *cmd_param)
{
	int i=0,k=0,err = 0;
	for ( i = 1; i < cmd_num; i++)
	{
		char* arg = cmd_in[i];
		if (!strcmp(arg, "--bind"))
		{
			cmd_param->bind=1;
		}
		else if (!strcmp(arg, "--workers") && i + 1 < cmd_num)
		{
			cmd_param->workers = atoi(cmd_in[++i]);
		}
		else if (!strcmp(arg, "--test_type") && i + 1 < cmd_num)
		{
			cmd_param->test_type = atoi(cmd_in[++i]);
		}
		else if (!strcmp(arg, "--passes") && i + 1 < cmd_num)
		{
			cmd_param->passes = strtoul(cmd_in[++i],NULL,10);
		}
		else if (!strcmp(arg, "--time") && i + 1 < cmd_num)
		{
			cmd_param->time = strtoul(cmd_in[++i],NULL,10);
		}
		else if (!strcmp(arg, "--interval") && i + 1 < cmd_num)
		{
			cmd_param->interval = strtoul(cmd_in[++i],NULL,10);
		}
		else if (!strcmp(arg, "--data_type") && i + 1 < cmd_num) 
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
    struct srio_dev *sriodev = args->sriodev;
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
    uint32_t passes=args->passes;
    uint32_t time=args->time;
    uint8_t bind=args->bind;
    int result=-1;
    struct timeval tm_start,tm_end;	
    CPU_ZERO(&cpuset);
    if(bind)
    {
        if(sched_getaffinity(0,sizeof(cpuset),&cpuset) == -1)
        {
           printf("warning: cound not get cpu affinity!\n");
           fflush(stdout);  
           return (void*)-1;
        }
	result = pthread_setaffinity_np(pthread_self(),sizeof(cpu_set_t),&cpuset);
        if (result)
	{
           printf("[SRIO_TEST]: Bind cpu  failed\n");
           fflush(stdout);  
           return (void*)-1;
	}

    }
    uint8_t data=1;
    uint32_t pi;
    gettimeofday(&tm_start,NULL);
    volatile struct srio_ctl *pcnt=NULL; 
    struct srio_ctl ctl_info;
    memset(&ctl_info,0,sizeof(struct srio_ctl));
 
    pcnt=(struct srio_ctl *)(send_data.virt.write_recv_data);

    for(pi=0;(pi<passes||(!passes));)
    {	
	if(test_type)
	{
	    uint32_t offset= 0;
	    if(srio_type!=3){
			src_phys = send_data.phys.write_data_prep+offset;	
				
			dest_phys = send_data.port_info.range_start+offset;
	    }else{
			src_phys = send_data.port_info.range_start+offset;			
			dest_phys = send_data.phys.read_recv_data+offset;
	    }
	    //memset((send_data.virt.write_data_prep+offset),0x5a,size);
            size=0x100000;
            
	    fsl_dma_direct_start(dmadev, src_phys, dest_phys,size);
	    err = fsl_dma_wait(dmadev);
	    if (err < 0) {
			printf("port %d: dma task error!\n", port + 1);
                        fsl_srio_clr_bus_err(sriodev);
			fflush(stdout);
	    }
            total_count++;
            gettimeofday(&tm_end,NULL);
	    float diff=(tm_end.tv_sec-tm_start.tv_sec)+(tm_end.tv_usec-tm_start.tv_usec)/1000000.0;
            if(diff>5)
            {
                double da_lu=total_count/diff;
                printf("  port:%d  length(byte): %-15u time(s): %-15f  avg MB/s: %-15f total_count:%lld \n",port,size,diff,da_lu,total_count);
                fflush(stdout);
	        if(da_lu<1000){
		         printf("speed error:%15f\n",da_lu);
		         fflush(stdout);
		}                           
                total_count=0;
                gettimeofday(&tm_start,NULL);	
                pi++;
            }
	}
	else
	{
		
		buf_number = pcnt->number;
		for(;usebuf_number<buf_number;)
		{
/*ctx add send packet */
			uint32_t offset= (usebuf_number%total_buf)*sizeof(struct srio_packet);
			if(srio_type!=3){
				src_phys = send_data.phys.write_data_prep+offset;	
				dest_phys = send_data.port_info.range_start+offset;
			}else{
				src_phys = send_data.port_info.range_start+offset;			
				dest_phys = send_data.phys.read_recv_data+offset;
			}

			memset((send_data.virt.write_data_prep+offset),data,size);
			data++;
/*ctx send*/
			fsl_dma_direct_start(dmadev, src_phys, dest_phys,size);
			err = fsl_dma_wait(dmadev);
			if (err < 0) {
				printf("port %d: dma task error!\n", port + 1);
                                fsl_srio_clr_bus_err(sriodev);
				fflush(stdout);
			}
			usebuf_number++;
			send_num++;
			if(send_num == SEND_TOTAL_NUM)
			{
		
                                ctl_info.number=usebuf_number;
				memcpy((send_data.virt.write_data_prep+SEND_NUM_OFFSET),&ctl_info,sizeof(struct srio_ctl)); 
				src_phys = send_data.phys.write_data_prep+SEND_NUM_OFFSET;	
				dest_phys = send_data.port_info.range_start+SEND_NUM_OFFSET;
				fsl_dma_direct_start(dmadev, src_phys, dest_phys,sizeof(struct srio_ctl));
				err = fsl_dma_wait(dmadev);
				if (err < 0) {
					printf("port %d: dma task error!\n", port + 1);
                                        fsl_srio_clr_bus_err(sriodev);
					fflush(stdout);
				}
				send_num=0;
                                pi++;
			}
                        if((pi == passes)&& (passes))
                             break; 
		}			
/*end*/
	}

     }

     pthread_exit(NULL);
}

uint32_t test_data(uint8_t src, uint8_t *buf,uint8_t count ,uint8_t step)
{
    uint8_t i=0;
    uint32_t error_count=0;
    for(i=0;i<count;i++)
    {
       if(src!=*buf)
       {
          error_count++;
          printf("###Receive ERROR Data:%02x  addr: %08x Test Data:%02x  option:%d\n",*buf,buf,src,i);
          fflush(stdout);
       }
       src=src+step;
       buf++;
    }
    return error_count;
}
static void *t_srio_receive(void *arg)
{
    struct task_arg_type *args = arg;
    struct dma_ch *dmadev = args->dmadev;
    struct srio_dev *sriodev = args->sriodev;
    struct srio_port_data_thread receive_data = args->port_data_thread;
    uint32_t port = args->port;
    int err = 0;
    uint32_t i=0,k=0;
    uint8_t srio_type = args->srio_type;
    uint64_t src_phys,dest_phys;

    uint32_t count=0,total_buf=BUFFER_NUM,buf_number=BUFFER_NUM;
    cpu_set_t cpuset;
    uint32_t receive_num=0,use_num=0,packet_num=0;
	
    uint32_t passes=args->passes;
    uint32_t time=args->time;
    uint8_t bind=args->bind;
    int result=-1;
    CPU_ZERO(&cpuset);

    if(bind)
    {
        if(sched_getaffinity(0,sizeof(cpuset),&cpuset) == -1)
        {
           printf("warning: cound not get cpu affinity!\n");
           return (void*)-1;
        }

	result = pthread_setaffinity_np(pthread_self(),sizeof(cpu_set_t),&cpuset);
        if (result)
        {
	   printf("[SRIO_TEST]: Bind cpu  failed\n");
           fflush(stdout);
           return (void*)-1;
	}

    }

    if(srio_type!=3){
	    src_phys = receive_data.phys.write_data_prep;			
	    dest_phys = receive_data.port_info.range_start;
    }else{
	    src_phys = receive_data.port_info.range_start;			
	    dest_phys = receive_data.phys.read_recv_data;
    }
    volatile struct srio_ctl *pcnt=NULL;
    struct srio_ctl ctl_info;
    printf("ctl size:%d\n",sizeof(struct srio_ctl));

    memset(&ctl_info,0,sizeof(struct srio_ctl));

    ctl_info.number=buf_number;
    memcpy(receive_data.virt.write_data_prep,&ctl_info,sizeof(struct srio_ctl));
	
    fsl_dma_direct_start(dmadev, src_phys, dest_phys, sizeof(struct srio_ctl));
    err = fsl_dma_wait(dmadev);
    if (err < 0) {
	printf("port %d: dma task error!\n", port + 1);
        fsl_srio_clr_bus_err(sriodev);
	fflush(stdout);
	return NULL;
    }
    pcnt=(struct srio_ctl *)(receive_data.virt.write_recv_data+SEND_NUM_OFFSET);
    uint8_t data=1;
    uint32_t receive_total=0;

    uint32_t pi;
     
    for(pi=0;(pi<passes||(!passes));)
    {
/*ctx add receive packet */
	uint32_t offset=(count%buf_number)*sizeof(struct srio_packet);

	receive_num = pcnt->number;
	if(receive_num>use_num)
	{
		packet_num = receive_num-use_num;
		if(packet_num >(buf_number - (count%buf_number)))
		{
			packet_num= buf_number - (count%buf_number);
		}
		uint8_t *p=(uint8_t *)(receive_data.virt.write_recv_data+offset);
		uint32_t error_count=0;
		fflush(stdout);
		for(k=use_num;k<(use_num+packet_num);k++)
		{
                      uint8_t pdata=*p;
                      error_count=test_data(data,p,PACKET_LENGTH,0);
                      if(error_count!=0)
                      {
			  printf("Receive ERROR Data:%02x  Test Data:%02x error Number:%08x port:%d\n",pdata,data,error_count,port);
		          fflush(stdout);
                          error_count=0;
                      }else{
	                  receive_total=receive_total+1;	
                      }
		      data++;
		}
		if(receive_total==100000)
		{
			printf("port:%d Data Right!\n",port);
			fflush(stdout);
                        receive_total=0;
		}
		pi=pi+(packet_num/SEND_TOTAL_NUM); 
		count=count+packet_num;
		total_buf=total_buf+packet_num;
                ctl_info.number=total_buf;
		memcpy(receive_data.virt.write_data_prep,&ctl_info,sizeof(struct srio_ctl));
/*ctx send*/
		fsl_dma_direct_start(dmadev, src_phys, dest_phys, sizeof(struct srio_ctl));
		err = fsl_dma_wait(dmadev);
		if (err < 0) {
			printf("port %d: dma task error!\n", port + 1);
                        fsl_srio_clr_bus_err(sriodev);
			fflush(stdout);
			break;
		}
		use_num=use_num+packet_num;	
                if((pi==passes)&&(passes))
                     break;
	}
/*end*/
    }
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


/* dma link data input */


int main(int argc, char *argv[])
{
	struct srio_dev *sriodev;
	struct dma_ch *send_dmadev[SEND_THREAD_NUM];
	struct dma_ch *receive_dmadev[RECEIVE_THREAD_NUM];
	struct dma_pool *dmapool = NULL;
	int i, err;
        int maxworkers=2;
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


	for (i = 0; i < port_num; i++) {
                fsl_srio_retrain(sriodev,i);
        }
	
        fsl_srio_uio_finish(sriodev);
	of_finish();

	return 0;
}
