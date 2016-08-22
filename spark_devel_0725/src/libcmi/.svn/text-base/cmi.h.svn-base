#ifndef __CMI_H__
#define __CMI_H__

#include <spark.h>
#include <cmi/cmi_intf.h>

#define byteswaps(A) ((((uint16_t)(A) & 0xff00) >> 8 ) | \
                  (((uint16_t)(A) & 0x00ff) << 8 ))

#define byteswapl(A) ((((uint32_t)(A) & 0xff000000) >> 24) | \
                  (((uint32_t)(A) & 0x00ff0000) >> 8 ) | \
                  (((uint32_t)(A) & 0x0000ff00) << 8 ) | \
                  (((uint32_t)(A) & 0x000000ff) << 24))

#define byteswapll(A) ((uint64_t)(A) >> 56) | \
                        (((uint64_t)(A) & 0x00ff000000000000) >> 40) | \
                        (((uint64_t)(A) & 0x0000ff0000000000) >> 24) | \
                        (((uint64_t)(A) & 0x000000ff00000000) >> 8) | \
                        (((uint64_t)(A) & 0x00000000ff000000) << 8) | \
                        (((uint64_t)(A) & 0x0000000000ff0000) << 24) | \
                        (((uint64_t)(A) & 0x000000000000ff00) << 40) | \
                        ((uint64_t)(A) << 56)

#define CMI_LOG(LEVEL, ...) \
    zlog(cmi_zc, __FILE__, sizeof(__FILE__)-1, __func__, sizeof(__func__)-1, __LINE__, \
        LEVEL, __VA_ARGS__);

extern zlog_category_t* cmi_zc;
extern const cmi_endian cmi_our_endian;

char* cmi_desc_sysstate2str(int state);
char* cmi_desc_cmdtype2str(int cmd_type);

ssize_t cmi_udp_read_msg(int sockfd, cmi_endian* endian,
                         void* buf, size_t buf_size,
                         struct sockaddr* cli_addr);
ssize_t cmi_udp_write_msg(int sockfd, cmi_endian endian, void* buf, size_t buf_size,
                          struct sockaddr *conn_addr);
ssize_t cmi_tcp_read_msg(int sockfd, cmi_endian* endian,
                         void* buf, size_t buf_size);
ssize_t cmi_tcp_write_msg(int sockfd, cmi_endian endian, void* buf,
                          size_t buf_size);
int cmi_sock_can_read(int sockfd, int wait_ms);
int cmi_sock_can_write(int sockfd, int wait_ms);

int cmi_msg_is_valid(void* msg, size_t size);
#endif
