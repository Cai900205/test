#include "ips.h"

static struct srio_dev *ips_sriodev = NULL;
pthread_mutex_t ips_dma_ch_lock;
pthread_mutex_t ips_zlog_lock;
static ips_epctx_t* ips_epctx_tbl[IPS_MAX_PORTS_IN_DEV] = {NULL};
zlog_category_t* ips_zc = NULL;
ips_mode_t ips_mode = {
    .ver = IPS_MODE_VERSION,
    .dma_use_nlwr = 0,
    .dma_use_chain = 0,
    .tx.use_wptr = 1,
    .tx.use_rptr = 1,
    .rx.use_wptr = 1,
    .rx.use_rptr = 1
};

static inline int get_order(size_t size)
{
    int order = -1;
    size_t sz_cpy = size;

    do {
        size >>= 1;
        order++;
    } while (size);

    if(unlikely(sz_cpy != (0x01 << order))) {
        IPS_LOGFATAL(IPS_EPID_UNKNOWN,
                     "get_order: size=%zu, order=%d",
                     sz_cpy, order);
        assert(0);
    }
    return order;
}

static const char* ips_desc_phase2str(IPS_PHASE phase)
{
    switch(phase) {
    case IPS_PHASE_UNKNOWN:
        return("PHASE_UNKNOWN");
        break;
    case IPS_PHASE_INIT:
        return("PHASE_INIT");
        break;
    case IPS_PHASE_IDLE:
        return("PHASE_IDLE");
        break;
    case IPS_PHASE_RX:
        return("PHASE_RX");
        break;
    case IPS_PHASE_TX:
        return("PHASE_TX");
        break;
    }
    assert(0);
    return(NULL);
}

const char* ips_desc_opcode2str(int opcode)
{
    switch(opcode) {
    case IPS_OPCMD_INIT:
        return("IPS_OPCMD_INIT");
        break;
    case IPS_OPCMD_START:
        return("IPS_OPCMD_START");
        break;
    case IPS_OPCMD_STOP:
        return("IPS_OPCMD_STOP");
        break;
    case IPS_OPCMD_CONFIG:
        return("IPS_OPCMD_CONFIG");
        break;
    case IPS_OPACK_INIT:
        return("IPS_OPACK_INIT");
        break;
    case IPS_OPACK_START:
        return("IPS_OPACK_START");
        break;
    case IPS_OPACK_STOP:
        return("IPS_OPACK_STOP");
        break;
    case IPS_OPACK_CONFIG:
        return("IPS_OPACK_CONFIG");
        break;
    case IPS_OPNTF_DMADONE:
        return("IPS_OPNTF_DMADONE");
        break;
    case IPS_OPNTF_FREEBUF:
        return("IPS_OPNTF_FREEBUF");
        break;
    case IPS_OPBL1_FBSTATUS:
        return("IPS_OPBL1_FBSTATUS");
        break;
    }
    return("UNKNOWN");
}

IPS_PHASE ips_chan_get_phase(ips_pcctx_t* pcctx)
{
    IPS_PHASE phase = pcctx->phase;
    return(phase);
}

void ips_chan_shift_phase(ips_pcctx_t* pcctx, IPS_PHASE new_phase)
{
    if (pcctx->phase != new_phase) {
        IPS_LOGNOTICE(IPS_GET_MYEPID(pcctx), "*** shift phase: pc=%d, %s->%s",
                        pcctx->pc_id,
                        ips_desc_phase2str(pcctx->phase),
                        ips_desc_phase2str(new_phase));
        pcctx->phase = new_phase;
    }
    return;
}

static int __ips_obwin_bind(IPS_EPID epid, int obwin_id, int dest_id,
                            dma_addr_t swnd_pa, uint64_t dest_offset, int wnd_sz)
{
    int rvl = -1;

    int port_id = IPS_EPID_2_PORTID(epid);

    // bind obwin
    rvl = fsl_srio_set_obwin(ips_sriodev, port_id, obwin_id,
                             swnd_pa, dest_offset, wnd_sz);
    IPS_LOGINFO(epid, "fsl_srio_set_obwin: port_id=%d, obwin_id=%d, "
                "swnd_pa=0x%lx, offset=0x%lx, wnd_sz=%d, ret=%d",
                port_id, obwin_id, swnd_pa, dest_offset, wnd_sz, rvl);

    // set obwin attributes
    rvl = fsl_srio_set_obwin_attr(ips_sriodev, port_id, obwin_id,
                                  SRIO_ATTR_NREAD, SRIO_ATTR_SWRITE);
    IPS_LOGINFO(epid, "fsl_srio_set_obwin_attr: obwin_id=%d, "
                "rd_attr=%d, wr_attr=%d, ret=%d",
                obwin_id, SRIO_ATTR_NREAD, SRIO_ATTR_SWRITE, rvl);
    assert(!rvl);

    // set obwin target id
    rvl = fsl_srio_set_targetid(ips_sriodev, port_id, obwin_id, dest_id);
    IPS_LOGINFO(epid, "fsl_srio_set_targetid: port_id=%d, "
                "obwin_id=%d, epid=0x%x, ret=%d",
                port_id, obwin_id, dest_id, rvl);
    assert(!rvl);

    return(rvl);
}

typedef struct ips_worker_args {
    int dev_id;
    int port_id;
    int pc_id;
} ips_worker_args_t;

// calculate cluster size
static inline size_t ips_chan_get_clssize(ips_pcctx_t* pcctx)
{
    return(pcctx->desc->sector_num * pcctx->desc->sector_sz);
}

