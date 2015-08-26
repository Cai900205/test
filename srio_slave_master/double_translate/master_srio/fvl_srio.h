
#ifndef __FVL_SRIO_H__
#define __FVL_SRIO_H__

#include <usdpaa/compat.h>
#include <usdpaa/of.h>
#include <usdpaa/dma_mem.h>
#include <usdpaa/fsl_dma.h>
#include <usdpaa/fsl_srio.h>
#include <error.h>
#include <atb_clock.h>
#include <readline.h>
#include <assert.h>
#include <inttypes.h>
#include <unistd.h>
#include "fvl_common.h"

#define FVL_SRIO_SYS_ADDR       0x10000000
#define FVL_SRIO_CTL_ADDR	0x01000000

#define FVL_SRIO_HEAD_ADDR	0x01000000

#define FVL_CTL_WIN_SIZE (3*256)
#define FVL_SRIO_PORT_NUM        2
#define FVL_PORT_DMA_NUM          8 //DMA MAX NUMBER
#define FVL_SRIO_CBMAX          (FVL_SRIO_PORT_NUM*FVL_PORT_DMA_NUM) //DMA MAX NUMBER

#define FVL_BASE_LAW 11
#define FVL_BASE_LAW_SIZE  0x1000

//default value

#define DE_BUF_SIZE 0x100000 
#define DE_BUF_NUM  0x10 
#define DE_CHAN_NUM 1
#define DE_CHAN_SIZE DE_BUF_SIZE*DE_BUF_NUM
#define DE_DMA_READ_SIZE  DE_CHAN_SIZE*DE_CHAN_NUM 

enum {
    FVL_SRIO_SWRITE,
    FVL_SRIO_NWRITE,
    FVL_SRIO_SWRITE_R,
    FVL_SRIO_NREAD,
};

typedef struct fvl_dma_pool {
    dma_addr_t dma_phys_base;
    void      *dma_virt_base;
} fvl_dma_pool_t;

typedef struct fvl_srio_init_param {
    uint8_t  mode; 
    uint32_t buf_size;
    uint32_t port_num;
    uint32_t buf_num;
    uint32_t chan_size;
    uint32_t chan_num;
    uint32_t target_id;
    fvl_dma_pool_t *port_rd_buf;
}fvl_srio_init_param_t;

//ctx add
typedef struct fvl_srio_portpool {
    struct srio_port_info port_info;
    void      *range_virt;
    uint8_t   *pwrite_result;
    uint8_t   *pread_result;
    uint8_t   *pwrite_data;
    dma_addr_t write_result;
    dma_addr_t read_result;
    dma_addr_t write_data;
    uint8_t   *pwrite_ctl_result;
    uint8_t   *pread_ctl_result;
    uint8_t   *pwrite_ctl_data;
    dma_addr_t write_ctl_result;
    dma_addr_t read_ctl_result;
    dma_addr_t write_ctl_data;
    uint64_t ctl_info_start;
} fvl_srio_portpool_t;
typedef struct fvl_srio_ctl_info {
    uint16_t fla;
    uint16_t com;
    uint16_t rev[2];
    uint64_t rev_info[31];
} fvl_srio_ctl_info_t;

typedef struct fvl_srio_head_info {
    uint8_t re_flag;  //receive mark
    uint8_t uflag;   // channel is or not use mark
    uint8_t rev[6];
    uint32_t buf_size;
    uint32_t buf_num;
    uint32_t chan_num;
    uint32_t chan_size;
    uint64_t data_re_addr;
    uint64_t data_se_addr;
    uint64_t ctl_re_addr;
    uint64_t ctl_se_addr;
    uint64_t REV_INFO[25];
} fvl_srio_head_info_t;

typedef struct fvl_srio_ctrlblk {
    struct dma_ch *dmadev;
    uint8_t        bfnum; //ctx dma init param 0~7
    uint8_t        port;
} fvl_srio_ctrlblk_t;


typedef struct fvl_srio_context {
    fvl_srio_ctrlblk_t  ctrlblk[FVL_SRIO_CBMAX];
    fvl_srio_portpool_t portpool[FVL_SRIO_PORT_NUM];
    struct srio_dev *sriodev;
    pthread_t chan_id[FVL_SRIO_PORT_NUM];

} fvl_srio_context_t;

typedef struct fvl_srio_channel {
    fvl_srio_ctrlblk_t  chanblk;
    fvl_srio_portpool_t chanpool;
    pthread_t chan_id;
} fvl_srio_channel_t;

typedef struct fvl_srio_ctable {
    char name[20];
    int  port;
    int  chan;
    int  flag;
} fvl_srio_ctable_t;

typedef struct fvl_ctl_thread_arg {
    int fd;
    uint32_t port_num;
    uint8_t *buf_virt;
}fvl_ctl_thread_t;

typedef struct fvl_head_thread_arg {
    uint8_t op_mode;  //0 ---- head_port 1---- head_channel
    uint32_t num;
    uint8_t *buf_virt;
}fvl_head_thread_t;

typedef struct fvl_read_rvl {
    uint32_t num;
    uint32_t len;
    void *buf_virt;
}fvl_read_rvl_t;

int fvl_get_law(uint32_t num);
int fvl_get_channel(char *name);
int fvl_srio_channel_open(char *name);
int fvl_srio_channel_close(int fd);
int fvl_srio_init(fvl_srio_init_param_t *param);
int fvl_srio_finish();
void fvl_srio_recv_ctl(void *arg);
void fvl_srio_recv_head(void *arg);
fvl_srio_ctrlblk_t * fvl_srio_getcb(uint32_t port, uint32_t tmid);
int fvl_srio_send(struct dma_ch *dmadev, uint64_t src_phys, uint64_t dest_phys, uint32_t size);
int fvl_srio_read(int fd,fvl_read_rvl_t *buf);
int fvl_srio_write(int fd,void  *buf,uint32_t length);
int fvl_srio_read_feedback(int fd, int num);
int dma_usmem_init(fvl_dma_pool_t *pool,uint64_t pool_size,uint64_t dma_size);
int dma_pool_init(fvl_dma_pool_t **pool,uint64_t pool_size,uint64_t dma_size);

#endif // __FVL_SRIO_H__

