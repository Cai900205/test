#include "fvl_srio.h"
#include "fvl_ssd.h"
#include "fvl_queue.h"
#include <sys/time.h>
#include <semaphore.h>
#define FVL_SRIO_PACKET_LENGTH 0x100000
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
#if 0
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
#endif
    uint32_t count;
    uint32_t count1;
    uint64_t REV_INFO[31];
} fvl_srio_ctl_info_t;


/*
static void *t_srio_send(void *arg)
{
    fvl_thread_arg_t *priv=arg;
    fvl_srio_portpool_t *ppool;
    fvl_srio_ctrlblk_t *pscb;
    int      rvl;
    volatile fvl_srio_ctl_info_t *pcnt;
    volatile fvl_srio_ctl_info_t *pclt;
    fvl_srio_ctl_info_t packet_info;//ctl word
    uint8_t *buf_virt;
    uint8_t buf_phys;
    uint8_t buf_dest;
    uint32_t port;
    uint32_t bfnum;
    uint64_t *head_virt;
    uint64_t head_phys;
    uint64_t head_dest;
    uint16_t send_size = sizeof(fvl_srio_ctl_info_t);
    uint8_t i;
    uint16_t tmid;
    
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
    tmid  = priv->tmid;
    bfnum = priv->bfnum;
    ppool = &priv->psrio->portpool[port];
    pscb  = fvl_srio_getcb(port,bfnum+tmid);
    if(pscb == NULL) {
        FVL_LOG("Srio getcb failed\n");
        return;
    }
    head_virt = (uint64_t*)(ppool->pwrite_ctl_data[bfnum]+tmid*send_size);
    head_phys = (uint64_t*)(ppool->write_ctl_data[bfnum]+tmid*send_size);
    head_dest = (uint64_t*)(ppool->port_info.range_start+FVL_SRIO_DMA_WINSIZE+bfnum*FVL_SRIO_CTL_BUFSIZE+tmid*send_size);//important
    pcnt  = (fvl_srio_ctl_info_t *)ppool->pwrite_ctl_result[bfnum];
    printf("se_port:%d\n",priv->port); 
    buf_virt = (uint8_t *)ppool->pwrite_data[bfnum];//send data
    buf_phys = (uint8_t *)ppool->write_data[bfnum];
    buf_dest = (uint8_t *)(ppool->port_info.range_start+bfnum*FVL_SRIO_DMA_BUFSIZE);//important
    pclt = pcnt+tmid;
    printf("cpu:#############%d\n",priv->cpu); 
    uint32_t buf_number=0,usebuf_number=0,total_buf=1024;
    uint32_t data=1;
    uint64_t src_phys,dest_phys;
    uint32_t send_num=0;
    uint32_t total_count=0;
    struct timeval tm_start,tm_end;
    gettimeofday(&tm_start,NULL);
    while (1) {
	   buf_number = pclt->count;
	   for(;usebuf_number<buf_number;)
           {
	       uint32_t offset= (usebuf_number%total_buf)*(0x10000);
	       src_phys =  buf_phys+offset;			
	       dest_phys = buf_dest+offset;
	       memset((buf_virt+offset),data,0x10000);
               fvl_srio_send(pscb, src_phys, dest_phys, 0x10000);
               data++;
               gettimeofday(&tm_end,NULL);
               float diff = (tm_end.tv_sec-tm_start.tv_sec)+(tm_end.tv_usec-tm_start.tv_usec)/1000000.0;
               total_count++;
               if(diff>5)
               {
                  double da_lu=total_count/16/diff;
                  printf("port:%d channel:%d tmid:%d length(byte): %-15u time(s): %-15f  avg MB/s: %-15f total_count:%lld \n",port,bfnum,tmid,FVL_SRIO_PACKET_LENGTH,diff,da_lu,total_count);
                  total_count=0;
                  gettimeofday(&tm_start,NULL);	
               }  
	       usebuf_number++;
	       send_num++;
               if(send_num==128)
               {
              		 packet_info.count=usebuf_number;
               		 memcpy(head_virt,&packet_info,send_size);
               		 fvl_srio_send(pscb, head_phys, head_dest, send_size);
                         send_num=0;	
               }
	     }
    }
err_dma: printf("Send error!\n");
	 pthread_exit(NULL);
}
*/
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
    fvl_thread_arg_t *priv=arg;
    fvl_srio_portpool_t *ppool;
    fvl_srio_ctrlblk_t *pscb;
    int      rvl;
    volatile fvl_srio_ctl_info_t *pcnt;
    volatile fvl_srio_ctl_info_t *pclt;
    fvl_srio_ctl_info_t packet_info;//ctl word
    uint8_t *buf_virt;
    uint32_t port;
    uint32_t bfnum;
    uint64_t *head_virt;
    uint64_t head_phys;
    uint64_t head_dest;
    uint16_t send_size = sizeof(fvl_srio_ctl_info_t);
    uint8_t i;
    uint16_t tmid;
    
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
    printf("re_port:%d\n",port);
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
    buf_virt  = ppool->pwrite_result[bfnum];//shou shuju receive data
    pclt = pcnt+tmid;

    packet_info.count = 64;
    memcpy(head_virt,&packet_info,send_size);
    fvl_srio_send(pscb, head_phys, head_dest, send_size);
    printf("send packet_info!:%d\n",send_size);
    uint32_t receive_num=0,use_num=0;
    uint32_t packet_num=0;
    uint32_t buf_number=64;
    uint32_t total_count=0;
    uint32_t count=0;
    struct timeval tm_start,tm_end;
    gettimeofday(&tm_start,NULL);
    uint8_t data=1;
    uint32_t receive_total=0;
    uint32_t k=0;
    while (1) 
    {
/*ctx add receive packet */
      uint32_t offset=(count%buf_number)*(FVL_SRIO_PACKET_LENGTH);
      receive_num = pclt->count;
      if(receive_num>use_num)
      {
	  packet_num = receive_num-use_num;
	  //printf("receive number:%d\n",receive_num);
          if((packet_num+(count%buf_number))>buf_number)
          {
            packet_num = buf_number-(count%buf_number);
          }	
	  uint8_t *p=(uint8_t *)(buf_virt+offset);
	  uint32_t error_count=0;
	  fflush(stdout);
	  for(k=use_num;k<(use_num+packet_num);k++)
	  {
                 uint8_t pdata=*p;
                 error_count=test_data(data,p,FVL_SRIO_PACKET_LENGTH,0);
                 if(error_count!=0)
                 {
			  printf("Receive ERROR Data:%02x  Test Data:%02x error Number:%08x port:%d\n",pdata,data,error_count,port);
		          fflush(stdout);
                          error_count=0;
                 }else
		 {
	                  receive_total=receive_total+1;	
                 }
		 data++;
	  }
	  if(receive_total==10000)
	  {
		printf("port:%d Data Right!\n",port);
	        fflush(stdout);
                receive_total=0;
	  }
	  gettimeofday(&tm_end,NULL);
          total_count=total_count+packet_num;
    //      printf("total_count:%d\n",total_count);		
          float  diff = tm_end.tv_sec-tm_start.tv_sec+(tm_end.tv_usec-tm_start.tv_usec)/1000000.0;
          if(diff>5)
          {
              double da_lu=total_count/diff;
              printf("port:%d channel:%d tmid:%d length(byte): %-15u time(s): %-15f  avg MB/s: %-15f total_count:%lld \n",port,bfnum,tmid,FVL_SRIO_PACKET_LENGTH,diff,da_lu,total_count);
              total_count=0;
              gettimeofday(&tm_start,NULL);	
          }  
	  count=count+packet_num;
          packet_info.count=packet_info.count+packet_num;
          memcpy(head_virt,&packet_info,send_size);
          fvl_srio_send(pscb, head_phys, head_dest, send_size);	
          use_num=use_num+packet_num;
	}
    }
    printf("Receive error!\n");
    pthread_exit(NULL);
}