int ips_chan_init_dxrrx(ips_pcctx_t* pcctx)
{
    ips_epctx_t* epctx = pcctx->epctx;
    IPS_EPID epid = IPS_GET_MYEPID(pcctx);
    int port_id = IPS_EPID_2_PORTID(epid);
    uint64_t offset = 0l;
    int i, pc;
    int ret;

    assert(pcctx->rx_dxr_offset);

    for (pc = 0; pc < pcctx->pc_id; pc++) {
        ips_pcctx_t* ctx;
        ctx = &epctx->pcctx_tbl[pc];
        offset += ips_chan_get_clssize(ctx);
    }
    size_t cls_sz = ips_chan_get_clssize(pcctx);
    assert(cls_sz <= IPS_MM_DXREG_SIZE);

    // alloc dx_region for rx
    pcctx->rx_dxr_base.va = ips_dmamem_alloc(cls_sz, &pcctx->rx_dxr_base.pa, &pcctx->rx_dxr_base.dma_mem);
    IPS_LOGNOTICE(epid, "Alloc RX_DXR_CLUSTER: size=%zu, va=%p, pa=0x%lx",
                  cls_sz, pcctx->rx_dxr_base.va, pcctx->rx_dxr_base.pa);
    assert(pcctx->rx_dxr_base.va);

    int wnd_sz = get_order(cls_sz) - 1;
    uint64_t mm_offset = pcctx->rx_dxr_offset;
    int win_id = IPS_IBID_DXREG + pcctx->pc_id;
    ret = fsl_srio_set_ibwin(ips_sriodev, port_id, win_id,
                             pcctx->rx_dxr_base.pa + offset, mm_offset + offset, wnd_sz);
    IPS_LOGNOTICE(epid, "Bind RX_DXR_CLUSTER: ibwin_id=%d, "
                  "pa=0x%lx, offset=0x%lx, wnd_sz=%d, ret=%d",
                  win_id, pcctx->rx_dxr_base.pa + offset,
                  mm_offset, wnd_sz, ret);
    assert(!ret);

    int sec_num = pcctx->desc->sector_num;
    size_t sec_sz = pcctx->desc->sector_sz;
    assert(sec_sz <= IPS_MAX_SECTOR_SIZE);
    IPS_LOGINFO(epid, "Assign RX_DXR_CLUSTER: pc_id=%d, "
                "sec_num=%d, sec_sz=%zu, base_va=%p",
                pcctx->pc_id, sec_num, sec_sz, pcctx->rx_dxr_base.va);
    for (i = 0; i < sec_num; i++) {
        pcctx->rx_dxr_va_tbl[i] = pcctx->rx_dxr_base.va + i * sec_sz;
    }

    return(0);
}

int ips_chan_init_dxrtx(ips_pcctx_t* pcctx)
{
    ips_epctx_t* epctx = pcctx->epctx;
    IPS_EPID epid = IPS_GET_MYEPID(pcctx);
//    int port_id = IPS_EPID_2_PORTID(epid);
    size_t cls_sz = 0l;
    int ret;

    assert(pcctx->tx_dxr_offset);

    for (int pc = 0; pc < pcctx->pc_id; pc++) {
        ips_pcctx_t* ctx = &epctx->pcctx_tbl[pc];
        cls_sz += ips_chan_get_clssize(ctx);
    }

    // alloc tx_dxr_sector
    pcctx->tx_dxr_sector.va = ips_dmamem_alloc(IPS_MAX_TX_SECTOR_SZ,
                              &pcctx->tx_dxr_sector.pa,
                              &pcctx->tx_dxr_sector.dma_mem);
    IPS_LOGNOTICE(epid, "Alloc TX_DXR_SECTOR: pc_id=%d, size=%d, "
                "va=%p, pa=0x%lx",
                pcctx->pc_id, IPS_MAX_TX_SECTOR_SZ,
                pcctx->tx_dxr_sector.va, pcctx->tx_dxr_sector.pa);
    assert(pcctx->tx_dxr_sector.va);

    // bind tx_dxr_sector to obwin
    pcctx->swnd_dxr_pa_base = epctx->tx_swnd_base.pa +
             IPS_SRIOWND_DXR_BASE + cls_sz;
    // swnd_offset must be size aligned
    assert(!((pcctx->swnd_dxr_pa_base - epctx->tx_swnd_base.pa) % ips_chan_get_clssize(pcctx)));
    uint64_t remote_offset = pcctx->tx_dxr_offset + cls_sz;
    int wnd_sz = get_order(ips_chan_get_clssize(pcctx)) - 1;
    int obwin_id = IPS_OBID_DXREG + pcctx->pc_id;
    IPS_LOGNOTICE(epid, "Bind TX_DXR_SECTOR: pc_id=%d, win_id=%d, "
                "swnd_pa=0x%lx, offset=0x%lx, wnd_sz=%d",
                pcctx->pc_id, obwin_id, pcctx->swnd_dxr_pa_base,
                remote_offset, wnd_sz);
    ret = __ips_obwin_bind(epid, obwin_id, pcctx->desc->dest_id,
                           pcctx->swnd_dxr_pa_base, remote_offset, wnd_sz);
    assert(!ret);

    // calculate sector remote offsets in cluster
    uint64_t offset = 0l;
    int sec_num = pcctx->desc->sector_num;
    size_t sec_sz = pcctx->desc->sector_sz;
    IPS_LOGINFO(epid, "Assign TX_DXR_CLUSTER: pc_id=%d, "
                "sec_num=%d, sec_sz=%zu, base_swnd=0x%lx",
                pcctx->pc_id, sec_num, sec_sz, pcctx->swnd_dxr_pa_base);

    for (int i = 0; i < sec_num; i++) {
        pcctx->swnd_dxr_pa_tbl[i] = pcctx->swnd_dxr_pa_base + i*sec_sz;
        offset += sec_sz;
    }

    return(SPK_SUCCESS);
}
int ips_chan_init_desc(ips_pcctx_t *pcctx)
{
    IPS_EPID epid = IPS_GET_MYEPID(pcctx);
    // alloc desc_sector
    pcctx->desc_sector.va = ips_dmamem_alloc(IPS_DESC_SPACE_SIZE,
                              &pcctx->desc_sector.pa,
                              &pcctx->desc_sector.dma_mem);
    IPS_LOGNOTICE(epid, "Alloc TX_DSC_SECTOR: pc_id=%d, size=%d, "
                "va=%p, pa=0x%lx",
                pcctx->pc_id, IPS_DESC_SPACE_SIZE,
                pcctx->desc_sector.va, pcctx->desc_sector.pa);
    assert(pcctx->desc_sector.va);

    pcctx->desc_base = pcctx->desc_sector.pa;

    return(SPK_SUCCESS);
}

