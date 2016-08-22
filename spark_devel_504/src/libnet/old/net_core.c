#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stddef.h>
#include <inttypes.h>
#include <assert.h>
#include <errno.h>
#include <pthread.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/tcp.h>

#include "net.h"

zlog_category_t* net_zc = NULL;

int net_module_init(const char* log_cat)
{
    if (net_zc) {
        fprintf(stderr, "%s: already initialized!\n", __func__);
        return(SPKERR_BADSEQ);
    }

    net_zc = zlog_get_category(log_cat?log_cat:"net");
    if (!net_zc) {
        fprintf(stderr, "%s: failed to initialize log system!\n", __func__);
        return(SPKERR_LOGSYS);
    }
    zlog_notice(net_zc, "module initialized.");

    signal(SIGPIPE, SIG_IGN);
    return(SPK_SUCCESS);
}

static void* __net_slice_worker(void* arg)
{
    net_slice_ctx_t* slice_ctx = (dfv_slice_ctx_t*)arg;
    int slice_num = slice_ctx->fmeta->slice_num;
    int slice_id = slice_ctx->slice_id;
    int cpu_base = slice_ctx->cpu_base;
    assert(slice_ctx->fd > 0);

    if (cpu_base > 0) {
        cpu_base += slice_id;
        spk_worker_set_affinity(cpu_base);
    }

    zlog_info(dfv_zc, "slice> spawned: id=%d, cpu=%d", slice_id, cpu_base);
    
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
            zlog_error(dfv_zc, "illegal chunk_sz: chunk_sz=%zu, slice_num=%d",
                        chunk_size, slice_num);
            access = SPKERR_PARAM;
            goto done;
        }

        if (slice_sz != slice_ctx->fmeta->slice_sz) {
            zlog_warn(dfv_zc, "unexpected slice size : slice_sz=%zu, expect=%zu",
                        slice_sz, slice_ctx->fmeta->slice_sz);
            // this chunk may the last in file
            // so just warn it and continue
        }

        if (slice_ctx->dir == SPK_DIR_WRITE) {
            // write
            access = net_tcp_write_data(slice_ctx->fd,
                           slice_ctx->data_chunk.buf + slice_id * slice_sz,
                           slice_sz);
        } else {
            // read
            access = net_tcp_read_data(slice_ctx->fd,
                           slice_ctx->data_chunk.buf + slice_id * slice_sz,
                           slice_sz);
        }
        if (access != slice_sz) {
            zlog_error(dfv_zc, "failed to access file: dir=%d, "
                        "slice_sz=%zu, offset=%ld, ret=%lu, errmsg=\'%s\'",
                        slice_ctx->dir, slice_sz,
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
    zlog_info(dfv_zc, "slice> terminated: id=%d", slice_id);
    return(NULL);
}

int net_intf_connect(net_intf_t* intf, const char* ipaddr, int port,int workers)
{
    int sock_fd[8] = -1;
    int ret = SPKERR_BADRES;
    struct sockaddr_in svr_addr;
    int i=0;

	for(i=0;i<workers;i++) {
        sock_fd[i] = socket(AF_INET,
                     (intf->intf_type == net_intf_tcp)?SOCK_STREAM:SOCK_DGRAM,0);
        if (sock_fd[i] < 0) {
            zlog_fatal(net_zc, "failed to create socket: errmsg=\'%s\'",
                   strerror(errno));
            goto errout;
        }
        svr_addr.sin_family = AF_INET;
        svr_addr.sin_addr.s_addr = inet_addr(ipaddr);
        svr_addr.sin_port = htons(port+i);
        if (intf->intf_type == net_intf_tcp) {
            ret = connect(sock_fd[i],
                          (struct sockaddr*)&svr_addr,
                          sizeof(struct sockaddr_in));
            if (ret < 0) {
                zlog_fatal(net_zc, "failed to connect to server: ip=%s:%d, errmsg=\'%s\'",
                           ipaddr, port, strerror(errno));
                goto errout;
            }
        }
    }

#if 0
    if (intf->conn_sockfd > 0) {
        int keepAlive = 1;//设定KeepAlive
        int keepIdle = 5;//开始首次KeepAlive探测前的TCP空闭时间
        int keepInterval = 5;//两次KeepAlive探测间的时间间隔
        int keepCount = 3;//判定断开前的KeepAlive探测次数
        assert(setsockopt(intf->conn_sockfd, SOL_SOCKET, SO_KEEPALIVE, (void*)&keepAlive,sizeof(keepAlive)) >= 0);
        assert(setsockopt(intf->conn_sockfd, SOL_TCP, TCP_KEEPIDLE, (void *)&keepIdle, sizeof(keepIdle)) >= 0);
        assert(setsockopt(intf->conn_sockfd, SOL_TCP, TCP_KEEPINTVL, (void *)&keepInterval, sizeof(keepInterval)) >= 0);
        assert(setsockopt(intf->conn_sockfd, SOL_TCP, TCP_KEEPCNT, (void *)&keepCount, sizeof(keepCount)) >= 0);
    }
#endif
    zlog_notice(net_zc, "net interface connected: sock=%d", intf->conn_sockfd);
    return(SPK_SUCCESS);

errout:
    if (sock_fd > 0) {
        close(sock_fd);
    }
    return(ret);
}


void net_intf_disconnect(net_intf_t* intf)
{
    if (intf->conn_valid) {
        pthread_mutex_lock(&intf->conn_lock);
        if (intf->conn_sockfd > 0) {
            close(intf->conn_sockfd);
            intf->conn_sockfd = -1;
        }
        if (intf->sock_svr > 0) {
            close(intf->sock_svr);
            intf->sock_svr = -1;
        }
        intf->conn_valid = 0;
        pthread_mutex_unlock(&intf->conn_lock);
        zlog_notice(net_zc, "net interface disconnected");
    }
}

int net_intf_is_connected(net_intf_t* intf)
{
    return(intf->conn_valid);
}

void net_intf_close(net_intf_t* intf)
{
    net_intf_disconnect(intf);
    SAFE_RELEASE(intf);

    zlog_notice(net_zc, "net interface closed");
    return;
}

ssize_t net_intf_read_msg(net_intf_t* intf, void* buf, size_t buf_size)
{
    ssize_t size = 0;

    if (!net_intf_is_connected(intf)) {
        return(SPKERR_BADSEQ);
    }

    switch(intf->intf_type) {
    case net_intf_tcp:
        size = net_tcp_read_data(intf->conn_sockfd,
                                buf,
                                buf_size);
        break;
    case net_intf_udp:
        size = net_udp_read_dataintf->conn_sockfd,
                                buf,
                                buf_size,
                                (struct sockaddr *)&intf->conn_addr);
        break;
    default:
        assert(0);
        break;
    }

    if (size == 0) {
        // no data
    } else if (size < 0) {
        // error
        zlog_error(net_zc, "failed to recv/read: sock=%d, ret=%ld, errmsg=\'%s\'",
                            intf->conn_sockfd, size, strerror(errno));
        size = SPKERR_RESETSYS;
    } else {
        // got a packet
        zlog_info(net_zc, "got packet: size=%zu", size);
        if (!net_msg_is_valid(buf, size)) {
            zlog_error(net_zc, "got illegal message");
            net_msg_dump(ZLOG_LEVEL_ERROR, buf, size);
            size = 0;
        }
    }

    return(size);
}