int main(int argc, char *argv[])
{
    fvl_srio_context_t *psrio;
    fvl_thread_arg_t receive_task_port1[FVL_SRIO_BUFFER_NUMBER];
    fvl_thread_arg_t receive_task_port2[FVL_SRIO_BUFFER_NUMBER];
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

    for(i=0;i<FVL_SRIO_BUFFER_NUMBER;i++)
    {
            receive_task_port1[i].tmid = 0;
            receive_task_port1[i].psrio = psrio;
	    receive_task_port1[i].port = 0;
	    receive_task_port1[i].bfnum = i;
	    receive_task_port1[i].cpu = i+1;
	    rvl = pthread_create(&port1_id[i], NULL,t_srio_receive, &receive_task_port1[i]);
	    if (rvl) {
		 printf("Port0 : receive thread failed!\n");
		 return -errno;
	    }
        receive_task_port2[i].tmid = 0;
	    receive_task_port2[i].psrio = psrio;
	    receive_task_port2[i].port = 1;
	    receive_task_port2[i].bfnum = i;
	    receive_task_port2[i].cpu = i+1+FVL_SRIO_BUFFER_NUMBER;
	    rvl = pthread_create(&port2_id[i], NULL,t_srio_receive, &receive_task_port2[i]);
	    if (rvl) {
		 printf("Port1 : receive thread failed!\n");
		 return -errno;
	    }
    }

    for(i=0;i<FVL_SRIO_BUFFER_NUMBER;i++)
    {
        pthread_join(port1_id[i],NULL);
        pthread_join(port2_id[i],NULL);
    }
    return 0;
}


