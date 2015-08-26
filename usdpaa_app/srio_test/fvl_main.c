
#include "fvl_test_srio.h"

//ctx add
typedef struct fvl_thread_arg {
    fvl_srio_context_t *psrio;
    uint16_t port;
    uint16_t bfnum;
    uint16_t cpu;
} fvl_thread_arg_t;
//ctx add 5/11
typedef struct fvl_srio_ctl_info {
    uint8_t FLA;
    uint8_t SET;
    uint8_t BIS;
    uint8_t REV;//
    uint8_t PK_ID;
    uint8_t SUB_BUF;
    uint8_t FCNT;
    uint8_t CH_ID;
    uint64_t BUF_ADDR;
    uint64_t BUF_SIZE;
    uint64_t INFO_ADDR;
} fvl_srio_ctl_info_t;

static inline uint32_t fvl_get_offset(uint64_t count)
{
    uint64_t num;
    uint64_t id;

    num = count;
    do {
        id  = num >> 16;
        id += num & 0x3fff;
        num = id;
    } while(id > 0x3fff);

    return (uint32_t)id;
}

static void fvl_srio_pattern_init(void *buf, uint32_t base, uint32_t step, uint32_t num)
{
    uint32_t i;
    uint32_t *p32 = buf;

    for(i = 0; i < num; i++) {
        p32[i] = base;
        base  += step;
    }

    return;
}
static void fvl_srio_send_t(void *arg)
{
    fvl_thread_arg_t *priv=arg;
    fvl_srio_portpool_t *ppool;
    fvl_srio_ctrlblk_t *pscb;
    volatile fvl_srio_ctl_info_t *pcnt;
    fvl_srio_ctl_info_t *pclt;
    fvl_srio_ctl_info_t ctl_msg;
    uint8_t *buf_virt;
    uint32_t port;
    uint32_t bfnum;
    uint64_t *data_virt;
    uint64_t data_phys;
    uint64_t data_dest;
    uint64_t *ctl_virt;
    uint64_t ctl_phys;
    uint64_t ctl_dest;
    int rvl;
    uint8_t i,j;
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(priv->cpu,&cpuset);
    rvl = pthread_setaffinity_np(pthread_self(),sizeof(cpu_set_t),&cpuset);
    if(rvl){
	printf("(%d)fail:pthread_setaffinity_np()\n",priv->cpu);
	return;
    }
    port  = priv->port;
    if(port >= FVL_SRIO_PORTNUM) {
        FVL_LOG("Invalid port num: %u\n", port);
        return;
    }

    bfnum = priv->bfnum;
    ppool = &priv->psrio->portpool[port];

    pscb  = fvl_srio_getcb(port,bfnum);
    if(pscb == NULL) {
        FVL_LOG("Srio getcb failed\n");
        return;
    }
    
    data_virt = (uint64_t*)ppool->pwrite_data[bfnum];//weishenqing mem
    data_phys = ppool->write_data[bfnum];
    uint16_t send_size = sizeof(fvl_srio_ctl_info_t);
    ctl_virt = (uint64_t*)ppool->pwrite_ctl_data[bfnum];
    ctl_phys = ppool->write_ctl_data[bfnum];

    pcnt  = (fvl_srio_ctl_info_t *)ppool->pwrite_ctl_result[bfnum];
    pclt = (volatile fvl_srio_ctl_info_t *)pcnt;
/*    while(1) {
        if(!(pclt->FLA&0x01)) {
            usleep(10000);
            continue;
        }
        break;
    }
*/
    uint32_t value = 0;
    for(i=0;i<8;i++)
    {
        j=i+8;
   	    data_dest = ppool->port_info.range_start+bfnum*FVL_SRIO_DMA_BUFSIZE+i*FVL_SRIO_DMA_BLKBYTES;//important 
    	//memset(data_virt,j,FVL_SRIO_DMA_BUFSIZE);
        fvl_srio_pattern_init(data_virt,value,1,FVL_SRIO_DMA_BLKBYTES/4);
        value=value+(FVL_SRIO_DMA_BLKBYTES/4);
    	fvl_srio_send(pscb, data_phys, data_dest, FVL_SRIO_DMA_BLKBYTES);
    	ctl_dest = ppool->port_info.range_start+FVL_SRIO_DMA_WINSIZE+bfnum*FVL_SRIO_CTL_BUFSIZE+i*send_size;//important
    	ctl_msg.FLA = 1;
    	ctl_msg.CH_ID=bfnum;
    	ctl_msg.SUB_BUF = i;
    	ctl_msg.PK_ID = 1;
    	ctl_msg.BUF_ADDR = 0x10000000;
    	memcpy(ctl_virt,&ctl_msg,send_size);
    	fvl_srio_send(pscb, ctl_phys, ctl_dest,send_size);
   }
    return;
}