ssize_t net_intf_write_msg(net_intf_t* intf, void* buf, size_t buf_size)
{
    ssize_t size = 0;

    if (!net_intf_is_connected(intf)) {
        return(SPKERR_BADSEQ);
    }

    if (!net_msg_is_valid(buf, buf_size)) {
        zlog_error(net_zc, "try to send illegal message");
        return(SPKERR_PARAM);
    }

    pthread_mutex_lock(&intf->conn_lock);
    switch(intf->intf_type) {
    case net_intf_tcp:
        size = net_tcp_write_data(intf->conn_sockfd,
                                   buf,
                                   buf_size);
        break;
    case net_intf_udp:
        size = net_udp_write_data(intf->conn_sockfd,
                                   buf,
                                   buf_size,
                                   (struct sockaddr*)&intf->conn_addr);
        break;
    default:
        assert(0);
        break;
    }
    pthread_mutex_unlock(&intf->conn_lock);

    if (size == 0) {
        // no data
    } else if (size < 0) {
        // error
        zlog_error(net_zc, "failed to write/send: sock=%d, ret=%ld, errmsg=\'%s\'",
                            intf->conn_sockfd, size, strerror(errno));
        size = SPKERR_RESETSYS;
    } else {
        assert(size == buf_size);
    }

    return(size);
}




