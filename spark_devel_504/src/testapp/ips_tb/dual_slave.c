/*************************************************************************
    > File Name: main.c
    > Author:
    > Mail:
    > Created Time: Wed 12 Aug 2015 05:16:49 PM CST
 ************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <inttypes.h>
#include <pthread.h>
#include <assert.h>
#include <sys/time.h>
#include "spark.h"
#include "ips/ips_intf.h"
#include "dfv/dfv_intf.h"
#include "idt/idt_intf.h"
#include "zlog/zlog.h"

#define WORK_CPU_BASE   (22)

#define PIPE_NUM    (2)
ips_linkdesc_t link_desc_tbl[PIPE_NUM];
pthread_t wkr[PIPE_NUM];
static pthread_mutex_t sync_cnt_lock;
static uint64_t sync_cnt = 0;

int break_req = 0;
pthread_barrier_t barrier;

typedef struct thread_args {
    int wkr_id;
    IPS_EPID epid;
    int pc_id;
} thread_args_t;

int __do_tx(int wkr_id, struct ips_pcctx* pcctx, IPS_EPID epid, int pc_id)
{
    uint64_t refresh_tick, hb_tick;
    
    void* txbuf = NULL;
    size_t txbuf_sz = 0;
    size_t tx_sz = 0;
    uint64_t sync_cnt_local = 0;

    uint64_t now = spk_get_tick_count();
    refresh_tick = hb_tick = now;
    
    spk_stats_t* stats = ips_chan_get_stats(pcctx);
    ips_chan_start(pcctx, SPK_DIR_WRITE);

    txbuf = ips_chan_get_txbuf(pcctx, &txbuf_sz);
    tx_sz = 0x100000;
    printf("TX#%d: start, txbuf_sz=0x%lx\n", wkr_id, txbuf_sz);

    while(1) {
        ssize_t write_ret;
        do {
            memcpy(txbuf, &now, sizeof(uint64_t));
            write_ret = ips_chan_write(pcctx, txbuf, tx_sz);
            if (write_ret != 0) {
                break;
            }
        } while(ips_chan_is_start(pcctx) && spk_get_tick_count() < hb_tick);
        
        if (write_ret <= 0 || !ips_chan_is_start(pcctx)) {
            // done or error
            break_req = 1;
            break;
        }

        // update heartbeat
        now = spk_get_tick_count();
        hb_tick = now + 2000;

        sync_cnt_local++;
        while ((sync_cnt % 2) != wkr_id && !break_req) {
            usleep(10);
        }
        sync_cnt++;

        assert(write_ret == tx_sz);
        if (now > refresh_tick) {
            printf("TX#%d: time:%lu pkts:%lu bytes:%lu ovl:%.3f wire:%.3f \n",
                wkr_id,
                spk_stats_get_time_elapsed(stats)/1000,
                spk_stats_get_xfer_pkts(stats),
                spk_stats_get_xfer_bytes(stats),
                BYTE2MB(spk_stats_get_bps_overall(stats)),
                BYTE2MB(spk_stats_get_bps_wire(stats)));
            refresh_tick = now + 1000;
        }
    }

    printf("TX#%d: Stop TX\n", wkr_id);
    
    return 0;
}

int __do_rx(int wkr_id, struct ips_pcctx* pcctx, IPS_EPID epid, int pc_id)
{
    uint64_t refresh_tick, timeout_tick;

    uint64_t now = spk_get_tick_count();
    refresh_tick = timeout_tick = now;
    uint64_t sync_cnt_local = 0;
    
    void* chunk_buf = NULL;

    spk_stats_t* stats = ips_chan_get_stats(pcctx);
    ips_chan_start(pcctx, SPK_DIR_READ);

    printf("RX#%d: start\n", wkr_id);

    while(1) {
        ssize_t read_size = ips_chan_read(pcctx, &chunk_buf, 0, 0);
        if (read_size < 0) {
            printf("RX#%d: error: ret=%ld\n", wkr_id, read_size);
            break;
        }
        
        now = spk_get_tick_count();
        if (read_size == 0) {
            if (now - timeout_tick > 2*1000) {
                // timeout
                printf("RX#%d: timeout\n", wkr_id);
                break;
            }
            continue;
        }
        ips_chan_free_buf(pcctx, read_size);

        timeout_tick = now;

        sync_cnt_local++;
        if (wkr_id == 0) {
            sync_cnt = sync_cnt_local;
        } else {
            while(sync_cnt > 0 && sync_cnt_local > sync_cnt) {
                usleep(100);
            }
        }

        if (now > refresh_tick) {
            printf("RX#%d: time:%lu pkts:%lu bytes:%lu ovl:%.3f wire:%.3f\n",
                wkr_id,
                (now - stats->start)/1000,
                stats->xfer.pkts,
                stats->xfer.bytes,
                BYTE2MB(spk_stats_get_bps_overall(stats)),
                BYTE2MB(spk_stats_get_bps_wire(stats)));
            refresh_tick = now + 1000;
        }
    }

    printf("RX#%d: stop\n", wkr_id);
    
    return 0;
}
static void* __worker_slave(void* args)
{
    thread_args_t* thread_args = (thread_args_t*) args;
    IPS_EPID epid = thread_args->epid;
    int pc_id = thread_args->pc_id;
    int wkr_id = thread_args->wkr_id;

    free(args);
    
    int cpu = WORK_CPU_BASE+wkr_id;
    printf("spk_worker_set_affinity: cpu=%d\n", cpu);
    spk_worker_set_affinity(cpu);

    struct ips_pcctx* pcctx = NULL;
    pthread_barrier_wait(&barrier);

RESTART:
    if (wkr_id == 0) {
        sync_cnt = 0;
    }

    printf("WRK#%d: wait for connect ...\n", wkr_id);
    pcctx = ips_chan_open(epid, pc_id);
    assert(pcctx);

    SPK_DIR dir = ips_chan_get_dir(pcctx);
    break_req = 0;
    if (dir == SPK_DIR_WRITE) {
        __do_tx(wkr_id, pcctx, epid, pc_id);
    } else if (dir == SPK_DIR_READ){
        __do_rx(wkr_id, pcctx, epid, pc_id);
    } else {
        assert(0);
    }

    ips_chan_stop(pcctx);
    ips_chan_close(pcctx);
    goto RESTART;

    return(NULL);
}

int main(int argc, char **argv)
{
    int rvl = 0;
    int i;

    zlog_init("./zlog.conf");
    rvl = ips_module_init(NULL);
    assert(!rvl);
    rvl = idt_module_init(NULL);
    assert(!rvl);
    
    // channel 0
    link_desc_tbl[0].mst_id = IPS_MAKE_EPID(IPS_EPMODE_MASTER, 0, 0x0);
    link_desc_tbl[0].slv_id = IPS_MAKE_EPID(IPS_EPMODE_SLAVE, 0, 0x0);
    link_desc_tbl[0].mst_port = 6;
    link_desc_tbl[0].slv_port = 1;
    link_desc_tbl[0].is_master = 0;
    
    link_desc_tbl[1].mst_id = IPS_MAKE_EPID(IPS_EPMODE_MASTER, 0, 0x1);
    link_desc_tbl[1].slv_id = IPS_MAKE_EPID(IPS_EPMODE_SLAVE, 0, 0x1);
    link_desc_tbl[1].mst_port = 3;
    link_desc_tbl[1].slv_port = 4;
    link_desc_tbl[0].is_master = 0;
    
    pthread_barrier_init(&barrier, NULL, PIPE_NUM);
    sync_cnt = 0;
    pthread_mutex_init(&sync_cnt_lock, NULL);

    int idt_fd = idt_dev_open(2, 0x67);
    assert(idt_fd > 0);    
    for(i=0; i<PIPE_NUM; i++){
        ips_linkdesc_t* desc = &link_desc_tbl[i];
        
        if (desc->is_master) {
            assert(0);
        } else {
            // reset link partner
            idt_port_recovery(idt_fd, desc->mst_port);
            // set routetbl entries
            idt_routetbl_set(idt_fd, desc->mst_port,
                                     desc->slv_id,
                                     desc->slv_port);
            idt_routetbl_set(idt_fd, desc->slv_port,
                                     desc->mst_id,
                                     desc->mst_port);
        }
    }
    idt_dev_close(idt_fd);

    for(int pipe=0; pipe<PIPE_NUM; pipe++) {
        ips_epdesc_t   epdesc;
        memset(&epdesc, 0, sizeof(ips_epdesc_t));
        epdesc.capacity = 0;
        epdesc.pc_num = 1;
        for (i = 0; i < epdesc.pc_num; i++) {
            ips_pcdesc_t* pcdesc = &epdesc.pcdesc_tbl[i];
            pcdesc->dest_id = link_desc_tbl[pipe].mst_id;
            pcdesc->capacity = epdesc.capacity;
        }
        rvl = ips_ep_init(link_desc_tbl[pipe].slv_id, &epdesc);
        assert(rvl == 0);
        for (i = 0; i < epdesc.pc_num; i++) {
            thread_args_t* args = malloc(sizeof(thread_args_t));
            args->epid = link_desc_tbl[pipe].slv_id;
            args->pc_id = i;
            args->wkr_id = pipe;
            rvl = pthread_create(&wkr[pipe], NULL, __worker_slave, args);
            assert(!rvl);
        }
    }

    while(1) {
        for(int pipe=0; pipe<PIPE_NUM; pipe++) {
            pthread_join(wkr[pipe], NULL);
        }
    };


    return 0;
}