static void fvl_srio_recver(void *arg)
{
  //  fvl_tcp_socket_t    tcp;
    fvl_thread_arg_t *priv=arg;
    fvl_srio_portpool_t *ppool;
    fvl_srio_ctrlblk_t *pscb;
    //in_addr_t tcp_ip;
    //int      tcp_port;
    int      rvl;
    volatile fvl_srio_ctl_info_t *pcnt;
    fvl_srio_ctl_info_t *pclt;
    fvl_srio_ctl_info_t packet_info;
    volatile uint8_t *buf_virt;
    uint32_t port;
    uint32_t bfnum;
    uint64_t *head_virt;
    uint64_t head_phys;
    uint64_t head_dest;
    uint8_t ctl_count=1;
    uint8_t i;
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(priv->cpu,&cpuset);
    rvl = pthread_setaffinity_np(pthread_self(),sizeof(cpu_set_t),&cpuset);
    if(rvl){
	printf("(%d)fail:pthread_setaffinity_np()\n",priv->cpu);
	return;
    }
    
    port  = priv->port;
    if(port >= FVL_SRIO_PORTNUM) {
        FVL_LOG("Invalid port num: %u\n", port);
        return;
    }

    bfnum = priv->bfnum;
    ppool = &priv->psrio->portpool[port];

    pscb  = fvl_srio_getcb(port,bfnum);
    if(pscb == NULL) {
        FVL_LOG("Srio getcb failed\n");
        return;
    }
    head_virt = (uint64_t*)ppool->pwrite_ctl_data[bfnum];
    head_phys = ppool->write_ctl_data[bfnum];
    head_dest = ppool->port_info.range_start+FVL_SRIO_DMA_WINSIZE+bfnum*FVL_SRIO_CTL_BUFSIZE;
    
    pcnt  = (fvl_srio_ctl_info_t *)ppool->pwrite_ctl_result[bfnum];
    buf_virt  = ppool->pwrite_result[bfnum];
    uint16_t send_size = sizeof(fvl_srio_ctl_info_t);
//    printf("send_size;%d\n",send_size);
    pclt = (volatile fvl_srio_ctl_info_t *)pcnt;
    int count=0;
  //  printf("pclt:%llx\n",pclt);
    while(1) {
        uint32_t offset;
        fvl_srio_ctl_info_t tmp;
        if(!(pclt->FLA&0x01)) {
            usleep(10000);
            continue;
        }
	printf("CPU:%d\n",priv->cpu);
	printf("ctl.channel_id:%d\n",pclt->CH_ID);
	printf("ctl.sub_buffer:%d\n",pclt->SUB_BUF);
	printf("data:");
	offset=pclt->SUB_BUF*FVL_SRIO_DMA_BLKBYTES;
	char *p = buf_virt + offset;
	for(i=0;i<10;i++)
	{
	   printf("%02x ",*p);
	   p++;
	}
	printf("\n");
        count++;
        if(count == 8)
        {
		break;
	}
	pclt++;
    }

    return;
}


int main(int argc, char *argv[])
{
    fvl_srio_context_t *psrio;
    fvl_thread_arg_t receive_task_port1[FVL_SRIO_BUFFER_NUMBER];
    fvl_thread_arg_t receive_task_port2[FVL_SRIO_BUFFER_NUMBER];
    int rvl;
    int i;
    pthread_t port1_id[FVL_SRIO_BUFFER_NUMBER];
    pthread_t port2_id[FVL_SRIO_BUFFER_NUMBER];

    /* just init srio as SWRITE mode */
    rvl = fvl_srio_init(&psrio, FVL_SRIO_SWRITE);
    if(rvl < 0) {
        FVL_LOG("Srio init failed, return %d\n", rvl);
        return -1;
    }
	
    for(i=0;i<1;i++)
    {
	receive_task_port1[i].psrio = psrio;
	receive_task_port1[i].port = 0;
	receive_task_port1[i].bfnum = i;
	receive_task_port1[i].cpu = i+1;
	rvl = pthread_create(&port1_id[i], NULL,fvl_srio_send_t, &receive_task_port1[i]);
	if (rvl) {
		printf("Port0 : receive thread failed!\n");
		return -errno;
	} 

	receive_task_port2[i].psrio = psrio;
	receive_task_port2[i].port = 1;
	receive_task_port2[i].bfnum = i;
	receive_task_port2[i].cpu = i+1+FVL_SRIO_BUFFER_NUMBER;
	rvl = pthread_create(&port2_id[i], NULL,fvl_srio_recver, &receive_task_port2[i]);
	if (rvl) {
		printf("Port1: receive thread failed!\n");
		return -errno;
	} 
    }

    for(i=0;i<1;i++)
    {
	pthread_join(port1_id[i],NULL);
	pthread_join(port2_id[i],NULL);
    }
    return 0;
}

