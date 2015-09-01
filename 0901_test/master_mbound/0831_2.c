
#include "fvl_srio.h"

#define HEAD_SIZE sizeof(fvl_srio_head_info_t)
fvl_srio_context_t g_srio_context;

fvl_srio_channel_t srio_channel_context[FVL_SRIO_CBMAX];

fvl_srio_head_info_t head_port[FVL_SRIO_PORT_NUM];

fvl_srio_head_info_t head_channel[FVL_SRIO_PORT_NUM*FVL_CHAN_NUM_MAX];

static uint64_t receive_num[FVL_SRIO_CBMAX]={0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
static uint64_t send_num[FVL_SRIO_CBMAX]={0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
static uint64_t rese_num[FVL_SRIO_CBMAX]={0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
static uint32_t ctl_count[FVL_SRIO_CBMAX]={0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
static uint64_t rfreeback_num[FVL_SRIO_CBMAX]={0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
static uint64_t read_num[FVL_SRIO_CBMAX]={0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};

static fvl_srio_ctable_t srio_ctable_context[FVL_CHAN_NUM_MAX*2]={
{"srio0-chan0",0,0,0},{"srio0-chan1",0,1,0},{"srio0-chan2",0,2,0},{"srio0-chan3",0,3,0},
{"srio1-chan0",1,0,0},{"srio1-chan1",1,1,0},{"srio1-chan2",1,2,0},{"srio1-chan3",1,3,0}
};

static fvl_ctl_thread_t  ctl_re_arg[FVL_SRIO_CBMAX];
static const uint32_t source_id[2]={0x11,0x14};
static const uint32_t target_id[2]={0x11,0x14};

static const uint32_t srio_test_win_attrv[] = {3, 4, 5, 4, 0};
static int init_flag=0;

static uint32_t srio_type[FVL_SRIO_PORT_NUM]={0,0};

int fvl_srio_init(fvl_srio_init_param_t *srio_param)
{
    fvl_srio_context_t  *psrio = &g_srio_context;
    fvl_srio_portpool_t *ppool;
    fvl_srio_ctrlblk_t *pscb,*cscb,*re_cscb;
    fvl_head_thread_t head_arg;
    uint32_t chan_num=srio_param->chan_num;
    uint32_t buf_num=srio_param->buf_num;
    uint32_t buf_size=srio_param->buf_size;
    uint32_t chan_size=srio_param->chan_size;
    uint32_t ctl_size=FVL_CTL_WIN_SIZE;
    uint32_t port_num=srio_param->port_num;
    fvl_dma_pool_t *port_dma_wr = srio_param->port_rd_buf;
    fvl_dma_pool_t *port_dma_ctl_wp;
    fvl_dma_pool_t *port_dma_ctl_wr;

    uint32_t win_size=chan_size*chan_num;
    uint32_t ctl_win_size=0x1000;//ctl_size*chan_num;
    uint32_t win_law=0,ctl_law=0,chan_law=0;
    struct srio_dev *sriodev;
    int rvl;

    if((port_num >= FVL_SRIO_PORT_NUM) || (port_num <0))
    {
        FVL_LOG("port number error:%d\n",port_num);
        return -1;
    }
    if((chan_num > FVL_CHAN_NUM_MAX) || ( chan_num<0))
    {
        FVL_LOG("channel number error:%d\n",chan_num);
        return -1;
    }
    
    if(!init_flag)    
    {
        of_init();
        rvl = fsl_srio_uio_init(&sriodev);
        if (rvl < 0) 
        {
            FVL_LOG("%s(): fsl_srio_uio_init return %d\n", __func__, rvl);
            return rvl;
        }
        psrio->sriodev=sriodev;
        init_flag=1;
    }
    else
    {
        sriodev=psrio->sriodev;
    }

//mode master
    head_port[port_num].buf_size=buf_size;
    head_port[port_num].buf_num=buf_num;
    head_port[port_num].chan_size=buf_size*buf_num;
    head_port[port_num].data_se_addr=FVL_SRIO_SYS_ADDR;
    head_port[port_num].data_re_addr=FVL_SRIO_SYS_ADDR;
    head_port[port_num].ctl_se_addr=FVL_SRIO_CTL_ADDR;
    head_port[port_num].ctl_re_addr=FVL_SRIO_CTL_ADDR;
    head_port[port_num].chan_num=0;
    head_port[port_num].re_flag=1;
    head_port[port_num].uflag=0;

    int i=0;
    int j=FVL_CHAN_NUM_MAX*port_num;
    for(i=0;i<chan_num;i++)
    {
        head_channel[j+i].data_se_addr=FVL_SRIO_SYS_ADDR+chan_size*i;
        head_channel[j+i].data_re_addr=FVL_SRIO_SYS_ADDR+chan_size*i;
        head_channel[j+i].ctl_re_addr=FVL_SRIO_CTL_ADDR+HEAD_SIZE+ctl_size*i;
        head_channel[j+i].ctl_se_addr=FVL_SRIO_CTL_ADDR+HEAD_SIZE+ctl_size*i;
        head_channel[j+i].re_flag=1;
        head_channel[j+i].uflag=0;
    }

//end master
    struct srio_port_info *pinfo;
    void *range;
    pinfo = &psrio->portpool[port_num].port_info;
    fsl_srio_connection(sriodev,port_num);
    if (fsl_srio_port_connected(sriodev) & (0x1 << port_num)) 
    {
        fsl_srio_get_port_info(sriodev, port_num+1, pinfo, &range);
        psrio->portpool[port_num].range_virt = range;
        FVL_LOG("Get port %u info, range=%p, range_start=%llx range_size=%llx\n", port_num, range, pinfo->range_start,pinfo->range_size);
    } 
    else 
    {
        FVL_LOG("%s(): fsl_srio_connection port %d failed\n", __func__, port_num+1);
        return -1;
    }
    
    rvl = fsl_srio_port_connected(sriodev);
    if (rvl <= 0) 
    {
        FVL_LOG("%s(): fsl_srio_port_connected return %d\n", __func__, rvl);
        return -1;
    }
    
    rvl = dma_pool_init(&port_dma_ctl_wp,ctl_win_size,ctl_win_size/2);
    if(rvl!=0)
    {
        FVL_LOG("port %d dma_pool_init failed!\n",port_num+1);
        return -errno;
    }
    rvl = dma_pool_init(&port_dma_ctl_wr,ctl_win_size,ctl_win_size/2);
    if(rvl!=0)
    {
        FVL_LOG("port %d dma_pool_init failed!\n",port_num+1);
        return -errno;
    }
 
    uint32_t attr_read, attr_write;
    attr_read = srio_test_win_attrv[3];
    attr_write = srio_test_win_attrv[0];
    FVL_LOG("attr_write = %u, srio_type = 0\n", attr_write);
        
    ppool = &psrio->portpool[port_num];
    ppool->write_result = port_dma_wr->dma_phys_base;
    ppool->pwrite_result = port_dma_wr->dma_virt_base;

    ppool->write_ctl_result = port_dma_ctl_wr->dma_phys_base;
    ppool->write_ctl_data = port_dma_ctl_wp->dma_phys_base;
    ppool->pwrite_ctl_result = port_dma_ctl_wr->dma_virt_base;
    ppool->pwrite_ctl_data = port_dma_ctl_wp->dma_virt_base;
   

    win_law=FVL_BASE_LAW+fvl_get_law((win_size/FVL_BASE_LAW_SIZE)-1);
    ctl_law=FVL_BASE_LAW+fvl_get_law((ctl_win_size/FVL_BASE_LAW_SIZE)-1);
    
    chan_law=FVL_BASE_LAW+fvl_get_law((chan_size/FVL_BASE_LAW_SIZE)-1);
    
    FVL_LOG("HOST CHAN_LAW:%d WIN_LAW:%d CTL_LAW:%d\n",chan_law,win_law,ctl_law);
    
//    for(i=0;i<chan_num;i++)
//    {
        i=0;
        fsl_srio_set_ibwin(sriodev, port_num, i, (ppool->write_result+i*chan_size),
            (FVL_SRIO_SYS_ADDR+i*chan_size), win_law);
//    }

    fsl_srio_set_ibwin(sriodev, port_num, 4, ppool->write_ctl_result,
            FVL_SRIO_CTL_ADDR, ctl_law);


    uint32_t win_offset=FVL_BASE_LAW_SIZE;
    for(i=FVL_BASE_LAW;i<win_law;i++)
    {
        win_offset=win_offset*2;
    }
    
    FVL_LOG("HOST WIN_OFFSET:%d,chan_num:%d\n",win_offset,chan_num);
    ppool->ctl_info_start=ppool->port_info.range_start+win_offset;

    if (fsl_srio_port_connected(sriodev) & (0x1 << port_num)) 
    {
//data
        for(i=0;i<chan_num;i++)
        {
            fsl_srio_set_obwin(sriodev, port_num, i+1,
                ppool->port_info.range_start+chan_size*i,
                FVL_SRIO_SYS_ADDR+chan_size*i, chan_law);
            fsl_srio_set_obwin_attr(sriodev, port_num,i+1,
                attr_read, attr_write);
        }
//ctl
        fsl_srio_set_obwin(sriodev, port_num, 7,
                ppool->port_info.range_start+win_offset,
                FVL_SRIO_CTL_ADDR, ctl_law);
        fsl_srio_set_obwin_attr(sriodev, port_num, 7,
                attr_read, attr_write);
    } 
    else 
    {
        FVL_LOG("SRIO port %d error!\n", port_num + 1);
        return -errno;
    }
    rvl=fsl_srio_set_deviceid(sriodev,port_num,source_id[port_num]);
    if(rvl!=0)
    {
        FVL_LOG("SRIO port %d set  source_id faile!\n",port_num);
        return -errno;
    }
    for(i=0;i<chan_num;i++)
    {
        rvl=fsl_srio_set_targetid(sriodev,port_num,i+1,target_id[port_num]);
        if(rvl!=0)
        {
            FVL_LOG("SRIO port %d set  target_id faile!\n",port_num);
            return -errno;
        }
    }
    rvl=fsl_srio_set_targetid(sriodev,port_num,7,target_id[port_num]);
    if(rvl!=0)
    {
        FVL_LOG("SRIO port %d set  target_id faile!\n",port_num);
        return -errno;
    }

    fsl_srio_set_phy_retry_threshold(sriodev,port_num,0,0);

    memset(port_dma_wr->dma_virt_base,0,win_size);        
    memset(port_dma_ctl_wp->dma_virt_base,0,ctl_win_size);        
    memset(port_dma_ctl_wr->dma_virt_base,0,ctl_win_size);        
    
    fvl_srio_channel_t *temp_channel;
  
    for(i=0;i<FVL_CHAN_NUM_MAX;i++)
    {
        pscb=fvl_srio_getcb(port_num,i*2);
        if(pscb==NULL)
        {
            FVL_LOG("port:%d channel:%d : dmadev init error.\n",port_num+1,2*i);
            return -1;
        }
        temp_channel=&srio_channel_context[FVL_CHAN_NUM_MAX*port_num+i];
        cscb=&(temp_channel->chanblk);
        cscb->dmadev=pscb->dmadev;
        cscb->bfnum=pscb->bfnum;
        cscb->port = pscb->port;
    
        pscb=fvl_srio_getcb(port_num,i*2+1);
        if(pscb==NULL)
        {
            FVL_LOG("port:%d channel:%d : dmadev init error.\n",port_num+1,2*i+1);
            return -1;
        }
        re_cscb=&(temp_channel->rechanblk);
        re_cscb->dmadev=pscb->dmadev;
        re_cscb->bfnum=pscb->bfnum;
        re_cscb->port = pscb->port;
    }    
//mode master
//need send packet_info out 
    memcpy(ppool->pwrite_ctl_data,&head_port[port_num],HEAD_SIZE);
    uint64_t dest_phys,src_phys;
    src_phys=ppool->write_ctl_data;
    dest_phys=ppool->ctl_info_start;
    pscb=&psrio->ctrlblk[FVL_PORT_DMA_NUM*port_num];
    fvl_srio_send(pscb->dmadev,src_phys,dest_phys,HEAD_SIZE);

    FVL_LOG("PORT_NUM:%d\n",port_num);
    head_arg.num = port_num;
    head_arg.op_mode = 0;
    head_arg.buf_virt=ppool->pwrite_ctl_result;
    
    fvl_srio_recv_head(&head_arg);
    
    return 0;
}

int fvl_get_law(uint32_t fd)
{
    int rvl=1;
    if(fd==0)
    {
        return 0;
    }
    else if(fd == 1)
    {
        return 1;
    }
    else
    {
        rvl+=fvl_get_law(fd/2);
    }

    return rvl;
}
int fvl_get_channel(char *name)
{
    int i=0;
    for(i=0;i<FVL_SRIO_CBMAX;i++)
    {
        if(!strcmp(name,srio_ctable_context[i].name))
        {
            if(!srio_ctable_context[i].flag)
            {
                srio_ctable_context[i].flag=1;
                return i;
            }
        }
    }
    return -1;
}

int fvl_get_channel_num(int port_num)
{
    if(port_num >= FVL_SRIO_PORT_NUM)
    {
        FVL_LOG("Error:have no port\n");
        return -1;
    }
    if(head_port[port_num].chan_num!=0)
    {
       return head_port[port_num].chan_num; 
    }

    return 0;
}

int fvl_srio_recv_head(fvl_head_thread_t *priv)
{
    volatile fvl_srio_head_info_t *pcnt;
    pcnt  = (fvl_srio_head_info_t *)(priv->buf_virt);
    int rvl=0;

    while(1) 
    {
        if(!priv->op_mode)
        {
            if(!pcnt->re_flag)
            {
                continue;
            }
            pcnt->re_flag=0;
            FVL_LOG("HOST: Receive Back Head info:chan_num %d\n",pcnt->chan_num);
            head_port[priv->num].re_flag=1;
            head_port[priv->num].chan_num=pcnt->chan_num;
            head_port[priv->num].uflag=1;
            FVL_LOG("port num:%d uflag:%d\n",priv->num,head_port[priv->num].uflag);
            break;
        }
        else if(priv->op_mode == 1)
        {
            if(!pcnt->re_flag)
            {
                continue;
            }
            pcnt->re_flag=0;
            head_channel[priv->num].re_flag=1;
            head_channel[priv->num].uflag=1;
            break;
        }
    }
    return 0;
}


//need change
void fvl_srio_recv_ctl(void *arg)
{
    fvl_ctl_thread_t  *priv=arg;
    volatile fvl_srio_ctl_info_t *pcnt;
    uint16_t ctl_count=0;
    pcnt  = (fvl_srio_ctl_info_t *)(priv->buf_virt);
    FVL_LOG("channel:%d Host recv ctl !\n",priv->fd);
    int rvl=0;
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(priv->cpu,&cpuset);
    rvl = pthread_setaffinity_np(pthread_self(),sizeof(cpu_set_t),&cpuset);
    if(rvl)
    {
	FVL_LOG("(%d)fail:pthread_setaffinity_np()\n",priv->cpu);
	return;
    }
    while(1) 
    {
        if(pcnt->fla==1)
        {
            FVL_LOG("recv buffer full\n");
            pcnt->fla=0;
            continue;
        }
        else
        {
            if(pcnt->com < ctl_count)
            {
                receive_num[priv->fd]=receive_num[priv->fd]+(head_port[priv->port_num].buf_num -ctl_count);
                ctl_count=0;
            }
            else if(pcnt->com >ctl_count)
            {
                receive_num[priv->fd]=receive_num[priv->fd]+(pcnt->com-ctl_count);
                ctl_count=pcnt->com;
            }
        }
    }
    return;
}

void fvl_srio_rese_ctl(fvl_ctl_thread_t *priv)
{
    volatile fvl_srio_ctl_info_t *pcnt;
    pcnt  = (fvl_srio_ctl_info_t *)(priv->buf_virt);
    int rvl=0;
            
    if(pcnt->com < ctl_count[priv->fd])
    {
         rese_num[priv->fd]=rese_num[priv->fd]+(head_port[priv->port_num].buf_num -ctl_count[priv->fd]);
         rese_num[priv->fd]=rese_num[priv->fd]+pcnt->com;
         ctl_count[priv->fd]=pcnt->com;
    }
    else if(pcnt->com >ctl_count[priv->fd])
    {
         rese_num[priv->fd]=rese_num[priv->fd]+(pcnt->com-ctl_count[priv->fd]);
         ctl_count[priv->fd]=pcnt->com;
    }
        
    return;
}


int fvl_srio_channel_open(char *name)
{
    int fd=0;
    int rvl=0;
    fvl_srio_context_t  *psrio = &g_srio_context;
    fvl_srio_portpool_t *cpool;
    fvl_srio_portpool_t *ppool;
    fvl_head_thread_t head_p_arg;
    int port_num=0,chan_size=0,ctl_size=0,bfnum=0;
    uint32_t offset=0,ctl_offset=0;
    fd=fvl_get_channel(name);
    if(fd==-1)
    {
        FVL_LOG("open error:channel name error.\n");
        return -1;
    }

    port_num=srio_ctable_context[fd].port;
    bfnum   = srio_ctable_context[fd].chan;
//mode master
    FVL_LOG("port num:%d uflag:%d\n",port_num,head_port[port_num].uflag);
    FVL_LOG("*********************************\n");
    volatile uint8_t *flag=&head_port[port_num].uflag;
    while(1)
    {
        fflush(stdout);
        if(*flag)
        {
            break;
        }
    }

    FVL_LOG("chan_fid:%d,bfnum:%d port_num:%d chan_num %d\n",fd,bfnum,port_num,head_port[port_num].chan_num);
    if(bfnum > head_port[port_num].chan_num)
    {
        FVL_LOG("open error:channel not exist.\n");
        return -1; 
    }

    rese_num[fd]=head_port[port_num].buf_num;
//end mode master
    FVL_LOG("*****after while ****Head_size:%d rese_num:%d \n",HEAD_SIZE,rese_num[fd]);

    chan_size = head_port[port_num].chan_size;
    ctl_size = FVL_CTL_WIN_SIZE;

    offset=chan_size*bfnum;
    ctl_offset=ctl_size*bfnum;

    fvl_srio_channel_t *temp_channel;
    temp_channel=&srio_channel_context[fd];
    cpool=&(temp_channel->chanpool);

    ppool = &psrio->portpool[port_num];

    cpool->write_result = ppool->write_result+offset;
    cpool->pwrite_result = ppool->pwrite_result+offset;
         
    cpool->write_ctl_result = ppool->write_ctl_result+HEAD_SIZE+ctl_offset;
    cpool->write_ctl_data = ppool->write_ctl_data+HEAD_SIZE+ctl_offset;
        
    cpool->pwrite_ctl_result = ppool->pwrite_ctl_result+HEAD_SIZE+ctl_offset;
    cpool->pwrite_ctl_data = ppool->pwrite_ctl_data+HEAD_SIZE+ctl_offset;
// very important
    cpool->port_info.range_start=ppool->port_info.range_start+offset;
    cpool->ctl_info_start = ppool->ctl_info_start+HEAD_SIZE+ctl_offset;

    head_p_arg.num = fd;
    head_p_arg.op_mode = 1;
    head_p_arg.buf_virt=cpool->pwrite_ctl_result;
    fvl_srio_recv_head(&head_p_arg);

// add 
    while(!head_channel[fd].uflag);
//end master
    FVL_LOG("channel:%d Host Receive Ctl head info!\n",fd);
    ctl_re_arg[fd].fd=fd;
    ctl_re_arg[fd].port_num=port_num;
    ctl_re_arg[fd].cpu=fd+3;
    ctl_re_arg[fd].buf_virt=cpool->pwrite_ctl_result+FVL_SRIO_RD_OP*HEAD_SIZE;

    rvl = pthread_create(&(temp_channel->chan_id), NULL,fvl_srio_recv_ctl, &ctl_re_arg[fd]);
    if (rvl) 
    {
        FVL_LOG("Create receive packet thread error!\n");
        return -errno;
    }
    return fd;
}

int fvl_srio_channel_close(int fd)
{
    fvl_srio_ctrlblk_t *pscb;
    fvl_srio_channel_t *temp_channel;
    temp_channel=&srio_channel_context[fd];
    pscb=&temp_channel->chanblk;
    pthread_join(temp_channel->chan_id,NULL);
    srio_ctable_context[fd].flag=0;
    fsl_dma_chan_finish(pscb->dmadev);
    return 0;
}
int fvl_srio_finish()
{
    struct srio_dev *sriodev;
    fvl_srio_context_t  *psrio = &g_srio_context;
    sriodev=psrio->sriodev;
    fsl_srio_uio_finish(sriodev);    
    of_finish();
    pthread_join(psrio->chan_id[0],NULL);
    pthread_join(psrio->chan_id[1],NULL);
    init_flag=0;
    return 0;
}

fvl_srio_ctrlblk_t * fvl_srio_getcb(uint32_t port, uint32_t bfnum)
{
    fvl_srio_ctrlblk_t *pscb;
    int rvl;
    int num;
    num = port * FVL_PORT_DMA_NUM+bfnum;
    if((num >= FVL_SRIO_CBMAX) || (port >= FVL_SRIO_PORT_NUM))
    {
        return NULL;
    }   
    pscb = &g_srio_context.ctrlblk[num];

    rvl = fsl_dma_chan_init(&pscb->dmadev, port, bfnum);	
    if (rvl < 0) 
    {
        FVL_LOG("%s(): fsl_dma_chan_init() return %d, args: %u, %u\n", __func__, rvl, port, bfnum);
        return NULL;
    }
    fsl_dma_chan_basic_direct_init(pscb->dmadev);
    fsl_dma_chan_bwc(pscb->dmadev, DMA_BWC_1024);
    pscb->bfnum = bfnum;
    pscb->port = port;
    return pscb;
}
int fvl_srio_send(struct dma_ch *dmadev, uint64_t src_phys, uint64_t dest_phys, uint32_t size)
{
    int rvl;
    fsl_dma_direct_start(dmadev, src_phys, dest_phys,size);
    rvl = fsl_dma_wait(dmadev);
    if (rvl < 0) 
    {
        FVL_LOG("dma task error!\n");
        return -1;
    }
    return 0;
}


int fvl_srio_write(int fd, uint64_t phys,uint32_t length)
{
//need vtop
    fvl_srio_ctrlblk_t *pscb;
    fvl_srio_portpool_t *cpool;
    fvl_srio_channel_t *temp_channel;
    fvl_ctl_thread_t  ctl_se_arg;
    volatile fvl_srio_ctl_info_t *pcnt;
    temp_channel=&srio_channel_context[fd];
    pscb=&(temp_channel->chanblk);
    cpool=&(temp_channel->chanpool);
    uint64_t dest_phys,src_phys;
    int port_num=0,buf_size=0,buf_num=0;
    uint32_t offset=0;
    uint32_t step=0;
    
    port_num=srio_ctable_context[fd].port;
    buf_num = head_port[port_num].buf_num;
    buf_size = head_port[port_num].buf_size;

    ctl_se_arg.fd=fd;
    ctl_se_arg.port_num=port_num;
    ctl_se_arg.buf_virt=cpool->pwrite_ctl_result+FVL_SRIO_WR_OP*HEAD_SIZE;
    fvl_srio_rese_ctl(&ctl_se_arg);

    step=length/buf_size;
    if((length%buf_size))
    {
        step=step+1;
    }
     
    if((send_num[fd]+step)>=rese_num[fd])
    {
        pcnt=(fvl_srio_ctl_info_t *)(cpool->pwrite_ctl_data+FVL_SRIO_WR_OP*HEAD_SIZE);
        pcnt->fla=1;
        pcnt->com=(send_num[fd]%buf_num);
        dest_phys=(uint64_t *)(cpool->ctl_info_start+FVL_SRIO_WR_OP*HEAD_SIZE);
        src_phys =(uint64_t *)(cpool->write_ctl_data+FVL_SRIO_WR_OP*HEAD_SIZE);
        fvl_srio_send(pscb->dmadev,src_phys,dest_phys,HEAD_SIZE);
        return -1;
    }

    offset=buf_size*(send_num[fd]%buf_num);
    dest_phys=(uint64_t *)(cpool->port_info.range_start+offset);
    src_phys = phys; 
    fvl_srio_send(pscb->dmadev,src_phys,dest_phys,length);
    
    send_num[fd]=send_num[fd]+step;
    pcnt=(fvl_srio_ctl_info_t *)(cpool->pwrite_ctl_data+FVL_SRIO_WR_OP*HEAD_SIZE);
    pcnt->fla=0;
    pcnt->com=(send_num[fd]%buf_num);
    dest_phys=(uint64_t *)(cpool->ctl_info_start+FVL_SRIO_WR_OP*HEAD_SIZE);
    src_phys =(uint64_t *)(cpool->write_ctl_data+FVL_SRIO_WR_OP*HEAD_SIZE);
    fvl_srio_send(pscb->dmadev,src_phys,dest_phys,HEAD_SIZE);
    
    return 0;
}


// return read_num and addr
// (myself advice packet_num not length)
//
int fvl_srio_read(int fd,fvl_read_rvl_t *rvl)
{
    uint32_t num=0,length=rvl->len;
    uint32_t offset=0;
    fvl_srio_portpool_t *cpool;
    fvl_srio_channel_t *temp_channel;
    void *buf_virt=NULL;
    temp_channel=&srio_channel_context[fd];
    int port_num=0,buf_size=0,buf_num=0,packet_num;
    cpool=&(temp_channel->chanpool);
    port_num=srio_ctable_context[fd].port;
    buf_num = head_port[port_num].buf_num;
    buf_size = head_port[port_num].buf_size;
    offset=buf_size*(read_num[fd]%buf_num);
    num=receive_num[fd] - read_num[fd];
    if(num > (buf_num -(read_num[fd]%buf_num)))
    {
        num=buf_num-(read_num[fd]%buf_num);
    }

    packet_num=length/buf_size;
    if((length%buf_size))
    {
        packet_num=packet_num+1;
    }

    if(num > packet_num)
    {
        rvl->len=length;
        read_num[fd]=read_num[fd]+packet_num;
        rvl->num=packet_num;
    }
    else 
    {
       if(num==0)
       {
           rvl->len=0;
       }
       else
       {
           rvl->len=buf_size*num;//end packet size;
       }
       read_num[fd]=read_num[fd]+num;
       rvl->num=num;
    }
    buf_virt=(cpool->pwrite_result+offset);
    rvl->buf_virt=buf_virt;

    return 0;
}

int fvl_srio_read_feedback(int fd,int num)
{
    fvl_srio_ctrlblk_t *pscb;
    fvl_srio_portpool_t *cpool;
    fvl_srio_channel_t *temp_channel;
    volatile fvl_srio_ctl_info_t *pcnt;
    temp_channel=&srio_channel_context[fd];
    pscb=&(temp_channel->rechanblk);
    cpool=&(temp_channel->chanpool);
    uint64_t dest_phys,src_phys;
    int port_num=0,buf_num=0;
    port_num=srio_ctable_context[fd].port;
    buf_num = head_port[port_num].buf_num;
	
    rfreeback_num[fd]=rfreeback_num[fd]+num;
    pcnt=(fvl_srio_ctl_info_t *)(cpool->pwrite_ctl_data+FVL_SRIO_RD_OP*HEAD_SIZE);
//need change ctl_info
    pcnt->com=(rfreeback_num[fd]%buf_num);
    dest_phys=(uint64_t *)(cpool->ctl_info_start+FVL_SRIO_RD_OP*HEAD_SIZE);
    src_phys =(uint64_t *)(cpool->write_ctl_data+FVL_SRIO_RD_OP*HEAD_SIZE);
    fvl_srio_send(pscb->dmadev,src_phys,dest_phys,HEAD_SIZE);
}


int dma_usmem_init(fvl_dma_pool_t *pool,uint64_t pool_size,uint64_t dma_size)
{
    int err;

    dma_mem_generic = dma_mem_create(DMA_MAP_FLAG_ALLOC,
			NULL, pool_size);
    if (!dma_mem_generic) 
    {
        err = -EINVAL;
        error(0, -err, "%s(): dma_mem_create()", __func__);
        return err;
    }

    pool->dma_virt_base = __dma_mem_memalign(4096, dma_size);
    if (!pool->dma_virt_base) 
    {  
        err = -EINVAL;
        error(0, -err, "%s(): __dma_mem_memalign()", __func__);
        return err;
    }
    pool->dma_phys_base = __dma_mem_vtop(pool->dma_virt_base);

    return 0;
}

int dma_pool_init(fvl_dma_pool_t **pool,uint64_t pool_size,uint64_t dma_size)
{
    fvl_dma_pool_t *dma_pool;
    int err;

    dma_pool = malloc(sizeof(*dma_pool));
    if (!dma_pool) 
    {
        error(0, errno, "%s(): DMA pool", __func__);
        return -errno;
    }
    memset(dma_pool, 0, sizeof(*dma_pool));
    *pool = dma_pool;

    err = dma_usmem_init(dma_pool,pool_size,dma_size);
    if (err < 0) 
    {
        error(0, -err, "%s(): DMA pool", __func__);
        free(dma_pool);
        return err;
    }
    return 0;
}

