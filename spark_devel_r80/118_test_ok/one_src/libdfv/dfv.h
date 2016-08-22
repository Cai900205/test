#ifndef __DFV_H__
#define __DFV_H__

#include <inttypes.h>
#include "spark.h"
#include "zlog/zlog.h"
#include "dfv/dfv_intf.h"

#define DFV_LOG(LEVEL, ...) \
    zlog(dfv_zc, __FILE__, sizeof(__FILE__)-1, __func__, sizeof(__func__)-1, __LINE__, \
        LEVEL, __VA_ARGS__);

#define DFV_META_VERSION    (1)

#define DFV_MAX_SLICES      (8)
#define DFV_MAX_SECSIZE     (4*1024*1024)

#define DFV_PREFIX_PATH     "media"
#define DFV_POSTFIX_PATH    "spk_data"
//#define DFV_SLICE_FILEPATH  "sd%d"
#define DFV_SLICE_FILENAME  "slice_%d.bin"
#define DFV_SPS_FMTTOOL     "serpence-format"

extern zlog_category_t* dfv_zc;

typedef struct
{
    void* buf;
    size_t size;
    int flag;
#define CHUNK_DATA_FLAG__DONE   (0)
#define CHUNK_DATA_FLAG__REQ    (1)
} dfv_chunk_desc_t;

typedef struct
{
    int slot_id;
    int slice_id;
    SPK_DIR     dir;
    int fd;
    int cpu_base;
    int quit_req;
    dfv_fmeta_t* fmeta;

    uint64_t wptr;
    uint64_t rptr;
    dfv_chunk_desc_t data_chunk;

    pthread_t* wkr_thread;
    pthread_mutex_t lock;
    pthread_cond_t not_empty;
    pthread_cond_t not_full;
} dfv_slice_ctx_t;

typedef struct dfv_repo
{
    int repo_id;
#if 1
    int dev_num;
#endif
    char root_path[SPK_MAX_PATHNAME];
    char meta_path[SPK_MAX_PATHNAME];
    char mnt_path[SPK_MAX_PATHNAME];
    char dev_path[SPK_MAX_PATHNAME];
    dfv_rmeta_t rmeta;
} dfv_repo_t;

typedef struct dfv_file
{
    dfv_repo_t* repo;
    int slot_id;
    int dir;
    dfv_fmeta_t* fmeta;
    dfv_slice_ctx_t* slice_tbl[DFV_MAX_SLICES];
    spk_stats_t xfer_stats;
} dfv_file_t;

void dfv_rmeta_dump(zlog_level ll, dfv_rmeta_t* rmeta);
int dfv_pipe_exec(const char* cmd, char* ret_str, int ret_size);
int dfv_system_exec(const char* cmd);
#define dfv_rmeta_get_now()    ((uint64_t)time(NULL))
int dfv_repo_get_freeslot(struct dfv_repo* repo);
int dfv_repo_format_common(struct dfv_repo* repo);
int dfv_repo_format_sps(struct dfv_repo* repo);

typedef struct dfvcm_ctx
{
    int         id;
    SPK_DIR     dir;
    int         cpu_base;
    int         quit_req;
    int         eof;

    dfv_file_t* file;
    size_t      slot_sz;

    dfv_bufq_t  workq;
    dfv_bufq_t  freeq;

    pthread_t   cm_thread;
} dfvcm_ctx_t;
#endif
