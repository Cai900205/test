
#include "spark.h"
#include "cmi/cmi_intf.h"
#include "ips/ips_intf.h"
#include "dfv/dfv_intf.h"
#include "idt/idt_intf.h"
#include "net/net_intf.h"
#include "zlog/zlog.h"

#define SYS_VERSION_MAJOR   0
#define SYS_VERSION_MINOR   9
#define SYS_VERSION_DATE    151230

#define SYS_VERSION         MAKE_VER_STR(SYS_VERSION_MAJOR, SYS_VERSION_MINOR, SYS_VERSION_DATE)
#define SYS_VERSION_INT     MAKE_VER_INT(SYS_VERSION_MAJOR, SYS_VERSION_MINOR, SYS_VERSION_DATE)

#define SYS_MAX_PIPES       (2)
#define IPS_CLS_SECTOR_NUM  (1024)
#define IPS_CLS_SECTOR_SIZE (0x10000)

#define DFV_SLICE_SIZE      (4*1024*1024)
#define DFV_SLICE_NUM       (4)
#define DFV_CHUNK_SIZE      (DFV_SLICE_SIZE*DFV_SLICE_NUM)

#define SYS_CACHE_SIZE      (DFV_CHUNK_SIZE*SYS_MAX_PIPES)

#define SYS_INTERLACE_SIZE  (4*1024)
#define SYS_SNAP_BUF_SZ     (SYS_INTERLACE_SIZE)
#define SYS_MAX_UPGRD_SCRIPT_SZ  (16*1024*1024)

typedef struct
{
    const char* mnt_path;
    const char* dev_path;
    int flag;
} sys_dfv_desc_t;

typedef struct
{
    cmi_intf_type       intf_type;
    sys_dfv_desc_t      dfv_desc_tbl[SYS_MAX_PIPES];
    
    ips_linkdesc_t      ips_linkdesc_tbl[SYS_MAX_PIPES];
    ips_epdesc_t        ips_desc_tbl[SYS_MAX_PIPES];

    cmi_endian          endian;
    const char*         ipaddr;
    int                 port;
    
#define SYSFEA_USE_LOCALSTATS       (0x01<<0)
    uint64_t            features;
#define SYSDBG_REC_NOTSAVE2DISK     (0x01<<0)
    uint64_t            dbg_flag;
} sys_env_t;

typedef struct
{
    struct list_head list;
    int         job_type;
    cmi_cmd_t*  cmd_ref;
    int         resp;
    uint64_t    arg;
} sys_jobq_node_t;
DECLARE_QUEUE(sys_jobq, sys_jobq_node_t);

typedef struct
{
    int wkr_id;
    int wkr_state;
    int reset_req;
    int quit_req;

    sys_jobq_t job_in;
    sys_jobq_t job_out;

    pthread_mutex_t buf_snap_lock;
    char    buf_snap[SYS_SNAP_BUF_SZ];

    pthread_t* wkr_thread;

    spk_stats_t stats;
} sys_wkr_ctx_t;

typedef struct
{
    int slot_id;
    uint64_t offset;
    uint64_t len;
    char* data;
    size_t slot_sz;
} sys_cache_t;

typedef struct
{
    struct  dfvcm_ctx* dfvcm;
    dfv_bufq_node_t* work_node;
} sys_ul_ctx_t;

typedef struct
{
    char* script_buf;
    size_t buf_sz;
} sys_upgrd_ctx_t;

typedef struct
{
    dfv_vault_t* vault;

    int sys_state;
    sys_wkr_ctx_t* wkr_ctx_tbl[SYS_MAX_PIPES];

    sys_cache_t* file_cache;
    ips_fcstat_lk1 fcstat[SYS_MAX_PIPES];
    size_t diskcap;
    int auto_rec;
    
    sys_ul_ctx_t* ul_ctx_tbl[SYS_MAX_PIPES];
    sys_upgrd_ctx_t* upgrd_ctx;

    cmi_intf_t* cmi_intf;
    
    int sysdown_req;
} sys_ctx_t;

extern zlog_category_t* sys_zc;
extern sys_env_t sys_env;
extern sys_ctx_t sys_ctx;

int sys_cmd_exec(cmi_cmd_t* cmd, size_t size);
int sys_cmd_exec_stopul(cmi_cmd_t* cmd);

void sys_change_state(int new_state);

uint32_t sys_systm_to_lktm(uint64_t systm);
uint64_t sys_lktm_to_systm(uint32_t lktm);

void* __sys_wkr_job(void* args);
void* __sys_wkr_autorec(void* args);
