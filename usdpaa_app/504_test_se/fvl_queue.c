#include "fvl_queue.h"

void fvl_queue_init(fvl_queue_t *fqueue)
{
    int ret;
    ret = posix_memalign(&fqueue->buf,4096,FVL_QUEUE_SIZE);
    if(ret != 0)
    {
        printf("posix_memalign error!\n");
        return;
    }
    memset(fqueue->buf,0x5a,FVL_QUEUE_SIZE);
    fqueue->enqueue_count=0;
    fqueue->dequeue_count=0;
    fqueue->enqueue_count_complete=0;
    fqueue->dequeue_count_complete=0;
    pthread_mutex_init(&fqueue->enqueue_mutex,NULL);
    pthread_mutex_init(&fqueue->dequeue_mutex,NULL);
    return;
}

int fvl_enqueue(fvl_queue_t *fqueue)
{
    uint8_t result=fqueue->enqueue_count - fqueue->dequeue_count_complete;
    if(result >= FVL_QUEUE_BNUM)
    {
        return -1;
    }
    else
    {
        int i= fqueue->enqueue_count &(FVL_QUEUE_BNUM -1);
        fqueue->enqueue_count=fqueue->enqueue_count+1;   
        return i;
    }
}

int fvl_dequeue(fvl_queue_t *fqueue,uint64_t count)
{
    uint8_t ret = fqueue->enqueue_count_complete-fqueue->dequeue_count;
    if(ret>=count)
    {
        int i= fqueue->dequeue_count &(FVL_QUEUE_BNUM -1);
        fqueue->dequeue_count=fqueue->dequeue_count+count;   
        return i;
    }
    else
    {
        return -1;
    }
}
void fvl_enqueue_complete(fvl_queue_t *fqueue,uint64_t count)
{
    pthread_mutex_lock(&fqueue->enqueue_mutex);
    fqueue->enqueue_count_complete=fqueue->enqueue_count_complete+count;   
    pthread_mutex_unlock(&fqueue->enqueue_mutex);
}
void fvl_dequeue_complete(fvl_queue_t *fqueue,uint64_t count)
{
    pthread_mutex_lock(&fqueue->dequeue_mutex);
    fqueue->dequeue_count_complete=fqueue->dequeue_count_complete+count;   
    pthread_mutex_unlock(&fqueue->dequeue_mutex);
}