void* __ips_daemon_slave(void* args)
{
    ips_worker_args_t* worker_args = (ips_worker_args_t*)args;

    int port_id = worker_args->port_id;
    int pc_id = worker_args->pc_id;

    ips_epctx_t* epctx = ips_epctx_tbl[port_id];
    ips_pcctx_t* pcctx = &epctx->pcctx_tbl[pc_id];
    IPS_EPID epid = IPS_GET_MYEPID(pcctx);

    SAFE_RELEASE(args);

    IPS_LOGNOTICE(epid, "spawn slave daemon: pc_id=%d", pc_id);

    while(1) {
        ips_mailbox_t cmd_in;
        ssize_t access_sz = ips_mb_read(pcctx, IPS_OFFSET_CMDREPO_IN_PCB, &cmd_in);
        if (access_sz > 0) {
            ips_cmd_execute(pcctx, &cmd_in);
            continue;
        }
        // no cmd, process data
        usleep(100);
    }

    IPS_LOGNOTICE(epid, " terminate slave daemon: pc_id=%d", pc_id);
    return(NULL);
}

int ips_ep_init(IPS_EPID epid, const ips_epdesc_t* epdesc)
{
    int rvl = 0;
    int i;

    int port_id = IPS_EPID_2_PORTID(epid);
    int dev_id = IPS_EPID_2_DEVID(epid);
    int mode = IPS_EPID_2_MODE(epid);

    IPS_LOGINFO(epid, "ips_ep_init: mode=%s, port_id=%d",
                mode ? "master" : "slave", port_id);

    assert(epdesc);

    // srio_dev struct must be initialized in init_module()
    assert(ips_sriodev);

    // check port_id
    if(!IPS_IS_PORT_VALID(port_id)) {
        zlog_error(ips_zc, "Invalid port id: port_id=%d\n", port_id);
        assert(0);
    }
    assert(!ips_epctx_tbl[port_id]);

    // check chan_num
    if(!IPS_IS_PCID_VALID(epdesc->pc_num)) {
        zlog_error(ips_zc, "Invalid chan number: num=%d\n", epdesc->pc_num);
        assert(0);
    }

    // create ep context
    ips_epctx_tbl[port_id] = malloc(sizeof(ips_epctx_t));
    ips_epctx_t* epctx = ips_epctx_tbl[port_id];
    assert(epctx);
    memset(epctx, 0, sizeof(ips_epctx_t));

    // preserve epdesc
    memcpy(&epctx->desc, epdesc, sizeof(ips_epdesc_t));

    // test connection
    fsl_srio_connection(ips_sriodev, port_id);
    if (!(fsl_srio_port_connected(ips_sriodev) & (0x1 << port_id))) {
        assert(0);
    }

    // set source_id, etc.
    rvl = fsl_srio_set_deviceid(ips_sriodev, port_id, epid);
    IPS_LOGDEBUG(epid, "fsl_srio_set_deviceid: port_id=%d, epid=0x%x",
                 port_id, epid);
    assert(!rvl);
    fsl_srio_set_err_rate_degraded_threshold(ips_sriodev, port_id, 0);
    fsl_srio_set_err_rate_failed_threshold(ips_sriodev, port_id, 0);
    fsl_srio_set_phy_retry_threshold(ips_sriodev, port_id, 0, 0);

    // acquire swnd
    struct srio_port_info srio_wnd;
    fsl_srio_get_port_info(ips_sriodev, port_id + 1, &srio_wnd, &epctx->tx_swnd_base.va);
    epctx->tx_swnd_base.pa = srio_wnd.range_start;
    IPS_LOGINFO(epid, "Acquire SWND: start=0x%lx, size=0x%lx, va=%p",
                epctx->tx_swnd_base.pa, srio_wnd.range_size, epctx->tx_swnd_base.va);
    assert(srio_wnd.range_size == IPS_SRIOWND_SIZE_ALL); // 256M

    // alloc ctrl region for rx
    epctx->rx_ctr_base.va = ips_dmamem_alloc(IPS_MM_CTLREG_SIZE,
                                             &epctx->rx_ctr_base.pa,
                                             &epctx->rx_ctr_base.dma_mem);
    IPS_LOGNOTICE(epid, "alloc RX_CTR: size=%d, va=%p, pa=0x%lx",
                  IPS_MM_CTLREG_SIZE, epctx->rx_ctr_base.va, epctx->rx_ctr_base.pa);
    assert(epctx->rx_ctr_base.va);

    // bind ctrl region to ibwin
    int wnd_sz = get_order(IPS_MM_CTLREG_SIZE) - 1;
    uint64_t offset = IPS_MM_CTLREG_BASE;
    int win_id = IPS_IBID_CTLREG;
    rvl = fsl_srio_set_ibwin(ips_sriodev, port_id, win_id,
                             epctx->rx_ctr_base.pa, offset, wnd_sz);
    IPS_LOGNOTICE(epid, "bind RX_CTR: ibwin_id=%d, "
                  "pa=0x%lx, offset=0x%lx, wnd_sz=%d, ret=%d",
                  win_id, epctx->rx_ctr_base.pa,
                  offset, wnd_sz, rvl);
    assert(!rvl);

    for (i = 0; i < IPS_MAX_PCS_IN_EP; i++) {
        // set rx_pcb/pcxr/vcbs in each CHAN
        ips_pcctx_t* pcctx = &epctx->pcctx_tbl[i];
        pcctx->pc_id = i;
        pcctx->epctx = epctx;

        pcctx->rx_ctr_pcb_va = epctx->rx_ctr_base.va + IPS_OFFSET_PCB_IN_CTLREG(i);
        pcctx->desc = &epctx->desc.pcdesc_tbl[i];
        pcctx->desc->src_id = epid;
    }

    // initialize chan contexts: tx part
    for (i = 0; i < epdesc->pc_num; i++) {
        ips_pcctx_t* pcctx = &epctx->pcctx_tbl[i];
        size_t pcb_size = IPS_SIZE_PCB;

        // alloc tx_ctr_pcb
        pcctx->tx_ctr_pcb.va = ips_dmamem_alloc(pcb_size,
                                 &pcctx->tx_ctr_pcb.pa,
                                 &pcctx->tx_ctr_pcb.dma_mem);
        IPS_LOGINFO(epid, "Alloc TX_CTR_PCB: pc_id=%d, size=%zu, "
                    "va=%p, pa=0x%lx",
                    i, pcb_size, pcctx->tx_ctr_pcb.va, pcctx->tx_ctr_pcb.pa);
        assert(pcctx->tx_ctr_pcb.va);

        // bind tx_ctr_pcb to obwin
        uint64_t remote_offset = IPS_MM_CTLREG_BASE + IPS_OFFSET_PCB_IN_CTLREG(i);
        pcctx->swnd_ctr_pcb_pa = epctx->tx_swnd_base.pa + IPS_SRIOWND_CTR_BASE + pcb_size * i;
        assert(!((pcctx->swnd_ctr_pcb_pa - epctx->tx_swnd_base.pa) % pcb_size));
        wnd_sz = get_order(pcb_size) - 1;
        win_id = IPS_OBID_CTLREG + i;
        IPS_LOGINFO(epid, "Bind TX_CTR_PCB: pc_id=%d, win_id=%d, "
                    "swnd_pa=0x%lx, offset=0x%lx, wnd_sz=%d",
                    i, win_id, pcctx->swnd_ctr_pcb_pa, remote_offset, wnd_sz);
        rvl = __ips_obwin_bind(epid, win_id, epctx->desc.pcdesc_tbl[i].dest_id,
                               pcctx->swnd_ctr_pcb_pa, remote_offset, wnd_sz);
        assert(!rvl);

        // initialize DMA ch
        pthread_mutex_init(&pcctx->dmach_ctr_lock, NULL);
        rvl = fsl_dma_chan_init(&pcctx->dmach_ctr, port_id, i/*dma_ch_id*/);
        IPS_LOGDEBUG(epid, "fsl_dma_chan_init: pc_id=%d, "
                     "id=%d, dev=%p, ret=%d",
                     i, i, pcctx->dmach_ctr,  rvl);
        assert(!rvl);
        int dma_use_nlwr = ips_mode.dma_use_nlwr;
        ips_dma_chan_basic_set_mode(dma_use_nlwr);
        fsl_dma_chan_basic_direct_init(pcctx->dmach_ctr);
        fsl_dma_chan_bwc(pcctx->dmach_ctr, DMA_BWC_1024);

        // initialize DMA ch
        pthread_mutex_init(&pcctx->dmach_dxr_lock, NULL);
        rvl = fsl_dma_chan_init(&pcctx->dmach_dxr, port_id, i + IPS_MAX_PCS_IN_EP);
        IPS_LOGDEBUG(epid, "fsl_dma_chan_init: pc_id=%d, id=%d, dev=%p, ret=%d",
                     i, i + IPS_MAX_PCS_IN_EP, pcctx->dmach_dxr, rvl);
        assert(!rvl);
        fsl_dma_chan_basic_direct_init(pcctx->dmach_dxr);
        fsl_dma_chan_bwc(pcctx->dmach_dxr, DMA_BWC_1024);
    }

    // connecting ...
    rvl = 0;
    for (i = 0; i < epdesc->pc_num; i++) {
        ips_pcctx_t* pcctx = &epctx->pcctx_tbl[i];
        pcctx->ctx_lock = malloc(sizeof(pthread_mutex_t));
        pthread_mutex_init(pcctx->ctx_lock, NULL);
        pcctx->open_cnt = 0;
        ips_chan_shift_phase(pcctx, IPS_PHASE_INIT);
        if(mode == IPS_EPMODE_SLAVE) {
            // spawn daemon thread
            pcctx->worker_thread = malloc(sizeof(pthread_t));
            ips_worker_args_t* worker_args = (ips_worker_args_t*)malloc(sizeof(ips_worker_args_t));
            worker_args->dev_id = dev_id;
            worker_args->port_id = port_id;
            worker_args->pc_id = i;
            rvl = pthread_create(pcctx->worker_thread, NULL, __ips_daemon_slave, (void*)worker_args);
        }
    }

    return (SPK_SUCCESS);
}

