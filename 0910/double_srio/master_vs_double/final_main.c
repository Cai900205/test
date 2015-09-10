/*************************************************************************
	> File Name: main.c
	> Author: 
	> Mail: 
	> Created Time: Wed 12 Aug 2015 05:16:49 PM CST
 ************************************************************************/

#include "fvl_common.h"
#include "fvl_srio.h"
#include <sys/time.h>
#define Buf_num 16
#define Buf_size 0x100000
#define Chan_num 4
#define Port_Num 2

#define Total_Buf_Size (Buf_num*Buf_size*Chan_num)

static const char *channame[]={
"srio0-chan0","srio0-chan1","srio0-chan2","srio0-chan3",
"srio1-chan0","srio1-chan1","srio1-chan2","srio1-chan3"};

typedef struct chan_send
{
   fvl_dma_pool_t *port_data;
   int fd;
}chan_send_t;

void thread_channel_send(void *arg)
{
    chan_send_t *param=(chan_send_t *)arg;
    fvl_dma_pool_t *port_data=param->port_data;
    int rvl=0;
    uint8_t i=0;
    int j=param->fd;
    int fd= j;
    struct timeval tm_start,tm_end;
    int cpu=j+14;
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(cpu,&cpuset);
    rvl = pthread_setaffinity_np(pthread_self(),sizeof(cpu_set_t),&cpuset);
    if(rvl){
	    printf("(%d)fail:pthread_setaffinity_np()\n",cpu);
	    return;
    }
    sleep(10);
    uint64_t total_count=0;
    gettimeofday(&tm_start,NULL);
    i=0;
    while(1)
    {
	memset(port_data->dma_virt_base,i,Buf_size);
        rvl=fvl_srio_write(fd,port_data->dma_phys_base,Buf_size);
        if(rvl!=0)
        {
            continue;
        }
        gettimeofday(&tm_end,NULL);
        i++;
        total_count++;        
        double diff=(tm_end.tv_sec-tm_start.tv_sec)+(tm_end.tv_usec-tm_start.tv_usec)/1000000.0;
        if(diff>5)
        {
            double da_lu=total_count*Buf_size/1048576/diff;
            printf("fd: %d length(byte): %-15u time(s): %-15f  avg MB/s: %-15f total_count:%lld \n",fd,Buf_size,diff,da_lu,total_count);
            fflush(stdout);
            total_count=0;
            gettimeofday(&tm_start,NULL);
        }
    }
}

int main(int argc, char **argv)
{
    fvl_srio_init_param_t srio_init[Port_Num];
    fvl_dma_pool_t *port_dma_wr[Port_Num];
    int rvl=0,j=0;
    uint8_t i=0;
    int k=1;
    int start=1;
    start = atoi(argv[1]);
    k = atoi(argv[2]);
    for(i=0;i<2;i++)
    {
        srio_init[i].buf_num=Buf_num;
        srio_init[i].buf_size=Buf_size;
        srio_init[i].chan_num=Chan_num;
        srio_init[i].chan_size=Buf_num*Buf_size;
        srio_init[i].port_num=i;
        rvl = dma_pool_init(&port_dma_wr[i],Total_Buf_Size,Total_Buf_Size/2);
        if(rvl!=0)
        {
            FVL_LOG("port %d dma_pool_init failed!\n",i+1);
            return -errno;
        }
        srio_init[i].port_rd_buf=port_dma_wr[i];
        rvl=fvl_srio_init(&srio_init[i]);
        if(rvl!=0)
        {
            FVL_LOG("srio init error!\n");
            return -errno;
        }
    }
    int fd[8];    
    pthread_t fd_id[8];
    chan_send_t p_send[8];
    
    for(j=start;j<k+start;j++)
    {
        rvl = dma_pool_init(&p_send[j].port_data,Buf_size,Buf_size/2);
        if(rvl!=0)
        {
           FVL_LOG("port %d dma_pool_init failed!\n",i+1);
           return -errno;
        }
    }
    sleep(1);
    for(j=start;j<k+start;j++)
    {
        fd[j]=fvl_srio_channel_open(channame[j]);
        printf("###################fd:%d*****************\n",fd[j]);
        if(fd[j]<0)
        {
            return;
        }
        p_send[j].fd=fd[j];
        pthread_create(&fd_id[j],NULL,thread_channel_send,&p_send[j]);
    }

   for(j=start;j<k+start;j++)
   {
       pthread_join(fd_id[j],NULL);
   }

   return 0;
}
