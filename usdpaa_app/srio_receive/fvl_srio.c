
#include "fvl_srio.h"

fvl_srio_context_t g_srio_context;

static const uint32_t srio_test_win_attrv[] = {3, 4, 5, 4, 0};

int fvl_srio_init(fvl_srio_context_t **ppsrio, uint32_t srio_type)
{
    fvl_srio_context_t  *psrio = &g_srio_context;
    fvl_srio_portpool_t *ppool;
    struct srio_dev *sriodev;
    fvl_dma_pool_t *dmapool = NULL;
    fvl_dma_pool_t *dmapool_ctl=NULL;
    
    int port_num;
    int rvl;
    int i,j;

    of_init();
    rvl = fsl_srio_uio_init(&sriodev);
    if (rvl < 0) {
        FVL_LOG("%s(): fsl_srio_uio_init return %d\n", __func__, rvl);
        return rvl;
    }
        
    port_num = fsl_srio_get_port_num(sriodev);
    if(port_num > FVL_SRIO_PORTNUM) {
        port_num = FVL_SRIO_PORTNUM;
    }

    for (i = 0; i < port_num; i++) {
//    for (i = 0; i < 1; i++) {
        struct srio_port_info *pinfo;
        void *range;

        pinfo = &psrio->portpool[i].port_info;
        fsl_srio_connection(sriodev, i);
        if (fsl_srio_port_connected(sriodev) & (0x1 << i)) {
            fsl_srio_get_port_info(sriodev, i+1, pinfo, &range);
            psrio->portpool[i].range_virt = range;
            FVL_LOG("Get port %u info, range=%p, range_start=%llx\n", i, range, pinfo->range_start);
        } else {
            FVL_LOG("%s(): fsl_srio_connection port %d failed\n", __func__, i);
        }
    }

    rvl = fsl_srio_port_connected(sriodev);
    if (rvl <= 0) {
        FVL_LOG("%s(): fsl_srio_port_connected return %d\n", __func__, rvl);
	    fsl_srio_uio_finish(sriodev);
	    of_finish();
        return -1;
    }
/*    rvl = fsl_srio_set_targetid(sriodev,0,1,0x11);
    if(rvl !=0)
    {
        printf("set targetid error!\n");
        return -1;
    }
    rvl = fsl_srio_set_targetid(sriodev,0,2,0x11);
    if(rvl !=0)
    {
        printf("set targetid error!\n");
        return -1;
    }
    rvl = fsl_srio_set_targetid(sriodev,1,1,0x14);
    if(rvl !=0)
    {
        printf("set targetid error!\n");
        return -1;
    }
    rvl = fsl_srio_set_targetid(sriodev,1,2,0x14);
    if(rvl !=0)
    {
        printf("set targetid error!\n");
        return -1;
    }
*/

    rvl = dma_pool_init(&dmapool,FVL_SRIO_DMA_POOLSIZE,FVL_SRIO_DMA_POOL_PORT_OFFSET);
    rvl = dma_pool_init(&dmapool_ctl,FVL_SRIO_CTL_POOLSIZE,FVL_SRIO_CTL_POOL_PORT_OFFSET);//ctx add

    for (i = 0; i < port_num; i++) {
        dma_addr_t phy_base;
	    dma_addr_t phy_base_ctl;
   	    uint8_t   *vir_base;
	    uint8_t   *vir_base_ctl;
	    uint32_t attr_read, attr_write;

        attr_read = srio_test_win_attrv[3];
        attr_write = srio_test_win_attrv[srio_type];
        printf("attr_write = %u, srio_type = %u\n", attr_write, srio_type);

        ppool = &psrio->portpool[i];

        phy_base = dmapool->dma_phys_base + FVL_SRIO_DMA_POOL_PORT_OFFSET * i;
	    phy_base_ctl = dmapool_ctl->dma_phys_base+FVL_SRIO_CTL_POOL_PORT_OFFSET*i;//ctxadd

	    for(j=0;j<FVL_SRIO_BUFFER_NUMBER;j++)//ctx add
   	    {
        	ppool->write_result[j] = phy_base+FVL_SRIO_DMA_BUFSIZE*j;
		    ppool->write_ctl_result[j] = phy_base_ctl +FVL_SRIO_CTL_BUFSIZE*j;
		    ppool->write_ctl_data[j] = phy_base_ctl + FVL_SRIO_CTL_WINSIZE+FVL_SRIO_CTL_BUFSIZE*j;
	    }
        vir_base = dmapool->dma_virt_base + FVL_SRIO_DMA_POOL_PORT_OFFSET * i;
	    vir_base_ctl = dmapool_ctl->dma_virt_base+FVL_SRIO_CTL_POOL_PORT_OFFSET*i;
	    for(j=0;j<FVL_SRIO_BUFFER_NUMBER;j++)
        {	
		    ppool->pwrite_result[j] = vir_base+FVL_SRIO_DMA_BUFSIZE*j;
		    ppool->pwrite_ctl_result[j] = vir_base_ctl +FVL_SRIO_CTL_BUFSIZE*j;
		    ppool->pwrite_ctl_data[j] = vir_base_ctl + FVL_SRIO_CTL_WINSIZE+FVL_SRIO_CTL_BUFSIZE*j;
	    }
        FVL_LOG("Port %d: virt base=%p, phys base=%llx\n", i, vir_base, phy_base);
        fsl_srio_set_ibwin(sriodev, i, 1, ppool->write_result[0],
                FVL_SRIO_SYS_ADDR, LAWAR_SIZE_64M);
        fsl_srio_set_ibwin(sriodev, i, 2, ppool->write_ctl_result[0],
                FVL_SRIO_CTL_ADDR, LAWAR_SIZE_4M);

        if (fsl_srio_port_connected(sriodev) & (0x1 << i)) {
//data
            fsl_srio_set_obwin(sriodev, i, 1,
                    ppool->port_info.range_start,
                    FVL_SRIO_SYS_ADDR, LAWAR_SIZE_64M);
            fsl_srio_set_obwin_attr(sriodev, i, 1,
                    attr_read, attr_write);
//ctl
            fsl_srio_set_obwin(sriodev, i, 2,
                    ppool->port_info.range_start+FVL_SRIO_DMA_WINSIZE,
                    FVL_SRIO_CTL_ADDR, LAWAR_SIZE_4M);
            fsl_srio_set_obwin_attr(sriodev, i, 2,
                    attr_read, attr_write);
        } else {
            printf("SRIO port %d error!\n", i + 1);
            return -errno;
        }
        memset(vir_base, 0, FVL_SRIO_DMA_POOL_PORT_OFFSET);
	memset(vir_base_ctl, 0, FVL_SRIO_CTL_POOL_PORT_OFFSET);
    }
    rvl = fsl_srio_set_targetid(sriodev,0,1,0x11);
    if(rvl !=0)
    {
        printf("set targetid error!\n");
        return -1;
    }
    rvl = fsl_srio_set_targetid(sriodev,0,2,0x11);
    if(rvl !=0)
    {
        printf("set targetid error!\n");
        return -1;
    }
    rvl = fsl_srio_set_targetid(sriodev,1,1,0x14);
    if(rvl !=0)
    {
        printf("set targetid error!\n");
        return -1;
    }
    rvl = fsl_srio_set_targetid(sriodev,1,2,0x14);
    if(rvl !=0)
    {
        printf("set targetid error!\n");
        return -1;
    }

    *ppsrio = psrio;
    return 0;
}

