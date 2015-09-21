
#include "fvl_srio.h"

#define CTL_TRUE_SIZE sizeof(fvl_srio_ctl_info_t)

fvl_srio_context_t g_srio_context;

fvl_srio_channel_t srio_channel_context[FVL_CHAN_NUM_MAX];

fvl_srio_head_info_t head_port[FVL_SRIO_PORT_NUM];
fvl_srio_response_info_t head_port_response[FVL_SRIO_PORT_NUM];
fvl_srio_response_info_t head_channel_response[FVL_CHAN_NUM_MAX];

pthread_mutex_t mutex[FVL_SRIO_PORT_NUM];

static uint64_t receive_num[FVL_CHAN_NUM_MAX]={0,0,0,0,0,0,0,0};
static uint64_t send_num[FVL_CHAN_NUM_MAX]={0,0,0,0,0,0,0,0};
static uint64_t rese_num[FVL_CHAN_NUM_MAX]={0,0,0,0,0,0,0,0};
static uint64_t re_ctl_count[FVL_CHAN_NUM_MAX]={0,0,0,0,0,0,0,0};
static uint64_t ctl_count[FVL_CHAN_NUM_MAX]={0,0,0,0,0,0,0,0};
static uint64_t rfreeback_num[FVL_CHAN_NUM_MAX]={0,0,0,0,0,0,0};
static uint64_t read_num[FVL_CHAN_NUM_MAX]={0,0,0,0,0,0,0,0};

static fvl_srio_ctable_t srio_ctable_context[FVL_CHAN_NUM_MAX]={
{"srio0-chan0",0,0,0},{"srio0-chan1",0,1,0},{"srio0-chan2",0,2,0},{"srio0-chan3",0,3,0},
{"srio1-chan0",1,0,0},{"srio1-chan1",1,1,0},{"srio1-chan2",1,2,0},{"srio1-chan3",1,3,0}
};

static fvl_head_thread_t slave_head_arg[FVL_SRIO_PORT_NUM];
static fvl_ctl_thread_t  ctl_re_arg[FVL_CHAN_NUM_MAX];

static uint32_t source_id[2]={0x11,0x14};
static uint32_t target_id[2]={0x11,0x14};

static const uint32_t srio_test_win_attrv[] = {3, 4, 5, 4, 0};

static int init_flag=0;
static int port_uflag[FVL_SRIO_PORT_NUM]={0,0};
static int channel_uflag[FVL_CHAN_NUM_MAX]={0,0,0,0,0,0,0,0};

static int second_handshake[FVL_SRIO_PORT_NUM]={0,0};//0-> no 1->need
static int align_mode[FVL_SRIO_PORT_NUM]={0,0}; //1 -> 256 0->true size 
static int offset_mode[FVL_SRIO_PORT_NUM]={0,0};//1 -> always add 0-> relative add

static uint32_t  HEAD_SIZE[FVL_SRIO_PORT_NUM]={CTL_TRUE_SIZE,CTL_TRUE_SIZE};

//port status
static uint32_t  SRIO_PORT_STATUS[FVL_SRIO_PORT_NUM]={0,0};

//static uint32_t srio_type[FVL_SRIO_PORT_NUM]={0,0};

static int port_mode[FVL_SRIO_PORT_NUM]={0,0};
static int rd_op_base[FVL_SRIO_PORT_NUM]={1,1};
static int wr_op_base[FVL_SRIO_PORT_NUM]={2,2};

static uint64_t version_mode[FVL_SRIO_PORT_NUM]={0x07,0x07};

int fvl_srio_init(fvl_srio_init_param_t *srio_param)
{
    int rvl=0;
    int port_num=srio_param->port_num;
    if(srio_param->mode==0)
    {
    	rd_op_base[port_num]=1;
    	wr_op_base[port_num]=2;
    	port_mode[port_num]=srio_param->mode;
        rvl=fvl_srio_init_master(srio_param);
        return rvl;
    }
    else if(srio_param->mode==1)
    {
        uint64_t vs_type=srio_param->version_mode;
        version_mode[port_num]=vs_type;
    	rd_op_base[port_num]=2;
    	wr_op_base[port_num]=1;
    	port_mode[port_num]=srio_param->mode;
        if(vs_type & 0x01)
        {
            second_handshake[port_num]=1;
        }
        else
        {
            second_handshake[port_num]=0;            	
        }
        if(vs_type & 0x02)
        {
            align_mode[port_num]=1;//
            HEAD_SIZE[port_num]=256;
        }
        else
        {
            align_mode[port_num]=0;// 
            HEAD_SIZE[port_num]=CTL_TRUE_SIZE;           	
        }
        if(vs_type & 0x04)
        {
            offset_mode[port_num]=1;
        }
        else
        {
            offset_mode[port_num]=0;            
        }
        rvl=fvl_srio_init_slave(srio_param);
        return rvl;
    }
    else
    {
        FVL_LOG("This program do not support this mode!\n");
        return -1;	
    }	
}

