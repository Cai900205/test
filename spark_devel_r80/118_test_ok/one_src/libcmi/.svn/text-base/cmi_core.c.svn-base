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

#include "cmi.h"

zlog_category_t* cmi_zc = NULL;

pthread_mutex_t cmi_data_tag_lock;
uint16_t cmi_data_tag = 0x01;

#if __BYTE_ORDER == __BIG_ENDIAN
const cmi_endian cmi_our_endian = cmi_endian_big;
#else
const cmi_endian cmi_our_endian = cmi_endian_little;
#endif

char* cmi_desc_cmdtype2str(int cmd_type)
{
    switch(cmd_type) {
    case cmd_type_inquiry:
        return("inquiry");
    case cmd_type_init:
        return("init");
    case cmd_type_filelist:
        return("filelist");
    case cmd_type_format:
        return("format");
    case cmd_type_delete:
        return("delete");
    case cmd_type_start_rec:
        return("start_rec");
    case cmd_type_stop_rec:
        return("stop_rec");
    case cmd_type_start_play:
        return("start_play");
    case cmd_type_stop_play:
        return("stop_play");
    case cmd_type_snapshot:
        return("snapshot");
    case cmd_type_start_ul:
        return("start_ul");
    case cmd_type_stop_ul:
        return("stop_ul");
    case cmd_type_start_dl:
        return("start_dl");
    case cmd_type_stop_dl:
        return("stop_dl");
    case cmd_type_sync_time:
        return("sync_time");
    case cmd_type_config:
        return("config");
    case cmd_type_sysdown:
        return("sysdown");
    case cmd_type_upgrade:
        return("upgrade");
    default:
        return("<UNKNOWN>");
    }
    return(NULL);
}

char* cmi_desc_sysstate2str(int state)
{
    switch(state) {
    case sys_state_idle:
        return("idle");
    case sys_state_rec:
        return("rec");
    case sys_state_play:
        return("play");
    case sys_state_ul:
        return("upload");
    case sys_state_dl:
        return("download");
    case sys_state_delete:
        return("delete");
    case sys_state_format:
        return("format");
    case sys_state_upgrade:
        return("upgrade");
    default:
        return("<UNKNOWN>");
        break;
    }
    return(NULL);
}

char* cmi_desc_endian2str(cmi_endian endian)
{
    switch(endian) {
    case cmi_endian_big:
        return("big");
    case cmi_endian_little:
        return("little");
    case cmi_endian_auto:
        return("auto");
    default:
        return("<UNKNOWN>");
        break;
    }
    return(NULL);
}

char* cmi_desc_type2str(cmi_type type)
{
    switch(type) {
    case cmi_type_server:
        return("server");
    case cmi_type_client:
        return("client");
    default:
        return("<UNKNOWN>");
        break;
    }
    return(NULL);
}

char* cmi_desc_intftype2str(cmi_intf_type type)
{
    switch(type) {
    case cmi_intf_tcp:
        return("tcp");
    case cmi_intf_udp:
        return("udp");
    default:
        return("<UNKNOWN>");
        break;
    }
    return(NULL);
}

int cmi_module_init(const char* log_cat)
{
    if (cmi_zc) {
        fprintf(stderr, "%s: already initialized!\n", __func__);
        return(SPKERR_BADSEQ);
    }

    cmi_zc = zlog_get_category(log_cat?log_cat:"CMI");
    if (!cmi_zc) {
        fprintf(stderr, "%s: failed to initialize log system!\n", __func__);
        return(SPKERR_LOGSYS);
    }

    pthread_mutex_init(&cmi_data_tag_lock, NULL);

    zlog_notice(cmi_zc, "module initialized.");

    signal(SIGPIPE, SIG_IGN);
    return(SPK_SUCCESS);
}

