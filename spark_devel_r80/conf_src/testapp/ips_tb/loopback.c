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

#define DMABUF_SECTOR_NUM   (1024)
#define DMABUF_SECTOR_SIZE  (0x10000)

char* dxbuf_tbl[DMABUF_SECTOR_NUM];


typedef struct thread_args {
    IPS_EPID epid;
    int pc_id;
} thread_args_t;

void* __worker_tx(void* args)
{
    thread_args_t* thread_args = (thread_args_t*) args;
    IPS_EPID epid = thread_args->epid;
    int pc_id = thread_args->pc_id;

    spk_worker_set_affinity(10);
    free(args);

    struct ips_pcctx* pcctx = ips_chan_open(epid, pc_id);
    ips_chan_start(pcctx, SPK_DIR_WRITE);
    sleep(1);

    uint64_t total_pkts = 0;
    uint64_t total_bytes = 0;
//    uint64_t refresh_tick = 0;
    struct timeval tm_start;
    void* txbuf = NULL;
    size_t txbuf_sz = 0;

    gettimeofday(&tm_start, NULL);
//    refresh_tick = GET_TICK_COUNT(tm_start) + 5000;
    txbuf = ips_chan_get_txbuf(pcctx, &txbuf_sz);
    printf("Start TX ...\n");
    while(1) {
//        memset(txbuf, total_pkts, DMABUF_SECTOR_SIZE);
        while(ips_chan_write(pcctx, txbuf, DMABUF_SECTOR_SIZE) == 0) {
//        memcpy(txbuf, dxbuf_tbl[total_pkts%DMABUF_SECTOR_NUM], DMABUF_SECTOR_SIZE);
//        while(ips_chan_write(pcctx, txbuf, DMABUF_SECTOR_SIZE) == 0) {
//            printf("TX retry!\n");
//            printf("TX: size=%zu, first=[%08x]\n", DMABUF_SECTOR_SIZE, *(int*)buffer);
            // retry;
        }

        total_pkts++;
        total_bytes += DMABUF_SECTOR_SIZE;
        /*
                gettimeofday(&now, NULL);
                if (GET_TICK_COUNT(now) > refresh_tick) {
                    printf("TX: %lu PKTS:%lu BYTES:%lu RATE:%.3f MBPS\n",
                            GET_TICK_COUNT(now) - GET_TICK_COUNT(tm_start),
                            total_pkts,
                            total_bytes,
                            ((double)total_bytes)/(GET_TICK_COUNT(now) - GET_TICK_COUNT(tm_start)) / 1000);
                    refresh_tick = GET_TICK_COUNT(now) + 5000;
                }
        */
    }

    return(NULL);
}

void* __worker_rx(void* args)
{
    thread_args_t* thread_args = (thread_args_t*) args;
    IPS_EPID epid = thread_args->epid;
    int pc_id = thread_args->pc_id;

    spk_worker_set_affinity(11);
    free(args);

    struct ips_pcctx* pcctx = ips_chan_open(epid, pc_id);
    assert(pcctx);
    ips_chan_start(pcctx, SPK_DIR_READ);

    void* buffer;
    size_t size;
    uint64_t total_pkts = 0;
    uint64_t total_bytes = 0;
    uint64_t refresh_tick = 0;
    struct timeval tm_start, now;
    
    gettimeofday(&tm_start, NULL);
    refresh_tick = GET_TICK_COUNT(tm_start) + 5000;
    printf("Start RX ...\n");
    while(1) {
        size = ips_chan_read(pcctx, &buffer, 0, 16*1024*1024);
        if (size > 0) {
            total_pkts += (size / DMABUF_SECTOR_SIZE);
            total_bytes += size;
            ips_chan_free_buf(pcctx, size);
        }
        gettimeofday(&now, NULL);
        if (GET_TICK_COUNT(now) > refresh_tick) {
            printf("RX: %lu PKTS:%lu BYTES:%lu RATE:%.3f MBPS\n",
                   GET_TICK_COUNT(now) - GET_TICK_COUNT(tm_start),
                   total_pkts,
                   total_bytes,
                   ((double)total_bytes) / (GET_TICK_COUNT(now) - GET_TICK_COUNT(tm_start)) / 1000);
            refresh_tick = GET_TICK_COUNT(now) + 5000;
        }
    }

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
    
    IPS_EPID master_id = IPS_MAKE_EPID(IPS_EPMODE_MASTER, 0, 0x1); // 0x41
    int master_port = 1;
    IPS_EPID slave_id = IPS_MAKE_EPID(IPS_EPMODE_SLAVE, 0, 0x0);  // 0x0
    int slave_port = 4;
    pthread_t  rx_thread;
    pthread_t  tx_thread;
    
    int idt_fd = idt_dev_open(2, 0x67);
    assert(idt_fd > 0);
    idt_routetbl_set(idt_fd, master_port, master_id, slave_port);
    idt_routetbl_set(idt_fd, slave_port, slave_id, master_port);
    idt_dev_close(idt_fd);

    {
        ips_epdesc_t   epdesc;
        memset(&epdesc, 0, sizeof(ips_epdesc_t));
        epdesc.capacity = 0;
        epdesc.pc_num = 1;
        for (i = 0; i < epdesc.pc_num; i++) {
            ips_pcdesc_t* pcdesc = &epdesc.pcdesc_tbl[i];
            pcdesc->dest_id = master_id;
            pcdesc->capacity = epdesc.capacity;
        }
        rvl = ips_ep_init(slave_id, &epdesc);
        assert(rvl == 0);
        for (i = 0; i < epdesc.pc_num; i++) {
            thread_args_t* args = malloc(sizeof(thread_args_t));
            args->epid = slave_id;
            args->pc_id = i;
            rvl = pthread_create(&tx_thread, NULL, __worker_tx, args);
            assert(!rvl);
        }
    }
    {
        ips_epdesc_t   epdesc;
        memset(&epdesc, 0, sizeof(ips_epdesc_t));
        epdesc.capacity = 0;
        epdesc.pc_num = 1;
        for (i = 0; i < epdesc.pc_num; i++) {
            ips_pcdesc_t* pcdesc = &epdesc.pcdesc_tbl[i];
            pcdesc->dest_id = slave_id;
            pcdesc->capacity = epdesc.capacity;
            pcdesc->sector_sz = DMABUF_SECTOR_SIZE;
            pcdesc->sector_num = DMABUF_SECTOR_NUM;
        }
        rvl = ips_ep_init(master_id, &epdesc);
        assert(rvl == 0);
        for (i = 0; i < epdesc.pc_num; i++) {
            thread_args_t* args = malloc(sizeof(thread_args_t));
            args->epid = master_id;
            args->pc_id = i;
            rvl = pthread_create(&rx_thread, NULL, __worker_rx, args);
            assert(!rvl);
        }
    }

    while(1) {
        usleep(100);
    };


    return 0;
}