void* ips_dmamem_alloc(size_t pool_size, dma_addr_t* pa, struct dma_mem** dma_mem)
{
    void* va = NULL;
    IPS_LOGDEBUG(IPS_EPID_UNKNOWN, "%s: size=%zu", __func__, pool_size);

    pthread_mutex_lock(&ips_dma_ch_lock);

    // FIXME: need protection
    dma_mem_generic = dma_mem_create(DMA_MAP_FLAG_ALLOC,
                                     NULL, pool_size);
    assert(dma_mem_generic);
    *dma_mem = dma_mem_generic;

    va = __dma_mem_memalign(4*1024, pool_size / 2/*FIXME: why?*/);
    assert(va);

    *pa = __dma_mem_vtop(va);
    memset(va, 0, pool_size);

    pthread_mutex_unlock(&ips_dma_ch_lock);

    return(va);
}

int ips_module_init(const char* log_cat)
{
    int i;
    int ret;

    if (ips_zc) {
        fprintf(stderr, "%s: already initialized!\n", __func__);
        return(SPKERR_BADSEQ);
    }
    ips_zc = zlog_get_category(log_cat?log_cat:"IPS");
    if (!ips_zc) {
        fprintf(stderr, "%s: failed to initialize log system!\n", __func__);
//        return(SPKERR_LOGSYS);
    }

    assert(!ips_sriodev);

    for (i = 0; i < IPS_MAX_PORTS_IN_DEV; i++) {
        ips_epctx_tbl[i] = NULL;
    }

    pthread_mutex_init(&ips_dma_ch_lock, NULL);
    pthread_mutex_init(&ips_zlog_lock, NULL);

    of_init();
    ret = fsl_srio_uio_init(&ips_sriodev);
    IPS_LOGINFO(IPS_EPID_UNKNOWN, "fsl_srio_uio_init: ret=%d", ret);
    if (ret) {
        return(SPKERR_EAGAIN);
    }

    zlog_notice(ips_zc, "module initialized.");

    return(SPK_SUCCESS);
}

int ips_chan_config(ips_pcctx_t* pcctx, char* config_buf, size_t buf_sz, char **out_buf, size_t* out_sz)
{
    IPS_EPID epid = IPS_GET_MYEPID(pcctx);
    int pc_id = pcctx->pc_id;
    ips_mailbox_t ack;

    int ret = -1;
    assert(buf_sz <= sizeof(ips_pl_config_t));

    if (pcctx->phase != IPS_PHASE_IDLE) {
        zlog_error(ips_zc, " unexpected sequence: phase=%s", ips_desc_phase2str(pcctx->phase));
        ret = SPKERR_BADSEQ;
        goto out;
    }

    // construct PKT_CMD_CONFIG
    ips_pl_config_t   payload;
    memcpy(&payload, config_buf, buf_sz);
    ret = ips_cmd_dispatch(pcctx, IPS_OPCMD_CONFIG, &payload, sizeof(ips_pl_config_t), &ack);
    if (ret != SPK_SUCCESS) {
        IPS_LOGFATAL(epid, "failed to config channel: pc=%d, ret=%d", pc_id, ret);
        goto out;
    }
    size_t pl_sz = (sizeof(ips_mailbox_t) - sizeof(ips_mb_header_t));
    *out_sz = pl_sz;
    *out_buf = (char *)(malloc(pl_sz)); 
    memcpy(*out_buf, ack.payload, pl_sz);
    IPS_LOGNOTICE(epid, "channel config done: pc=%d", pc_id);
    ret = SPK_SUCCESS;

out:
    return(ret);
}

