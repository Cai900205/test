#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <inttypes.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <pthread.h>
#include <assert.h>
#include <errno.h>
#include <sys/time.h>

#include "dfv.h"

void* __dfvcm_worker(void* args)
{
    dfvcm_ctx_t* ctx = (dfvcm_ctx_t*)args;
    size_t slot_sz = ctx->slot_sz;
    dfv_file_t* file = ctx->file;
    spk_stats_t* stats = dfv_file_get_stats(file);
    dfv_bufq_node_t* node = NULL;
    uint64_t xferred = 0;
    ssize_t xfer_sz = 0;
    size_t xfer_req = 0;

    zlog_notice(dfv_zc, "dfvcm#%d> worker spawned: cpu_base=%d",
                        ctx->id, ctx->cpu_base);

    while(!ctx->quit_req && !ctx->eof) {
        if (ctx->dir == SPK_DIR_READ) {
            // read
            node = dfv_bufq_dequeue(&ctx->freeq);
            if (!node) {
                usleep(100);
                continue;
            }
            xfer_req = MIN(slot_sz-xferred, node->buf_sz);
            xfer_sz = dfv_file_read(file, node->buf_ptr, xfer_req);
            if (xfer_sz != xfer_req) {
                dfv_bufq_enqueue(&ctx->freeq, node);
                ctx->eof = -1;
                zlog_error(dfv_zc, "dfvcm#%d> read error: xfer_req=%zu, xfer_sz=%ld",
                                   ctx->id, xfer_req, xfer_sz);
                break;
            }
            xferred += xfer_req;
            if(xferred >= slot_sz) {
                ctx->eof = 1;
            }
            node->valid_sz = xfer_req;
            dfv_bufq_enqueue(&ctx->workq, node);
        } else {
            // write
            node = dfv_bufq_dequeue(&ctx->workq);
            if (!node) {
                usleep(100);
                continue;
            }
            xfer_req = node->valid_sz;
            assert(xfer_req > 0);
            xfer_sz = dfv_file_write(file, node->buf_ptr, xfer_req);
            if (xfer_sz != xfer_req) {
                dfv_bufq_enqueue(&ctx->freeq, node);
                ctx->eof = -1;
                zlog_error(dfv_zc, "dfvcm#%d> write error: xfer_req=%zu, xfer_sz=%ld",
                                   ctx->id, xfer_req, xfer_sz);
                break;
            }
            xferred += xfer_req;
            dfv_bufq_enqueue(&ctx->freeq, node);
        }
    }
    if (stats) {
        zlog_notice(dfv_zc, "    dfvcm#%d> time=%lu pkts=%lu bytes=%lu spd=%.3f MBPS",
                ctx->id,
                spk_stats_get_time_elapsed(stats)/1000,
                spk_stats_get_xfer_pkts(stats),
                spk_stats_get_xfer_bytes(stats),
                BYTE2MB(spk_stats_get_bps_overall(stats)));
    }
    zlog_notice(dfv_zc, "dfvcm#%d> worker terminated", ctx->id);

    return(NULL);
}

dfvcm_ctx_t* dfvcm_open(int id, dfv_slot_def_t* slot_def, SPK_DIR dir, dfv_slice_def_t* slice_def, int cpu_base)
{
    assert(dir == SPK_DIR_READ || dir == SPK_DIR_WRITE);

    dfvcm_ctx_t* ctx = NULL;
        
    zlog_notice(dfv_zc, "dfvcm#%d> open: repo={%d:%d}, dir=%s, slice={%d, 0x%lx}",
                         id,
                         dfv_repo_get_id(slot_def->repo), slot_def->slot_id,
                         spk_desc_dir2str(dir),
                         slice_def?slice_def->num:0, slice_def?slice_def->size:0);

    ctx = malloc(sizeof(dfvcm_ctx_t));
    assert(ctx);
    memset(ctx, 0, sizeof(dfvcm_ctx_t));

    ctx->id = id;
    ctx->dir = dir;
    ctx->cpu_base = cpu_base;
    if (dir == SPK_DIR_READ) {
        ctx->slot_sz = dfv_repo_get_slotsize(slot_def->repo, slot_def->slot_id);
        if (ctx->slot_sz <= 0) {
            goto errout;
        }
    }
    ctx->file = dfv_file_open(slot_def->repo, slot_def->slot_id, dir, slice_def, cpu_base);
    if (!ctx->file) {
        goto errout;
    }

    dfv_bufq_init(&ctx->freeq);
    dfv_bufq_init(&ctx->workq);

    pthread_create(&ctx->cm_thread, NULL, __dfvcm_worker, ctx);

    return(ctx);

errout:
    if (ctx) {
        if (ctx->file) {
            dfv_file_close(ctx->file);
            ctx->file = NULL;
        }
        SAFE_RELEASE(ctx);
    }
    return(NULL);
}

void dfvcm_close(dfvcm_ctx_t* ctx, int free_buf)
{
    if (ctx) {
        if (ctx->dir == SPK_DIR_WRITE) {
            zlog_notice(dfv_zc, "dfvcm#%d> workq not empty, wait...", ctx->id);
            // wait all work in queue be done
            while(dfv_bufq_get_count(&ctx->workq) > 0) {
                usleep(100);
            }
        }
        ctx->quit_req = 1;
        pthread_join(ctx->cm_thread, NULL);
    }
    if (ctx->file) {
        dfv_file_close(ctx->file);
        ctx->file = NULL;
    }
    dfv_bufq_node_t* node = NULL;
    while(!!(node = dfv_bufq_dequeue(&ctx->workq))) {
        if (free_buf) {
            SAFE_RELEASE(node->buf_ptr);
        }
        SAFE_RELEASE(node);
    }
    while(!!(node = dfv_bufq_dequeue(&ctx->freeq))) {
        if (free_buf) {
            SAFE_RELEASE(node->buf_ptr);
        }
        SAFE_RELEASE(node);
    }

    zlog_notice(dfv_zc, "dfvcm#%d> closed", ctx->id);
    SAFE_RELEASE(ctx);

    return;
}

dfv_bufq_t* dfvcm_get_workq(dfvcm_ctx_t* ctx)
{
    return(&ctx->workq);
}

dfv_bufq_t* dfvcm_get_freeq(dfvcm_ctx_t* ctx)
{
    return(&ctx->freeq);
}

int dfvcm_get_eof(dfvcm_ctx_t* ctx)
{
    return(ctx->eof);
}

int dfvcm_get_id(dfvcm_ctx_t* ctx)
{
    return(ctx->id);
}