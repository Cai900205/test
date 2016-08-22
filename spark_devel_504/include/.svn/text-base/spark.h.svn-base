#ifndef __SPARK_H__
#define __SPARK_H__
#include <string.h>
#include <sys/time.h>
#include <inttypes.h>
#include <pthread.h>
#include "fica_list.h"
#include "fica_queue.h"

#define SPK_SUCCESS   (0)

#define SPKERR_BASE  (1000)

#define SPKERR_PARAM    -(SPKERR_BASE +  1)
#define SPKERR_LOGSYS   -(SPKERR_BASE +  2)
#define SPKERR_BADSEQ   -(SPKERR_BASE +  3)
#define SPKERR_BADRES   -(SPKERR_BASE +  4)
#define SPKERR_EACCESS  -(SPKERR_BASE +  5)
#define SPKERR_EAGAIN   -(SPKERR_BASE +  6)
#define SPKERR_RESETSYS -(SPKERR_BASE +  7)
#define SPKERR_TIMEOUT  -(SPKERR_BASE +  8)


#define SPK_MAX_PATHNAME    (1024)

#define SAFE_RELEASE(PTR)   if(PTR){free(PTR);PTR=NULL;}
#define MAX(X, Y)           (((X)>(Y))?(X):(Y))
#define MIN(X, Y)           (((X)>(Y))?(Y):(X))
#define GET_TICK_COUNT(tm)  (uint64_t)((uint64_t)tm.tv_sec*1000 + (uint64_t)tm.tv_usec/1000)
#define LOW32(U64)          ((uint32_t)(U64))
#define HGH32(U64)          ((uint32_t)((U64)>>32))
#define MAKE64(H, L)        (((uint64_t)(H))<<32 | (uint64_t)(L))
#define BYTE2MB(B)          ((B)/(1000*1000))
#ifndef ALIGN
#define ALIGN(data, align)  ((data+align-1) & (-align))
#endif
#define unlikely(x) __builtin_expect(!!(x), 0)
#define likely(x)   __builtin_expect(!!(x), 1)

typedef enum {
    SPK_DIR_NONE = -1,
    SPK_DIR_READ = 0,
    SPK_DIR_WRITE = 1,
    SPK_MAX_DIR
} SPK_DIR;

typedef struct spk_stats_elem
{
    uint64_t bytes;
    uint64_t pkts;
} spk_stats_elem_t;

typedef struct spk_stats
{
    uint64_t start;
    uint64_t last_tick;
    spk_stats_elem_t xfer;
    spk_stats_elem_t xfer_last;
    spk_stats_elem_t error;
    spk_stats_elem_t overflow;
    spk_stats_elem_t drop;    
} spk_stats_t;

#define MAKE_VER_STR(MA, MI, D)     #MA#MI#D
#define MAKE_VER_INT(MA, MI, D)     ((((uint32_t)MA) << 28) | (((uint32_t)MI) << 24) | (((uint32_t)D)))

static inline const char* spk_desc_dir2str(SPK_DIR dir)
{
    switch(dir) {
    case SPK_DIR_NONE:      return("NONE"); break;  
    case SPK_DIR_READ:      return("READ"); break;  
    case SPK_DIR_WRITE:     return("WRITE"); break;  
    default:
        break;
    }
    return("<UNKNOWN>");
}

static inline uint64_t spk_get_tick_count(void)
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return(GET_TICK_COUNT(tv));
}

static inline void spk_stats_reset(spk_stats_t* s)
{
    memset(s, 0, sizeof(spk_stats_t));
}

static inline uint64_t spk_stats_get_time_elapsed(spk_stats_t* s)
{
    return((s->start > 0)?(spk_get_tick_count() - s->start) : 0);
}

static inline double spk_stats_get_bps_overall(spk_stats_t* s)
{
    if (!s->start)
        return 0;

    uint64_t now = spk_get_tick_count();
    if (now <= s->start) {
        return 0;
    }
    double rate = (s->xfer.bytes) / (now - s->start); // bytes per ms
    return(rate * 1000); // bytes per second
}

static inline double spk_stats_get_bps_wire(spk_stats_t* s)
{
    if (!s->start)
        return 0;

    uint64_t now = spk_get_tick_count();
    if (now <= s->last_tick) {
        return 0;
    }
    uint64_t bytes = s->xfer.bytes - s->xfer_last.bytes;
    double rate = (bytes) / (now - s->last_tick); // bytes per ms

    s->xfer_last.bytes = s->xfer.bytes;
    s->last_tick = now;
    return(rate * 1000); // bytes per second
}

#define DECL_SPK_STATS_INC(CAT) \
static inline void spk_stats_inc_##CAT(spk_stats_t* s, uint64_t b, uint64_t p) \
{ \
    s->CAT.bytes += b; \
    s->CAT.pkts += p; \
    if(unlikely(s->start <= 0)) { \
        s->last_tick = s->start = spk_get_tick_count(); \
    } \
}
DECL_SPK_STATS_INC(xfer);
DECL_SPK_STATS_INC(error);
DECL_SPK_STATS_INC(overflow);
DECL_SPK_STATS_INC(drop);

#define DECL_SPK_STATS_GETBYTES(CAT) \
static inline uint64_t spk_stats_get_##CAT##_bytes(spk_stats_t* s) \
{ \
    return(s->CAT.bytes); \
}
DECL_SPK_STATS_GETBYTES(xfer)
DECL_SPK_STATS_GETBYTES(error)
DECL_SPK_STATS_GETBYTES(overflow)
DECL_SPK_STATS_GETBYTES(drop)

#define DECL_SPK_STATS_GETPKTS(CAT) \
static inline uint64_t spk_stats_get_##CAT##_pkts(spk_stats_t* s) \
{ \
    return(s->CAT.pkts); \
}

DECL_SPK_STATS_GETPKTS(xfer)
DECL_SPK_STATS_GETPKTS(error)
DECL_SPK_STATS_GETPKTS(overflow)
DECL_SPK_STATS_GETPKTS(drop)

static inline void spk_worker_set_affinity(int cpu)
{
#ifdef ARCH_ppc64
    int ret;
    cpu_set_t cpuset;

    CPU_ZERO(&cpuset);
    CPU_SET(cpu, &cpuset);
    ret = pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);
    assert(!ret);
#endif
    return;
}

static inline int spk_os_exec(const char* cmd)
{
    fprintf(stderr, "exec cmd: cmd=\'%s\'\n", cmd);
    return(system(cmd));
}
#endif
