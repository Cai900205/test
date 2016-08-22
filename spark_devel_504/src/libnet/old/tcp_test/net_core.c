#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <inttypes.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <pthread.h>
#include <assert.h>
#include <errno.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <netinet/tcp.h>

#include "net.h"

zlog_category_t* net_zc=NULL;

int net_module_init(const char* log_cat)
{
    if (net_zc) {
        fprintf(stderr, "%s: already initialized!\n", __func__);
        return(SPKERR_BADSEQ);
    }

    net_zc = zlog_get_category(log_cat?log_cat:"NET");
    if (!net_zc) {
        fprintf(stderr, "%s: failed to initialize log system!\n", __func__);
        return(SPKERR_LOGSYS);
    }
    zlog_notice(net_zc, "module initialized.");

    return(SPK_SUCCESS);
}


static void* __net_slice_worker(void* arg)
{
    net_slice_ctx_t* slice_ctx = (net_slice_ctx_t*)arg;
    int slice_num = slice_ctx->slice_num;
    int slice_id = slice_ctx->slice_id;
    int cpu_base = slice_ctx->cpu_base;

    assert(slice_ctx->fd > 0);

    if (cpu_base > 0) {
        cpu_base += slice_id;
        spk_worker_set_affinity(cpu_base);
    }
    zlog_info(net_zc, "slice> spawned: id=%d, cpu=%d", slice_id, cpu_base);
    
    while (!slice_ctx->quit_req) {
        pthread_mutex_lock(&slice_ctx->lock);
        if(slice_ctx->wptr == slice_ctx->rptr) {
            struct timeval now;
            struct timespec outtime;
            gettimeofday(&now, NULL);
            outtime.tv_sec = now.tv_sec + 1;
            outtime.tv_nsec = now.tv_usec * 1000;
            pthread_cond_timedwait(&slice_ctx->not_empty, &slice_ctx->lock, &outtime);
        }
        if (slice_ctx->wptr == slice_ctx->rptr) {
            pthread_mutex_unlock(&slice_ctx->lock);
            continue;
        }

        assert(slice_ctx->data_chunk.flag == CHUNK_DATA_FLAG__REQ);

        size_t chunk_size = slice_ctx->data_chunk.size;
        size_t slice_sz = chunk_size / slice_num;
        ssize_t access = 0;

        // check chunk_size and slice_sz
        if ((chunk_size % slice_num) ||
            (slice_sz & (0x4000-1))) { // 16k alignment
            zlog_error(net_zc, "illegal chunk_sz: chunk_sz=%zu, slice_num=%d",
                        chunk_size, slice_num);
            access = SPKERR_PARAM;
            goto done;
        }

        if (slice_sz != slice_ctx->slice_sz) {
            zlog_warn(net_zc, "unexpected slice size : slice_sz=%zu, expect=%zu",slice_sz, slice_ctx->slice_sz);
            // this chunk may the last in file
        }

        if (slice_ctx->dir == SPK_DIR_WRITE) {
            // write
            access = write(slice_ctx->fd,
                           slice_ctx->data_chunk.buf + slice_id * slice_sz,
                           slice_sz);
        } else {
            // read
            access = read(slice_ctx->fd,
                           slice_ctx->data_chunk.buf + slice_id * slice_sz,
                           slice_sz);
        }

        if (access != slice_sz) {
            zlog_error(net_zc, "failed to access file: dir=%d, "
                        "sock_fd:%d slice_sz=%zu, offset=%ld, ret=%ld, errmsg=\'%s\'",
                        slice_ctx->dir, slice_ctx->fd,slice_sz,
                        slice_id * slice_sz, access, strerror(errno));
            access = SPKERR_EACCESS;
            goto done;
        }

done:
        if (access == slice_sz) {
            slice_ctx->data_chunk.flag = CHUNK_DATA_FLAG__DONE;
        } else {
            slice_ctx->data_chunk.flag = access;
        }
        slice_ctx->rptr++;
        pthread_cond_signal(&slice_ctx->not_full);
        pthread_mutex_unlock(&slice_ctx->lock);
    }
    zlog_info(net_zc, "slice> terminated: id=%d", slice_id);
    return(NULL);
}

