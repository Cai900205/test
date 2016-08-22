#include <sys/time.h>
#include "ips.h"

extern pthread_mutex_t ips_zlog_lock;
extern int __ips_chan_stop(ips_pcctx_t* pcctx);
extern void __ips_chan_close(ips_pcctx_t* pcctx);

static void __ips_mb_dump_cmd_init(zlog_level ll, IPS_EPID epid, ips_mailbox_t* cmd, size_t pl_size)
{
    ips_pl_init_t* pl_init = (ips_pl_init_t*)cmd->payload;
    assert(pl_size >= sizeof(ips_pl_init_t));

    IPS_LOG(ll, epid, " payload for OPCODE_INIT@%p", cmd);
    IPS_LOG(ll, epid, " local: offset=0x%x, sec_num=%d, sec_sz=%u",
            pl_init->local.mm_offset,
            pl_init->local.sector_num,
            pl_init->local.sector_sz * IPS_SECTOR_SZ_MULTIPLIER);
    IPS_LOG(ll, epid, " remote: offset=0x%x, sec_num=%d, sec_sz=%u",
            pl_init->remote.mm_offset,
            pl_init->remote.sector_num,
            pl_init->remote.sector_sz * IPS_SECTOR_SZ_MULTIPLIER);

    return;
}

static void __ips_mb_dump_cmn(zlog_level ll, IPS_EPID epid, ips_mailbox_t* cmd, size_t pl_size)
{
    char* payload = (char*)(cmd->payload);
    IPS_LOG(ll, epid, " payload = 0x%08x 0x%08x 0x%08x 0x%08x", 
                    *(uint32_t*)(payload+0),
                    *(uint32_t*)(payload+4),
                    *(uint32_t*)(payload+8),
                    *(uint32_t*)(payload+12));
    return;
}

static void ips_mb_dump(zlog_level ll, IPS_EPID epid, ips_mailbox_t* cmd, size_t pl_size)
{
    pthread_mutex_lock(&ips_zlog_lock);
    IPS_LOG(ll, epid, "-------------------------------- dump cmd %p, pl_size=%zu", cmd, pl_size);
    IPS_LOG(ll, epid, " MAGIC  = 0x%x", cmd->hdr.magic);
    IPS_LOG(ll, epid, " SEQ    = 0x%x", cmd->hdr.seq);
    IPS_LOG(ll, epid, " OPCODE = 0x%x", cmd->hdr.opcode);
    switch(cmd->hdr.opcode) {
    case IPS_OPCMD_INIT:
        __ips_mb_dump_cmd_init(ll, epid, cmd, pl_size);
        break;
    default:
        __ips_mb_dump_cmn(ll, epid, cmd, pl_size);
    }
    IPS_LOG(ll, epid, "-------------------------------- %s", "dump done");
    pthread_mutex_unlock(&ips_zlog_lock);

    return;
}
static int ips_pkt_build_desc(struct dma_ch *dmadev, uint64_t src_pa, uint64_t dest_pa,size_t size,
       size_t num, uint64_t desc_pa, void *desc_va,int do_start )
{
    struct dma_link_setup_data *link_data;
    struct dma_link_dsc *link_desc;
    size_t i;

    link_data = (struct dma_link_setup_data *)malloc(sizeof(*link_data) * num);
    link_desc = (struct dma_link_dsc *)desc_va;

    for (i = 0; i < num; i++) {
        link_data[i].byte_count = size;
        link_data[i].src_addr = src_pa + (i * size);
        link_data[i].dst_addr = dest_pa + (i * size);
        link_data[i].dst_snoop_en = 1;
        link_data[i].src_snoop_en = 1;
        link_data[i].dst_nlwr = ips_mode.dma_use_nlwr;
        link_data[i].dst_stride_en = 0;
        link_data[i].src_stride_en = 0;
        link_data[i].dst_stride_dist = 0;
        link_data[i].src_stride_dist = 0;
        link_data[i].dst_stride_size = 0;
        link_data[i].src_stride_size = 0;
        link_data[i].err_interrupt_en = 0;
        link_data[i].seg_interrupt_en = 0;
        link_data[i].link_interrupt_en = 0;
    }

    fsl_dma_chain_link_build(link_data, link_desc, desc_pa, num);
    if (do_start) {
   	    fsl_dma_chain_basic_start(dmadev, link_data, desc_pa);
    }

    free(link_data);
    
    return 0;
}

