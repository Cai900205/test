
#ifndef __FVL_TASK_H__
#define __FVL_TASK_H__

#include "fvl_common.h"
#include "fvl_srio.h"

#define FVL_SRIO_FLAG_BIST      0x0001
#define FVL_SRIO_FLAG_CHAN      0x0002

//ctx add
typedef struct fvl_thread_arg {
    fvl_srio_context_t *psrio;
    void *priv;
    uint64_t stat_bytes;
    uint64_t stat_count;
    uint16_t port;
    uint16_t bfnum;
    uint16_t cpu;
    uint16_t flag;
} fvl_thread_arg_t;

void* fvl_srio_recver(void *arg);
void* fvl_srio_sender(void *arg);

#endif // __FVL_TASK_H__

