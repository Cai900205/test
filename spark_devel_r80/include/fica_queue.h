#ifndef __FICA_QUEUE_H__
#define __FICA_QUEUE_H__

#define DECLARE_QUEUE(NAME, OBJTYPE)                                            \
typedef struct                                                                  \
{                                                                               \
    pthread_mutex_t lock;                                                       \
    uint64_t count;                                                             \
    struct list_head listhead;                                                  \
} NAME##_t;                                                                     \
                                                                                \
static inline void NAME##_init(NAME##_t* q)                                     \
{                                                                               \
    pthread_mutex_init(&q->lock, NULL);                                         \
    INIT_LIST_HEAD(&q->listhead);                                               \
    q->count = 0;                                                               \
}                                                                               \
                                                                                \
static inline void NAME##_enqueue(NAME##_t* q, OBJTYPE* node)                   \
{                                                                               \
    pthread_mutex_lock(&q->lock);                                               \
    list_add_tail(&node->list, &q->listhead);                                   \
    q->count++;                                                                 \
    pthread_mutex_unlock(&q->lock);                                             \
}                                                                               \
                                                                                \
static inline OBJTYPE* NAME##_dequeue(NAME##_t* q)                              \
{                                                                               \
    OBJTYPE* node = NULL;                                                       \
                                                                                \
    if (!list_empty(&q->listhead)) {                                            \
        pthread_mutex_lock(&q->lock);                                           \
        if (!list_empty_careful(&q->listhead)) {                                \
            node = list_entry(q->listhead.next, OBJTYPE, list);                 \
            list_del_init(&node->list);                                         \
            q->count--;                                                         \
        }                                                                       \
        pthread_mutex_unlock(&q->lock);                                         \
    }                                                                           \
                                                                                \
    return(node);                                                               \
}                                                                               \
                                                                                \
static inline uint64_t NAME##_get_count(NAME##_t* q)                            \
{                                                                               \
    uint64_t count = -1;                                                        \
    pthread_mutex_lock(&q->lock);                                               \
    count = q->count;                                                           \
    pthread_mutex_unlock(&q->lock);                                             \
    return(count);                                                              \
}                                                                               \

#endif