int ips_chan_start(ips_pcctx_t* pcctx, SPK_DIR dir)
{
    IPS_EPID epid = IPS_GET_MYEPID(pcctx);
    int pc_id = pcctx->pc_id;
    int mode = IPS_EPID_2_MODE(epid);
    ips_mailbox_t ack;

    int ret = -1;

    if (mode != IPS_EPMODE_MASTER) {
        zlog_error(ips_zc, " unexpected sequence: phase=%s", ips_desc_phase2str(pcctx->phase));
        ret = SPKERR_BADSEQ;
        goto out;
    }
    assert(dir == SPK_DIR_READ || dir == SPK_DIR_WRITE);

    if (pcctx->phase != IPS_PHASE_IDLE) {
        zlog_error(ips_zc, " unexpected sequence: phase=%s", ips_desc_phase2str(pcctx->phase));
        ret = SPKERR_BADSEQ;
        goto out;
    }

    // start DX
    if (dir == SPK_DIR_READ) {
        ips_chan_shift_phase(pcctx, IPS_PHASE_RX);
    } else {
        ips_chan_shift_phase(pcctx, IPS_PHASE_TX);
    }
    pcctx->dir = dir;

    IPS_LOGNOTICE(epid, "start data transfer: pc=%d", pc_id);
    // construct PKT_CMD_START
    ips_pl_start_t   payload;
    memset(&payload, 0, sizeof(ips_pl_start_t));
    payload.dir = dir;
    ret = ips_cmd_dispatch(pcctx, IPS_OPCMD_START, &payload, sizeof(ips_pl_start_t), &ack);
    if (ret != SPK_SUCCESS) {
        IPS_LOGFATAL(epid, "start data transfer failed: pc_id=%d, ret=%d", pc_id, ret);
        goto out;
    }
    IPS_LOGNOTICE(epid, "data transfer started: pc=%d", pc_id);
    ret = SPK_SUCCESS;

out:
    return(ret);    
}

static int __ips_chan_open_master(ips_pcctx_t* pcctx)
{
    int ret;
    ips_epctx_t* epctx = pcctx->epctx;
    IPS_EPID epid = IPS_GET_MYEPID(pcctx);
    int pc_id = pcctx->pc_id;
    ips_mailbox_t ack;
    
    // must be in IPS_PHASE_INIT
    // if corrently closed last time
    assert(ips_chan_get_phase(pcctx) == IPS_PHASE_INIT);
    
    if (pcctx->phase == IPS_PHASE_INIT) {
        size_t cls_sz = 0l;
        for (int pc = 0; pc < pc_id; pc++) {
            ips_pcctx_t* ctx = &epctx->pcctx_tbl[pc];
            cls_sz += ips_chan_get_clssize(ctx);
        }
        pcctx->rx_dxr_offset = IPS_MM_DXREG_BASE + cls_sz;
        ips_chan_init_dxrrx(pcctx);
        pcctx->tx_dxr_offset = IPS_MM_DXREG_BASE + cls_sz; // TBD: equals to local
        ips_chan_init_dxrtx(pcctx);
    
        ips_chan_init_desc(pcctx);
        pcctx->desc_num = (IPS_MAX_TX_SECTOR_SZ/pcctx->desc->sector_sz);
        // handshake
        // construct PKT_CMD_INIT
        IPS_LOGNOTICE(epid, "start handshake: pc=%d", pc_id);
        ips_pl_init_t   hs1_payload;
        memset(&hs1_payload, 0, sizeof(ips_pl_init_t));
        ips_cls_def_t* clsdef = &hs1_payload.local;
        clsdef->mm_offset = pcctx->rx_dxr_offset;
        clsdef->sector_num = pcctx->desc->sector_num;
        clsdef->sector_sz  = pcctx->desc->sector_sz / IPS_SECTOR_SZ_MULTIPLIER;
    
        clsdef = &hs1_payload.remote;
        clsdef->mm_offset = pcctx->tx_dxr_offset;
        clsdef->sector_num = pcctx->desc->sector_num;
        clsdef->sector_sz  = pcctx->desc->sector_sz / IPS_SECTOR_SZ_MULTIPLIER;
    
        ret = ips_cmd_dispatch(pcctx, IPS_OPCMD_INIT, &hs1_payload, sizeof(ips_pl_init_t), &ack);
        if (ret != SPK_SUCCESS) {
            IPS_LOGFATAL(epid, "failed to handshake: pc_id=%d, ret=%d", pc_id, ret);
            goto out;
        }
        IPS_LOGNOTICE(epid, "handshake done: pc=%d", pc_id);

        ips_chan_shift_phase(pcctx, IPS_PHASE_IDLE);
    }

    ret = SPK_SUCCESS;

out:
    if (ret != SPK_SUCCESS) {
        dma_mem_destroy(pcctx->rx_dxr_base.dma_mem);
        pcctx->rx_dxr_base.va = NULL;
        dma_mem_destroy(pcctx->tx_dxr_sector.dma_mem);
        pcctx->tx_dxr_sector.va = NULL;
    }

    return(ret);
}

static int __ips_chan_open_slave(ips_pcctx_t* pcctx)
{
    // wait daemon for phase change
    while(1) {
        if (pcctx->phase == IPS_PHASE_RX) {
            pcctx->dir = SPK_DIR_READ;
            break;
        }
        if (pcctx->phase == IPS_PHASE_TX) {
            pcctx->dir = SPK_DIR_WRITE;
            break;
        }
        usleep(100);
    }
    
    return(SPK_SUCCESS);
}


