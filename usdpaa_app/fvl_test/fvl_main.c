
#include "fvl_srio.h"
#include "fvl_tcp.h"
#include "fvl_ssd.h"
#include "fvl_queue.h"
#include <sys/time.h>
#include <semaphore.h>
//ctx add
typedef struct fvl_thread_arg {
    fvl_srio_context_t *psrio;
    fvl_queue_t *fqueue[4];
    uint16_t port;
    uint16_t bfnum;
    uint16_t cpu;
    uint16_t tmid;
} fvl_thread_arg_t;
typedef struct fvl_ssd_arg {
    fvl_queue_t *fqueue;
    uint8_t index;
    uint16_t cpu;
}fvl_ssd_arg_t;
//ctx add 5/11
typedef struct fvl_srio_ctl_info {
#ifdef OLD_VERSION
    uint8_t FLA;
    uint8_t SET;
    uint8_t BIS;
    uint8_t REV;
#else
    uint8_t REV:3;
    uint8_t RST:1;
    uint8_t CEN:1;
    uint8_t BIS:1;
    uint8_t SET:1;
    uint8_t FLA:1;
    uint8_t REV_BYTE[3];
#endif
    uint8_t PK_ID;
    uint8_t SUB_BUF;
    uint8_t FCNT;
    uint8_t CH_ID;
    uint64_t BUF_ADDR;
    uint64_t BUF_SIZE;
    uint64_t INFO_ADDR;
    uint64_t REV_INFO[28];
} fvl_srio_ctl_info_t;

sem_t sem_clt[2];
sem_t sem_rtl[2];
void print_ctl(fvl_srio_ctl_info_t tmp)
{
	printf("CTL INFO START:\n");
	printf("FLA:%d\n",tmp.FLA);
	printf("SET:%d\n",tmp.SET);
	printf("BIST:%d\n",tmp.BIS);
	printf("PK_ID:%d\n",tmp.PK_ID);
	printf("SUB_BUF:%d\n",tmp.SUB_BUF);
	printf("FCNT:%d\n",tmp.FCNT);
	printf("CH_ID:%d\n",tmp.CH_ID);
	printf("BUF_ADDR:%lld\n",tmp.BUF_ADDR);
	printf("BUF_SIZE:%lld\n",tmp.BUF_SIZE);
	printf("INFO_ADDR:%lld\n",tmp.INFO_ADDR);	
    printf("CTL INFO END!\n");
}

uint8_t  test_data(void  *buf,uint32_t right_num)
{
    char *p= (char *)buf;
    uint8_t error_num;
    uint8_t error_count;
    printf("data:");
	for(error_num=0;error_num<64;error_num++)
	{
	    printf("%02x ",*p);
	    p++;
	}
	printf("\n");
	uint32_t *q=(uint32_t*)buf;
	if(*q!=0)
	{
	    error_count++;	
	}
	uint32_t *tm=(uint32_t *)(buf+FVL_SRIO_DMA_BLKBYTES-4);
	right_num+=524287;
	if(*tm!=right_num)
	{
	    error_count++;	
	}
    return error_count;
}

