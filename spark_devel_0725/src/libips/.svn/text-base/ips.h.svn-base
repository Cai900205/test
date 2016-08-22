#ifndef __IPS_INTERNAL_H__
#define __IPS_INTERNAL_H__

#include <sched.h>
#include <error.h>
#include <assert.h>
#include <inttypes.h>
#include <unistd.h>
#include <usdpaa/compat.h>
#include <usdpaa/of.h>
#include <usdpaa/dma_mem.h>
#include <usdpaa/fsl_dma.h>
#include <usdpaa/fsl_srio.h>
#include <atb_clock.h>
#include "spark.h"
#include "ips/ips_intf.h"

#define IPS_PROT_VERSION            (0x01)     // protocol version
#define IPS_MAGIC                   (0xf1a10000 | IPS_PROT_VERSION)
#define IPS_MODE_VERSION            (0x01)

#define IPS_MAX_SECTOR_SIZE         (4*1024*1024)
#define IPS_MAX_SECTORS_IN_CLS      (64*1024)
#define IPS_SECTOR_SZ_MULTIPLIER    (64*1024)
#define IPS_MAX_TX_SECTOR_SZ        (128*1024*1024)
#define IPS_DESC_SPACE_SIZE         (1*1024*1024)
#define IPS_DESC_SIZE               (0x20)
#define IPS_SRIOWND_SIZE_ALL        (512*1024*1024) // 512M 
#define IPS_SRIOWND_CTR_BASE        (448*1024*1024) // 
#define IPS_SRIOWND_DXR_BASE        (0)

// IPS memory map definition
#define IPS_MM_CTLREG_BASE  (0x01000000)
#define IPS_MM_CTLREG_SIZE  (0x01000000) // 16M
#define IPS_MM_DXREG_BASE   (0x10000000)
#define IPS_MM_DXREG_SIZE   (0x10000000) // 256M 

#define IPS_IBID_CTLREG     (1)
#define IPS_IBID_DXREG      (IPS_IBID_CTLREG + 1)
#define IPS_OBID_CTLREG     (1)
#define IPS_OBID_DXREG      (IPS_OBID_CTLREG + IPS_MAX_PCS_IN_EP)

typedef struct ips_dma_addr {
    dma_addr_t  pa;
    void*       va;
    struct dma_mem* dma_mem;
} ips_dma_addr_t;

typedef struct __ips_mm_cmd_repo {
    uint8_t     dummy[0x1000]; // ips_mailbox_t optype: cmd/ack
} __ips_mm_cmd_repo_t;

typedef struct __ips_mm_stat_repo {
    uint8_t     ch_stat[0x100]; // ips_mailbox_t optype:bl1
    uint8_t     rx_stat[0x100]; // ips_mailbox_t optype:ntf
    uint8_t     tx_stat[0x100]; // ips_mailbox_t optype:ntf
    uint8_t     rsvd[0x0D00];
} __ips_mm_stat_repo_t;

// PCB: 16KB
typedef struct __ips_mm_pcb {
    __ips_mm_cmd_repo_t     cmd_repo;
    __ips_mm_stat_repo_t    stat_repo;
    uint8_t         rsvd[0x2000];
} __ips_mm_pcb_t;

typedef struct __ips_mm_ctlreg {
    __ips_mm_pcb_t    pcbs[IPS_MAX_PCS_IN_EP];
} __ips_mm_ctlreg_t;

#define IPS_SIZE_PCB      (sizeof(__ips_mm_pcb_t))
#define IPS_OFFSET_PCB_IN_CTLREG(PC_ID)     (offsetof(__ips_mm_ctlreg_t, pcbs[PC_ID]))
#define IPS_OFFSET_CMDREPO_IN_PCB           (offsetof(__ips_mm_pcb_t, cmd_repo))
#define IPS_OFFSET_STATREPO_IN_PCB          (offsetof(__ips_mm_pcb_t, stat_repo))
#define IPS_OFFSET_CHSTAT_IN_REPO           (offsetof(__ips_mm_stat_repo_t, ch_stat))
#define IPS_OFFSET_RXSTAT_IN_REPO           (offsetof(__ips_mm_stat_repo_t, rx_stat))
#define IPS_OFFSET_TXSTAT_IN_REPO           (offsetof(__ips_mm_stat_repo_t, tx_stat))

typedef enum {
    IPS_PHASE_UNKNOWN = 0x0,
    IPS_PHASE_INIT,
    IPS_PHASE_IDLE,
    IPS_PHASE_RX,
    IPS_PHASE_TX,
} IPS_PHASE;

typedef uint64_t IPS_RWPTR;
typedef struct ips_dx_inst {
    IPS_RWPTR    wptr;
    IPS_RWPTR    rptr;
} ips_dx_inst_t;