net_handle_t * net_open(const char *ipaddr,int port,int slice_num,int cpu_base,int dir)
{
       
    int sock_fd;
    int ret = SPKERR_BADRES;
    struct sockaddr_in svr_addr;
    int i=0;
	//int flags;
	// create ctx
    net_handle_t* file_ctx = NULL;
    file_ctx = malloc(sizeof(net_handle_t));
    assert(file_ctx);
    memset(file_ctx, 0, sizeof(net_handle_t));
    file_ctx->slice_num = slice_num;
    file_ctx->dir = dir;

    // spawn workers
    for (i=0; i<slice_num; i++) {
        net_slice_ctx_t* slice_ctx = malloc(sizeof(net_slice_ctx_t));
        assert(slice_ctx);
        memset(slice_ctx, 0, sizeof(net_slice_ctx_t));
        slice_ctx->slice_id = i;
        slice_ctx->dir = dir;
        slice_ctx->slice_sz=0x4000;//16K
        slice_ctx->slice_num=slice_num;//4
        
		pthread_mutex_init(&slice_ctx->lock, NULL);
        pthread_cond_init(&slice_ctx->not_full, NULL);
        pthread_cond_init(&slice_ctx->not_empty, NULL);
        slice_ctx->wkr_thread = malloc(sizeof(pthread_t));
        
		sock_fd = socket(AF_INET,SOCK_STREAM,0);
        if (sock_fd < 0) {
            zlog_fatal(net_zc, "failed to create socket: errmsg=\'%s\'",
                   strerror(errno));
            goto errout;
        }
        
		svr_addr.sin_family = AF_INET;
        svr_addr.sin_addr.s_addr = inet_addr(ipaddr);
        svr_addr.sin_port = htons(port);
		//flags = fcntl(sock_fd,F_GETFL,0);//获取建立的sockfd的当前状态（非阻塞）
		//fcntl(sock_fd,F_SETFL,flags|O_NONBLOCK);//将当前sockfd设置为非阻塞

        ret = connect(sock_fd,(struct sockaddr*)&svr_addr,
                          sizeof(struct sockaddr_in));
        if (ret < 0) {
                zlog_fatal(net_zc, "failed to connect to server: ip=%s:%d, errmsg=\'%s\'",ipaddr, port, strerror(errno));
                goto errout;
        }
        zlog_info(net_zc, "net connect successful: id=%d, sockfd=%d", i,sock_fd);
        file_ctx->conn_valid[i]=1;
        slice_ctx->fd = sock_fd;
        file_ctx->slice_tbl[i] = slice_ctx;
        slice_ctx->cpu_base = cpu_base;
        pthread_create(slice_ctx->wkr_thread, NULL,
                         __net_slice_worker, slice_ctx);
    }

    if (file_ctx) {
        spk_stats_reset(&file_ctx->xfer_stats);
    }

errout:
    return (file_ctx);
}


int net_intf_is_connected(net_handle_t* file_ctx)
{
    int slice_num=file_ctx->slice_num;
	int conn_valid=1;
	int i;
	for(i=0;i<slice_num;i++) {
	    conn_valid=conn_valid & file_ctx->conn_valid[i];
	}
    return(conn_valid);
}

int net_close(net_handle_t* file_ctx)
{
    assert(file_ctx);

    int i;
    int slice_num=file_ctx->slice_num;
	net_slice_ctx_t* slice_ctx = NULL;
    
    for (i=0; i<slice_num; i++) {
        // set quit_req flag
		if(file_ctx->conn_valid[i]) {
            slice_ctx = file_ctx->slice_tbl[i];
            slice_ctx->quit_req = 1;
            pthread_cond_signal(&slice_ctx->not_empty);
		}
    }

    for (i=0; i<slice_num; i++) {
        // wait thread quit
		if(file_ctx->conn_valid[i]) {
            slice_ctx = file_ctx->slice_tbl[i];
            pthread_join(*slice_ctx->wkr_thread, NULL);
            close(slice_ctx->fd);
            SAFE_RELEASE(slice_ctx->wkr_thread);
            SAFE_RELEASE(slice_ctx);
            file_ctx->slice_tbl[i] = NULL;
		}
    } 

    SAFE_RELEASE(file_ctx);

    return(SPK_SUCCESS);
}