static void fvl_ssd_write_t(void *arg)
{
    fvl_ssd_arg_t *priv=arg;
    fvl_queue_t *fqueue=priv->fqueue;
    int rvl;
    int fd ;
    uint32_t  index = priv->index;
    uint8_t count=0;
    char path1[20];
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(priv->cpu,&cpuset);
    rvl = pthread_setaffinity_np(pthread_self(),sizeof(cpu_set_t),&cpuset);
    if(rvl){
	    printf("(%d)fail:pthread_setaffinity_np()\n",priv->cpu);
	    return;
    }
    sprintf(path1,"/mnt/test%d",index);
    fd=fvl_ssd_open(path1);
    int dequeue_num=-1;
    void *buf=NULL;
    while(1)
    {
        dequeue_num=fvl_dequeue(fqueue,4);
        if(dequeue_num != -1)
        {
            buf = fqueue->buf+dequeue_num*FVL_SRIO_DMA_BLKBYTES;
            fvl_ssd_write(fd,buf,4*FVL_SRIO_DMA_BLKBYTES);
            fvl_dequeue_complete(fqueue,4);
            count++;
            if(count == 16)
            {
                fvl_ssd_close(fd);
                index=index+4;
                sprintf(path1,"/mnt/test%d",index);
                fd=fvl_ssd_open(path1);
                count = 0;
            }
        }
    }

}
static void fvl_srio_recver(void *arg)
{
    fvl_tcp_socket_t    tcp;
    fvl_thread_arg_t *priv=arg;
    fvl_srio_portpool_t *ppool;
    fvl_srio_ctrlblk_t *pscb;
    fvl_queue_t *fqueue = NULL;
    in_addr_t tcp_ip;
    int      tcp_port;
    int      rvl;
    volatile fvl_srio_ctl_info_t *pcnt;
    volatile fvl_srio_ctl_info_t *pclt;
    fvl_srio_ctl_info_t packet_info;
    fvl_srio_ctl_info_t test_info;
    uint8_t *buf_virt;
    uint32_t port;
    uint32_t bfnum;
    uint64_t *head_virt;
    uint64_t head_phys;
    uint64_t head_dest;
    uint8_t ctl_count=1;
    uint16_t send_size = sizeof(fvl_srio_ctl_info_t);
    uint8_t i;
    uint16_t tmid;
    uint8_t fqueue_num=0;
    struct timeval tm_start,tm_end;
    
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(priv->cpu,&cpuset);
    rvl = pthread_setaffinity_np(pthread_self(),sizeof(cpu_set_t),&cpuset);
    if(rvl){
	    printf("(%d)fail:pthread_setaffinity_np()\n",priv->cpu);
	    return;
    }
    tcp_ip   = inet_addr("192.168.10.20");
    tcp_port = 5000;
 /*   rvl = fvl_tcp_init(&tcp, tcp_ip, tcp_port);
    if(rvl < 0) {
        FVL_LOG("fvl_tcp_init failed, return %d\n", rvl);
        return;
    }*/
    port  = priv->port;
    if(port >= FVL_SRIO_PORTNUM) {
        FVL_LOG("Invalid port num: %u\n", port);
        return;
    }
    tmid  = priv->tmid;
    bfnum = priv->bfnum;
    ppool = &priv->psrio->portpool[port];
    pscb  = fvl_srio_getcb(port,bfnum+tmid);
    if(pscb == NULL) {
        FVL_LOG("Srio getcb failed\n");
        return;
    }
    head_virt = (uint64_t*)(ppool->pwrite_ctl_data[bfnum]+tmid*send_size);
    head_phys = (uint64_t *)(ppool->write_ctl_data[bfnum]+tmid*send_size);
    head_dest = (uint64_t *)(ppool->port_info.range_start+FVL_SRIO_DMA_WINSIZE+bfnum*FVL_SRIO_CTL_BUFSIZE+tmid*send_size);//important
    pcnt  = (fvl_srio_ctl_info_t *)ppool->pwrite_ctl_result[bfnum];
    buf_virt  = ppool->pwrite_result[bfnum];
    pclt = pcnt+tmid;
    if(tmid  == 0)
    {  
        bzero(&test_info,send_size);
        test_info.FLA=0;
        test_info.SET=0;
        test_info.BIS=0;
#ifndef OLD_VERSION
        test_info.RST=1;
        test_info.CEN=0;
#endif 
        test_info.PK_ID=0;
        test_info.SUB_BUF=0;
        test_info.FCNT=0;
        test_info.CH_ID=bfnum;
        test_info.BUF_ADDR = 0;
        test_info.BUF_SIZE = 1;
        test_info.INFO_ADDR = 1;
        memcpy(head_virt,&test_info,send_size);
        fvl_srio_send(pscb, head_phys, head_dest, send_size); 
        printf("RESET FPGA!\n"); 
        usleep(5000);
        bzero(&packet_info,send_size);
        packet_info.FLA=0;
        packet_info.SET=1;
        packet_info.BIS=0;
#ifndef OLD_VERSION
        packet_info.RST=0;
        packet_info.CEN=0;
#endif
        packet_info.PK_ID=1;
        packet_info.SUB_BUF=0;
        packet_info.FCNT = 0;
        packet_info.CH_ID = bfnum;
        packet_info.BUF_ADDR = FVL_SRIO_SYS_ADDR+bfnum*FVL_SRIO_DMA_BUFSIZE; 
        packet_info.BUF_SIZE = FVL_SRIO_DMA_BUFSIZE;
        packet_info.INFO_ADDR = FVL_SRIO_CTL_ADDR +bfnum*FVL_SRIO_CTL_BUFSIZE;
        memcpy(head_virt,&packet_info,send_size);
        fvl_srio_send(pscb, head_phys, head_dest, send_size);
        bzero(&test_info,send_size);
        test_info.FLA=0;
        test_info.SET=0;
        test_info.BIS=1;
#ifndef OLD_VERSION
        test_info.RST=0;
        test_info.CEN=1;
#endif
        test_info.PK_ID=0;
        test_info.SUB_BUF=0;
        test_info.FCNT=0;
        test_info.CH_ID=bfnum;
        test_info.BUF_ADDR = 0;
        test_info.BUF_SIZE = 1;
        test_info.INFO_ADDR = 1;
        memcpy(head_virt,&test_info,send_size);
        fvl_srio_send(pscb, head_phys, head_dest, send_size);     
        printf("head!\n");
    }
    uint64_t total_count=0;
    uint8_t en_count=0;
    if(ctl_count == 1)
    {
        gettimeofday(&tm_start,NULL);		
    }
    int enqueue_num=-1;
    printf("total:%d\n",tmid);
    while(1) {
        uint32_t offset;
        fvl_srio_ctl_info_t tmp;
        sem_wait(&sem_clt[tmid]);
        //printf("sem_wait!\n");
        while(1)
        {
            if(!(pclt->FLA&0x01)) {
                continue;
            }
            break;
        }
        fqueue=priv->fqueue[fqueue_num];
        do{
             enqueue_num = fvl_enqueue(fqueue);
        }while(enqueue_num == -1);
        if(tmid == 0)
        {
           sem_post(&sem_clt[1]);
        }
        gettimeofday(&tm_end,NULL);
        uint32_t diff = tm_end.tv_sec-tm_start.tv_sec;
        if(diff>5)
        {
            double da_lu=total_count*2/diff;
            printf("port:%d channel:%d tmid:%d length(byte): %-15u time(s): %d  avg MB/s: %-15f total_count:%lld \n",port,bfnum,tmid,FVL_SRIO_DMA_BLKBYTES,diff,da_lu,total_count);
            total_count=0;
            gettimeofday(&tm_start,NULL);	
        }  
       // printf("tmp%d\n",tmid);
    	tmp.FLA=pclt->FLA;
    	tmp.SET=pclt->SET;
        tmp.BIS=pclt->BIS;
#ifndef OLD_VERSION
        tmp.RST=pclt->RST;
        tmp.CEN=pclt->CEN;
#endif
    	tmp.PK_ID=pclt->PK_ID;
    	tmp.SUB_BUF=pclt->SUB_BUF;
    	tmp.FCNT = pclt->FCNT;
    	tmp.CH_ID = pclt->CH_ID;
    	tmp.BUF_ADDR = pclt->BUF_ADDR; 
    	tmp.BUF_SIZE = pclt->BUF_SIZE;
    	tmp.INFO_ADDR = pclt->INFO_ADDR;
        pclt->FLA = 0;
	offset=tmp.SUB_BUF*FVL_SRIO_DMA_BLKBYTES;
       // fvl_tcp_send(&tcp, buf_virt + offset, FVL_SRIO_DMA_BLKBYTES); 
//        memcpy(buf,buf_virt+offset,FVL_SRIO_DMA_BLKBYTES); 
        //fvl_ssd_write(fd,buf_virt+offset,FVL_SRIO_DMA_BLKBYTES);
        void *enqueue_buf=fqueue->buf+enqueue_num*FVL_SRIO_DMA_BLKBYTES;
     //   printf("enqueue_buf%d\n",tmid);
        memcpy(enqueue_buf,buf_virt+offset,FVL_SRIO_DMA_BLKBYTES);
        fvl_enqueue_complete(fqueue,1);
        en_count++;
        if(en_count == 32)
        {
            fqueue_num++;
            if(fqueue_num == 4)
            {
                fqueue_num =0;
            }
            en_count = 0;
        }
        if(tmid == 1)
        {
		sem_wait(&sem_rtl[1]);
	}
   //     printf("memcpy!%d\n",tmid);
        memcpy(head_virt,&tmp,send_size);
	fvl_srio_send(pscb, head_phys, head_dest, send_size);
 //       printf("srio_send!%d\n",tmid);
        if(tmid == 1)
        {
//                printf("sem_post%d\n",tmid);
		sem_post(&sem_clt[0]);
	}
        else
        {
//                printf("sem_post%d\n",tmid);
		sem_post(&sem_rtl[1]);
	}
	if(ctl_count == (FVL_SRIO_CTL_SUBBUF_NUM)/2)
	{
              pclt= pcnt+tmid;
	      ctl_count=1;
	}
	else
	{
	      ctl_count++;
	      pclt=pclt+2;
	}
        total_count++;
    }

    return;
}


