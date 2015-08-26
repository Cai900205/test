
#include "fvl_srio.h"
#include "fvl_tcp.h"
#include "fvl_ssd.h"
#include "fvl_queue.h"
#include <sys/time.h>
#include <semaphore.h>
//ctx add
/******************CRC32***********************/
static uint32_t right_crc_table[256]={
0xed6fe10c,0xcb39ea9f,0x6b3c617d,0x8795fdb9,0x3ab9e7af,0x1cefec3c,0xbcea67de,0x1ecdd3f5,
0x99b2ea0b,0xbfe4e198,0x1fe16a7a,0xf348f6be,0x4e64eca8,0x6832e73b,0xc8376cd9,0xf70c892c,
0x04d5f702,0x2283fc91,0x82867773,0x6e2febb7,0xd303f1a1,0xf555fa32,0x555071d0,0xf777c5fb,
0x7008fc05,0x565ef796,0xf65b7c74,0x1af2e0b0,0xa7defaa6,0x8188f135,0x218d7ad7,0xffff3adf,
0xe56acb51,0xc33cc0c2,0x63394b20,0x8f90d7e4,0x32bccdf2,0x14eac661,0xb4ef4d83,0x16c8f9a8,
0x91b7c056,0xb7e1cbc5,0x17e44027,0xfb4ddce3,0x4661c6f5,0x6037cd66,0xc0324684,0xff09a371,
0x0cd0dd5f,0x2a86d6cc,0x8a835d2e,0x662ac1ea,0xdb06dbfc,0xfd50d06f,0x5d555b8d,0xff72efa6,
0x780dd658,0x5e5bddcb,0xfe5e5629,0x12f7caed,0xafdbd0fb,0x898ddb68,0x2988508a,0xee185d39,
0xfd65b5b6,0xdb33be25,0x7b3635c7,0x979fa903,0x2ab3b315,0x0ce5b886,0xace03364,0x0ec7874f,
0x89b8beb1,0xafeeb522,0x0feb3ec0,0xe342a204,0x5e6eb812,0x7838b381,0xd83d3863,0xe706dd96,
0x14dfa3b8,0x3289a82b,0x928c23c9,0x7e25bf0d,0xc309a51b,0xe55fae88,0x455a256a,0xe77d9141,
0x6002a8bf,0x4654a32c,0xe65128ce,0x0af8b40a,0xb7d4ae1c,0x9182a58f,0x31872e6d,0xeff56e65,
0xf5609feb,0xd3369478,0x73331f9a,0x9f9a835e,0x22b69948,0x04e092db,0xa4e51939,0x06c2ad12,
0x81bd94ec,0xa7eb9f7f,0x07ee149d,0xeb478859,0x566b924f,0x703d99dc,0xd038123e,0xef03f7cb,
0x1cda89e5,0x3a8c8276,0x9a890994,0x76209550,0xcb0c8f46,0xed5a84d5,0x4d5f0f37,0xef78bb1c,
0x680782e2,0x4e518971,0xee540293,0x02fd9e57,0xbfd18441,0x99878fd2,0x39820430,0xcdd692f5,
0xcd7b4878,0xeb2d43eb,0x4b28c809,0xa78154cd,0x1aad4edb,0x3cfb4548,0x9cfeceaa,0x3ed97a81,
0xb9a6437f,0x9ff048ec,0x3ff5c30e,0xd35c5fca,0x6e7045dc,0x48264e4f,0xe823c5ad,0xd7182058,
0x24c15e76,0x029755e5,0xa292de07,0x4e3b42c3,0xf31758d5,0xd5415346,0x7544d8a4,0xd7636c8f,
0x501c5571,0x764a5ee2,0xd64fd500,0x3ae649c4,0x87ca53d2,0xa19c5841,0x0199d3a3,0xdfeb93ab,
0xc57e6225,0xe32869b6,0x432de254,0xaf847e90,0x12a86486,0x34fe6f15,0x94fbe4f7,0x36dc50dc,
0xb1a36922,0x97f562b1,0x37f0e953,0xdb597597,0x66756f81,0x40236412,0xe026eff0,0xdf1d0a05,
0x2cc4742b,0x0a927fb8,0xaa97f45a,0x463e689e,0xfb127288,0xdd44791b,0x7d41f2f9,0xdf6646d2,
0x58197f2c,0x7e4f74bf,0xde4aff5d,0x32e36399,0x8fcf798f,0xa999721c,0x099cf9fe,0xce0cf44d,
0xdd711cc2,0xfb271751,0x5b229cb3,0xb78b0077,0x0aa71a61,0x2cf111f2,0x8cf49a10,0x2ed32e3b,
0xa9ac17c5,0x8ffa1c56,0x2fff97b4,0xc3560b70,0x7e7a1166,0x582c1af5,0xf8299117,0xc71274e2,
0x34cb0acc,0x129d015f,0xb2988abd,0x5e311679,0xe31d0c6f,0xc54b07fc,0x654e8c1e,0xc7693835,
0x401601cb,0x66400a58,0xc64581ba,0x2aec1d7e,0x97c00768,0xb1960cfb,0x11938719,0xcfe1c711,
0xd574369f,0xf3223d0c,0x5327b6ee,0xbf8e2a2a,0x02a2303c,0x24f43baf,0x84f1b04d,0x26d60466,
0xa1a93d98,0x87ff360b,0x27fabde9,0xcb53212d,0x767f3b3b,0x502930a8,0xf02cbb4a,0xcf175ebf,
0x3cce2091,0x1a982b02,0xba9da0e0,0x56343c24,0xeb182632,0xcd4e2da1,0x6d4ba643,0xcf6c1268,
0x48132b96,0x6e452005,0xce40abe7,0x22e93723,0x9fc52d35,0xb99326a6,0x1996ad44,0xedc23b81
};
static unsigned int crc_table[256];

