
#include "fvl_tcp.h"

int fvl_tcp_init(fvl_tcp_socket_t *tcp, in_addr_t ip, int port)
{
    int sock;
    int ret;

    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("socket");
        return -1;
    }

    tcp->sock = sock;

    tcp->dest.sin_family = AF_INET;
    tcp->dest.sin_port = htons(port);
    tcp->dest.sin_addr.s_addr = ip;
    
    ret = connect(sock,(const struct sockaddr *)&tcp->dest,sizeof(struct sockaddr_in));
    if(ret<0){
        printf("connect error!\n");
        return -1;
    }
    
    return sock;
}

void fvl_tcp_finish(fvl_tcp_socket_t *tcp)
{
    close(tcp->sock);
}

int fvl_tcp_send(fvl_tcp_socket_t *tcp, uint8_t *buf, int len)
{
    int ret;
    int sock;
    struct sockaddr *addr;

    sock = tcp->sock;
   /* addr = (struct sockaddr *)&tcp->dest;*/
    ret = send(sock, buf, len,0);
    if(ret < 0){
        printf("send error!\n");
        return -1;
    }
    return ret;
}

int fvl_tcp_recv(fvl_tcp_socket_t *tcp, uint8_t *buf, int len)
{
    int ret;
    int sock;
    struct sockaddr *addr;
    socklen_t alen;

    sock = tcp->sock;
/*    addr = (struct sockaddr *)&tcp->from;
    alen = sizeof(*addr);*/

    ret = recv(sock, buf, len, 0);
    if(ret < 0){
        printf("recv  error!\n");
        return -1;
    }
    return ret;
}


