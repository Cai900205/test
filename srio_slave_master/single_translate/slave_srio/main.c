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
#define Chan_num 2
#define Port_Num 2

#define Total_Buf_Size (Buf_num*Buf_size*Chan_num)

static char channame[]="srio0-chan0";
int main(int argc, char **argv)
{
    fvl_srio_init_param_t srio_init[Port_Num];
    fvl_dma_pool_t *port_dma_wr[Port_Num];
    fvl_read_rvl_t rlen;
    char data[Buf_size];
    int rvl=0;
    uint8_t i=0;
    for(i=0;i<2;i++)
    {
//        srio_init[i].buf_num=Buf_num;
//        srio_init[i].buf_size=Buf_size;
        srio_init[i].chan_num=Chan_num;
//        srio_init[i].chan_size=Buf_num*Buf_size;
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
    printf("###################fd:%d*****************\n",fd);

    sleep(3);
//    while(1)
    for(i=0;i<32;i++)
    {
//        i++;
	memset(data,i,Buf_size);
        rvl=fvl_srio_write(fd,data,Buf_size);
        if(rvl!=0)
        {
            printf("error!\n");
        }
    }
    
    return 0;
}