ssize_t net_read(net_handle_t* file_ctx, void* buf, size_t size)
{
    assert(file_ctx);

    net_slice_ctx_t* slice_ctx = NULL;
    ssize_t ret_sz = 0;
    int i;
    
    zlog_info(net_zc, "file read: ctx=%p, buf=%zu@%p", file_ctx, size, buf);

    int slice_num=file_ctx->slice_num;
    
	for (i=0; i<slice_num; i++) {
        slice_ctx = file_ctx->slice_tbl[i];
        // wait for slice worker to be idle
        pthread_mutex_lock(&slice_ctx->lock);
        if(slice_ctx->wptr != slice_ctx->rptr) {
            pthread_cond_wait(&slice_ctx->not_full, &slice_ctx->lock);
        }
        assert(slice_ctx->data_chunk.flag == CHUNK_DATA_FLAG__DONE);
        // push chunk
        slice_ctx->data_chunk.buf = buf;
        slice_ctx->data_chunk.size = size;
        slice_ctx->data_chunk.flag = CHUNK_DATA_FLAG__REQ;
        // update wptr
        slice_ctx->wptr++;
        // trig slice worker
        pthread_cond_signal(&slice_ctx->not_empty);
        pthread_mutex_unlock(&slice_ctx->lock);
    }

    ret_sz = size;
    for (i=0; i<slice_num; i++) {
        slice_ctx = file_ctx->slice_tbl[i];
        // wait for slice worker to be idle
        pthread_mutex_lock(&slice_ctx->lock);
        if(slice_ctx->wptr != slice_ctx->rptr) {
            pthread_cond_wait(&slice_ctx->not_full, &slice_ctx->lock);
        }
        if (slice_ctx->data_chunk.flag != CHUNK_DATA_FLAG__DONE) {
            // error
            slice_ctx->data_chunk.flag = CHUNK_DATA_FLAG__DONE;
            ret_sz = 0;
        }
        pthread_mutex_unlock(&slice_ctx->lock);
    }
    
    if (ret_sz > 0) {
//        file_ctx->fmeta->file_pos += ret_sz;
        spk_stats_inc_xfer(&file_ctx->xfer_stats, ret_sz, 1);
    }
    return(ret_sz);
}

ssize_t net_write(net_handle_t* file_ctx, void* buf, size_t size)
{
    assert(file_ctx);
    net_slice_ctx_t* slice_ctx;
    ssize_t ret_sz = 0;
    int i;
    int slice_num=file_ctx->slice_num;
    
	for (i=0; i<slice_num; i++) {
        slice_ctx = file_ctx->slice_tbl[i];
        // wait for slice worker to be idle
        pthread_mutex_lock(&slice_ctx->lock);
        if(slice_ctx->wptr != slice_ctx->rptr) {
            pthread_cond_wait(&slice_ctx->not_full, &slice_ctx->lock);
        }
        assert(slice_ctx->data_chunk.flag == CHUNK_DATA_FLAG__DONE);
        // push chunk
        slice_ctx->data_chunk.buf = buf;
        slice_ctx->data_chunk.size = size;
        slice_ctx->data_chunk.flag = CHUNK_DATA_FLAG__REQ;
        // update wptr
        slice_ctx->wptr++;
        // trig slice worker
        pthread_cond_signal(&slice_ctx->not_empty);
        pthread_mutex_unlock(&slice_ctx->lock);
    }
    ret_sz = size;
    
	for (i=0; i<slice_num; i++) {
        slice_ctx = file_ctx->slice_tbl[i];
        // wait for slice worker to be idle
        pthread_mutex_lock(&slice_ctx->lock);
        if(slice_ctx->wptr != slice_ctx->rptr) {
            pthread_cond_wait(&slice_ctx->not_full, &slice_ctx->lock);
        }
        if (slice_ctx->data_chunk.flag < 0) {
            // error
            ret_sz = slice_ctx->data_chunk.flag;
        }
        slice_ctx->data_chunk.flag = CHUNK_DATA_FLAG__DONE;
        pthread_mutex_unlock(&slice_ctx->lock);
    }
    
    if (ret_sz > 0) {
        //file_ctx->fmeta->file_pos += ret_sz;
        spk_stats_inc_xfer(&file_ctx->xfer_stats, ret_sz, 1);
    }

    return(ret_sz);
}

spk_stats_t* net_get_stats(net_handle_t* ctx)
{
    return(&ctx->xfer_stats);
}
