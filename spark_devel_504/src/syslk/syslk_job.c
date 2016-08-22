/*************************************************************************
    > File Name: syslk_job.c
    > Author:
    > Mail:
    > Created Time: Wed 12 Aug 2015 05:16:49 PM CST
 ************************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <malloc.h>
#include <unistd.h>
#include <string.h>
#include <inttypes.h>
#include <pthread.h>
#include <assert.h>
#include <sys/time.h>
#include "syslk.h"

static void sys_job_update_stats(struct ips_pcctx* pcctx, int wkr_id)
{
#ifdef ARCH_ppc64
    // get ch_stat
    size_t chstat_sz;
    int chstat_ret;
    char chstat[256];
    memset(chstat, 0, 256);
    chstat_ret = ips_chan_get_chstat(pcctx, chstat, &chstat_sz);
    if (chstat_ret == SPK_SUCCESS) {
        // got, update sys_ctx
        assert(chstat_sz >= sizeof(ips_fcstat_lk1));
        memcpy(&sys_ctx.fcstat[wkr_id], chstat, sizeof(ips_fcstat_lk1));
    }
#endif
    return;
}

static int sys_func_dl_prepare(int slot_id, uint64_t blk_start, uint64_t blk_num, uint32_t req_frag)
{
    int ret;
    sys_ctx_t* ctx = &sys_ctx;
    int repo_num = ctx->vault->repo_num;
    size_t cache_sz = SYS_CACHE_SIZE;
    int i, j;
    int interlace_sz = SYS_INTERLACE_SIZE;

    sys_cache_t* file_cache = ctx->file_cache;
    assert(file_cache);
    assert(file_cache->data);

    // get slot_sz
    ssize_t slot_sz;
    if (file_cache->slot_id == slot_id ) {
        slot_sz = file_cache->slot_sz;
    } else {
        slot_sz = dfv_vault_get_slotsize(ctx->vault, slot_id);
        if (slot_sz < 0) {
            zlog_error(sys_zc, "bad file size: slot_id=%d", slot_id);
            return(SPKERR_BADRES);
        }
    }

    // request region
    uint64_t req_start = blk_start * SYS_CACHE_SIZE;
    uint64_t req_end = (blk_num>0)?(req_start + blk_num * SYS_CACHE_SIZE):slot_sz;

    // frag region
    uint64_t frag_start = req_start + (uint64_t)req_frag * CMI_MAX_FRAGSIZE;
    uint64_t frag_end = frag_start + CMI_MAX_FRAGSIZE;

    // check if region is valid
    if (frag_start > slot_sz) {
        zlog_error(sys_zc, "illegal frag_start: slot_id=%d, slot_sz=%zu, "
                   "frag_start=%lu, req_frag=%u",
                   slot_id, slot_sz, frag_start, req_frag);
        return(SPKERR_PARAM);
    }

    if (frag_end > slot_sz) {
        zlog_warn(sys_zc, "truncate frag: slot_id=%d, slot_sz=%zu, "
                  "frag=%lu+%lu",
                  slot_id, slot_sz, frag_start, frag_end);
        frag_end = slot_sz;
    }

    // check cache hit
    int cache_hit = 0;
    do {
        if (file_cache->slot_id != slot_id) {
            // slot not match
            break;
        }
        if (frag_start < file_cache->offset ||
            frag_end > file_cache->offset + file_cache->len) {
            break;
        }
        cache_hit = 1;
    } while(0);

    if (cache_hit) {
        return(SPK_SUCCESS);
    }

    // do caching
    cache_sz = MIN(SYS_CACHE_SIZE, slot_sz - frag_start);
    size_t cache_sz_repo = cache_sz / repo_num;
    zlog_notice(sys_zc, "cache file: slot_id=%d, slot_sz=%zu, req_frag=%u, "
                "req=%lu+%lu, frag=%lu+%lu, cache_sz=%zu, blk=%lu+%lu",
                slot_id, slot_sz, req_frag,
                req_start, req_end-req_start,
                frag_start, frag_end-frag_start,
                cache_sz, blk_start, blk_num);

    struct dfv_file* file_ctx = NULL;
    void* file_buffer = memalign(SYS_INTERLACE_SIZE, cache_sz_repo);
    for (i=0; i<repo_num; i++) {
        struct dfv_repo * repo = ctx->vault->repo_tbl[i];
        assert(repo);

        file_ctx = dfv_file_open(repo, slot_id, SPK_DIR_READ, NULL, 12+4*i);
        if (!file_ctx) {
            ret = SPKERR_BADRES;
            goto out;
        }

        ret = dfv_file_seek(file_ctx, frag_start / repo_num);
        if (ret) {
            goto out;
        }

        ssize_t read_sz = dfv_file_read(file_ctx, file_buffer, cache_sz_repo);
        if (read_sz != cache_sz_repo) {
            ret = SPKERR_BADRES;
            goto out;
        }

        dfv_file_close(file_ctx);
        file_ctx = NULL;

        // do interlace while copy data
        for (j=0; j<cache_sz_repo/interlace_sz; j++) {
            memcpy(file_cache->data+(j*repo_num+i)*interlace_sz,
                   file_buffer+j*interlace_sz,
                   interlace_sz);
        }
    }
    // cache done
    file_cache->slot_id = slot_id;
    file_cache->offset = frag_start;
    file_cache->len = cache_sz;
    file_cache->slot_sz = slot_sz;
    zlog_notice(sys_zc, "cache done: slot_id=%d, slot_sz=%lu, cache=%lu+%lu",
                slot_id, slot_sz, file_cache->offset,
                file_cache->len);

    ret = SPK_SUCCESS;

out:
    if (file_ctx) {
        dfv_file_close(file_ctx);
        file_ctx = NULL;
    }
    SAFE_RELEASE(file_buffer);

    return(ret);
}

static int sys_func_dl_sendfrag(int slot_id, uint64_t blk_start,
                                uint64_t blk_num, uint32_t req_frag, int* eof_req)
{
    sys_cache_t* file_cache = sys_ctx.file_cache;
    uint64_t req_start = blk_start * SYS_CACHE_SIZE;
    uint64_t frag_start = req_start + (uint64_t)req_frag * CMI_MAX_FRAGSIZE;

    if (file_cache->slot_id != slot_id ||
        frag_start < file_cache->offset ) {
        assert(0);
    }

    uint64_t req_end = (blk_num>0)?(req_start + blk_num * SYS_CACHE_SIZE):
                       (file_cache->slot_sz);
    uint64_t frag_end = frag_start + CMI_MAX_FRAGSIZE;

    req_end = MIN(req_end, file_cache->slot_sz);
    frag_end = MIN(frag_end, req_end);
    int is_eof = (frag_end == req_end);
    if (eof_req && *eof_req) {
        is_eof |= *eof_req; // abort
    }

    char* obuf = file_cache->data + (frag_start - file_cache->offset);
    size_t obuf_sz = frag_end - frag_start;

    cmi_data_t msg_data;
    cmi_msg_build_datafrag(&msg_data, data_type_dl,
                           obuf,
                           obuf_sz,
                           req_frag,
                           is_eof);
    ssize_t written = cmi_intf_write_msg(sys_ctx.cmi_intf,
                                         &msg_data, sizeof(cmi_data_t));
    if (written != sizeof(cmi_data_t)) {
        return(SPKERR_RESETSYS);
    }
    zlog_info(sys_zc, "> dldata sent: req_frag=%u, frag=%lu+%lu, written=%ld, eof=%d",
              req_frag, frag_start, obuf_sz, written, is_eof);
    if (eof_req) {
        *eof_req = is_eof;
    }

    return(SPK_SUCCESS);
}

#define SYS_AUTOSTOP_TIMEOUT    (10) // second
static int __sys_job_do_record(sys_wkr_ctx_t* wkr_ctx,
                                 IPS_EPID src_id, int pc_id, size_t ips_sec_sz,
                                 dfv_slot_def_t* slot_def,
                                 dfv_slice_def_t* slice_def)
{
    int ret = -1;
#ifdef ARCH_ppc64
    struct ips_pcctx* pcctx = NULL;
    struct net_handle* file_ctx = NULL;

    int wkr_id = wkr_ctx->wkr_id;
    spk_stats_t* stats = &wkr_ctx->stats;

    void* chunk_buf = NULL;

	size_t chunk_size = slice_def->size * slice_def->num;
    
	int dfv_cpu_base = 12+4*wkr_ctx->wkr_id;

    zlog_notice(sys_zc, "wkr#%d> prepare for recording: ips={0x%x:%d}, dfv={repo={%d:%d}, "
                        "slice={%d, 0x%lx}, cpu_base=%d}",
                        wkr_id,
                        src_id, pc_id,
                        dfv_repo_get_id(slot_def->repo), slot_def->slot_id,
                        slice_def->num, slice_def->size,
                        dfv_cpu_base);
    // reset stats
    spk_stats_reset(stats);

    // open net slot
	if(wkr_id==1) {
        file_ctx = net_open("192.168.11.3",9720,4,dfv_cpu_base,SPK_DIR_WRITE,net_intf_tcp);
        if (!file_ctx) {
           zlog_fatal(sys_zc,"wkr#%d> failed to open net",wkr_id);
           ret = SPKERR_BADRES;
           goto out;
        }
	} else if(wkr_id ==0) {
        file_ctx = net_open("192.168.10.3",9734,4,dfv_cpu_base,SPK_DIR_WRITE,net_intf_tcp);
        if (!file_ctx) {
           zlog_fatal(sys_zc,"wkr#%d> failed to open net",wkr_id);
           ret = SPKERR_BADRES;
           goto out;
        }
	} else {
        zlog_fatal(sys_zc, "wkr#%d> error wkr_id", wkr_id);
        ret = SPKERR_EACCESS;
        goto out;
	}
    
    ret= net_intf_is_connected(file_ctx);
    if(!ret) {
        zlog_fatal(sys_zc,"wkr#:%d>net is not connected",wkr_id);
	    goto out;
    }
    // open ips srio
    pcctx = ips_chan_open(src_id, pc_id);
    if (!pcctx) {
        zlog_fatal(sys_zc, "wkr#%d> failed to open ips channel", wkr_id);
        ret = SPKERR_EACCESS;
        goto out;
    }

    ret = ips_chan_start(pcctx, SPK_DIR_READ);
    if (ret != SPK_SUCCESS) {
        zlog_fatal(sys_zc, "wkr#%d> failed to start ips channel", wkr_id);
        goto out;
    }
    zlog_info(sys_zc, "wkr#%d> ips channel started for reading", wkr_id);

    // start recording
    uint64_t now = spk_get_tick_count();
    uint64_t tm_upstats = now;
    uint64_t tm_log = now;
    uint64_t tm_heartbeat = now;
    ret = SPK_SUCCESS;
    size_t xfer;
    zlog_notice(sys_zc, "wkr#%d> ---------- start recording ----------", wkr_id);
    while(!wkr_ctx->reset_req) {
        // read from ips for one chunk
        // FIXME: our link partner must have enough data
        // before being stopped
        ssize_t read_size = ips_chan_read(pcctx, &chunk_buf, chunk_size, chunk_size);
        if (read_size > 0) {
            // got a chunk from ips
            assert(read_size == chunk_size);
            // preserve first buf for snapshot use
            //pthread_mutex_lock(&wkr_ctx->buf_snap_lock);
          //  memcpy(wkr_ctx->buf_snap, chunk_buf, SYS_SNAP_BUF_SZ);
           // pthread_mutex_unlock(&wkr_ctx->buf_snap_lock);

            // write to dfv
            if (sys_env.dbg_flag & SYSDBG_REC_NOTSAVE2DISK) {
                xfer = chunk_size;
            } else {
                xfer = net_write(file_ctx, chunk_buf, read_size);
            }
            // notify ips to free buffer first
            ips_chan_free_buf(pcctx, read_size);
            if (xfer != read_size) {
                zlog_fatal(sys_zc, "wkr#%d> failed to write to dfv: xfer=%ld, expect=%lu",
                                    wkr_id, xfer, read_size);
                ret = SPKERR_EACCESS;
                break;
            }
        } else if (read_size == 0) {
            // no data
            if (sys_ctx.auto_rec) {
                if (spk_get_tick_count() - tm_heartbeat > SYS_AUTOSTOP_TIMEOUT*1000) {
                    // timeout
                    zlog_warn(sys_zc, "wkr#%d> no data received since last %d secs, stop.",
                              wkr_id, SYS_AUTOSTOP_TIMEOUT);
                    break;
                }
            }
        } else {
            // failed to got a chunk from ips
            zlog_fatal(sys_zc, "wkr#%d> failed to read from ips: read_size=%ld", wkr_id, read_size);
            ret = read_size;
            break;
        }

        tm_heartbeat = now = spk_get_tick_count();

        // update local stats
        spk_stats_inc_xfer(stats, read_size, (read_size / ips_sec_sz));
        if (now > tm_upstats) {
            if (!(sys_env.features & SYSFEA_USE_LOCALSTATS)) {
                // update channel stats
                sys_job_update_stats(pcctx, wkr_id);
            }
            tm_upstats = now + 1000;
        }

        if (now > tm_log) {
            zlog_notice(sys_zc, "    wkr#%d> time=%lu pkts=%lu bytes=%lu ovfl=%lu spd=%.3f MBPS",
                        wkr_id,
                        spk_stats_get_time_elapsed(stats)/1000,
                        spk_stats_get_xfer_pkts(stats),
                        spk_stats_get_xfer_bytes(stats),
                        spk_stats_get_overflow_bytes(stats),
                        BYTE2MB(spk_stats_get_bps_overall(stats)));
            tm_log = now + 10*1000;
        }
    }

    //
    // stop recording
    //
    // 1. send stop to link partner
    int stop_ret = ips_chan_stop(pcctx);
    if (stop_ret != SPK_SUCCESS) {
        zlog_fatal(sys_zc, "wkr#%d> failed to close ips channel: ret=%d",
                            wkr_id, stop_ret);
        ret = stop_ret;
        goto out;
    }

    // 2. drain remained data
    if (ret == SPK_SUCCESS) {
        // wait for link partner to do padding
        sleep(1);

        ssize_t tail_size;
        do {
            tail_size = ips_chan_read(pcctx, &chunk_buf, 0, chunk_size);
            if (tail_size <= 0) {
                // done
                break;
            }
            zlog_notice(sys_zc, "wkr#%d> got remained data: size=%zd", wkr_id, tail_size);
            // data must been padded to 64k alignment by link partner
            assert(!(tail_size & (0x10000-1)));

            // save remained data to dfv slot
            if (sys_env.dbg_flag & SYSDBG_REC_NOTSAVE2DISK) {
                xfer = tail_size;
            } else {
                xfer = net_write(file_ctx, chunk_buf, tail_size);
            }
            ips_chan_free_buf(pcctx, tail_size);
            if (xfer != tail_size) {
                zlog_fatal(sys_zc, "wkr#%d> failed to write to dfv: xfer=%ld, expect=%lu",
                                    wkr_id, xfer, tail_size);
                ret = SPKERR_EACCESS;
                goto out;
            }
        } while(1);
    }

    // 3. update chstat for last time
    if (!(sys_env.features & SYSFEA_USE_LOCALSTATS)) {
        sys_job_update_stats(pcctx, wkr_id);
    }

out:
    zlog_notice(sys_zc, "wkr#%d> ---------- stop recording ---------- ret=%d,", wkr_id, ret);
    if (stats) {
        zlog_notice(sys_zc, "wkr#%d> elapsed=%lu, pkts=%lu, bytes=%lu, ovfl=%lu",
                            wkr_id, spk_stats_get_time_elapsed(stats)/1000,
                            stats->xfer.pkts, stats->xfer.bytes, stats->overflow.bytes);
    }

    // close dfv slot
    if (file_ctx) {
        net_close(file_ctx);
        file_ctx = NULL;
    }

    // close ips srio
    if (pcctx) {
        ips_chan_close(pcctx);
        pcctx = NULL;
    }
#endif
    return(ret);
}

static int __sys_job_do_config(sys_wkr_ctx_t* wkr_ctx,
                               IPS_EPID src_id, int pc_id, char* config_buf, size_t buf_sz)
{
    int ret = -1;
#ifdef ARCH_ppc64
    struct ips_pcctx* pcctx = NULL;
    int wkr_id = wkr_ctx->wkr_id;

    zlog_notice(sys_zc, "wkr#%d> prepare for config: ips={0x%x:%d}, config={0x%08x}+%lu",
                        wkr_id, src_id, pc_id,
                        *(uint32_t*)config_buf, buf_sz);


    // open ips srio
    pcctx = ips_chan_open(src_id, pc_id);
    if (!pcctx) {
        zlog_fatal(sys_zc, "wkr#%d> failed to open ips channel", wkr_id);
        ret = SPKERR_EACCESS;
        goto out;
    }

    zlog_notice(sys_zc, "wkr#%d> ---------- start config ----------", wkr_id);
    ret = ips_chan_config(pcctx, config_buf, buf_sz);

out:
    zlog_notice(sys_zc, "wkr#%d> ---------- stop config ---------- ret=%d", wkr_id, ret);

    // close ips srio
    if (pcctx) {
        ips_chan_close(pcctx);
        pcctx = NULL;
    }
#endif
    return(ret);
}

static int __sys_job_do_playback(sys_wkr_ctx_t* wkr_ctx,
                                 IPS_EPID src_id, int pc_id, size_t ips_sec_sz,
                                 dfv_slot_def_t* slot_def, uint64_t slot_sz)
{
    int ret = -1;
#ifdef ARCH_ppc64
    struct ips_pcctx* pcctx = NULL;
    int wkr_id = wkr_ctx->wkr_id;
    spk_stats_t* stats = &wkr_ctx->stats;
    struct dfvcm_ctx* dfvcm = NULL;
    void * txbuf = NULL;
    size_t txbuf_sz = 0;
    int i;
    int dfv_cpu_base = 12+4*wkr_ctx->wkr_id;

    zlog_notice(sys_zc, "wkr#%d> prepare for playback: ips={0x%x:%d}, "
                        "dfv={repo={%d:%d}, sz=%lu, cpu_base=%d}",
                        wkr_id,
                        src_id, pc_id,
                        dfv_repo_get_id(slot_def->repo), slot_def->slot_id,
                        slot_sz, dfv_cpu_base);

    // reset stats
    spk_stats_reset(stats);

    // open ips srio
    pcctx = ips_chan_open(src_id, pc_id);
    if (!pcctx) {
        zlog_fatal(sys_zc, "wkr#%d> failed to open ips channel", wkr_id);
        ret = SPKERR_EACCESS;
        goto out;
    }

    ret = ips_chan_start(pcctx, SPK_DIR_WRITE);
    if (ret != SPK_SUCCESS) {
        zlog_fatal(sys_zc, "wkr#%d> failed to start ips channel", wkr_id);
        goto out;
    }
    zlog_info(sys_zc, "wkr#%d> ips channel started for writing", wkr_id);

    // clean stats
    if (!(sys_env.features & SYSFEA_USE_LOCALSTATS)) {
        stats = ips_chan_get_stats(pcctx);
    }

    int dfvcm_buf_nodes = 2;
    // we use ips's tx_buffers directly (for zero-copy)
    // scratch bufs from ips
    txbuf = ips_chan_get_txbuf(pcctx, &txbuf_sz);
    // WARNING: IPS must allocated enough tx_buffer, so check it
    assert(txbuf && txbuf_sz >= dfvcm_buf_nodes*DFV_CHUNK_SIZE);

    // open dfv cache manager for read
    dfvcm = dfvcm_open(wkr_id, slot_def, SPK_DIR_READ, NULL, dfv_cpu_base);
    assert(dfvcm);

    for (i=0; i<dfvcm_buf_nodes; i++) {
        // initialize buf nodes
        dfv_bufq_node_t* node = malloc(sizeof(dfv_bufq_node_t));
        assert(node);
        memset(node, 0, sizeof(dfv_bufq_node_t));
        // points to txbuf
        node->buf_ptr = txbuf + i*DFV_CHUNK_SIZE;
        node->buf_sz = DFV_CHUNK_SIZE;
        // enqueue to dfvcm's freeq
        // dfvcm will start reading immediatly
        dfv_bufq_enqueue(dfvcm_get_freeq(dfvcm), node);
        zlog_notice(sys_zc, "wkr#%d> enqueue node to dfvcm: buf={%p+%zu}",
                            wkr_id, node->buf_ptr, node->buf_sz);
    }

    // start playback
    zlog_notice(sys_zc, "wkr#%d> ---------- start playback ----------", wkr_id);
    uint64_t now = spk_get_tick_count();
    uint64_t tm_log = now;
    ret = SPK_SUCCESS;
    while(!wkr_ctx->reset_req) {
        // dequeue buffers from dfvcm
        dfv_bufq_node_t* node = dfv_bufq_dequeue(dfvcm_get_workq(dfvcm));
        if (!node) {
            if (dfvcm_get_eof(dfvcm)) {
                break;
            }
            usleep(100);
            continue;
        }
        // got one node
        assert(node->valid_sz);
        // write to ips
        uint64_t tmout = spk_get_tick_count() + 5*1000; // timeout 5secs
        while(!wkr_ctx->reset_req) {
            ssize_t xfer = ips_chan_write(pcctx, node->buf_ptr, node->valid_sz);
            if (xfer == node->valid_sz) {
                // done
                spk_stats_inc_xfer(stats, node->valid_sz, 1);
                // recycle buffer to dfvcm
                dfv_bufq_enqueue(dfvcm_get_freeq(dfvcm), node);
                break;
            }
            if (xfer != 0) {
                zlog_error(sys_zc, "wkr#%d> failed to write to ips: xfer=%ld, expect=%ld",
                                    wkr_id, xfer, node->valid_sz);
                ret = SPKERR_EACCESS;
                goto out;
            }
            // nothing written
            spk_stats_inc_drop(stats, 0, 1); // just recored retried times
            if (spk_get_tick_count() > tmout) {
                // timeout
                zlog_error(sys_zc, "wkr#%d> write to ips timeout: xfer=%ld, expect=%ld",
                                    wkr_id, xfer, node->valid_sz);
                ret = SPKERR_EACCESS;
                goto out;
            }
            usleep(100);
        };

        now = spk_get_tick_count();
        if (now > tm_log) {
            zlog_notice(sys_zc, "    wkr#%d> time=%lu pkts=%lu bytes=%lu spd:%.3f MBPS retried=%lu",
                        wkr_id,
                        spk_stats_get_time_elapsed(stats)/1000,
                        spk_stats_get_xfer_pkts(stats),
                        spk_stats_get_xfer_bytes(stats),
                        BYTE2MB(spk_stats_get_bps_overall(stats)),
                        spk_stats_get_drop_pkts(stats));
            tm_log = now + 10*1000;
        }
    }
    if (wkr_ctx->reset_req || dfvcm_get_eof(dfvcm) > 0)
        ret = SPK_SUCCESS;
    else
        ret = dfvcm_get_eof(dfvcm);

out:
    zlog_notice(sys_zc, "wkr#%d> ---------- stop playback ---------- ret=%d", wkr_id, ret);
    if (stats) {
        zlog_notice(sys_zc, "wkr#%d> elapsed=%lu, pkts=%lu, bytes=%lu, retried=%lu",
                            wkr_id, spk_stats_get_time_elapsed(stats)/1000,
                            stats->xfer.pkts, stats->xfer.bytes, stats->drop.bytes);
    }

    // close dfv cache manager
    if (dfvcm) {
        dfvcm_close(dfvcm, 0);
        dfvcm = NULL;
    }

    // close ips srio
    if (pcctx) {
        ips_chan_close(pcctx);
        pcctx = NULL;
    }
#endif
    return(ret);
}

static int __sys_job_do_download(sys_wkr_ctx_t* wkr_ctx,
                                 int slot_id,
                                 uint32_t req_frag,
                                 uint64_t blk_start,
                                 uint64_t blk_num)
{
    int wkr_id = wkr_ctx->wkr_id;

    int ret = -1;
    uint32_t frag_id = 0;
    int is_eof = 0;

    do {
        // cache if necessary
        ret = sys_func_dl_prepare(slot_id,
                                  blk_start,
                                  blk_num,
                                  frag_id);

        if (ret != SPK_SUCCESS) {
            break;
        }
        if (wkr_ctx->reset_req) {
            // download aborted
            is_eof = 0x02;
        }
        ret = sys_func_dl_sendfrag(slot_id, blk_start,
                                   blk_num, frag_id, &is_eof);
        if (ret != SPK_SUCCESS) {
            break;
        }
        frag_id++;
    } while(!is_eof);

    zlog_notice(sys_zc, "wkr#%d> download done: eof=%d", wkr_id, is_eof);

    return(ret);
}

#define __JOB_RESPONSE(STATE, RESP) \
            wkr_ctx->wkr_state = STATE; \
            job_node->resp = RESP; \
            sys_jobq_enqueue(&wkr_ctx->job_out, job_node);

void* __sys_wkr_job(void* args)
{
    sys_wkr_ctx_t* wkr_ctx = (sys_wkr_ctx_t*)args;

    int wkr_id = wkr_ctx->wkr_id;
    int ips_pipe_id = wkr_id;
    int ips_pcid = 0;
    int dfv_pipe_id = wkr_id;

    sys_ctx_t* ctx = &sys_ctx;
    sys_env_t* env = &sys_env;

    ips_epdesc_t* epdesc = &env->ips_desc_tbl[ips_pipe_id];
    ips_pcdesc_t* pcdesc = &epdesc->pcdesc_tbl[ips_pcid];
    IPS_EPID epid = pcdesc->src_id;

    dfv_vault_t* vault = ctx->vault;
    struct dfv_repo* repo = vault->repo_tbl[dfv_pipe_id];
    size_t ips_sec_sz = pcdesc->sector_sz;

    dfv_slice_def_t slice_def;
    memset(&slice_def, 0, sizeof(dfv_slice_def_t));
    slice_def.num = DFV_SLICE_NUM;
    slice_def.size = 0x4000;//DFV_SLICE_SIZE;

    dfv_slot_def_t slot_def;

    int cpu = wkr_id + 5;
    spk_worker_set_affinity(cpu);
    zlog_notice(sys_zc, "wkr#%d> spawn: cpu=%d", wkr_id, cpu);

    int ret = -1;

    while(!wkr_ctx->quit_req) {
        wkr_ctx->reset_req = 0;
        ret = -1;
        sys_jobq_node_t* job_node = sys_jobq_dequeue(&wkr_ctx->job_in);
        if (!job_node) {
            // idle
            usleep(100);
            continue;
        }
        int job_type = job_node->job_type;
        // got job to do
        zlog_notice(sys_zc, "wkr#%d> start job: job_type=%s",
                             wkr_id, cmi_desc_cmdtype2str(job_type));

        if (job_node->job_type == cmd_type_config) {
            // config : immediate job
            ret = __sys_job_do_config(wkr_ctx, epid, ips_pcid,
                                      (char*)job_node->cmd_ref->u.all.words,
                                      sizeof(job_node->cmd_ref->u.all));
            job_node->resp = ret;
            sys_jobq_enqueue(&wkr_ctx->job_out, job_node);
        } else if (job_node->job_type == cmd_type_start_rec) {
            // start_rec: deferred job
            int slot_id = (int)(intptr_t)job_node->arg;
            memset(&slot_def, 0, sizeof(dfv_slot_def_t));
            slot_def.repo = repo;
            slot_def.slot_id = slot_id;
            __JOB_RESPONSE(sys_state_rec, SPK_SUCCESS);
            spk_stats_reset(&wkr_ctx->stats);
            ret = __sys_job_do_record(wkr_ctx,
                                      epid, ips_pcid, ips_sec_sz,
                                      &slot_def, &slice_def);
            spk_stats_reset(&wkr_ctx->stats);
        } else if (job_node->job_type == cmd_type_start_play) {
            // start_play: deferred job
            uint64_t slot_sz = job_node->arg;
            int slot_id = job_node->cmd_ref->u.file.index;
            __JOB_RESPONSE(sys_state_play, SPK_SUCCESS);
            spk_stats_reset(&wkr_ctx->stats);
            memset(&slot_def, 0, sizeof(dfv_slot_def_t));
            slot_def.repo = repo;
            slot_def.slot_id = slot_id;
            ret = __sys_job_do_playback(wkr_ctx,
                                        epid, ips_pcid, ips_sec_sz,
                                        &slot_def, slot_sz);
            spk_stats_reset(&wkr_ctx->stats);
        } else if (job_node->job_type == cmd_type_start_dl) {
            // start_dl: immediate/deferred job
            assert(wkr_id == 0);
            int slot_id = job_node->cmd_ref->u.file.index;
            uint32_t req_frag = job_node->cmd_ref->u.file.frag_id;
            uint64_t blk_start = job_node->cmd_ref->u.file.blk_start;
            uint64_t blk_num = job_node->cmd_ref->u.file.blk_num;
            if (req_frag == (uint32_t)-1) {
                // fast mode : deferred
                __JOB_RESPONSE(sys_state_dl, SPK_SUCCESS);
                ret = __sys_job_do_download(wkr_ctx,
                                            slot_id,
                                            req_frag,
                                            blk_start,
                                            blk_num);
            } else {
                // slow mode: immediate
                ret = sys_func_dl_prepare(slot_id,
                                          blk_start,
                                          blk_num,
                                          req_frag);
                if (ret == SPK_SUCCESS) {
                    ret = sys_func_dl_sendfrag(slot_id,
                                               blk_start,
                                               blk_num,
                                               req_frag,
                                               NULL);
                }
                job_node->resp = ret;
                sys_jobq_enqueue(&wkr_ctx->job_out, job_node);
            }
        } else if (job_node->job_type == cmd_type_format) {
            // format: deferred job
            assert(wkr_id == 0);
            __JOB_RESPONSE(sys_state_format, SPK_SUCCESS);
            ret = dfv_vault_format(sys_ctx.vault);
            sys_ctx.diskcap = dfv_vault_get_diskcap(sys_ctx.vault);
        } else {
            // unknown job
            assert(0);
        }
        zlog_notice(sys_zc, "wkr#%d> job done: job_type=%s, ret=%d",
                             wkr_id, cmi_desc_cmdtype2str(job_type), ret);

        if (wkr_id == 0 && ret == SPKERR_RESETSYS) {
            zlog_warn(sys_zc, "wkr#%d> disconnect cmi due to errors", wkr_id);
            cmi_intf_disconnect(sys_ctx.cmi_intf);
        }

        wkr_ctx->wkr_state = sys_state_idle;
    }

    zlog_notice(sys_zc, "wkr#%d> terminated", wkr_id);

    return(NULL);
}
