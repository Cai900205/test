
#ifndef __FVL_UDP_H__
#define __FVL_UDP_H__

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "fvl_common.h"

typedef struct fvl_udp_socket {
    int                 sock;
    struct sockaddr_in  dest;
    struct sockaddr_in  from;
} fvl_udp_socket_t;

int fvl_udp_init(fvl_udp_socket_t *udp, in_addr_t ip, int port);
void fvl_udp_finish(fvl_udp_socket_t *udp);

int fvl_udp_send(fvl_udp_socket_t *udp, char *buf, int len);
int fvl_udp_recv(fvl_udp_socket_t *udp, char *buf, int len);


#endif // __FVL_UDP_H__