static void init_crc_table(void);
static unsigned int crc32(unsigned int crc, unsigned char * buffer, unsigned int size);
static void init_crc_table(void)
{
	unsigned int c;
	unsigned int i, j;
	
	for (i = 0; i < 256; i++) {
		c = (unsigned int)i;
		for (j = 0; j < 8; j++) {
			if (c & 1)
				c = 0xedb88320L ^ (c >> 1);
			else
				c = c >> 1;
		}
		crc_table[i] = c;
	}
}

static unsigned int crc32(unsigned int crc,unsigned char *buffer, unsigned int size)
{
	unsigned int i;
	for (i = 0; i < size; i++) {
		crc = crc_table[(crc ^ buffer[i]) & 0xff] ^ (crc >> 8);
	}
	return crc ;
}

/********************CRC32*********************/
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

/*uint8_t  test_data(void  *buf,uint32_t right_num)
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
}*/



static void fvl_ssd_write_t(void *arg)
{
    fvl_ssd_arg_t *priv=arg;
    fvl_queue_t *fqueue=priv->fqueue;
    int rvl;
    int fd ;
    uint64_t  index = priv->index;
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
/*    sprintf(path1,"/mnt/test%d",index);
    fd=fvl_ssd_open(path1);*/
    int dequeue_num=-1;
    /* *buf=NULL;*/
    uint32_t test_data=0x00000021+index*0x01000000;
    uint64_t test_times=0;
    uint64_t error_count=0;
    uint32_t crc=0xffffffff;
    while(1)
    {
        dequeue_num=fvl_dequeue(fqueue,4);
        if(dequeue_num != -1)
        {
            void *buf = (void *)(fqueue->buf+dequeue_num*FVL_SRIO_DMA_BLKBYTES);
            /*uint32_t *buf = (uint32_t *)(fqueue->buf+dequeue_num*FVL_SRIO_DMA_BLKBYTES);*/
            /*fvl_ssd_write(fd,buf,4*FVL_SRIO_DMA_BLKBYTES);*/
	   /* for(times=0;times < 0x100000 ;times++)
	    {
		if(*buf != test_data)
		{
			error_count++;
			printf("data:%08x   test_data:%08x\n",*buf,test_data);
		}
		buf++;
		if(*buf != test_data)
		{
			error_count++;
			printf("data:%08x   test_data:%08x\n",*buf,test_data);
		}
		buf++;
		test_data++;
	    }*/
            crc=crc32(crc,buf, 0x800000);/*CRC*/
            fvl_dequeue_complete(fqueue,4);
            count++;
            if(count == 16)
            {
                /*fvl_ssd_close(fd);
                sprintf(path1,"/mnt/test%d",index);
                fd=fvl_ssd_open(path1);*/
		/*if(error_count!=0)
		test_data=test_data+0x03000000;*/
                if(crc != right_crc_table[(4*test_times+index)%256])/*CRC*/
                {
			error_count++;
                        printf("error_count:%lld\n",error_count);
		}
		else
		{
			crc=0xffffffff;
		}
		test_times++;
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
        test_info.RST=0;
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
        test_info.BIS=0;
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
    init_crc_table();/*CRC*/
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