typedef struct ips_pcctx {
    int             pc_id;
    IPS_PHASE       phase;
    SPK_DIR         dir;
    pthread_t*      worker_thread;
    ips_pcdesc_t*   desc;
    struct ips_epctx* epctx;
    pthread_mutex_t* ctx_lock;
    int             open_cnt;

    // ctrl_region - rx
    void*           rx_ctr_pcb_va;

    // ctrl_region - tx
    ips_dma_addr_t  tx_ctr_pcb;
    dma_addr_t      swnd_ctr_pcb_pa;
    struct dma_ch*  dmach_ctr;
    pthread_mutex_t dmach_ctr_lock;

    // dx_region - rx
    ips_dma_addr_t  rx_dxr_base;
    uint64_t        rx_dxr_offset;
    void*           rx_dxr_va_tbl[IPS_MAX_SECTORS_IN_CLS];
    ips_dx_inst_t   rx_inst;

    // dx_region - tx
    uint64_t        tx_dxr_offset;
    ips_dma_addr_t  tx_dxr_sector;
    dma_addr_t      swnd_dxr_pa_base;
    dma_addr_t      swnd_dxr_pa_tbl[IPS_MAX_SECTORS_IN_CLS];
    struct dma_ch*  dmach_dxr;
    pthread_mutex_t dmach_dxr_lock;
    ips_dx_inst_t   tx_inst;
    
    //link_dsc_region - tx 
    ips_dma_addr_t  desc_sector;
    ips_dx_inst_t   desc_inst;
    uint32_t        desc_num;
    //link_dsc_region - rx
    dma_addr_t      desc_base;
    // statistics
    spk_stats_t     stats_xfer;
} ips_pcctx_t;

#define IPS_GET_MYEPID(PCCTX)  (PCCTX->desc->src_id)

typedef struct ips_epctx {
    ips_epdesc_t    desc;

    ips_dma_addr_t  rx_ctr_base;
    ips_dma_addr_t  tx_swnd_base;

    ips_pcctx_t     pcctx_tbl[IPS_MAX_PCS_IN_EP];
} ips_epctx_t;

#define IPS_OPTYPE_CMD      (0x0)
#define IPS_OPTYPE_ACK      (0x1)
#define IPS_OPTYPE_NTF      (0x2)
#define IPS_OPTYPE_BL1      (0x80)

#define MAKE_OPCODE(TYPE, CODE) ((TYPE<<8) | CODE)
#define IPS_GET_OPTYPE(OPCODE)  ((OPCODE & 0x0000FF00)>>8)
#define IPS_GET_OPCMD(OPCODE)   ((OPCODE & 0x000000FF)>>8)

#define IPS_OPCMD_INIT     MAKE_OPCODE(IPS_OPTYPE_CMD, 0x01)
#define IPS_OPCMD_START    MAKE_OPCODE(IPS_OPTYPE_CMD, 0x02)
#define IPS_OPCMD_STOP     MAKE_OPCODE(IPS_OPTYPE_CMD, 0x03)
#define IPS_OPCMD_CONFIG   MAKE_OPCODE(IPS_OPTYPE_CMD, 0x04)

#define IPS_OPACK_INIT     MAKE_OPCODE(IPS_OPTYPE_ACK, 0x01)
#define IPS_OPACK_START    MAKE_OPCODE(IPS_OPTYPE_ACK, 0x02)
#define IPS_OPACK_STOP     MAKE_OPCODE(IPS_OPTYPE_ACK, 0x03)
#define IPS_OPACK_CONFIG   MAKE_OPCODE(IPS_OPTYPE_ACK, 0x04)

#define IPS_OPNTF_DMADONE  MAKE_OPCODE(IPS_OPTYPE_NTF, 0x01)
#define IPS_OPNTF_FREEBUF  MAKE_OPCODE(IPS_OPTYPE_NTF, 0x02)

#define IPS_OPBL1_FBSTATUS MAKE_OPCODE(IPS_OPTYPE_BL1, 0x01)

typedef struct ips_mb_header {
    uint32_t magic;
    uint32_t seq;
    uint32_t opcode;
    uint32_t rsvd;
} ips_mb_header_t;

typedef struct ips_mailbox {
    ips_mb_header_t hdr;
    uint8_t  payload[0x100 - sizeof(ips_mb_header_t)];
} ips_mailbox_t;
#define IPS_MAX_MAILBOX_SIZE    (sizeof(ips_mailbox_t))
#define IPS_MAX_MAILBOX_PLSIZE  (sizeof(ips_mailbox_t) - offsetof(ips_mailbox_t, payload))

