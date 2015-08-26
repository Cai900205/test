
#ifndef __FVL_SRIO_H__
#define __FVL_SRIO_H__

//#include <usdpaa/compat.h>
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
#define FVL_SRIO_CTL_ADDR	0x20000000

#define FVL_SRIO_PORTNUM        2
//DATA BUFFER
#define FVL_SRIO_DMA_BUFSIZE    (16*1024*1024)
#define FVL_SRIO_DMA_CHANTNUM    4 // channel number
#define FVL_SRIO_DMA_SECTNUM     1
#define FVL_SRIO_DMA_WINSIZE    (FVL_SRIO_DMA_CHANTNUM*FVL_SRIO_DMA_BUFSIZE)
#define FVL_SRIO_DMA_POOLSIZE   (FVL_SRIO_PORTNUM*FVL_SRIO_DMA_WINSIZE*FVL_SRIO_DMA_SECTNUM)
#define FVL_SRIO_DMA_BLKBYTES   0x200000
#define FVL_SRIO_DMA_POOL_PORT_OFFSET (FVL_SRIO_DMA_WINSIZE*FVL_SRIO_DMA_SECTNUM)

//CTL BUFFER
#define FVL_SRIO_CTL_BUFSIZE    (1*1024*1024)
#define FVL_SRIO_CTL_BUFNUM     (FVL_SRIO_DMA_CHANTNUM)
#define FVL_SRIO_CTL_WINSIZE  	(FVL_SRIO_CTL_BUFSIZE*FVL_SRIO_CTL_BUFNUM)
#define FVL_SRIO_CTL_SECNUM     2
#define FVL_SRIO_CTL_POOLSIZE (FVL_SRIO_CTL_WINSIZE*FVL_SRIO_PORTNUM*FVL_SRIO_CTL_SECNUM)
#define FVL_SRIO_CTL_POOL_PORT_OFFSET  (FVL_SRIO_CTL_SECNUM*FVL_SRIO_CTL_WINSIZE)

#define FVL_SRIO_CTL_SUBBUF_NUM  8// sub buffer number

#define FVL_SRIO_CBMAX          16 //DMA MAX NUMBER

#define FVL_SRIO_BUFFER_NUMBER       (FVL_SRIO_DMA_CHANTNUM)//ctx add 

enum {
    FVL_SRIO_SWRITE,
    FVL_SRIO_NWRITE,
    FVL_SRIO_SWRITE_R,
    FVL_SRIO_NREAD,
};

typedef struct fvl_srio_bufhead {
    uint64_t block_num;    // based on 256B block
    uint64_t padding[31];
} fvl_srio_bufhead_t;

//ctx add
typedef struct fvl_srio_portpool {
    struct srio_port_info port_info;
    void      *range_virt;
    uint8_t   *pwrite_result[FVL_SRIO_BUFFER_NUMBER];
    uint8_t   *pread_result[FVL_SRIO_BUFFER_NUMBER];
    uint8_t   *pwrite_data[FVL_SRIO_BUFFER_NUMBER];
    dma_addr_t write_result[FVL_SRIO_BUFFER_NUMBER];
    dma_addr_t read_result[FVL_SRIO_BUFFER_NUMBER];
    dma_addr_t write_data[FVL_SRIO_BUFFER_NUMBER];
    uint8_t   *pwrite_ctl_result[FVL_SRIO_BUFFER_NUMBER];
    uint8_t   *pread_ctl_result[FVL_SRIO_BUFFER_NUMBER];
    uint8_t   *pwrite_ctl_data[FVL_SRIO_BUFFER_NUMBER];
    dma_addr_t write_ctl_result[FVL_SRIO_BUFFER_NUMBER];
    dma_addr_t read_ctl_result[FVL_SRIO_BUFFER_NUMBER];
    dma_addr_t write_ctl_data[FVL_SRIO_BUFFER_NUMBER];
} fvl_srio_portpool_t;

typedef struct fvl_srio_ctrlblk {
    struct dma_ch *dmadev;
    uint8_t        bfnum;
    uint8_t        port;
} fvl_srio_ctrlblk_t;

typedef struct fvl_dma_pool {
    dma_addr_t dma_phys_base;
    void      *dma_virt_base;
} fvl_dma_pool_t;

typedef struct fvl_srio_context {
    fvl_srio_ctrlblk_t  ctrlblk[FVL_SRIO_CBMAX];
    fvl_srio_portpool_t portpool[FVL_SRIO_PORTNUM];
} fvl_srio_context_t;


int fvl_srio_init(fvl_srio_context_t **ppsrio, uint32_t srio_type);
fvl_srio_ctrlblk_t * fvl_srio_getcb(uint32_t port, uint32_t tmid);
int fvl_srio_send(fvl_srio_ctrlblk_t *pscb, uint64_t src_phys, uint64_t dest_phys, uint32_t size);
int dma_usmem_init(fvl_dma_pool_t *pool,uint64_t pool_size,uint64_t dma_size);
int dma_pool_init(fvl_dma_pool_t **pool,uint64_t pool_size,uint64_t dma_size);

#endif // __FVL_SRIO_H__

