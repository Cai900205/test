
#include "fvl_udp.h"

int fvl_udp_init(fvl_udp_socket_t *udp, in_addr_t ip, int port)
{
    int sock;

    sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        perror("socket");
        return -1;
    }

    udp->sock = sock;

    udp->dest.sin_family = AF_INET;
    udp->dest.sin_port = htons(port);
    udp->dest.sin_addr.s_addr = ip;

    return sock;
}

void fvl_udp_finish(fvl_udp_socket_t *udp)
{
    close(udp->sock);
}

int fvl_udp_send(fvl_udp_socket_t *udp, char *buf, int len)
{
    int ret;
    int sock;
    struct sockaddr *addr;

    sock = udp->sock;
    addr = (struct sockaddr *)&udp->dest;
    ret = sendto(sock, buf, len, 0, addr, sizeof(*addr));

    return ret;
}

int fvl_udp_recv(fvl_udp_socket_t *udp, char *buf, int len)
{
    int ret;
    int sock;
    struct sockaddr *addr;
    socklen_t alen;

    sock = udp->sock;
    addr = (struct sockaddr *)&udp->from;
    alen = sizeof(*addr);

    ret = recvfrom(sock, buf, len, 0, addr, &alen);
    return ret;
}


