
#ifndef __FVL_SRIO_H__
#define __FVL_SRIO_H__

#include <usdpaa/compat.h>
#include <usdpaa/of.h>
#include <usdpaa/dma_mem.h>
#include <usdpaa/fsl_dma.h>
#include <usdpaa/fsl_srio.h>
#include <error.h>
#include <atb_clock.h>
//#include <readline.h>
#include <assert.h>
#include <inttypes.h>
#include <unistd.h>
#include "fvl_common.h"

#define FVL_SRIO_SYS_ADDR       0x10000000
#define FVL_SRIO_CTL_ADDR	0x01000000

// CTL SIZE 
#define FVL_CTL_WIN_SIZE  0x400
#define FVL_CTL_PACKET_SIZE 0x100
#define FVL_CTL_HEAD_SIZE 0x1000


#define FVL_SRIO_PORT_NUM        2
#define FVL_PORT_DMA_NUM          8 //DMA MAX NUMBER
#define FVL_PORT_CHAN_NUM_MAX        4

#define FVL_CHAN_NUM_MAX        (FVL_SRIO_PORT_NUM*FVL_PORT_CHAN_NUM_MAX)

#define FVL_SRIO_CBMAX          (FVL_SRIO_PORT_NUM*FVL_PORT_DMA_NUM) //DMA MAX NUMBER

#define FVL_BASE_LAW 11
#define FVL_BASE_LAW_SIZE  0x1000

// operation type
#define FVL_BUFSIZE_unit 0x400

//CMD
#define FVL_SRIO_SYMBOL 0xf1A10001
#define FVL_SRIO_STATUS 0x00000001

#define FVL_INIT_CMD    0x00000001
#define FVL_INIT_READY  0x00000101

#define FVL_START_CMD   0x00000002
#define FVL_START_READY 0x00000102

#define FVL_STOP_CMD    0x00000003
#define FVL_STOP_READY  0x00000103

//add
#define FVL_INIT_CHAN_READY     0x00000104

//#define FVL_SRIO_VESION_TYPE    0x03


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

typedef struct fvl_cluster {
    uint32_t cluster_addr;
    uint16_t buf_num;
    uint16_t buf_size;
} fvl_cluster_t;

typedef struct fvl_chan_cluster {
    fvl_cluster_t receive_cluster;
    fvl_cluster_t send_cluster;
} fvl_chan_cluster_t;

typedef struct fvl_srio_init_param {
    uint8_t  mode;//slave--1,master--0; 
    uint32_t port_num;
    uint32_t buf_size[FVL_PORT_CHAN_NUM_MAX];
    uint32_t buf_num[FVL_PORT_CHAN_NUM_MAX];
    uint32_t chan_num;
    uint32_t target_id;
    uint32_t source_id;
    uint64_t version_mode;//slave use 006
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
    uint64_t   ctl_info_start;
} fvl_srio_portpool_t;
//change

typedef struct fvl_srio_ctl_info {
    uint32_t symbol;
    uint32_t status;
    uint64_t write_point;  
    uint64_t read_point; 
} fvl_srio_ctl_info_t;

typedef struct fvl_srio_head_info {
    uint32_t symbol;
    uint32_t status;
    uint32_t cmd;
    fvl_chan_cluster_t channel[FVL_PORT_CHAN_NUM_MAX];
} fvl_srio_head_info_t;

typedef struct fvl_srio_response_info {
    uint32_t symbol;
    uint32_t status;
    uint32_t cmd_ack;
    uint64_t vs_type;
} fvl_srio_response_info_t;
//end
typedef struct fvl_srio_ctrlblk {
    struct dma_ch *dmadev;
    uint8_t        bfnum; //ctx dma init param 0~7
    uint8_t        port;
} fvl_srio_ctrlblk_t;


typedef struct fvl_srio_context {
    fvl_srio_ctrlblk_t  ctrlblk[FVL_SRIO_CBMAX];
    fvl_srio_portpool_t portpool[FVL_SRIO_PORT_NUM];
    struct srio_dev *sriodev;
    uint32_t chan_num[FVL_SRIO_PORT_NUM]; //
    pthread_t chan_id[FVL_SRIO_PORT_NUM]; //
} fvl_srio_context_t;

//need add
typedef struct fvl_srio_channel {
    fvl_srio_ctrlblk_t  chanblk;
    fvl_srio_ctrlblk_t  rechanblk;
    fvl_srio_portpool_t chanpool;
// channel info
    uint32_t chan_size;
    uint32_t buf_size;
    uint32_t buf_num;
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
    int cpu;
    uint32_t port_num;
    uint8_t *buf_virt;
}fvl_ctl_thread_t;

typedef struct fvl_head_thread_arg {
    uint8_t op_mode;  //0 ---- head_port 1---- head_channel
    int cpu;
    uint32_t num;
    uint8_t *buf_virt;
}fvl_head_thread_t;

typedef struct fvl_read_rvl {
    uint32_t num;
    uint32_t len;
    void *buf_virt;
}fvl_read_rvl_t;
/*
typedef struct fvl_fiber_status {
    uint8_t fc_link_state;
    uint8_t srio_link_state;
    uint64_t 
}£»*/


int fvl_get_law(uint32_t num);
int fvl_get_channel(char *name);
int fvl_srio_channel_open(char *name);
int fvl_srio_channel_open_slave(int fd);
int fvl_srio_channel_open_master(int fd);
int fvl_srio_channel_close(int fd);
int fvl_srio_init(fvl_srio_init_param_t *param);
int fvl_srio_init_master(fvl_srio_init_param_t *param);
int fvl_srio_init_slave(fvl_srio_init_param_t *param);
int fvl_srio_finish();
void* fvl_srio_recv_ctl(void *arg);
void fvl_srio_rese_ctl(fvl_ctl_thread_t *priv);
int fvl_srio_recv_head_master(fvl_head_thread_t *priv);
void* fvl_srio_recv_head_slave(void *arg);
fvl_srio_ctrlblk_t * fvl_srio_getcb(uint32_t port, uint32_t tmid);
int fvl_srio_send(struct dma_ch *dmadev, uint64_t src_phys, uint64_t dest_phys, uint32_t size);
int fvl_srio_read(int fd,fvl_read_rvl_t *buf);
int fvl_srio_write(int fd,uint64_t phys,uint32_t length);
int fvl_srio_read_feedback(int fd, int num);
int dma_usmem_init(fvl_dma_pool_t *pool,uint64_t pool_size,uint64_t dma_size);
int dma_pool_init(fvl_dma_pool_t **pool,uint64_t pool_size,uint64_t dma_size);
int fvl_get_fiber_status(int port_num,char *data);
int fvl_fiber_op_cmd(int port_num,int cmd);

#endif // __FVL_SRIO_H__

