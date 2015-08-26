/*
 *author:ctx
 *
 *date:2014/11/20
 */

#ifndef __FVL_QUEUE_H__
#define __FVL_QUEUE_H__

#include "fvl_common.h"
#include <pthread.h>
#include <unistd.h>

#define FVL_QUEUE_SIZE 0x8000000
#define FVL_QUEUE_BSIZE 0x200000

#define FVL_QUEUE_BNUM (FVL_QUEUE_SIZE/FVL_QUEUE_BSIZE)


typedef struct fvl_queue{
    uint64_t enqueue_count;
    uint64_t enqueue_count_complete;
    uint64_t dequeue_count;
    uint64_t dequeue_count_complete;
    pthread_mutex_t enqueue_mutex;
    pthread_mutex_t dequeue_mutex;
    void *buf;
}fvl_queue_t;

void fvl_queue_init(fvl_queue_t *fqueue);
int fvl_enqueue(fvl_queue_t *fqueue);
int fvl_dequeue(fvl_queue_t *fqueue,uint64_t count);
void fvl_enqueue_complete(fvl_queue_t *fqueue,uint64_t count);
void fvl_dequeue_complete(fvl_queue_t *fqueue,uint64_t count);

#endif
