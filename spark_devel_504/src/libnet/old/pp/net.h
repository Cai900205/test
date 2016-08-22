#ifndef __NET_H__
#define __NET_H__

#include <inttypes.h>
#include "spark.h"
#include "zlog/zlog.h"
#include "net/net_intf.h"

#define NET_LOG(LEVEL, ...) \
    zlog(net_zc, __FILE__, sizeof(__FILE__)-1, __func__, sizeof(__func__)-1, __LINE__, \
        LEVEL, __VA_ARGS__);

#define NET_MAX_SLICES      (8)
#define NET_MAX_SECSIZE     (4*1024*1024)

extern zlog_category_t* net_zc;

typedef struct
{
    void* buf;
    size_t size;
    int flag;
#define CHUNK_DATA_FLAG__DONE   (0)
#define CHUNK_DATA_FLAG__REQ    (1)
} net_chunk_desc_t;

typedef struct
{
    int slice_id;
    int slice_num;
	int fd;
    int cpu_base;
    int quit_req;
    SPK_DIR     dir;
	size_t slice_sz;

    uint64_t wptr;
    uint64_t rptr;
    net_chunk_desc_t data_chunk;

    pthread_t* wkr_thread;
    pthread_mutex_t lock;
    pthread_cond_t not_empty;
    pthread_cond_t not_full;
} net_slice_ctx_t;

typedef struct net_handle    
{
    int dir;
    int slice_num;
    int conn_valid[NET_MAX_SLICES];
    net_slice_ctx_t* slice_tbl[NET_MAX_SLICES];
	spk_stats_t xfer_stats;
} net_handle_t;
#endif
