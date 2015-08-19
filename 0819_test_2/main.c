/*************************************************************************
	> File Name: main.c
	> Author: 
	> Mail: 
	> Created Time: Wed 12 Aug 2015 05:16:49 PM CST
 ************************************************************************/

#include "fvl_common.h"
#include "fvl_srio.h"
#define Buf_num 16
#define Buf_size 0x100000
#define Chan_num 4
#define Port_Num 2

#define Total_Buf_Size (Buf_num*Buf_size*Chan_num)
static char channame[]="srio0-chan0";
int main(int argc, char **argv)
{
    fvl_srio_init_param_t srio_init[Port_Num];
    fvl_dma_pool_t *port_dma_wr[Port_Num];
    fvl_read_rvl_t rlen;
    int i=0,rvl=0;
    for(i=0;i<Port_Num;i++)
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
//    open one channel
    fd=fvl_srio_channel_open(channame);
    while(1)
    {
        rlen.len=0x400000;
 	rvl=fvl_srio_read(fd,&rlen);
        if(rvl==0)
        {
          printf("##################\n"); 
        }
    }
    
    return 0;
}