fvl_srio_ctrlblk_t * fvl_srio_getcb(uint32_t port, uint32_t bfnum)
{
    fvl_srio_ctrlblk_t *pscb;
    int rvl;
    int num;
    num = port * FVL_SRIO_DMA_CHANTNUM+bfnum;
    if((num >= FVL_SRIO_CBMAX) || (port >= FVL_SRIO_PORTNUM))
        return NULL;

    pscb = &g_srio_context.ctrlblk[num];

    rvl = fsl_dma_chan_init(&pscb->dmadev, port, bfnum);	
    if (rvl < 0) {
        FVL_LOG("%s(): fsl_dma_chan_init() return %d, args: %u, %u\n", __func__, rvl, port, bfnum);
        return NULL;
    }

    fsl_dma_chan_basic_direct_init(pscb->dmadev);
    fsl_dma_chan_bwc(pscb->dmadev, DMA_BWC_1024);

    pscb->bfnum = bfnum;
    pscb->port = port;

    return pscb;
}

int fvl_srio_send(fvl_srio_ctrlblk_t *pscb, uint64_t src_phys, uint64_t dest_phys, uint32_t size)
{
    struct dma_ch *dmadev;
    int rvl;

    dmadev = pscb->dmadev;
    fsl_dma_direct_start(dmadev, src_phys, dest_phys,size);
    rvl = fsl_dma_wait(dmadev);
    if (rvl < 0) {
        FVL_LOG("Thread %u port %u: dma task error!\n", pscb->bfnum, pscb->port);
        return -1;
    }

    return 0;
}

int dma_usmem_init(fvl_dma_pool_t *pool,uint64_t pool_size,uint64_t dma_size)
{
	int err;

	dma_mem_generic = dma_mem_create(DMA_MAP_FLAG_ALLOC,
					NULL, pool_size);
	if (!dma_mem_generic) {
		err = -EINVAL;
		error(0, -err, "%s(): dma_mem_create()", __func__);
		return err;
	}

	pool->dma_virt_base = __dma_mem_memalign(4096, dma_size);
	if (!pool->dma_virt_base) {
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
	if (!dma_pool) {
		error(0, errno, "%s(): DMA pool", __func__);
		return -errno;
	}
	memset(dma_pool, 0, sizeof(*dma_pool));
	*pool = dma_pool;

	err = dma_usmem_init(dma_pool,pool_size,dma_size);
	if (err < 0) {
		error(0, -err, "%s(): DMA pool", __func__);
		free(dma_pool);
		return err;
	}

	return 0;
}


