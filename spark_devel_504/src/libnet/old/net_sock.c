#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stddef.h>
#include <inttypes.h>
#include <assert.h>
#include <errno.h>
#include <pthread.h>
#include "net.h"

int net_sock_can_read(int sockfd, int wait_ms)
{
    fd_set readfds;
    struct timeval timeout;

    FD_ZERO(&readfds);
    FD_SET(sockfd, &readfds);
    timeout.tv_sec = (wait_ms/1000);
    timeout.tv_usec = (wait_ms%1000) * 1000;
    
    int ret = select(sockfd+1, &readfds, NULL, NULL, &timeout);
    if (ret <= 0) {
        return (ret);
    }

    return(FD_ISSET(sockfd, &readfds));
}

int net_sock_can_write(int sockfd, int wait_ms)
{
    fd_set writefds;
    struct timeval timeout;
    FD_ZERO(&writefds);
    FD_SET(sockfd, &writefds);
    timeout.tv_sec = (wait_ms/1000);
    timeout.tv_usec = (wait_ms%1000) * 1000;
    int ret = select(sockfd+1, NULL, &writefds, NULL, &timeout);
    if (ret <= 0) {
        return (ret);
    }

    return(FD_ISSET(sockfd, &writefds));
}


ssize_t net_udp_read_data(int sockfd,void* buf, size_t buf_size,
                         struct sockaddr* cli_addr)
{
    ssize_t ret_sz, recved;

    ret_sz = net_sock_can_read(sockfd, 1);
    if (ret_sz <= 0) {
        // not ready
        return(ret_sz);
    }

    socklen_t cli_addrlen = sizeof(struct sockaddr_in);

    recved = 0;
    while(recved < buf_size) {
        ret_sz = recvfrom(sockfd,
                      buf+recved,
                      (buf_size-recved),
                      0,
                      cli_addr,
                      &cli_addrlen);
        if (ret_sz <= 0) {
            if (ret_sz < 0) {
                if (errno == EINTR ||
                    errno == EAGAIN) {
                    continue;
                }
            }
            return(SPKERR_RESETSYS);
        }
        recved += ret_sz;
    }

    return(recved);
}

ssize_t net_udp_write_data(int sockfd, void* buf, size_t buf_size,
                          struct sockaddr *conn_addr)
{
    ssize_t ret_sz = 0;

    if (net_sock_can_write(sockfd, 0) <= 0) {
        // not ready
        zlog_warn(net_zc, "socket buffer full: sockfd=%d", sockfd);
        return(ret_sz);
    }

    ssize_t xferred = 0;
    while(xferred < buf_size) {
        ret_sz = sendto(sockfd,
                    buf+xferred,
                    (buf_size-xferred),
                    0,
                    conn_addr,
                    sizeof(struct sockaddr_in));
        if (ret_sz <= 0) {
            if (ret_sz < 0) {
                if (errno == EINTR ||
                    errno == EAGAIN) {
                    continue;
                }
            }
            return(SPKERR_RESETSYS);
        }
        xferred += ret_sz;
    }
    return(xferred);
}

ssize_t net_tcp_read_data(int sockfd, void* buf, size_t buf_size)
{
    ssize_t ret_sz, recved;
    int ret = -1;

    ret = net_sock_can_read(sockfd, 1);
    if (ret == 0) {
        return 0;
    }
    if (ret < 0) {
        // closed?
        zlog_warn(net_zc, "cmi_sock_can_read: sock=%d, ret=%d, errmsg=\'%s\'",
                           sockfd, ret, strerror(errno));
        return(SPKERR_RESETSYS);
    }
    recved = 0;
    while(recved < buf_size) {
        ret_sz = read(sockfd, buf+recved, buf_size-recved);
        if (ret_sz <= 0) {
            if (ret_sz < 0) {
                if (errno == EINTR ||
                    errno == EAGAIN) {
                    continue;
                }
            }
            return(SPKERR_RESETSYS);
        }
        recved += ret_sz;
    }
    
	return(recved);
}

ssize_t net_tcp_write_data(int sockfd,void* buf, size_t buf_size)
{
    ssize_t ret_sz = 0;
    ssize_t xferred = 0;
    while(xferred < buf_size) {
        ret_sz = write(sockfd, buf+xferred, buf_size-xferred);
        if (ret_sz <= 0) {
            if (ret_sz < 0) {
                if (errno == EINTR ||
                    errno == EAGAIN) {
                    continue;
                }
            }
            return(SPKERR_RESETSYS);
        }
        xferred += ret_sz;
    }
    return(xferred);
}
