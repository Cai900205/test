#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stddef.h>
#include <inttypes.h>
#include <assert.h>
#include <errno.h>
#include <pthread.h>
#include "cmi.h"

int cmi_sock_can_read(int sockfd, int wait_ms)
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

int cmi_sock_can_write(int sockfd, int wait_ms)
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


ssize_t cmi_udp_read_msg(int sockfd, cmi_endian* endian,
                         void* buf, size_t buf_size,
                         struct sockaddr* cli_addr)
{
    ssize_t ret_sz;

    ret_sz = cmi_sock_can_read(sockfd, 1);
    if (ret_sz <= 0) {
        // not ready
        return(ret_sz);
    }

    socklen_t cli_addrlen = sizeof(struct sockaddr_in);
    ret_sz = recvfrom(sockfd,
                      buf,
                      buf_size,
                      0,
                      cli_addr,
                      &cli_addrlen);
    if (ret_sz > 0) {
        if (ret_sz <= sizeof(cmi_msg_hdr_t)) {
            ret_sz = SPKERR_RESETSYS;
            assert(0);
        }
        if (*endian == cmi_endian_auto) {
            // try to probe endian
            if (MSG_SYNCTAG(buf) == CMI_SYNC_TAG) {
                *endian = cmi_our_endian;
            } else if (MSG_SYNCTAG(buf) == byteswaps(CMI_SYNC_TAG)) {
                if (cmi_our_endian == cmi_endian_big) {
                    *endian = cmi_endian_little;
                } else {
                    *endian = cmi_endian_big;
                }
            } else {
                assert(0);
            }
        }
        // reform
        cmi_msg_reform_hdr((cmi_msg_hdr_t*)buf, *endian);
        cmi_msg_reform_body(MSG_CODE(buf), buf, *endian);
    }

    return(ret_sz);
}

ssize_t cmi_udp_write_msg(int sockfd, cmi_endian endian, void* buf, size_t buf_size,
                          struct sockaddr *conn_addr)
{
    ssize_t ret_sz = 0;
    uint16_t msg_code;

    assert(endian == cmi_endian_big || endian == cmi_endian_little);

    if (cmi_sock_can_write(sockfd, 0) <= 0) {
        // not ready
        zlog_warn(cmi_zc, "socket buffer full: sockfd=%d", sockfd);
        return(ret_sz);
    }

    // reform
    msg_code = ((cmi_msg_hdr_t*)buf)->msg_code;
    cmi_msg_reform_hdr((cmi_msg_hdr_t*)buf, endian);
    cmi_msg_reform_body(msg_code, buf, endian);

    ret_sz = sendto(sockfd,
                    buf,
                    buf_size,
                    0,
                    conn_addr,
                    sizeof(struct sockaddr_in));
    return(ret_sz);
}

ssize_t cmi_tcp_read_msg(int sockfd, cmi_endian* endian, void* buf, size_t buf_size)
{
    ssize_t ret_sz, recved;
    int ret = -1;
    cmi_msg_hdr_t* hdr = (cmi_msg_hdr_t*)buf;
    assert(endian);

    ret = cmi_sock_can_read(sockfd, 1);
    if (ret == 0) {
        return 0;
    }
    if (ret < 0) {
        // closed?
        zlog_warn(cmi_zc, "cmi_sock_can_read: sock=%d, ret=%d, errmsg=\'%s\'",
                           sockfd, ret, strerror(errno));
        return(SPKERR_RESETSYS);
    }

    recved = 0;
    while(recved < sizeof(cmi_msg_hdr_t)) {
        ret_sz = read(sockfd, buf+recved, sizeof(cmi_msg_hdr_t)-recved);
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

    if (*endian == cmi_endian_auto) {
        // try to probe endian
        if (hdr->sync_tag == CMI_SYNC_TAG) {
            *endian = cmi_our_endian;
        } else if (hdr->sync_tag == byteswaps(CMI_SYNC_TAG)) {
            if (cmi_our_endian == cmi_endian_big) {
                *endian = cmi_endian_little;
            } else {
                *endian = cmi_endian_big;
            }
        } else {
            assert(0);
        }
        zlog_notice(cmi_zc, "endian detected: endian=%d", *endian);
    }

    // reform
    cmi_msg_reform_hdr(hdr, *endian);

    int msg_len = hdr->msg_len;
    assert(msg_len <= buf_size);

    int remain = msg_len - sizeof(cmi_msg_hdr_t);
    void* pl_buf = buf + sizeof(cmi_msg_hdr_t);
    recved = 0;
    while(recved < remain) {
        ret_sz = read(sockfd, pl_buf+recved, remain-recved);
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

    // reform
    cmi_msg_reform_body(hdr->msg_code, buf, *endian);
    return(msg_len);
}

ssize_t cmi_tcp_write_msg(int sockfd, cmi_endian endian, void* buf, size_t buf_size)
{
    ssize_t ret_sz = 0;
    ssize_t xferred = 0;

    assert(endian == cmi_endian_big || endian == cmi_endian_little);

    // reform
    cmi_msg_reform_body(MSG_CODE(buf), buf, endian);
    cmi_msg_reform_hdr((cmi_msg_hdr_t*)buf, endian);

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