ips_pcctx_t* ips_chan_open(IPS_EPID epid, int pc_id)
{
    ips_pcctx_t* pcctx = NULL;
    int ret;
    int mode = IPS_EPID_2_MODE(epid);

    IPS_LOGINFO(epid, "ips_chan_open: pc=%d", pc_id);

    int port_id = IPS_EPID_2_PORTID(epid);
    // check port_id
    if(!IPS_IS_PORT_VALID(port_id)) {
        IPS_LOGERROR(epid, "invalid port id: pc=%d\n", pc_id);
        assert(0);
    }

    // check pc_id
    if(pc_id || pc_id >= IPS_MAX_PCS_IN_EP) {
        IPS_LOGERROR(epid, "invalid pc_id: pc=%d\n", pc_id);
        assert(0);
    }

    ips_epctx_t* epctx = ips_epctx_tbl[port_id];
    pcctx = &epctx->pcctx_tbl[pc_id];
    
    pthread_mutex_lock(pcctx->ctx_lock);
    if (pcctx->open_cnt > 0) {
        IPS_LOGERROR(epid, "channel already opened: pc=%d", pc_id);
        pthread_mutex_unlock(pcctx->ctx_lock);
        return(NULL);
    }
    pcctx->open_cnt++;

    // initialize rx/tx instance
    memset(&pcctx->rx_inst, 0, sizeof(ips_dx_inst_t));
    memset(&pcctx->tx_inst, 0, sizeof(ips_dx_inst_t));
    memset(&pcctx->desc_inst, 0, sizeof(ips_dx_inst_t));
    spk_stats_reset(&pcctx->stats_xfer);

    // clear rx/tx stat repo
    ips_mailbox_t stat;
    ips_mb_read(pcctx,
                IPS_OFFSET_STATREPO_IN_PCB + IPS_OFFSET_RXSTAT_IN_REPO, &stat);
    ips_mb_read(pcctx,
                IPS_OFFSET_STATREPO_IN_PCB + IPS_OFFSET_TXSTAT_IN_REPO, &stat);

    if (mode == IPS_EPMODE_MASTER) {
        ret = __ips_chan_open_master(pcctx);
        if (ret != SPK_SUCCESS) {
            pcctx->open_cnt--;
            pthread_mutex_unlock(pcctx->ctx_lock);
            pcctx = NULL;
        } else {
            pthread_mutex_unlock(pcctx->ctx_lock);
        }
    } else {
        // must unlock before __ips_chan_open_slave
        // daemon thread want this lock to execute cmds
        pthread_mutex_unlock(pcctx->ctx_lock);
        ret = __ips_chan_open_slave(pcctx);
    }

    return(pcctx);
}

ssize_t ips_chan_write(ips_pcctx_t* pcctx, void* buf, size_t bufsize)
{
    ips_dx_inst_t* dx_inst = &pcctx->tx_inst;
    IPS_EPID    epid = IPS_GET_MYEPID(pcctx);
    ssize_t stat_sz;
    ips_mailbox_t tx_stat;
    ssize_t ret_sz = 0;
    
    pthread_mutex_lock(pcctx->ctx_lock);

    if (pcctx->phase != IPS_PHASE_TX &&
        pcctx->phase != IPS_PHASE_IDLE) {
        ret_sz = SPKERR_BADSEQ;
        goto out;
    }

    size_t sector_sz = pcctx->desc->sector_sz;
    int sec_num_total = pcctx->desc->sector_num;
    int sec_num_req = bufsize / sector_sz;
    int sec_num_free;

    if (ips_mode.tx.use_rptr) {
        sec_num_free = sec_num_total - (dx_inst->wptr - dx_inst->rptr);
        if (sec_num_free < sec_num_req) {
        // not enough free buffer, update rptr to try again
            stat_sz = ips_mb_read(pcctx,
                                    IPS_OFFSET_STATREPO_IN_PCB + IPS_OFFSET_TXSTAT_IN_REPO, &tx_stat);
            if (stat_sz > 0) {
                ips_pl_sync_t* plsync = (ips_pl_sync_t*)tx_stat.payload;
                dx_inst->rptr = plsync->rptr;
                sec_num_free = sec_num_total - (dx_inst->wptr - dx_inst->rptr);
            }
        }

        if (sec_num_free < sec_num_req) {
            ret_sz = 0;
            goto out;
            IPS_LOGWARN(epid, "not enough buffer: "
                         "wptr=0x%lx, rptr=0x%lx, total=%d, free_buf=%d, expect=%d",
                         dx_inst->wptr,
                         dx_inst->rptr,
                         sec_num_total,
                         sec_num_free,
                         sec_num_req);
        }
    }

    if (!ips_mode.dma_use_chain) {
        IPS_LOGINFO(epid, "ips_chan_write:direct_dma_use_chain=%d",ips_mode.dma_use_chain);
        for(int i = 0; i < sec_num_req; i++) {
            size_t written = ips_data_write(pcctx, buf + i * sector_sz, sector_sz);
            if (written != sector_sz) {
                ret_sz = written;
                goto out;
            }
            spk_stats_inc_xfer(&pcctx->stats_xfer, sector_sz, 1);
        }
        if (ips_mode.tx.use_wptr) {
            uint64_t repo_offset = IPS_OFFSET_STATREPO_IN_PCB + IPS_OFFSET_RXSTAT_IN_REPO;
            ips_pl_sync_t pl_sync;
            memset(&pl_sync, 0, sizeof(ips_pl_sync_t));
            pl_sync.wptr = pcctx->tx_inst.wptr;
            pl_sync.rptr = pcctx->tx_inst.rptr;
            ssize_t access_sz = ips_mb_write(pcctx, repo_offset,
                                     IPS_OPNTF_DMADONE,
                                     &pl_sync, sizeof(ips_pl_sync_t));
            if (access_sz <= 0) {
                ret_sz = SPKERR_EACCESS;
                goto out;
            }
        }
    } else {
        IPS_LOGINFO(epid, "ips_chan_write:chan_dma_use_chain=%d",ips_mode.dma_use_chain);
        size_t written = ips_data_write(pcctx, buf,bufsize);
        if (written != bufsize) {
             ret_sz = written;
             goto out;
        }
        spk_stats_inc_xfer(&pcctx->stats_xfer, bufsize, 1);
    }

    // success
    ret_sz = bufsize;

out:
    pthread_mutex_unlock(pcctx->ctx_lock);

    return(ret_sz);
}