int fvl_srio_init_slave(fvl_srio_init_param_t *srio_param)
{
    fvl_srio_context_t  *psrio = &g_srio_context;
    fvl_srio_portpool_t *ppool;
    fvl_srio_ctrlblk_t *pscb,*cscb,*re_cscb;
    uint32_t chan_num = srio_param->chan_num; 
    uint32_t port_num = srio_param->port_num;
    fvl_dma_pool_t *port_dma_wr = srio_param->port_rd_buf;
    fvl_dma_pool_t *port_dma_ctl_wp;
    fvl_dma_pool_t *port_dma_ctl_wr;
    
    uint32_t ctl_win_size=0x400000;//ctl_size*chan_num;
    uint32_t ctl_law=21;
    struct srio_dev *sriodev;
    int rvl;

    if((port_num >= FVL_SRIO_PORT_NUM) || (port_num <0))
    {
        FVL_LOG("port number error:%d\n",port_num);
        return -1;
    }
    if((chan_num > FVL_PORT_CHAN_NUM_MAX) || ( chan_num<0))
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

//mode slave	
    head_port_response[port_num].symbol = FVL_SRIO_SYMBOL;
    head_port_response[port_num].status = FVL_SRIO_STATUS;
    head_port_response[port_num].vs_type= version_mode[port_num];
    int i=0;
    int j=FVL_PORT_CHAN_NUM_MAX*port_num;
    
    FVL_LOG("Slave: chan_num %d\n",chan_num);
    
    for(i=0;i<chan_num;i++)
    {
        head_channel_response[j+i].symbol = FVL_SRIO_SYMBOL;
        head_channel_response[j+i].status = FVL_SRIO_STATUS;
        head_channel_response[j+i].vs_type= version_mode[port_num];
    }
    
    psrio->chan_num[port_num]=chan_num;
    source_id[port_num]=srio_param->source_id;
    target_id[port_num]=srio_param->target_id;
   
    FVL_LOG("port_num:%d target_id:%04x source_id:%04x\n",port_num,target_id[port_num],source_id[port_num]);
    struct srio_port_info *pinfo;
    void *range;
    pinfo = &psrio->portpool[port_num].port_info;
    fsl_srio_connection(sriodev,port_num);
    if (fsl_srio_port_connected(sriodev) & (0x1 << port_num)) 
    {
        fsl_srio_get_port_info(sriodev, port_num+1, pinfo, &range);
        psrio->portpool[port_num].range_virt = range;
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

    FVL_LOG("*****************************************************\n");    
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
    FVL_LOG("*********************dma pool init end**************\n");    
    uint32_t attr_read=0, attr_write=0;
    attr_read = srio_test_win_attrv[3];
    attr_write = srio_test_win_attrv[0];
    FVL_LOG("attr_write = %u, srio_type = %u\n", attr_write,attr_read);
        
    ppool = &psrio->portpool[port_num];
    ppool->write_result = port_dma_wr->dma_phys_base;
    ppool->pwrite_result = port_dma_wr->dma_virt_base;

    ppool->write_ctl_result = port_dma_ctl_wr->dma_phys_base;
    ppool->write_ctl_data = port_dma_ctl_wp->dma_phys_base;
    ppool->pwrite_ctl_result = port_dma_ctl_wr->dma_virt_base;
    ppool->pwrite_ctl_data = port_dma_ctl_wp->dma_virt_base;
    
    FVL_LOG("ctl law:%d\n",ctl_law);

    fsl_srio_set_ibwin(sriodev, port_num, 2, ppool->write_ctl_result,
            FVL_SRIO_CTL_ADDR, ctl_law);
    
    rvl=fsl_srio_set_deviceid(sriodev,port_num,source_id[port_num]);
    if(rvl!=0)
    {
        FVL_LOG("SRIO port %d set  source_id faile!\n",port_num);
        return -errno;
    }
    rvl=fsl_srio_set_targetid(sriodev,port_num,1,target_id[port_num]);
    if(rvl!=0)
    {
        FVL_LOG("SRIO port %d set  target_id faile!\n",port_num);
        return -errno;
    }
    rvl=fsl_srio_set_targetid(sriodev,port_num,2,target_id[port_num]);
    if(rvl!=0)
    {
        FVL_LOG("SRIO port %d set  target_id faile!\n",port_num);
        return -errno;
    }
    fsl_srio_set_err_rate_degraded_threshold(sriodev,port_num,0);
    fsl_srio_set_err_rate_failed_threshold(sriodev,port_num,0);
    fsl_srio_set_phy_retry_threshold(sriodev,port_num,0,0);
           
    memset(port_dma_ctl_wp->dma_virt_base,0,ctl_win_size);        
    memset(port_dma_ctl_wr->dma_virt_base,0,ctl_win_size); 
    
    fvl_srio_channel_t *temp_channel;
    j=FVL_PORT_CHAN_NUM_MAX*port_num;
    for(i=0;i<FVL_PORT_CHAN_NUM_MAX;i++)
    {
        pscb=fvl_srio_getcb(port_num,i*2);
        if(pscb==NULL)
        {
            FVL_LOG("port:%d channel:%d : dmadev init error.\n",port_num+1,2*i);
            return -1;
        }
        temp_channel=&srio_channel_context[j+i];
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

//create thread:
    FVL_LOG("port_num:%d\n",port_num);
    slave_head_arg[port_num].num = port_num;
    slave_head_arg[port_num].cpu = port_num+1;
    slave_head_arg[port_num].op_mode = 0;
    slave_head_arg[port_num].buf_virt=ppool->pwrite_ctl_result;

    rvl = pthread_create(&(psrio->chan_id[port_num]), NULL,fvl_srio_recv_head_slave, &slave_head_arg[port_num]);

    if (rvl) 
    {
        FVL_LOG("Create receive packet thread error!\n");
        return -errno;
    }
    FVL_LOG("SRIO Initial complete\n");    

    return 0;
}
int fvl_srio_init_master(fvl_srio_init_param_t *srio_param)
{
    fvl_srio_context_t  *psrio = &g_srio_context;
    fvl_srio_portpool_t *ppool;
    fvl_srio_ctrlblk_t *pscb,*cscb,*re_cscb;
    fvl_head_thread_t head_arg;
    uint32_t chan_num=srio_param->chan_num;
    uint32_t port_num=srio_param->port_num;
    fvl_dma_pool_t *port_dma_wr = srio_param->port_rd_buf;
    fvl_dma_pool_t *port_dma_ctl_wp;
    fvl_dma_pool_t *port_dma_ctl_wr;

    uint32_t win_size=0; //need change
    uint32_t ctl_win_size=0x400000;//ctl_size*chan_num;
    uint32_t win_law=0,ctl_law=21;
    struct srio_dev *sriodev;
    int rvl;
    int i=0;
    if((port_num >= FVL_SRIO_PORT_NUM) || (port_num <0))
    {
        FVL_LOG("port number error:%d\n",port_num);
        return -1;
    }
    if((chan_num > FVL_PORT_CHAN_NUM_MAX) || ( chan_num<0))
    {
        FVL_LOG("channel number error:%d\n",chan_num);
        return -1;
    }
    
    if(SRIO_PORT_STATUS[port_num]==FVL_INIT_READY)
    {
        FVL_LOG("The port:%d has been inited ready,don't need to be inited again!\n",port_num);
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
    uint32_t offset=0;
    int j=FVL_PORT_CHAN_NUM_MAX*port_num;
    for(i=0;i<chan_num;i++)
    {
        uint32_t buf_num=srio_param->buf_num[i];
        uint32_t buf_size=(srio_param->buf_size[i]/FVL_BUFSIZE_unit);
        uint32_t chan_size=buf_size*buf_num*FVL_BUFSIZE_unit;
        head_port[port_num].channel[i].receive_cluster.buf_size=buf_size;
        head_port[port_num].channel[i].receive_cluster.buf_num=buf_num;
        head_port[port_num].channel[i].receive_cluster.cluster_addr=FVL_SRIO_SYS_ADDR+offset;
        head_port[port_num].channel[i].send_cluster.buf_size=buf_size;
        head_port[port_num].channel[i].send_cluster.buf_num=buf_num;
        head_port[port_num].channel[i].send_cluster.cluster_addr=FVL_SRIO_SYS_ADDR+offset;
        head_port[port_num].symbol=FVL_SRIO_SYMBOL;
        head_port[port_num].status=FVL_SRIO_STATUS;
        head_port[port_num].cmd=FVL_INIT_CMD;
        offset=offset+chan_size;
        srio_channel_context[(j+i)].chan_size=chan_size; 
        srio_channel_context[(j+i)].buf_size=srio_param->buf_size[i];
        srio_channel_context[(j+i)].buf_num=buf_num;
        win_size=win_size+chan_size;
        FVL_LOG("channel:%d buf_size:%d buf_num: %d chan_size:%08x\n",(j+i),srio_param->buf_size[i],buf_num,chan_size);
//head channel init 
    }
    psrio->chan_num[port_num] = chan_num;
    source_id[port_num]=srio_param->source_id;
    target_id[port_num]=srio_param->target_id;
    
    FVL_LOG("port_num:%d target_id:%04x source_id:%04x\n",port_num,target_id[port_num],source_id[port_num]);
//end master
    struct srio_port_info *pinfo;
    void *range;
    pinfo = &psrio->portpool[port_num].port_info;
    fsl_srio_connection(sriodev,port_num);
    if (fsl_srio_port_connected(sriodev) & (0x1 << port_num)) 
    {
        fsl_srio_get_port_info(sriodev, port_num+1, pinfo, &range);
        psrio->portpool[port_num].range_virt = range;
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
  //  ctl_law=FVL_BASE_LAW+fvl_get_law((ctl_win_size/FVL_BASE_LAW_SIZE)-1);
   
 
    FVL_LOG("HOST WIN_LAW:%d CTL_LAW:%d\n",win_law,ctl_law);
     
    fsl_srio_set_ibwin(sriodev, port_num, 1, ppool->write_result,
            FVL_SRIO_SYS_ADDR, win_law);
    fsl_srio_set_ibwin(sriodev, port_num, 2, ppool->write_ctl_result,
            FVL_SRIO_CTL_ADDR, ctl_law);

    uint32_t win_offset=FVL_BASE_LAW_SIZE;

    for(i=FVL_BASE_LAW;i<win_law;i++)
    {
        win_offset=win_offset*2;
    }

    FVL_LOG("HOST WIN_OFFSET:%d\n",win_offset);

    ppool->ctl_info_start=ppool->port_info.range_start+win_offset;

    if (fsl_srio_port_connected(sriodev) & (0x1 << port_num)) 
    {
//ctl
        fsl_srio_set_obwin(sriodev, port_num, 2,
                ppool->port_info.range_start+win_offset,
                FVL_SRIO_CTL_ADDR, ctl_law);
        fsl_srio_set_obwin_attr(sriodev, port_num, 2,
                attr_read, attr_write);
//data
        fsl_srio_set_obwin(sriodev, port_num, 1,
                ppool->port_info.range_start,
                FVL_SRIO_SYS_ADDR, win_law);
        fsl_srio_set_obwin_attr(sriodev, port_num, 1,
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
    rvl=fsl_srio_set_targetid(sriodev,port_num,1,target_id[port_num]);
    if(rvl!=0)
    {
        FVL_LOG("SRIO port %d set  target_id faile!\n",port_num);
        return -errno;
    }
    rvl=fsl_srio_set_targetid(sriodev,port_num,2,target_id[port_num]);
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
    j=FVL_PORT_CHAN_NUM_MAX*port_num;
    for(i=0;i<FVL_PORT_CHAN_NUM_MAX;i++)
    {
        pscb=fvl_srio_getcb(port_num,i*2);
        if(pscb==NULL)
        {
            FVL_LOG("port:%d channel:%d : dmadev init error.\n",port_num+1,2*i);
            return -1;
        }
        temp_channel=&srio_channel_context[(j+i)];
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

    pthread_mutex_init(&mutex[port_num],NULL);  

//mode master
//need send packet_info out 
    memcpy(ppool->pwrite_ctl_data,&head_port[port_num],256);
    uint64_t dest_phys,src_phys;
    src_phys=ppool->write_ctl_data;
    dest_phys=ppool->ctl_info_start;

    pscb=&psrio->ctrlblk[FVL_PORT_DMA_NUM*port_num+7];

    pthread_mutex_lock(&mutex[port_num]); 
    fvl_srio_send(pscb->dmadev,src_phys,dest_phys,256);
    pthread_mutex_unlock(&mutex[port_num]);

    FVL_LOG("PORT_NUM:%d HEAD_INFO_SIZE:256\n",port_num);
    head_arg.num = port_num;
    head_arg.op_mode = 0;
    head_arg.buf_virt=ppool->pwrite_ctl_result;
    rvl=fvl_srio_recv_head_master(&head_arg);
    if(rvl!=0)
    {
        FVL_LOG("Host:Port %d  is not ready!\n",port_num);
        return -1;
    }
    
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
void* fvl_srio_recv_head_slave(void *arg)
{
    fvl_head_thread_t  *priv=arg;
    fvl_srio_ctrlblk_t *pscb;
    fvl_srio_portpool_t *ppool;
    volatile fvl_srio_head_info_t *pcnt;
    pcnt  = (fvl_srio_head_info_t *)(priv->buf_virt);
    uint32_t  port_num=priv->num;
    FVL_LOG("port:%d fvl_srio_recv_head!\n",port_num);
    int rvl=0;
    cpu_set_t cpuset;
    if(!priv->op_mode)
    {
        CPU_ZERO(&cpuset);
        CPU_SET(priv->cpu,&cpuset);
        rvl = pthread_setaffinity_np(pthread_self(),sizeof(cpu_set_t),&cpuset);
        if(rvl)
        {
            FVL_LOG("(%d)fail:pthread_setaffinity_np()\n",priv->cpu);
            return NULL;
        }
    }
    while(1) 
    {
        if(!priv->op_mode)
        {
            if(pcnt->cmd!=FVL_INIT_CMD)
            {
                continue;
            }
            fvl_srio_context_t  *psrio = &g_srio_context;
            uint32_t win_size=0,chan_size=0,buf_size=0,buf_num=0;
            int win_law=0,i,rvl=0;
            struct srio_dev *sriodev;
            uint32_t attr_read, attr_write;
            attr_read = srio_test_win_attrv[3];
            attr_write = srio_test_win_attrv[0];
            ppool = &psrio->portpool[port_num];
            pcnt->cmd=0;
            FVL_LOG("chan_num:%d\n",psrio->chan_num[port_num]);
            int j=FVL_PORT_CHAN_NUM_MAX*port_num;
            for(i=0;i<psrio->chan_num[port_num];i++)
            {
                head_port[port_num].channel[i].receive_cluster.buf_size=pcnt->channel[i].receive_cluster.buf_size;
                head_port[port_num].channel[i].receive_cluster.buf_num=pcnt->channel[i].receive_cluster.buf_num;
                head_port[port_num].channel[i].receive_cluster.cluster_addr=pcnt->channel[i].receive_cluster.cluster_addr;
                head_port[port_num].channel[i].send_cluster.buf_size=pcnt->channel[i].send_cluster.buf_size;
                head_port[port_num].channel[i].send_cluster.buf_num=pcnt->channel[i].send_cluster.buf_num;
                head_port[port_num].channel[i].send_cluster.cluster_addr=pcnt->channel[i].send_cluster.cluster_addr;
                buf_size=pcnt->channel[i].send_cluster.buf_size;
                buf_num=pcnt->channel[i].send_cluster.buf_num;
                FVL_LOG("buf_size:%d buf_num:%d\n",buf_size,buf_num);
                chan_size =buf_num*buf_size*FVL_BUFSIZE_unit;
                srio_channel_context[(j+i)].chan_size=chan_size;
                srio_channel_context[(j+i)].buf_size=buf_size*FVL_BUFSIZE_unit;
                srio_channel_context[(j+i)].buf_num=buf_num;
                win_size=win_size+chan_size;
                FVL_LOG("win_size:%08x\n",win_size);
            }      
            FVL_LOG("*************************************************************\n");
            sriodev=psrio->sriodev;
            win_law=FVL_BASE_LAW+fvl_get_law((win_size/FVL_BASE_LAW_SIZE)-1);
            FVL_LOG("WIN_LAW:%d, WIN_SIZE:%08x\n",win_law,win_size);
            fsl_srio_set_ibwin(sriodev, port_num, 1, ppool->write_result,
                               head_port[port_num].channel[0].receive_cluster.cluster_addr, win_law);
            uint32_t win_offset=FVL_BASE_LAW_SIZE;
            for(i=FVL_BASE_LAW;i<win_law;i++)
            {
                win_offset=win_offset*2;
            }
            FVL_LOG("WIN_OFFSET:%08x\n",win_offset);
            ppool->ctl_info_start=ppool->port_info.range_start+win_offset;

            if (fsl_srio_port_connected(sriodev) & (0x1 << port_num)) 
            {
//data
                fsl_srio_set_obwin(sriodev, port_num, 1,
                                   ppool->port_info.range_start,
                                   head_port[port_num].channel[0].send_cluster.cluster_addr, win_law);
                fsl_srio_set_obwin_attr(sriodev, port_num, 1,
                                        attr_read, attr_write);
                fsl_srio_set_obwin(sriodev, port_num, 2,
                           ppool->port_info.range_start+win_offset,
                           FVL_SRIO_CTL_ADDR, 21);
                fsl_srio_set_obwin_attr(sriodev, port_num, 2,
                                attr_read, attr_write);
            } 
            else 
            {
                FVL_LOG("SRIO port %d error!\n", port_num + 1);
                return NULL;
            }
//add debug    
            rvl=fsl_srio_set_targetid(sriodev,port_num,1,target_id[port_num]);
            if(rvl!=0)
            {
                FVL_LOG("SRIO port %d set  target_id faile!\n",port_num);
                return NULL;
            }
            rvl=fsl_srio_set_targetid(sriodev,port_num,2,target_id[port_num]);
            if(rvl!=0)
            {
                FVL_LOG("SRIO port %d set  target_id faile!\n",port_num);
                return NULL;
            }
//end
            // reflag need set 
            head_port_response[port_num].cmd_ack=FVL_INIT_READY;
            memcpy(ppool->pwrite_ctl_data,&head_port_response[port_num],256);
            uint64_t dest_phys,src_phys;
            pscb=&psrio->ctrlblk[FVL_PORT_DMA_NUM*port_num];
            src_phys= ppool->write_ctl_data;
            dest_phys= ppool->ctl_info_start;
            fvl_srio_send(pscb->dmadev,src_phys,dest_phys,256);
            port_uflag[port_num]=1;
            FVL_LOG("port_num:%d Receive Head info and send chan num info sucess!\n",port_num);
        }
    }
    pthread_exit(NULL);
    return NULL;
}

int fvl_srio_recv_head_master(fvl_head_thread_t *priv)
{
    volatile fvl_srio_response_info_t *pcnt;
    pcnt  = (fvl_srio_response_info_t *)(priv->buf_virt);
    struct timeval tm_start,tm_end;
    int i=0;
    gettimeofday(&tm_start,NULL);
    while(1) 
    {
        if(priv->op_mode==0)
        {
            if(pcnt->cmd_ack!=FVL_INIT_READY)
            {
                gettimeofday(&tm_end,NULL);
                if((tm_end.tv_sec-tm_start.tv_sec)>10)
                {
                    FVL_LOG("Host: Receive Back Head info timeout!\n");
                    return -1;
                }
                usleep(1000);
                continue;
            }
            pcnt->cmd_ack=0;
            uint64_t vs_type=pcnt->vs_type;

            if(vs_type & 0x01)
            {
            	second_handshake[priv->num]=1;
            }
            else
            {
            	second_handshake[priv->num]=0;            	
            }
            if(vs_type & 0x02)
            {
            	align_mode[priv->num]=1;//
            	HEAD_SIZE[priv->num]=256;
            }
            else
            {
            	align_mode[priv->num]=0;// 
            	HEAD_SIZE[priv->num]=CTL_TRUE_SIZE;           	
            }
            if(vs_type & 0x04)
            {
            	offset_mode[priv->num]=1;
            }
            else
            {
            	offset_mode[priv->num]=0;            
            }
            port_uflag[priv->num]=1;
            SRIO_PORT_STATUS[priv->num]=FVL_INIT_READY;
            break;
        }
        else if(priv->op_mode == 1)
        {
            if(pcnt->cmd_ack!=FVL_INIT_CHAN_READY)
            {
                gettimeofday(&tm_end,NULL);
                if((tm_end.tv_sec-tm_start.tv_sec)>10)
                {
                    FVL_LOG("Host: Receive logic channel ready info timeout!\n");
                    return -1;
                }
                usleep(1000);
                continue;
            }
            pcnt->cmd_ack=0;
            channel_uflag[priv->num]=1;
            break;
        }
        else if(priv->op_mode==2)
        {
            if(pcnt->cmd_ack!=FVL_START_READY)
            {
                gettimeofday(&tm_end,NULL);
                if((tm_end.tv_sec-tm_start.tv_sec)>10)
                {
                    FVL_LOG("Host: Receive Back Head info timeout!\n");
                    return -1;
                }
                usleep(1000);
                continue;
            }
            pcnt->cmd_ack=0;
            uint64_t vs_type=pcnt->vs_type;
            if(vs_type & 0x01)
            {
            	second_handshake[priv->num]=1;
            }
            else
            {
            	second_handshake[priv->num]=0;            	
            }
            if(vs_type & 0x02)
            {
            	align_mode[priv->num]=1;//
            	HEAD_SIZE[priv->num]=256;
            }
            else
            {
            	align_mode[priv->num]=0;// 
            	HEAD_SIZE[priv->num]=CTL_TRUE_SIZE;           	
            }
            if(vs_type & 0x04)
            {
            	offset_mode[priv->num]=1;
            }
            else
            {
            	offset_mode[priv->num]=0;            
            }
         //   port_uflag[priv->num]=1;
            SRIO_PORT_STATUS[priv->num]=FVL_START_READY;
            break;
        }
        else if(priv->op_mode == 3)
        {
            if(pcnt->cmd_ack!=FVL_STOP_READY)
            {
                gettimeofday(&tm_end,NULL);
                if((tm_end.tv_sec-tm_start.tv_sec)>10)
                {
                    FVL_LOG("Host: Receive Back Head info timeout!\n");
                    return -1;
                }
                usleep(1000);
                continue;
            }
            pcnt->cmd_ack=0;
//clear count 
            int j=FVL_PORT_CHAN_NUM_MAX*priv->num;
            for(i=0;i<FVL_PORT_CHAN_NUM_MAX;i++)
            {
            	  receive_num[j+i]=0;
            	  send_num[j+i]=0;
            	  rese_num[j+i]=0;
            	  ctl_count[j+i]=0;
            	  rfreeback_num[j+i]=0;
            	  read_num[j+i]=0;
            	  re_ctl_count[j+i]=0;
            }
            SRIO_PORT_STATUS[priv->num]=FVL_STOP_READY;  
            break;	
        }
    }
    return 0;
}


//need change
void* fvl_srio_recv_ctl(void *arg)
{
    fvl_ctl_thread_t  *priv=arg;
    volatile fvl_srio_ctl_info_t *pcnt;
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
        return NULL;
    }
    if(offset_mode[priv->port_num]==0)
    {
        while(1) 
        {
            uint32_t count=0;
            count=pcnt->write_point;
            if(pcnt->status==1)
            {
                FVL_LOG("recv buffer full\n");
                pcnt->status=0;
            }
          
            if(count < re_ctl_count[priv->fd])
            {
                receive_num[priv->fd]=receive_num[priv->fd]+(srio_channel_context[priv->fd].buf_num -re_ctl_count[priv->fd]);
                re_ctl_count[priv->fd]=0;
            }
            else if(count >re_ctl_count[priv->fd])
            {
                receive_num[priv->fd]=receive_num[priv->fd]+(count-re_ctl_count[priv->fd]);
                re_ctl_count[priv->fd]=count;
            }
        }
    }
    else if(offset_mode[priv->port_num]==1)
    {
        while(1) 
        {
            uint32_t count=0;
            count=pcnt->write_point;
            if(pcnt->status==1)
            {
                FVL_LOG("recv buffer full\n");
                pcnt->status=0;
            }
            
            if(count >re_ctl_count[priv->fd])
            {
                receive_num[priv->fd]=receive_num[priv->fd]+(count-re_ctl_count[priv->fd]);
                re_ctl_count[priv->fd]=count;
            }
            
        }    	
    	
    }
    
    pthread_exit(NULL);
    return NULL;
}

void fvl_srio_rese_ctl(fvl_ctl_thread_t *priv)
{
    volatile fvl_srio_ctl_info_t *pcnt;
    pcnt  = (fvl_srio_ctl_info_t *)(priv->buf_virt);
    uint32_t count=0;
    if(offset_mode[priv->port_num]==0)
    {
       count=pcnt->read_point;        
       if(count < ctl_count[priv->fd])
       {
         rese_num[priv->fd]=rese_num[priv->fd]+(srio_channel_context[priv->fd].buf_num -ctl_count[priv->fd]);
         rese_num[priv->fd]=rese_num[priv->fd]+count;
         ctl_count[priv->fd]=count;
       }
       else if(count >ctl_count[priv->fd])
       {
         rese_num[priv->fd]=rese_num[priv->fd]+(count-ctl_count[priv->fd]);
         ctl_count[priv->fd]=count;
       }
    }
    else if(offset_mode[priv->port_num]==1)
    {
       count=pcnt->read_point;        
       if(count >ctl_count[priv->fd])
       {
         rese_num[priv->fd]=rese_num[priv->fd]+(count-ctl_count[priv->fd]);
         ctl_count[priv->fd]=count;
       }
    }
        
    return;
}

int fvl_srio_channel_open(char *name)
{
    int fd=0;
    int port_num=0;
    fd=fvl_get_channel(name);
    if(fd==-1)
    {
        FVL_LOG("open error:channel name error.\n");
        return -1;
    }
    port_num=srio_ctable_context[fd].port;
    if(port_mode[port_num]==0)
    {
    	  fd=fvl_srio_channel_open_master(fd);
    	  return fd;
    }
    else if(port_mode[port_num]==1)
    {
      	fd=fvl_srio_channel_open_slave(fd);
      	return fd;
    }
    else
    {
        FVL_LOG("This program do not support this mode!\n");
        return -1;    	
    } 	
}
int fvl_srio_channel_open_slave(int fd)
{
    int rvl=0,i=0;
    fvl_srio_context_t  *psrio = &g_srio_context;
    fvl_srio_portpool_t *cpool;
    fvl_srio_portpool_t *ppool;
    fvl_srio_ctrlblk_t *pscb;
    int port_num=0,ctl_size=0,bfnum=0;
    uint32_t offset=0,ctl_offset=0;
    
    port_num=srio_ctable_context[fd].port;
    bfnum = srio_ctable_context[fd].chan;
    if(bfnum > psrio->chan_num[port_num])
    {
        FVL_LOG("open error:channel not exist.\n");
        return -1; 
    }
    FVL_LOG("************************************************\n");
    FVL_LOG("port_num:%d uflag:%d\n",port_num,port_uflag[port_num]);
    while(1)
    {
        fflush(stdout);
        if(port_uflag[port_num])
        {
            break;
        }
    }
    rese_num[fd]= srio_channel_context[fd].buf_num;

//end mode master
    FVL_LOG("*****after while ****Head_size:%d rese_num:%lu \n",HEAD_SIZE[port_num],rese_num[fd]);

    ctl_size = FVL_CTL_WIN_SIZE;
    ctl_offset=ctl_size*bfnum;
    int j=port_num*FVL_PORT_CHAN_NUM_MAX;

    for(i=0;i<bfnum;i++)
    {
        offset=offset+srio_channel_context[(j+i)].chan_size;
    }
    FVL_LOG("ctl_offset:%08x offset:%08x\n",ctl_offset,offset);
 
    fvl_srio_channel_t *temp_channel;

    temp_channel=&srio_channel_context[fd];
    cpool=&(temp_channel->chanpool);
    pscb=&(temp_channel->chanblk);

    ppool = &psrio->portpool[port_num];

    cpool->write_result = ppool->write_result+offset;
    cpool->pwrite_result = ppool->pwrite_result+offset;
         
    cpool->write_ctl_result = ppool->write_ctl_result+FVL_CTL_HEAD_SIZE+ctl_offset;
    cpool->write_ctl_data = ppool->write_ctl_data+FVL_CTL_HEAD_SIZE+ctl_offset;
        
    cpool->pwrite_ctl_result = ppool->pwrite_ctl_result+FVL_CTL_HEAD_SIZE+ctl_offset;
    cpool->pwrite_ctl_data = ppool->pwrite_ctl_data+FVL_CTL_HEAD_SIZE+ctl_offset;
// very important
    cpool->port_info.range_start=ppool->port_info.range_start+offset;
    cpool->ctl_info_start = ppool->ctl_info_start+FVL_CTL_HEAD_SIZE+ctl_offset;
    
    uint64_t dest_phys,src_phys;
    FVL_LOG("##### channel:%d Slave receive ctl_head info!\n",fd);

    dest_phys=cpool->ctl_info_start;
    src_phys=cpool->write_ctl_data;
    
    head_channel_response[fd].cmd_ack= FVL_INIT_CHAN_READY;

    FVL_LOG("Head size:%08x FVL:%08x cmd_ack:%08x\n",HEAD_SIZE[port_num],FVL_CTL_HEAD_SIZE,head_channel_response[fd].cmd_ack);

    memcpy(cpool->pwrite_ctl_data,&head_channel_response[fd],sizeof(head_channel_response[fd]));
    fvl_srio_send(pscb->dmadev,src_phys,dest_phys,256);

    FVL_LOG("##### channel:%d Slave reback ctl_head info!\n",fd);

// add
    ctl_re_arg[fd].fd=fd;
    ctl_re_arg[fd].port_num=port_num;
    ctl_re_arg[fd].cpu=fd+3;
    FVL_LOG("RD_OP_CMD:%d\n",rd_op_base[port_num]);
    ctl_re_arg[fd].buf_virt=cpool->pwrite_ctl_result+rd_op_base[port_num]*FVL_CTL_PACKET_SIZE;

    rvl = pthread_create(&(temp_channel->chan_id), NULL,fvl_srio_recv_ctl, &ctl_re_arg[fd]);
    if (rvl) 
    {
        FVL_LOG("Create receive packet thread error!\n");
        return -errno;
    }

    return fd;
}

int fvl_srio_channel_open_master(int fd)
{
    int rvl=0,i=0;
    fvl_srio_context_t  *psrio = &g_srio_context;
    fvl_srio_portpool_t *cpool;
    fvl_srio_portpool_t *ppool;
    fvl_head_thread_t head_p_arg;
    int port_num=0,ctl_size=0,bfnum=0;
    uint32_t offset=0,ctl_offset=0;
    
    port_num= srio_ctable_context[fd].port;
    bfnum   = srio_ctable_context[fd].chan;
//mode master
    FVL_LOG("port num:%d uflag:%d\n",port_num,port_uflag[port_num]);
    FVL_LOG("*********************************\n");
    while(1)
    {
        fflush(stdout);
        if(port_uflag[port_num])
        {
            break;
        }
    }
    FVL_LOG("chan_fid:%d,bfnum:%d port_num:%d \n",fd,bfnum,port_num);
    if(bfnum > psrio->chan_num[port_num])
    {
        FVL_LOG("open error:channel not exist.\n");
        return -1; 
    }
    
    rese_num[fd]= srio_channel_context[fd].buf_num;


//end mode master
    FVL_LOG("*****after while ****Head_size:%d rese_num:%lu \n",HEAD_SIZE[port_num],rese_num[fd]);
    
    ctl_size = FVL_CTL_WIN_SIZE;
    ctl_offset=ctl_size*bfnum;
    int j=port_num*FVL_PORT_CHAN_NUM_MAX;
    for(i=0;i<bfnum;i++)
    {
        offset=offset+srio_channel_context[(j+i)].chan_size;
    }
    FVL_LOG("*********ctl_offset: %08x offset:%08x\n**********",ctl_offset,offset);

    fvl_srio_channel_t *temp_channel;
    temp_channel=&srio_channel_context[fd];
    cpool=&(temp_channel->chanpool);
    ppool = &psrio->portpool[port_num];
    cpool->write_result = ppool->write_result+offset;
    cpool->pwrite_result = ppool->pwrite_result+offset;
         
    cpool->write_ctl_result = ppool->write_ctl_result+FVL_CTL_HEAD_SIZE+ctl_offset;
    cpool->write_ctl_data = ppool->write_ctl_data+FVL_CTL_HEAD_SIZE+ctl_offset;
        
    cpool->pwrite_ctl_result = ppool->pwrite_ctl_result+FVL_CTL_HEAD_SIZE+ctl_offset;
    cpool->pwrite_ctl_data = ppool->pwrite_ctl_data+FVL_CTL_HEAD_SIZE+ctl_offset;
// very important
    cpool->port_info.range_start=ppool->port_info.range_start+offset;
    cpool->ctl_info_start = ppool->ctl_info_start+FVL_CTL_HEAD_SIZE+ctl_offset;

    if(second_handshake[port_num])
    {
        head_p_arg.num = fd;
        head_p_arg.op_mode = 1;
        head_p_arg.buf_virt=cpool->pwrite_ctl_result;
        rvl=fvl_srio_recv_head_master(&head_p_arg);
        if(rvl!=0)
        {
            FVL_LOG("Host:channel %d  is not ready!\n",fd);
            return -1;
        }
// add 

        while(!channel_uflag[fd]);
        FVL_LOG("channel:%d Host Receive Ctl head info!\n",fd);
    }
//end master


    ctl_re_arg[fd].fd=fd;
    ctl_re_arg[fd].port_num=port_num;
    ctl_re_arg[fd].cpu=fd+3;
    FVL_LOG("RD_OP_CMD:%d\n",rd_op_base[port_num]);
    ctl_re_arg[fd].buf_virt=cpool->pwrite_ctl_result+rd_op_base[port_num]*FVL_CTL_PACKET_SIZE;

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
    FVL_LOG("port:%d bfnum:%d num:%d\n",port,bfnum,num); 
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
    int port_num=0;
    uint32_t buf_size=0,buf_num=0;
    uint32_t offset=0;
    uint32_t step=0;
    
    port_num = srio_ctable_context[fd].port;
    buf_num  = srio_channel_context[fd].buf_num;
    buf_size = srio_channel_context[fd].buf_size;
    
    uint32_t ctl_offset=wr_op_base[port_num]*FVL_CTL_PACKET_SIZE;
    
    ctl_se_arg.fd=fd;
    ctl_se_arg.port_num=port_num;
    ctl_se_arg.buf_virt=cpool->pwrite_ctl_result+ctl_offset;

    fvl_srio_rese_ctl(&ctl_se_arg);

    step=length/buf_size;
    if((length%buf_size))
    {
        step=step+1;
    } 
    if((send_num[fd]+step)>=rese_num[fd])
    {
        pcnt=(fvl_srio_ctl_info_t *)(cpool->pwrite_ctl_data+ctl_offset);
        pcnt->status=1;
        if(offset_mode[port_num])
        {
            pcnt->write_point=send_num[fd];
        }
        else
        {
            pcnt->write_point=(send_num[fd]%buf_num);
        }
        dest_phys=(uint64_t )(cpool->ctl_info_start+ctl_offset);
        src_phys =(uint64_t )(cpool->write_ctl_data+ctl_offset);
        fvl_srio_send(pscb->dmadev,src_phys,dest_phys,HEAD_SIZE[port_num]);
        return -1;
    }

    offset=buf_size*(send_num[fd]%buf_num);
    dest_phys=(uint64_t )(cpool->port_info.range_start+offset);
    src_phys = phys; 
    fvl_srio_send(pscb->dmadev,src_phys,dest_phys,length);
    
    send_num[fd]=send_num[fd]+step;
    pcnt=(fvl_srio_ctl_info_t *)(cpool->pwrite_ctl_data+ctl_offset);
    pcnt->status=0;
    if(offset_mode[port_num])
    {
        pcnt->write_point=send_num[fd];
    }
    else
    {
        pcnt->write_point=(send_num[fd]%buf_num);
    }
    
    dest_phys=(uint64_t )(cpool->ctl_info_start+ctl_offset);
    src_phys =(uint64_t )(cpool->write_ctl_data+ctl_offset);
    fvl_srio_send(pscb->dmadev,src_phys,dest_phys,HEAD_SIZE[port_num]);
    
    return 0;
}


//
int fvl_srio_read(int fd,fvl_read_rvl_t *rvl)
{
    uint32_t num=0,length=rvl->len;
    uint32_t offset=0;
    fvl_srio_portpool_t *cpool;
    fvl_srio_channel_t *temp_channel;
    void *buf_virt=NULL;
    temp_channel=&srio_channel_context[fd];
    int buf_size=0,buf_num=0,packet_num=0;
    cpool=&(temp_channel->chanpool);
 //   port_num=srio_ctable_context[fd].port;
    buf_num  = srio_channel_context[fd].buf_num;
    buf_size = srio_channel_context[fd].buf_size;
    
    offset=buf_size*(read_num[fd]%buf_num);
//
//important
    num=receive_num[fd] - read_num[fd];
    if(num > buf_num)
    {
        FVL_LOG("data overflow!\n");
        return -1;	
    }
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
    buf_num  = srio_channel_context[fd].buf_num;
    
    uint32_t ctl_offset=rd_op_base[port_num]*FVL_CTL_PACKET_SIZE;
    rfreeback_num[fd]=rfreeback_num[fd]+num;
    pcnt=(fvl_srio_ctl_info_t *)(cpool->pwrite_ctl_data+ctl_offset);
//need change ctl_info
    if(offset_mode[port_num])
    {
        pcnt->read_point=rfreeback_num[fd];
    }
    else
    {
        pcnt->read_point=(rfreeback_num[fd]%buf_num);	
    }
    dest_phys=(uint64_t )(cpool->ctl_info_start+ctl_offset);
    src_phys =(uint64_t )(cpool->write_ctl_data+ctl_offset);
    int Index=(fd%FVL_PORT_CHAN_NUM_MAX);
    if(Index==(FVL_PORT_CHAN_NUM_MAX-1))
    {
        pthread_mutex_lock(&mutex[port_num]); 
    }
    fvl_srio_send(pscb->dmadev,src_phys,dest_phys,HEAD_SIZE[port_num]);
    if(Index==(FVL_PORT_CHAN_NUM_MAX-1))
    {
        pthread_mutex_unlock(&mutex[port_num]); 
    }        
    return 0; 
}

int fvl_get_fiber_status(int port_num,char *data)
{
    fvl_srio_context_t  *psrio = &g_srio_context;
    fvl_srio_portpool_t *ppool;
    if((port_num >= FVL_SRIO_PORT_NUM) || (port_num <0))
    {
        FVL_LOG("port number error:%d\n",port_num);
        return -1;
    }
    ppool = &psrio->portpool[port_num];
    memcpy(data,ppool->pwrite_ctl_result+256,256);
    return 0;
}

/*
 *
 */
int fvl_fiber_op_cmd(int port_num,int cmd)
{
    int rvl=-1;
    fvl_head_thread_t head_op;
    if((port_num >= FVL_SRIO_PORT_NUM) || (port_num <0))
    {
        FVL_LOG("port number error:%d\n",port_num);
        return -1;
    }
    if(cmd==FVL_START_CMD)
    {
        if(SRIO_PORT_STATUS[port_num]==FVL_START_READY)
        {
            FVL_LOG("The port:%d has been start ready,don't need to be start again!\n",port_num); 	
            return -1; 
        }
    }
    else if(cmd == FVL_STOP_CMD)
    {
        if(SRIO_PORT_STATUS[port_num]==FVL_STOP_READY)
        {
            FVL_LOG("The port:%d has been stop ready,don't need to be stop again!\n",port_num); 	
            return -1;
        }
    }
    else
    {
        FVL_LOG("cmd op error:%d\n",cmd);
        return -1;        	
    }
    fvl_srio_context_t  *psrio = &g_srio_context;
    fvl_srio_portpool_t *ppool;
    fvl_srio_ctrlblk_t *pscb;
    ppool = &psrio->portpool[port_num];
    head_port[port_num].cmd=cmd;
    memcpy(ppool->pwrite_ctl_data,&head_port[port_num],256);
    
    uint64_t dest_phys,src_phys;
    src_phys=ppool->write_ctl_data;
    dest_phys=ppool->ctl_info_start;
    pscb=&psrio->ctrlblk[FVL_PORT_DMA_NUM*port_num+7];

    pthread_mutex_lock(&mutex[port_num]); 
    fvl_srio_send(pscb->dmadev,src_phys,dest_phys,256);
    pthread_mutex_unlock(&mutex[port_num]);

    head_op.num = port_num;
    head_op.op_mode = (cmd &0x0f);
    head_op.buf_virt=ppool->pwrite_ctl_result;
    rvl=fvl_srio_recv_head_master(&head_op);
    if(rvl!=0)
    {
        FVL_LOG("Host:Port %d  is not ready!\n",port_num);
        return -1;
    }
    return 0;	
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

    pool->dma_virt_base = __dma_mem_memalign(64, dma_size);
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

