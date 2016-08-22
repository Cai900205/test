#ifndef __DFV_INTF_H__
#define __DFV_INTF_H__
#include <inttypes.h>

#define DFV_MOD_VER     "0.9.160815"

#define DFV_MAX_REPOS   (4) // max repos in vault
#define DFV_MAX_SLOTS   (999)

typedef struct
{
    struct  list_head list;
    void*   buf_ptr;
    size_t  buf_sz;
    size_t  valid_sz;
    size_t  buf_offset;
} dfv_bufq_node_t;

DECLARE_QUEUE(dfv_bufq, dfv_bufq_node_t)

struct dfv_repo;

typedef struct
{
    int num;
    ssize_t size;
} dfv_slice_def_t;

typedef struct
{
    struct dfv_repo* repo;
    int slot_id;
} dfv_slot_def_t;

int dfv_module_init(const char* log_cat);

// repo
struct dfv_repo* dfv_repo_open(int repo_id, const char* mnt_path, const char* dev_path, int flag);
void dfv_repo_close(struct dfv_repo* repo);
int dfv_repo_get_id(struct dfv_repo* repo);
ssize_t dfv_repo_get_slotsize(struct dfv_repo* repo, int slot_id);
struct dfv_fmeta* dfv_repo_get_fmeta(struct dfv_repo* repo, int slot_id);
int dfv_repo_get_freeslot(struct dfv_repo* repo);
int dfv_repo_delete(struct dfv_repo* repo, int slot_id);
ssize_t dfv_repo_get_diskfree(struct dfv_repo* repo);
ssize_t dfv_repo_get_diskcap(struct dfv_repo* repo);
char* dfv_repo_get_rootpath(struct dfv_repo* repo);
int dfv_repo_sync(struct dfv_repo* repo);

// file
struct dfv_file* dfv_file_open(struct dfv_repo* repo, int slot_id, SPK_DIR dir, dfv_slice_def_t* slice_def, int cpu_base);
int dfv_file_close(struct dfv_file* file_ctx);
int dfv_file_seek(struct dfv_file* file, uint64_t offset);
ssize_t dfv_file_read(struct dfv_file* file_ctx, void* buf, size_t size);
ssize_t dfv_file_write(struct dfv_file* file_ctx, void* buf, size_t size);
spk_stats_t* dfv_file_get_stats(struct dfv_file* ctx);

// meta
typedef struct dfv_fmeta
{
    int slot_id;
    int slice_num;
    size_t slice_sz;
    size_t slot_sz;
    uint64_t file_time;

    pthread_mutex_t open_cnt_lock;
    int open_cnt;
    uint64_t file_pos;
} dfv_fmeta_t;

typedef struct dfv_rmeta
{
    int version;
    dfv_fmeta_t* fmeta_tbl[DFV_MAX_SLOTS];
} dfv_rmeta_t;

void dfv_rmeta_reset(dfv_rmeta_t* rmeta);
dfv_rmeta_t* dfv_rmeta_get(struct dfv_repo* repo);
int dfv_rmeta_load(dfv_rmeta_t* rmeta, const char* path);
int dfv_rmeta_save(dfv_rmeta_t* rmeta, const char* path);
dfv_fmeta_t* dfv_fmeta_get(struct dfv_repo* repo, int slot_id);

// vault
typedef struct dfv_vault
{
    int repo_num;
    struct dfv_repo* repo_tbl[DFV_MAX_REPOS];
} dfv_vault_t;

dfv_vault_t* dfv_vault_open(int repo_num, int dev_num, const char* mnt_tbl[], const char* dev_tbl[], int flag);
void dfv_vault_close(dfv_vault_t* v);
int dfv_vault_get_freeslot(dfv_vault_t* v);
ssize_t dfv_vault_get_slotsize(dfv_vault_t* v, int slot_id);
int dfv_vault_format(dfv_vault_t* v);
int dfv_vault_del_slot(dfv_vault_t* v, int slot_id);
ssize_t dfv_vault_get_diskfree(dfv_vault_t* v);
ssize_t dfv_vault_get_diskcap(dfv_vault_t* v);
struct dfv_repo* dfv_vault_get_repo(dfv_vault_t* v, int repo_id);
int dfv_vault_sync(dfv_vault_t* v);

// cm: cache manager
struct dfvcm_ctx;
struct dfvcm_ctx* dfvcm_open(int id, dfv_slot_def_t* slot_def, SPK_DIR dir, dfv_slice_def_t* slice_def, int cpu_base);
void dfvcm_close(struct dfvcm_ctx* ctx, int free_buf);
dfv_bufq_t* dfvcm_get_workq(struct dfvcm_ctx* ctx);
dfv_bufq_t* dfvcm_get_freeq(struct dfvcm_ctx* ctx);
int dfvcm_get_eof(struct dfvcm_ctx* ctx);
int dfvcm_get_id(struct dfvcm_ctx* ctx);

#endif