static int __ips_pkt_write(struct dma_ch *dmadev, uint64_t src_pa, uint64_t dest_pa, size_t size)
{
    int rvl;

    IPS_LOGDEBUG(IPS_EPID_UNKNOWN, "fsl_dma_direct_start: dev=%p, src_pa=0x%lx, "
                 "desc_pa=0x%lx, size=%zu",
                 dmadev, src_pa, dest_pa, size);
    rvl = fsl_dma_direct_start(dmadev, src_pa, dest_pa, size);
    if (!rvl) {
        rvl = fsl_dma_wait(dmadev);
    }

    return (rvl);
}

ssize_t ips_data_write(ips_pcctx_t* pcctx, void* buf, size_t bufsize)
{
    if ((ips_mode.tx.use_wptr) && (ips_mode.tx.use_rptr)) {
        assert(pcctx->tx_inst.wptr - pcctx->tx_inst.rptr < pcctx->desc->sector_num);
    }
    size_t sector_sz = pcctx->desc->sector_sz;
    int ret;
    ssize_t sz_ret;

    IPS_RWPTR wptr = pcctx->tx_inst.wptr;
    IPS_RWPTR rptr = pcctx->tx_inst.rptr;
    uint32_t sec_idx = wptr % pcctx->desc->sector_num;
    int zero_copy = 1;

    pthread_mutex_lock(&pcctx->dmach_dxr_lock);

    if (buf < pcctx->tx_dxr_sector.va ||
        buf + bufsize > pcctx->tx_dxr_sector.va + IPS_MAX_TX_SECTOR_SZ) {
        memcpy(pcctx->tx_dxr_sector.va, buf, bufsize);
        zero_copy = 0;
        IPS_LOGWARN(IPS_GET_MYEPID(pcctx), "none zero-copy detected: buf=%zu@%p, va=%u@%p",
                        bufsize, buf, IPS_MAX_TX_SECTOR_SZ, pcctx->tx_dxr_sector.va);
        assert(0);
    }
    IPS_LOGDEBUG(IPS_GET_MYEPID(pcctx), "ips_data_write: pc_id=%d, "
                 "wptr=0x%lx, rptr=0x%lx, sec_idx=%d, pl=%zu@%p, first=[%08x]",
                 pcctx->pc_id, wptr, rptr, sec_idx,
                 bufsize, pcctx->tx_dxr_sector.va,
                 *(uint32_t*)pcctx->tx_dxr_sector.va);
    size_t  write_num = (bufsize/sector_sz);
    if (!ips_mode.dma_use_chain) {
        IPS_LOGINFO(IPS_GET_MYEPID(pcctx), "ips_data_write: direct_dma_use_chain=%d",ips_mode.dma_use_chain);
        ret = __ips_pkt_write(pcctx->dmach_dxr,
                          pcctx->tx_dxr_sector.pa + (zero_copy?(buf - pcctx->tx_dxr_sector.va):0),
                          pcctx->swnd_dxr_pa_tbl[sec_idx],
                          bufsize);
    } else {
        IPS_LOGINFO(IPS_GET_MYEPID(pcctx), "ips_data_write: chan_dma_use_chain=%d",ips_mode.dma_use_chain);
        IPS_RWPTR desc_wptr = pcctx->desc_inst.wptr;
        uint32_t desc_idx = desc_wptr % pcctx->desc_num;
        size_t  write_num = (bufsize/sector_sz);
        int do_start = 0;
        if (desc_wptr == 0) {
            do_start = 1;
        } else {
            do_start = 0;
        }
        ret = ips_pkt_build_desc(pcctx->dmach_dxr,
                          pcctx->tx_dxr_sector.pa + (zero_copy?(buf - pcctx->tx_dxr_sector.va):0),
                          pcctx->swnd_dxr_pa_tbl[desc_idx],
                          sector_sz, write_num, pcctx->desc_sector.pa + (desc_idx*IPS_DESC_SIZE),
                          pcctx->desc_sector.va + (desc_idx * IPS_DESC_SIZE), do_start);

        if (!do_start) {
            uint32_t update_desc_idx=(desc_wptr -1) % pcctx->desc_num;
            ips_dma_change_desc_eaddr(pcctx->desc_sector.va+(update_desc_idx * IPS_DESC_SIZE),
                                pcctx->desc_sector.pa+(desc_idx * IPS_DESC_SIZE));
        }
        ips_dma_enable_cc(pcctx->dmach_dxr);

        pcctx->desc_inst.wptr += write_num;
    }
    if (ret < 0) {
        sz_ret = ret;
    } else {
        sz_ret = bufsize;
    }
    pcctx->tx_inst.wptr += write_num;
    pthread_mutex_unlock(&pcctx->dmach_dxr_lock);

    return(sz_ret);
}

