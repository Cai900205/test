#ifndef __NET_H__
#define __NET_H__

#include <spark.h>
#include <net/net_intf.h>

extern zlog_category_t* net_zc;
/*
typedef struct {
    net_initf_type

}*/ 

ssize_t net_udp_read_data(int sockfd,void* buf, size_t buf_size,
                         struct sockaddr* cli_addr);
ssize_t net_udp_write_data(int sockfd, void* buf, size_t buf_size,
                          struct sockaddr *conn_addr);
ssize_t net_tcp_read_data(int sockfd, void* buf, size_t buf_size);
ssize_t net_tcp_write_data(int sockfd, void* buf,size_t buf_size);

int net_sock_can_read(int sockfd, int wait_ms);
int net_sock_can_write(int sockfd, int wait_ms);
#endif