cmi_intf_t* cmi_intf_open(cmi_type type,
                          cmi_intf_type intf_type,
                          cmi_endian endian_req)
{
    cmi_intf_t* cmi_intf = NULL;

    assert(intf_type == cmi_intf_tcp ||
           intf_type == cmi_intf_udp);
    // only server can use auto endian
    assert(!((endian_req == cmi_endian_auto) && (type != cmi_type_server)));

    zlog_notice(cmi_zc, "open cmi intf: type=%s, intf_type=%s, endian=%s",
                        cmi_desc_type2str(type),
                        cmi_desc_intftype2str(intf_type),
                        cmi_desc_endian2str(endian_req));
    cmi_intf = malloc(sizeof(cmi_intf_t));
    assert(cmi_intf);
    memset(cmi_intf, 0, sizeof(cmi_intf_t));

    cmi_intf->type = type;
    cmi_intf->intf_type = intf_type;
    cmi_intf->endian_req = endian_req;
    pthread_mutex_init(&cmi_intf->conn_lock, NULL);

    return (cmi_intf);
}

int cmi_intf_connect(cmi_intf_t* intf, const char* ipaddr, int port)
{
    int sock_fd = -1;
    int ret = SPKERR_BADRES;
    struct sockaddr_in svr_addr;

    if (cmi_intf_is_connected(intf)) {
        return(SPKERR_BADSEQ);
    }

    // reset endian
    intf->endian_cur = intf->endian_req;

    zlog_notice(cmi_zc, "connecting interface: svraddr=%s:%d",
                ipaddr?ipaddr:"0.0.0.0", port);

    sock_fd = socket(AF_INET,
                     (intf->intf_type == cmi_intf_tcp)?SOCK_STREAM:SOCK_DGRAM,
                     0);
    if (sock_fd < 0) {
        zlog_fatal(cmi_zc, "failed to create socket: errmsg=\'%s\'",
                   strerror(errno));
        goto errout;
    }

    if (intf->type == cmi_type_server) {
        // server
        // set SO_REUSEADDR
        int on = 1;
        setsockopt(sock_fd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));

        // bind socket
        memset(&svr_addr, 0, sizeof(svr_addr));
        svr_addr.sin_family = AF_INET;
        svr_addr.sin_port = htons(port);
        svr_addr.sin_addr.s_addr = htonl(INADDR_ANY);
        if (bind(sock_fd,
                 (struct sockaddr *)&svr_addr,
                 sizeof(svr_addr)) < 0) {
            zlog_fatal(cmi_zc, "failed to bind socket: errmsg=\'%s\'",
                       strerror(errno));
            goto errout;
        }
        if (intf->intf_type == cmi_intf_tcp) {
            // tcp
            intf->sock_svr = sock_fd;
            sock_fd = -1;
            if (listen(intf->sock_svr, 1) < 0) {
                zlog_fatal(cmi_zc, "failed to listen: errmsg=\'%s\'",
                           strerror(errno));
                goto errout;
            }
            zlog_notice(cmi_zc, "tcp: waiting for accept ...");
            socklen_t addr_len = sizeof(struct sockaddr_in);
            intf->conn_sockfd = accept(intf->sock_svr,
                                       (struct sockaddr *)&intf->conn_addr,
                                       &addr_len);
            if (intf->conn_sockfd < 0) {
                zlog_fatal(cmi_zc, "failed to accept: errmsg=\'%s\'",
                           strerror(errno));
                goto errout;
            }

            zlog_notice(cmi_zc, "connected from %s:%d via TCP",
                        inet_ntoa(intf->conn_addr.sin_addr),
                        htons(intf->conn_addr.sin_port));
            zlog_notice(cmi_zc, "tcp: ready");
            intf->conn_valid = 1;
        } else {
            // udp
            zlog_notice(cmi_zc, "udp: ready");
            intf->conn_sockfd = sock_fd;
            sock_fd = -1;
            intf->conn_valid = 1;
        }
    } else {
        // client
        svr_addr.sin_family = AF_INET;
        svr_addr.sin_addr.s_addr = inet_addr(ipaddr);
        svr_addr.sin_port = htons(port);
        if (intf->intf_type == cmi_intf_tcp) {
            ret = connect(sock_fd,
                          (struct sockaddr*)&svr_addr,
                          sizeof(struct sockaddr_in));
            if (ret < 0) {
                zlog_fatal(cmi_zc, "failed to connect to server: ip=%s:%d, errmsg=\'%s\'",
                           ipaddr, port, strerror(errno));
                goto errout;
            }
        }
        intf->conn_sockfd = sock_fd;
        sock_fd = -1;
        intf->conn_valid = 1;
    }
    if (intf->conn_sockfd > 0) {
        int x = fcntl(intf->conn_sockfd, F_GETFL, 0);
        fcntl(intf->conn_sockfd, F_SETFL, x | O_NONBLOCK);
    }