ssize_t ips_chan_read(ips_pcctx_t* pcctx, void** pbuf, size_t min_size, size_t max_size)
{
    ips_dx_inst_t* dx_inst = &pcctx->rx_inst;
    IPS_EPID    epid = IPS_GET_MYEPID(pcctx);
    ssize_t stat_sz;
    ssize_t ret_sz = 0;
    int total_sec_num = pcctx->desc->sector_num;

    pthread_mutex_lock(pcctx->ctx_lock);

    if (pcctx->phase != IPS_PHASE_RX &&
        pcctx->phase != IPS_PHASE_IDLE) {
        ret_sz = SPKERR_BADSEQ;
        goto out;
    }

    // update wptr
    if (ips_mode.rx.use_wptr) {
        ips_mailbox_t rx_stat;
        stat_sz = ips_mb_read(pcctx,
                            IPS_OFFSET_STATREPO_IN_PCB + IPS_OFFSET_RXSTAT_IN_REPO, &rx_stat);

        if (stat_sz > 0) {
            ips_pl_sync_t* plsync = (ips_pl_sync_t*)rx_stat.payload;
            dx_inst->wptr = plsync->wptr;
        }
    }

    int work_sec_num = dx_inst->wptr - dx_inst->rptr;
    if (work_sec_num == 0) {
        ret_sz = 0;
        goto out;
    }

    if (work_sec_num < 0) {
        IPS_LOGFATAL(epid, "### OUT_OF_SEQ: src_id=0x%x, wptr=%lu, rptr=%lu",
                        pcctx->desc->src_id,
                        dx_inst->wptr,
                        dx_inst->rptr);
        ret_sz = SPKERR_BADSEQ;
        goto out;
    } else {
        if (work_sec_num > total_sec_num) {
            // overflow
            IPS_LOGFATAL(epid, "### OVERFLOW: src_id=0x%x, w/rptr=%lu/%lu, screwed=%d, force drop!",
                            pcctx->desc->src_id, 
                            dx_inst->wptr,
                            dx_inst->rptr,
                            work_sec_num);
            do {
                dx_inst->rptr += total_sec_num;
                work_sec_num -= total_sec_num;
                spk_stats_inc_overflow(&pcctx->stats_xfer,
                                         total_sec_num*pcctx->desc->sector_sz,
                                         total_sec_num);                            
            } while(work_sec_num > total_sec_num);
        }
    }
    
    uint32_t startsec_idx = dx_inst->rptr % total_sec_num;
    if (startsec_idx + work_sec_num > total_sec_num) {
        // prevent from wrap, leave remain sectors
        work_sec_num = total_sec_num - startsec_idx;
    }

    size_t work_data_sz = work_sec_num * pcctx->desc->sector_sz;
    if (min_size > 0) {
        assert(!(min_size % pcctx->desc->sector_sz));
        if(work_data_sz < min_size) {
            // not enough data
            ret_sz = 0;
            goto out;
        }
    }

    if (max_size > 0) {
        assert(!(max_size % pcctx->desc->sector_sz));
        if(work_data_sz > max_size) {
            work_data_sz = max_size;
            work_sec_num = max_size / pcctx->desc->sector_sz;
        }
    }

    IPS_LOGDEBUG(epid, "ips_chan_read: wptr=0x%lx, rptr=0x%lx, va=%p, size=%zu",
                 dx_inst->wptr, dx_inst->rptr,
                 pcctx->rx_dxr_va_tbl[startsec_idx],
                 work_data_sz)
    *pbuf = pcctx->rx_dxr_va_tbl[startsec_idx];
    ret_sz = work_data_sz;

    spk_stats_inc_xfer(&pcctx->stats_xfer, work_data_sz, work_sec_num);

out:
    pthread_mutex_unlock(pcctx->ctx_lock);

    return(ret_sz);
}

int ips_chan_free_buf(ips_pcctx_t* pcctx, size_t buf_sz)
{
    ips_dx_inst_t* dx_inst = &pcctx->rx_inst;

    assert(!(buf_sz % pcctx->desc->sector_sz));
    int buf_cnt = buf_sz / pcctx->desc->sector_sz;

    ips_pl_sync_t pl_sync;
    memset(&pl_sync, 0, sizeof(ips_pl_sync_t));

    pthread_mutex_lock(pcctx->ctx_lock);
    dx_inst->rptr += buf_cnt;
    pl_sync.rptr = dx_inst->rptr;
    pl_sync.wptr = dx_inst->wptr;
    if (ips_mode.rx.use_rptr) {
        ssize_t access_sz = ips_mb_write(pcctx,
                                     IPS_OFFSET_STATREPO_IN_PCB + IPS_OFFSET_TXSTAT_IN_REPO,
                                     IPS_OPNTF_FREEBUF,
                                     &pl_sync, sizeof(ips_pl_sync_t));
        pthread_mutex_unlock(pcctx->ctx_lock);

        if (access_sz <= 0) {
            return(SPKERR_EACCESS);
        }
    }

    return(SPK_SUCCESS);
}
int ips_chan_get_tx_freebn(ips_pcctx_t* pcctx,size_t buf_sz)
{
    size_t sector_sz = pcctx->desc->sector_sz;
    IPS_EPID    epid = IPS_GET_MYEPID(pcctx);

    uint32_t local_desc_pa = (pcctx->desc_base & 0xffffffff);
    uint32_t total_desc_num = pcctx->desc_num;
    uint32_t current_desc_pa = ips_dma_get_current_desc_addr(pcctx->dmach_dxr);
    size_t buf_cnt = buf_sz/sector_sz;
    size_t buf_num = (IPS_MAX_TX_SECTOR_SZ) / buf_sz;
    int free_num = 0;
    
    free_num = (((total_desc_num*IPS_DESC_SIZE)+current_desc_pa - local_desc_pa)
            / (buf_cnt * IPS_DESC_SIZE)) % buf_num;
    if (free_num > 0 ) {
        IPS_LOGINFO(epid,"current_desc_pa:%x,local_pa:%x buf_cnt:%lx", current_desc_pa, local_desc_pa, buf_cnt);
        pcctx->desc_inst.rptr += (free_num * buf_cnt);
        pcctx->desc_base = current_desc_pa;
        if (ips_mode.tx.use_wptr) {
            uint64_t repo_offset = IPS_OFFSET_STATREPO_IN_PCB + IPS_OFFSET_RXSTAT_IN_REPO;
            ips_pl_sync_t pl_sync;
            memset(&pl_sync, 0, sizeof(ips_pl_sync_t));
            pl_sync.wptr = pcctx->desc_inst.rptr;
            pl_sync.rptr = pcctx->tx_inst.rptr;
            ssize_t access_sz = ips_mb_write(pcctx, repo_offset,
                                     IPS_OPNTF_DMADONE,
                                     &pl_sync, sizeof(ips_pl_sync_t));
            if (access_sz <= 0) {
                IPS_LOGWARN(epid,"ips_mb_write update wptr failed:%lx",access_sz);
            }
        }
    }

    return free_num;
}

