
#include "fvl_srio.h"
#include "fvl_tcp.h"
#include <sys/time.h>

//ctx add
typedef struct fvl_thread_arg {
    fvl_srio_context_t *psrio;
    uint16_t port;
    uint16_t bfnum;
    uint16_t cpu;
    uint8_t POST;
    uint8_t PRE;
    uint8_t BALAN;
    uint8_t ZHE;
} fvl_thread_arg_t;
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

static void fvl_srio_recver(fvl_thread_arg_t *arg)
{
    fvl_tcp_socket_t    tcp;
    fvl_thread_arg_t *priv=arg;
    fvl_srio_portpool_t *ppool;
    fvl_srio_ctrlblk_t *pscb;
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
    struct timeval tm_start,tm_end;
    /*cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(priv->cpu,&cpuset);
    rvl = pthread_setaffinity_np(pthread_self(),sizeof(cpu_set_t),&cpuset);
    if(rvl){
	    printf("(%d)fail:pthread_setaffinity_np()\n",priv->cpu);
	    return;
    }*/
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

    bfnum = priv->bfnum;
    ppool = &priv->psrio->portpool[port];
    pscb  = fvl_srio_getcb(port,bfnum);
    if(pscb == NULL) {
        FVL_LOG("Srio getcb failed\n");
        return;
    }
    head_virt = (uint64_t*)ppool->pwrite_ctl_data[bfnum];
    head_phys = ppool->write_ctl_data[bfnum];
    head_dest = ppool->port_info.range_start+FVL_SRIO_DMA_WINSIZE+bfnum*FVL_SRIO_CTL_BUFSIZE;//important
    pcnt  = (fvl_srio_ctl_info_t *)ppool->pwrite_ctl_result[bfnum];
    buf_virt  = ppool->pwrite_result[bfnum];
    pclt = pcnt;
   /* bzero(&test_info,send_size);
    test_info.CON=1;
    test_info.PRE=priv->PRE;
    test_info.POST=priv->POST;
    test_info.BALAN=priv->BALAN;
    test_info.ZHE = priv->ZHE;
    *memcpy(head_virt,&test_info,send_size);
    fvl_srio_send(pscb, head_phys, head_dest, send_size);*
    printf("SIZE:%d CON:%d POST:%d PRE:%d BALAN:%d ZHE:%d\n",send_size,test_info.CON,test_info.POST,test_info.PRE,test_info.BALAN,test_info.ZHE);
    bzero(&test_info,send_size);
    test_info.RST=1;
    test_info.POST=1;
    memcpy(head_virt,&test_info,send_size);
    fvl_srio_send(pscb, head_phys, head_dest, send_size);
    printf("RST:%d POST:%d PRE:%d\n",test_info.RST,test_info.POST,test_info.PRE);
    
    return; */
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
    printf("RST:%d\n",test_info.RST);
    usleep(5000);
    return;
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
    uint64_t total_count=0;
    if(ctl_count == 1)
    {
        gettimeofday(&tm_start,NULL);		
    }
    while(1) {
        uint32_t offset;
        fvl_srio_ctl_info_t tmp;
        if(!(pclt->FLA&0x01)) {
            continue;
        }
	total_count++;
        gettimeofday(&tm_end,NULL);
        uint32_t diff = tm_end.tv_sec-tm_start.tv_sec;
        if(diff>5)
        {
        	double da_lu=total_count*2/diff;
            printf("port:%d channel:%d length(byte): %-15u time(s): %d  avg MB/s: %-15f total_count:%lld \n",port,bfnum,FVL_SRIO_DMA_BLKBYTES,diff,da_lu,total_count);
            total_count=0;
            gettimeofday(&tm_start,NULL);	
        }  
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
	printf("%02x\n",pclt->FLA);
        char *p=buf_virt+offset;
        for(i=0;i<64;i++)
        {
	    printf(" %02x ",*p);
     	    p++;
            if((i+1)%16 == 0)
            printf("\n");	
	}
        printf("bfnum=%d\n",bfnum);
        memcpy(head_virt,&tmp,send_size);
	fvl_srio_send(pscb, head_phys, head_dest, send_size);
	if(ctl_count == FVL_SRIO_CTL_SUBBUF_NUM)
	{
		    pclt= pcnt;
		    ctl_count=1;
	}
	else
	{
	      ctl_count++;
		    pclt++;
	}
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
    if(argc != 5)
    {
	printf("param error!\n");
        return -1;
    }

    int pre=atoi(argv[1]);
    int post = atoi(argv[2]);
    int balan = atoi(argv[3]);
    int zhe = atoi(argv[4]);
    /* just init srio as SWRITE mode */
    rvl = fvl_srio_init(&psrio, FVL_SRIO_SWRITE);
    if(rvl < 0) {
        FVL_LOG("Srio init failed, return %d\n", rvl);
        return -1;
    }
	
   /* for(i=0;i<FVL_SRIO_BUFFER_NUMBER;i++)*/
    for(i=0;i<1;i++)
    {
	receive_task_port1[i].psrio = psrio;
	receive_task_port1[i].port = 0;
	receive_task_port1[i].bfnum = i;
	receive_task_port1[i].cpu = i+1;
	receive_task_port1[i].PRE = pre;
	receive_task_port1[i].POST = post;
	receive_task_port1[i].BALAN = balan;
	receive_task_port1[i].ZHE = zhe;
        
        fvl_srio_recver(&receive_task_port1[i]);
/*	rvl = pthread_create(&port1_id[i], NULL,fvl_srio_recver, &receive_task_port1[i]);
	if (rvl) {
		printf("Port0 : receive thread failed!\n");
		return -errno;
	} */
/*
	receive_task_port2[i].psrio = psrio;
	receive_task_port2[i].port = 1;
	receive_task_port2[i].bfnum = i;
	receive_task_port2[i].cpu = i+1+FVL_SRIO_BUFFER_NUMBER;
	rvl = pthread_create(&port2_id[i], NULL,fvl_srio_recver, &receive_task_port2[i]);
	if (rvl) {
		printf("Port1: receive thread failed!\n");
		return -errno;
	} */
    }
    
/*    for(i=0;i<1;i++)
    {
	    pthread_join(port1_id[i],NULL);
	    pthread_join(port2_id[i],NULL);
    }*/
    return 0;
}