#if 1
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
    zlog_notice(cmi_zc, "cmi interface connected: sock=%d", intf->conn_sockfd);
    return(SPK_SUCCESS);

errout:
    if (sock_fd > 0) {
        close(sock_fd);
    }
    return(ret);
}

void cmi_intf_disconnect(cmi_intf_t* intf)
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
        zlog_notice(cmi_zc, "cmi interface disconnected");
    }
}

int cmi_intf_is_connected(cmi_intf_t* intf)
{
    return(intf->conn_valid);
}

void cmi_intf_close(cmi_intf_t* intf)
{
    cmi_intf_disconnect(intf);
    SAFE_RELEASE(intf);

    zlog_notice(cmi_zc, "cmi interface closed");
    return;
}

ssize_t cmi_intf_read_msg(cmi_intf_t* intf, void* buf, size_t buf_size)
{
    ssize_t size = 0;

    if (!cmi_intf_is_connected(intf)) {
        return(SPKERR_BADSEQ);
    }

    switch(intf->intf_type) {
    case cmi_intf_tcp:
        size = cmi_tcp_read_msg(intf->conn_sockfd,
                                &intf->endian_cur,
                                buf,
                                buf_size);
        break;
    case cmi_intf_udp:
        size = cmi_udp_read_msg(intf->conn_sockfd,
                                &intf->endian_cur,
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
        zlog_error(cmi_zc, "failed to recv/read: sock=%d, ret=%ld, errmsg=\'%s\'",
                            intf->conn_sockfd, size, strerror(errno));
        size = SPKERR_RESETSYS;
    } else {
        // got a packet
        zlog_info(cmi_zc, "got packet: size=%zu", size);
        if (!cmi_msg_is_valid(buf, size)) {
            zlog_error(cmi_zc, "got illegal message");
            cmi_msg_dump(ZLOG_LEVEL_ERROR, buf, size);
            size = 0;
        }
    }

    return(size);
}

ssize_t cmi_intf_write_msg(cmi_intf_t* intf, void* buf, size_t buf_size)
{
    ssize_t size = 0;

    if (!cmi_intf_is_connected(intf)) {
        return(SPKERR_BADSEQ);
    }

    if (!cmi_msg_is_valid(buf, buf_size)) {
        zlog_error(cmi_zc, "try to send illegal message");
        cmi_msg_dump(ZLOG_LEVEL_ERROR, buf, buf_size);
        return(SPKERR_PARAM);
    }

    pthread_mutex_lock(&intf->conn_lock);
    switch(intf->intf_type) {
    case cmi_intf_tcp:
        size = cmi_tcp_write_msg(intf->conn_sockfd,
                                   intf->endian_cur,
                                   buf,
                                   buf_size);
        break;
    case cmi_intf_udp:
        size = cmi_udp_write_msg(intf->conn_sockfd,
                                   intf->endian_cur,
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
        zlog_error(cmi_zc, "failed to write/send: sock=%d, ret=%ld, errmsg=\'%s\'",
                            intf->conn_sockfd, size, strerror(errno));
        size = SPKERR_RESETSYS;
    } else {
        assert(size == buf_size);
    }

    return(size);
}

int cmi_intf_write_flist(cmi_intf_t* intf, cmi_data_filelist_t* dfl,
                         uint32_t req_frag)
{
    ssize_t written;
    cmi_data_t msg_data;
    int ret = -1;

    if (req_frag == (uint32_t)-1) {
        // fast mode
        size_t xferred = 0;
        int frag_id = 0;
        while(xferred < sizeof(cmi_data_filelist_t)) {
            size_t xfer_sz = MIN(CMI_MAX_FRAGSIZE,
                                 sizeof(cmi_data_filelist_t) - xferred);
            int is_eof = (xferred+xfer_sz == sizeof(cmi_data_filelist_t));

            cmi_msg_build_datafrag(&msg_data,
                                   data_type_flist,
                                   ((char*)dfl)+xferred,
                                   xfer_sz,
                                   frag_id++,
                                   is_eof);
            written = cmi_intf_write_msg(intf,
                                         &msg_data,
                                         sizeof(cmi_data_t));
            if (written != sizeof(cmi_data_t)) {
                ret = SPKERR_RESETSYS;
                goto out;
            }
            xferred += xfer_sz;
        }
        zlog_notice(cmi_zc, "flist sent: mode=fast, written=%ld", xferred);
        ret = SPK_SUCCESS;
    } else {
        // slow mode
        uint64_t frag_start = (uint64_t)req_frag * CMI_MAX_FRAGSIZE;
        uint64_t frag_end = frag_start + CMI_MAX_FRAGSIZE;

        if (frag_start >= sizeof(cmi_data_filelist_t)) {
            zlog_warn(cmi_zc, "invalid req_frag: req_frag = %u", req_frag);
            ret = SPKERR_PARAM;
            goto out;
        }
        frag_end = MIN(frag_end, sizeof(cmi_data_filelist_t));
        int is_eof = (frag_end == sizeof(cmi_data_filelist_t));

        cmi_msg_build_datafrag(&msg_data,
                               data_type_flist,
                               ((char*)dfl)+frag_start,
                               frag_end - frag_start,
                               req_frag,
                               is_eof);
        written = cmi_intf_write_msg(intf,
                                     &msg_data,
                                     sizeof(cmi_data_t));
        if (written != sizeof(cmi_data_t)) {
            ret = SPKERR_RESETSYS;
            goto out;
        }
        if (is_eof) {
            zlog_notice(cmi_zc, "flist sent: mode=slow, written=%ld",
                        frag_end - frag_start);
        }
        ret = SPK_SUCCESS;
    }

out:
    if (ret != SPK_SUCCESS) {
        zlog_error(cmi_zc, "failed to sent flist: ret=%d", ret);
    }
    return(ret);
}

int cmi_intf_write_snapshot(cmi_intf_t* intf, char* buf_snap, size_t buf_snap_sz)
{
    ssize_t written;
    cmi_data_t msg_data;

    cmi_msg_build_datafrag(&msg_data,
                           data_type_snap,
                           buf_snap,
                           buf_snap_sz, -1/*frag_id*/, 1/*eof*/);
    written = cmi_intf_write_msg(intf,
                                 &msg_data,
                                 sizeof(cmi_data_t));
    if (written != sizeof(cmi_data_t)) {
        return(SPKERR_RESETSYS);
    }
    return(SPK_SUCCESS);
}

int cmi_intf_write_cmdresp(cmi_intf_t* intf,
                           uint16_t cmd_type,
                           uint16_t success)
{
    ssize_t written;
    cmi_cmdresp_t msg_cmdresp;

    cmi_msg_build_hdr((cmi_msg_hdr_t*)&msg_cmdresp,
                      msg_code_cmdresp,
                      sizeof(cmi_cmdresp_t));
    msg_cmdresp.cmd_type = cmd_type;
    msg_cmdresp.success = success;
    written = cmi_intf_write_msg(intf,
                                 &msg_cmdresp,
                                 sizeof(cmi_cmdresp_t));
    if (written != sizeof(cmi_cmdresp_t)) {
        return(SPKERR_RESETSYS);
    }
    return(SPK_SUCCESS);
}

cmi_endian cmi_intf_get_endian(cmi_intf_t* intf)
{
    assert(intf->endian_cur == cmi_endian_big ||
           intf->endian_cur == cmi_endian_little);

    return(intf->endian_cur);
}