ssize_t ips_mb_write(ips_pcctx_t* pcctx, uint64_t repo_offset,
                  int opcode, void* payload, size_t pl_size)
{
    assert(pl_size <= IPS_MAX_MAILBOX_PLSIZE);

    ips_mailbox_t* mb = pcctx->tx_ctr_pcb.va + repo_offset;
    int ret;

    pthread_mutex_lock(&pcctx->dmach_ctr_lock);

    mb->hdr.magic = IPS_MAGIC;
    mb->hdr.seq = 0;
    mb->hdr.opcode = opcode;
    if (pl_size > 0) {
        memcpy(mb->payload, payload, pl_size);
    }

    IPS_LOGDEBUG(IPS_GET_MYEPID(pcctx), "ips_mb_write: pc_id=%d, repo_offset=0x%lx, "
                "opcode=0x%x, pl=%zu@%p",
                pcctx->pc_id, repo_offset, opcode, pl_size, payload);
//    ips_mb_dump(ZLOG_LEVEL_DEBUG, pcctx->epctx->desc.epid, cmd, pl_size);
    ret = __ips_pkt_write(pcctx->dmach_ctr,
                          pcctx->tx_ctr_pcb.pa + repo_offset,
                          pcctx->swnd_ctr_pcb_pa + repo_offset,
                          sizeof(ips_mailbox_t));
    pthread_mutex_unlock(&pcctx->dmach_ctr_lock);

    return((ret==0)?IPS_MAX_MAILBOX_PLSIZE:0);
}

ssize_t ips_mb_read(ips_pcctx_t* pcctx, uint64_t repo_offset, ips_mailbox_t* mb_out)
{
    volatile ips_mailbox_t*   pcxr = pcctx->rx_ctr_pcb_va + repo_offset;

    assert(pcxr);

    if (pcxr->hdr.magic == IPS_MAGIC) {
        memcpy(mb_out, (ips_mailbox_t*)pcxr, sizeof(ips_mailbox_t));
        pcxr->hdr.magic = 0; // clear
        IPS_LOGDEBUG(IPS_GET_MYEPID(pcctx), "ips_mb_read: pc_id=%d, opcode=0x%x",
                    pcctx->pc_id, mb_out->hdr.opcode);
        ips_mb_dump(ZLOG_LEVEL_DEBUG, IPS_GET_MYEPID(pcctx),
                     mb_out, IPS_MAX_MAILBOX_PLSIZE);
        return(IPS_MAX_MAILBOX_PLSIZE);
    }

    return (0);
}

int ips_cmd_dispatch(ips_pcctx_t* pcctx, int opcode, void* payload, size_t pl_size, ips_mailbox_t* ack_cmd)
{
    int ret = -1;

    // send command
    ssize_t access_sz = ips_mb_write(pcctx, IPS_OFFSET_CMDREPO_IN_PCB, opcode, payload, pl_size);
    if (access_sz <= 0) {
        return(SPKERR_EACCESS);
    }
    // wait for ack
    uint64_t tm_timeout = spk_get_tick_count() + 5*1000; // 5seconds
    do {
        access_sz = ips_mb_read(pcctx, IPS_OFFSET_CMDREPO_IN_PCB, ack_cmd);
        if (access_sz > 0 && ack_cmd->hdr.opcode == (opcode | IPS_OPTYPE_ACK<<8)) {
            ret = SPK_SUCCESS;
            break;
        }
        if (access_sz < 0) {
            ret = SPKERR_EACCESS;
            break;
        }
        if (spk_get_tick_count() > tm_timeout) {
            ret = SPKERR_TIMEOUT;
            break;
        }
        usleep(100);
    } while(1);
    return(ret);
}

