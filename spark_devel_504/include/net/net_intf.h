#ifndef __NET_INTF_H__
#define __NET_INTF_H__
#include <inttypes.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#define NET_MOD_VER     "0.9.160407"

int net_module_init(const char* log_cat);

typedef enum {
    net_intf_tcp = 1,
	net_intf_udp,
	net_intf_max
} net_intf_type;
// file
struct net_handle* net_open(const char *ipaddr,int port,int workers,int cpu_base,int dir,net_intf_type type);
int net_close(struct net_handle* file_ctx);
ssize_t net_read(struct net_handle* file_ctx, void* buf, size_t size);
ssize_t net_write(struct net_handle* file_ctx, void* buf, size_t size);
spk_stats_t* net_get_stats(struct net_handle* ctx);
int net_intf_is_connected(struct net_handle* file_ctx);
#endif