typedef struct ips_cls_def {
    uint32_t mm_offset;
    uint16_t sector_num;
    uint16_t sector_sz;
} ips_cls_def_t;

typedef struct ips_pl_init {
    ips_cls_def_t local;
    ips_cls_def_t remote;
} ips_pl_init_t;

typedef struct ips_pl_init_r {
    uint32_t capacity;
} ips_pl_init_r_t;

typedef struct ips_pl_start {
    uint32_t dir;
    uint32_t mode;
} ips_pl_start_t;

typedef struct ips_pl_start_r {
    uint32_t status;
} ips_pl_start_r_t;

typedef struct ips_pl_config {
    uint8_t config_data[0x100 - sizeof(ips_mb_header_t)];
} ips_pl_config_t;

typedef struct ips_pl_config_r {
    uint32_t status;
} ips_pl_config_r_t;

typedef struct ips_pl_sync {
    IPS_RWPTR wptr;
    IPS_RWPTR rptr;
} ips_pl_sync_t;

extern ips_mode_t ips_mode;
#include "zlog/zlog.h"
extern zlog_category_t* ips_zc;
#define IPS_EPID_UNKNOWN        (0xff)
#define IPS_LOG(LEVEL, EPID, ...) \
    zlog(ips_zc, __FILE__, sizeof(__FILE__)-1, __func__, sizeof(__func__)-1, __LINE__, \
        LEVEL, __VA_ARGS__);
#define IPS_LOGINFO(EPID,FMT,...)   \
    if (EPID != IPS_EPID_UNKNOWN) {\
        zlog_info(ips_zc, "ep#%02x> "FMT, EPID, __VA_ARGS__); \
    } else {\
        zlog_info(ips_zc, FMT, __VA_ARGS__); \
    }
#define IPS_LOGDEBUG(EPID,FMT,...)   \
    if (EPID != IPS_EPID_UNKNOWN) {\
        zlog_debug(ips_zc, "ep#%02x> "FMT, EPID, __VA_ARGS__); \
    } else {\
        zlog_debug(ips_zc, FMT, __VA_ARGS__); \
    }
#define IPS_LOGNOTICE(EPID,FMT,...)   \
    if (EPID != IPS_EPID_UNKNOWN) {\
        zlog_notice(ips_zc, "ep#%02x> "FMT, EPID, __VA_ARGS__); \
    } else {\
        zlog_notice(ips_zc, FMT, __VA_ARGS__); \
    }
#define IPS_LOGERROR(EPID,FMT,...)   \
    if (EPID != IPS_EPID_UNKNOWN) {\
        zlog_error(ips_zc, "ep#%02x> "FMT, EPID, __VA_ARGS__); \
    } else {\
        zlog_error(ips_zc, FMT, __VA_ARGS__); \
    }
#define IPS_LOGWARN(EPID,FMT,...)   \
    if (EPID != IPS_EPID_UNKNOWN) {\
        zlog_warn(ips_zc, "ep#%02x "FMT, EPID, __VA_ARGS__); \
    } else {\
        zlog_warn(ips_zc, FMT, __VA_ARGS__); \
    }
#define IPS_LOGFATAL(EPID,FMT,...)   \
    if (EPID != IPS_EPID_UNKNOWN) {\
        zlog_fatal(ips_zc, "ep#%02x "FMT, EPID, __VA_ARGS__); \
    } else {\
        zlog_fatal(ips_zc, FMT, __VA_ARGS__); \
    }

void* ips_dmamem_alloc(size_t pool_size, dma_addr_t* pa, struct dma_mem** dma_mem);

ssize_t ips_mb_read(ips_pcctx_t* pcctx, uint64_t repo_offset, ips_mailbox_t* cmd_out);
ssize_t ips_mb_write(ips_pcctx_t* pcctx, uint64_t repo_offset,
                  int opcode, void* payload, size_t pl_size);
ssize_t ips_data_write(ips_pcctx_t* pcctx, void* data, size_t size);

int ips_cmd_dispatch(ips_pcctx_t* pcctx, int opcode, void* payload, size_t pl_size, ips_mailbox_t* ack_cmd);
int ips_cmd_execute(ips_pcctx_t* pcctx, ips_mailbox_t* cmd_in);

int ips_chan_init_dxrtx(ips_pcctx_t* pcctx);
int ips_chan_init_dxrrx(ips_pcctx_t* pcctx);
int ips_chan_init_desc(ips_pcctx_t* pcctx);/*add*/
void ips_chan_shift_phase(ips_pcctx_t* pcctx, IPS_PHASE new_phase);

const char* ips_desc_opcode2str(int opcode);
#endif // __IPS_INTERNEL_H__