static int __ips_cmd_exec_init(ips_pcctx_t* pcctx, ips_pl_init_t* pl_init, ips_mailbox_t* cmdout)
{
    int ret = 0;
    ips_pcdesc_t* pcdesc = pcctx->desc;
    
    __ips_chan_close(pcctx);

    ips_cls_def_t* clsdef = &pl_init->local;
    assert(clsdef->mm_offset);
    pcdesc->sector_num = clsdef->sector_num;
    pcdesc->sector_sz = clsdef->sector_sz * IPS_SECTOR_SZ_MULTIPLIER;

    pcctx->rx_dxr_offset = pl_init->remote.mm_offset;
    ips_chan_init_dxrrx(pcctx);
    pcctx->tx_dxr_offset = pl_init->local.mm_offset;
    ips_chan_init_dxrtx(pcctx);

    ips_chan_init_desc(pcctx);
    pcctx->desc_num = (IPS_MAX_TX_SECTOR_SZ/pcctx->desc->sector_sz);

    ips_chan_shift_phase(pcctx, IPS_PHASE_IDLE);

    // construct ACK
    ips_pl_init_r_t* init_r = (ips_pl_init_r_t*)cmdout->payload;
    memset(init_r, 0, sizeof(ips_pl_init_r_t));
    init_r->capacity = 0x0; // TBD
    
    return(ret);
}

static int __ips_cmd_exec_start(ips_pcctx_t* pcctx,
                         ips_pl_start_t* pl_start, ips_mailbox_t* cmdout)
{
    int ret = 0;
    ips_pl_start_r_t* start_r = (ips_pl_start_r_t*)cmdout->payload;

    assert (pcctx->phase == IPS_PHASE_IDLE);
    memset(start_r, 0, sizeof(ips_pl_start_r_t));

    __ips_chan_stop(pcctx);

    if (pl_start->dir == SPK_DIR_READ) {
        memset(&pcctx->rx_inst, 0, sizeof(ips_dx_inst_t));
        ips_chan_shift_phase(pcctx, IPS_PHASE_TX);
    } else if (pl_start->dir == SPK_DIR_WRITE) {
        memset(&pcctx->tx_inst, 0, sizeof(ips_dx_inst_t));
        ips_chan_shift_phase(pcctx, IPS_PHASE_RX);
    } else {
        start_r->status = SPKERR_BADSEQ;
        return(SPK_SUCCESS);
    }

    start_r->status = 0x0;

    return(ret);
}

static int __ips_cmd_exec_stop(ips_pcctx_t* pcctx)
{
    __ips_chan_stop(pcctx);

    return(SPK_SUCCESS);
}

int ips_cmd_execute(ips_pcctx_t* pcctx, ips_mailbox_t* cmd_in)
{
    ips_mailbox_t cmd_out;
    int out_opcode = 0;
    size_t payload_size = 0;
    int ret = -1;

    pthread_mutex_lock(pcctx->ctx_lock);
    memset(&cmd_out, 0, sizeof(ips_mailbox_t));
    IPS_LOGNOTICE(IPS_GET_MYEPID(pcctx),
                 "execute cmd: opcode=%s",
                 ips_desc_opcode2str(cmd_in->hdr.opcode));
    switch(cmd_in->hdr.opcode) {
    case IPS_OPCMD_INIT:
        ret = __ips_cmd_exec_init(pcctx, (ips_pl_init_t*)cmd_in->payload, &cmd_out);
        out_opcode = IPS_OPACK_INIT;
        payload_size = sizeof(ips_pl_init_r_t);
        break;
    case IPS_OPCMD_START:
        ret = __ips_cmd_exec_start(pcctx, (ips_pl_start_t*)cmd_in->payload, &cmd_out);
        out_opcode = IPS_OPACK_START;
        payload_size = sizeof(ips_pl_start_r_t);
        break;
    case IPS_OPCMD_STOP:
        ret = __ips_cmd_exec_stop(pcctx);
        out_opcode = IPS_OPACK_STOP;
        payload_size = 0;
        break;
    default:
        IPS_LOGERROR(IPS_GET_MYEPID(pcctx),
                     "Unknown opcode: opcode=0x%x\n",
                     cmd_in->hdr.opcode);
        break;
    }

    if (ret == SPK_SUCCESS && out_opcode) {
        ssize_t access_sz = ips_mb_write(pcctx, IPS_OFFSET_CMDREPO_IN_PCB,
                            out_opcode, cmd_out.payload, payload_size);
        if (access_sz <= 0) {
            ret = SPKERR_EACCESS;
        }
    }
    pthread_mutex_unlock(pcctx->ctx_lock);

    return(ret);
}