int main(int argc, char *argv[])
{
    fvl_srio_context_t *psrio;
    fvl_thread_arg_t receive_task_port1[FVL_SRIO_BUFFER_NUMBER];
    fvl_thread_arg_t receive_task_port2[FVL_SRIO_BUFFER_NUMBER];
    fvl_queue_t fvl_ssd_queue[FVL_SRIO_BUFFER_NUMBER];
    fvl_ssd_arg_t send_task1[FVL_SRIO_BUFFER_NUMBER];
    int rvl;
    int i,j;
    pthread_t port1_id[FVL_SRIO_BUFFER_NUMBER];
    pthread_t port2_id[FVL_SRIO_BUFFER_NUMBER];
    /*ssd add*/

    /* just init srio as SWRITE mode */
    rvl = fvl_srio_init(&psrio, FVL_SRIO_SWRITE);
    if(rvl < 0) {
        FVL_LOG("Srio init failed, return %d\n", rvl);
        return -1;
    }
	
    for(j=0;j<4;j++)
    {
        fvl_queue_init(&fvl_ssd_queue[j]);
    }
    sem_init(&sem_clt[0],0,1);
    sem_init(&sem_clt[1],0,0);
    sem_init(&sem_rtl[1],0,0);
    for(i=0;i<2;i++)
    {
        for(j=0;j<4;j++)
        {
	        receive_task_port1[i].fqueue[j] = &fvl_ssd_queue[j];
        }
        receive_task_port1[i].tmid = i;
	receive_task_port1[i].psrio = psrio;
	receive_task_port1[i].port = 0;
	receive_task_port1[i].bfnum = 0;
	receive_task_port1[i].cpu = i+1;
	rvl = pthread_create(&port1_id[i], NULL,fvl_srio_recver, &receive_task_port1[i]);
	if (rvl) {
		 printf("Port0 : receive thread failed!\n");
		 return -errno;
	    }
    }

    for(j=0;j<4;j++)
    {
        send_task1[j].index = j;
        send_task1[j].cpu=j+1+FVL_SRIO_BUFFER_NUMBER;
        send_task1[j].fqueue=&fvl_ssd_queue[j];
	    rvl = pthread_create(&port2_id[j], NULL,fvl_ssd_write_t, &send_task1[j]);
	    if (rvl) {
		    printf("send task error\n");
		    return -errno;
	    }
    }
    

    for(i=0;i<2;i++)
    {
	    pthread_join(port1_id[i],NULL);
    }
    for(i=0;i<FVL_SRIO_BUFFER_NUMBER;i++)
    {
        pthread_join(port2_id[i],NULL);
    }
    return 0;
}

