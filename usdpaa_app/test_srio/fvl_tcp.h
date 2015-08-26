
#ifndef __FVL_TCP_H__
#define __FVL_TCP_H__

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "fvl_common.h"

typedef struct fvl_tcp_socket {
    int                 sock;
    struct sockaddr_in  dest;
    struct sockaddr_in  from;
} fvl_tcp_socket_t;

int fvl_tcp_init(fvl_tcp_socket_t *tcp, in_addr_t ip, int port);
void fvl_tcp_finish(fvl_tcp_socket_t *tcp);

int fvl_tcp_send(fvl_tcp_socket_t *tcp, uint8_t *buf, int len);
int fvl_tcp_recv(fvl_tcp_socket_t *tcp, uint8_t *buf, int len);


#endif // __FVL_TCP_H__