void* ips_chan_get_txbuf(ips_pcctx_t* pcctx, size_t* size)
{
    *size = IPS_MAX_TX_SECTOR_SZ;
    return(pcctx->tx_dxr_sector.va);
}

int ips_chan_get_chstat(ips_pcctx_t* pcctx, void* chstat, size_t* stat_size)
{
    ips_mailbox_t mb;
    
    pthread_mutex_lock(pcctx->ctx_lock);
    ssize_t access_sz = ips_mb_read(pcctx,
                                    IPS_OFFSET_STATREPO_IN_PCB + IPS_OFFSET_CHSTAT_IN_REPO,
                                    &mb);
    *stat_size = access_sz;
    pthread_mutex_unlock(pcctx->ctx_lock);
    if (access_sz <= 0) {
        return(SPKERR_EACCESS);
    }
    memcpy(chstat, mb.payload, access_sz);
    return(SPK_SUCCESS);
}

int ips_chan_set_chstat(ips_pcctx_t* pcctx, void* chstat, size_t stat_size)
{
    pthread_mutex_lock(pcctx->ctx_lock);
    ssize_t access_sz = ips_mb_write(pcctx,
                                     IPS_OFFSET_STATREPO_IN_PCB + IPS_OFFSET_CHSTAT_IN_REPO,
                                     IPS_OPBL1_FBSTATUS,
                                     chstat,
                                     stat_size);
    pthread_mutex_unlock(pcctx->ctx_lock);
    if (access_sz <= 0) {
        return(SPKERR_EACCESS);
    }
    return(SPK_SUCCESS);
}

static int __ips_chan_stop_master(ips_pcctx_t* pcctx)
{
    int ret;
    ips_mailbox_t ack;
    
    ret = ips_cmd_dispatch(pcctx, IPS_OPCMD_STOP, NULL, 0, &ack);

    // we do not care that the slave is still alive or not
    // just shift our phase to IDLE
    ips_chan_shift_phase(pcctx, IPS_PHASE_IDLE);

    return(ret);
}

static int __ips_chan_stop_slave(ips_pcctx_t* pcctx)
{
    ips_chan_shift_phase(pcctx, IPS_PHASE_IDLE);

    return(SPK_SUCCESS);
}

int __ips_chan_stop(ips_pcctx_t* pcctx)
{
    IPS_EPID    epid = IPS_GET_MYEPID(pcctx);
    int mode = IPS_EPID_2_MODE(epid);
    int ret = SPK_SUCCESS;
    
    if (ips_mode.dma_use_chain) { 
        fsl_dma_wait(pcctx->dmach_dxr);
    }
    IPS_PHASE phase = ips_chan_get_phase(pcctx);
    if (phase == IPS_PHASE_RX || phase == IPS_PHASE_TX) {
        if (mode == IPS_EPMODE_MASTER) {
            ret = __ips_chan_stop_master(pcctx);
        } else {
            ret = __ips_chan_stop_slave(pcctx);
        }
    
        IPS_LOGNOTICE(epid, "channel stopped: pc=%d, ret=%d", pcctx->pc_id, ret);
    }

    return(ret);
}

int ips_chan_stop(ips_pcctx_t* pcctx)
{
    int ret;
    
    pthread_mutex_lock(pcctx->ctx_lock);
    ret = __ips_chan_stop(pcctx);
    pthread_mutex_unlock(pcctx->ctx_lock);

    return(ret);
}

void __ips_chan_close(ips_pcctx_t* pcctx)
{
    IPS_EPID    epid = IPS_GET_MYEPID(pcctx);

    if (ips_chan_is_start(pcctx)) {
        // still running, close it first
        __ips_chan_stop(pcctx);
    }

    if (ips_chan_get_phase(pcctx) == IPS_PHASE_IDLE) {
        if (pcctx->rx_dxr_base.va) {
            dma_mem_destroy(pcctx->rx_dxr_base.dma_mem);
            pcctx->rx_dxr_base.va = NULL;
        }
        
        if (pcctx->tx_dxr_sector.va) {
            dma_mem_destroy(pcctx->tx_dxr_sector.dma_mem);
            pcctx->tx_dxr_sector.va = NULL;
        }
        
        if (pcctx->desc_sector.va) {
            dma_mem_destroy(pcctx->desc_sector.dma_mem);
            pcctx->desc_sector.va = NULL;
        }
        ips_chan_shift_phase(pcctx, IPS_PHASE_INIT);
        IPS_LOGNOTICE(epid, "channel closed: pc=%d", pcctx->pc_id);
    }
    return;
}

void ips_chan_close(ips_pcctx_t* pcctx)
{
    pthread_mutex_lock(pcctx->ctx_lock);
    __ips_chan_close(pcctx);
    pcctx->open_cnt--;
    assert(pcctx->open_cnt == 0);
    pthread_mutex_unlock(pcctx->ctx_lock);

    return;
}

int ips_chan_is_start(ips_pcctx_t* pcctx)
{
    return (ips_chan_get_phase(pcctx) == IPS_PHASE_RX ||
        ips_chan_get_phase(pcctx) == IPS_PHASE_TX);
}

SPK_DIR ips_chan_get_dir(ips_pcctx_t* pcctx)
{
    return(pcctx->dir);
}

spk_stats_t* ips_chan_get_stats(ips_pcctx_t* pcctx)
{
    return(&pcctx->stats_xfer);
}

int ips_init_mode(ips_mode_t mode)
{
    ips_mode.dma_use_nlwr = mode.dma_use_nlwr;
    ips_mode.dma_use_chain = mode.dma_use_chain;
    ips_mode.tx.use_wptr = mode.tx.use_wptr;
    ips_mode.tx.use_rptr = mode.tx.use_rptr;
    ips_mode.rx.use_wptr = mode.rx.use_wptr;
    ips_mode.rx.use_rptr = mode.rx.use_rptr;
    
    return 0;
}
