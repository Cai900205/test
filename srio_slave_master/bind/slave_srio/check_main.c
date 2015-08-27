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
static char channame[]="srio1-chan3";

uint32_t test_data(uint8_t src, uint8_t *buf,uint64_t count ,uint8_t step)
{
    uint64_t i=0;
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

int main(int argc, char **argv)
{
    fvl_srio_init_param_t srio_init[Port_Num];
    fvl_dma_pool_t *port_dma_wr[Port_Num];
    fvl_read_rvl_t rlen;
    int rvl=0;
    uint8_t i=0;
    struct timeval tm_start,tm_end;
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
    int fd=0;
    fd=fvl_srio_channel_open(channame);

    printf("##################fd:%d*****************\n",fd);
    int  num=0;
    uint64_t total_count=0;
    gettimeofday(&tm_start,NULL);
    i=0;
    while(1)
    {
        rlen.len=0x100000;
 	rvl=fvl_srio_read(fd,&rlen);
        if(rlen.len!=0)
        {
            test_data(i,rlen.buf_virt,10000,0);
            i++;    
            gettimeofday(&tm_end,NULL);
            total_count++;        
            double diff=(tm_end.tv_sec-tm_start.tv_sec)+(tm_end.tv_usec-tm_start.tv_usec)/1000000.0;
            if(diff>5)
            {
                double da_lu=total_count/diff;
                printf("length(byte): %-15u time(s): %-15f  avg MB/s: %-15f total_count:%lld \n",rlen.len,diff,da_lu,total_count);
                fflush(stdout);
                total_count=0;
                gettimeofday(&tm_start,NULL);
            }       
        }
        fvl_srio_read_feedback(fd,rlen.num);
    }
    
    return 0;
}
