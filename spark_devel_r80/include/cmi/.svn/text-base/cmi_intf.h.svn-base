#ifndef __CMI_INTF_H__
#define __CMI_INTF_H__

#define CMI_MOD_VER     "0.9.151230"

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include "zlog/zlog.h"

#define CMI_MAX_FRAGSIZE    (16*1024)
#define CMI_MAX_FCNUM       (4)
#define CMI_MAX_SLOTS       (999)

enum {
    cmd_type_base = 0xff00,
    cmd_type_inquiry = cmd_type_base,    // 0xFF00 状态查询
    cmd_type_init,      // 0xFF01 初始化
    cmd_type_filelist,  // 0xFF02 获取文件列表内容
    cmd_type_format,    // 0xFF03 格式化
    cmd_type_delete,    // 0xFF04 删除文件
    cmd_type_start_rec, // 0xFF05 开始采集记录
    cmd_type_stop_rec,  // 0xFF06 停止采集记录
    cmd_type_start_play,// 0xFF07 开始回放
    cmd_type_stop_play, // 0xFF08 停止回放
    cmd_type_snapshot,  // 0xFF09 获取快视数据
    cmd_type_start_ul,  // 0xFF0A 开始数据上传
    cmd_type_stop_ul,   // 0xFF0B 停止数据上传
    cmd_type_start_dl,  // 0xFF0C 开始数据下传
    cmd_type_stop_dl,   // 0xFF0D 停止数据下传
    cmd_type_sync_time, // 0xFF0E
    cmd_type_config,    // 0xFF0F
    cmd_type_sysdown,   // 0xFF10
    cmd_type_upgrade,   // 0xFF11
    cmd_type_max
} cmi_cmd_type;

// 系统工作状态
enum {
    sys_state_base = 0,
    sys_state_idle = sys_state_base,    // 0x0 空闲
    sys_state_rec,      // 0x1 采集中
    sys_state_play,     // 0x2 回放中
    sys_state_ul,       // 0x3 本地上传中
    sys_state_dl,       // 0x4 本地下载中
    sys_state_delete,   // 0x5 文件删除中
    sys_state_format,   // 0x6 格式化中
    sys_state_upgrade,  // 0x7
    sys_state_max
} cmi_sys_state;

enum {
    data_type_none  = 0x0,
    data_type_flist = 0xfe01,
    data_type_ul,       // 0xfe02
    data_type_dl,       // 0xfe03
    data_type_snap,     // 0xfe04
    data_type_upgrade,  // 0xfe05
    data_type_max,
} cmi_data_type;

enum {
    msg_code_cmd        = 0x01dc,
    msg_code_cmdresp    = 0x01ef,
    msg_code_status     = 0x0118,
    msg_code_data       = 0x0119,
} cmi_msg_code;

#define CMI_SYNC_TAG        (0x5a5b)
typedef struct {
    uint16_t sync_tag;  // 同步字符CMI_SYNC_TAG
    uint16_t msg_len;   // 消息长度
    uint16_t msg_code;  // enum cmi_msg_code
    uint16_t msg_srcid; // 源设备标识
    uint16_t msg_destid;// 目的设备标识
    uint16_t pcid;      // 工作机器序号（留扩展用）
    uint32_t msg_ts;    // 时间标签
    uint32_t version;
    uint32_t csum;      // 检查和:用于校验数据
} cmi_msg_hdr_t;

typedef struct {  //结构体大小: 32B
    cmi_msg_hdr_t hdr;
    uint16_t cmd_type;  // 命令类型 cmi_cmd_type
    uint16_t rsvd[3];   // 预留信息
#define CMI_CMD_VAR_LEN     (4)
    union {
        struct {
            uint32_t words[CMI_CMD_VAR_LEN];
        } all;
        struct {
            uint32_t index;     // 操作序号可以是删除、回放时的文件序号也可以为获取文件列表内容的序号
            uint32_t blk_start; //dl 32M
            uint32_t blk_num;   //dl 32M
            uint32_t frag_id;   //dl
        } file;
        struct {
            uint32_t lktime;
        } tm;
        struct {
            uint16_t hdr;
            uint16_t len;
            uint16_t mode;
            uint8_t  agc;
            uint8_t  clk_ch;
            uint32_t sample;
            uint8_t  freq;
            uint8_t  ctl;
            uint16_t tail;
        } config;
    }u;
} cmi_cmd_t;

typedef struct {
    cmi_msg_hdr_t hdr;
#define CMI_CMDEXEC_FAIL    (0xF0F0)
#define CMI_CMDEXEC_SUCC    (0xF1F1)
#define CMI_CMDEXEC_UNKNOWN (0xF2F2)
    uint16_t success;
    uint16_t cmd_type;  // 命令类型 cmi_cmd_type
#define CMI_CMDRESP_VAR_LEN     (4)
    union {
        struct {
            uint32_t words[CMI_CMDRESP_VAR_LEN];
        } all;
        struct {
            uint32_t sys_ver;
            uint32_t agt_ver;
        } version;
    }u;
} cmi_cmdresp_t;

typedef struct {
    cmi_msg_hdr_t hdr;
    uint32_t sys_state;     // 系统工作状态 cmi_sys_state
    uint32_t svr_time;
    uint16_t fb_link[CMI_MAX_FCNUM];    // 4路光纤连接状态1表示连接0表示不连接
    uint32_t fb_speed[CMI_MAX_FCNUM];   // 4路光纤接收数据速率
    uint64_t fb_count[CMI_MAX_FCNUM];   // 4路光纤收/发计数
    uint64_t disk_size;     // 存储容量
    uint64_t disk_free;     // 存储设备剩余容量
    uint32_t disk_rspd;
    uint32_t disk_wspd;
#define CMI_STATUS_VAR_LEN     (16)
    uint32_t rsvd2[CMI_STATUS_VAR_LEN];
} cmi_status_t;

typedef struct {
    cmi_msg_hdr_t hdr;
    uint16_t data_type;
    uint16_t eof;
    uint32_t frag_len;
    uint32_t frag_id;
    uint8_t  frag_data[CMI_MAX_FRAGSIZE];    // 数据内容
} cmi_data_t;
#define CMI_MAX_MSGSIZE     (sizeof(cmi_data_t))

typedef struct {
    uint32_t tag; // 0x18efdc0a
    uint16_t slot_id;
    uint16_t data_src;
    uint32_t work_mark;
    uint32_t begin_tm;
    uint32_t end_tm;
    uint16_t task_cmd[8];
    uint16_t work_place[8];
    uint16_t file_desc[14];
    uint32_t file_sz_h;
    uint32_t file_sz_l;
} cmi_work_list_t;

typedef struct {
    uint16_t tag;
    uint16_t machine;
    uint16_t slot_num;
    uint16_t rsvd;
    uint32_t total_sz_h;
    uint32_t total_sz_l;
    uint32_t free_sz_h;
    uint32_t free_sz_l;
} cmi_sysinfo_t;

typedef struct {
    cmi_sysinfo_t   sys_info;
    cmi_work_list_t work_list[CMI_MAX_SLOTS];
} cmi_data_filelist_t;

typedef enum {
    cmi_intf_tcp = 1,
    cmi_intf_udp,
    cmi_intf_max
} cmi_intf_type;

typedef enum {
    cmi_endian_big,
    cmi_endian_little,
    cmi_endian_auto,
} cmi_endian;

typedef enum {
    cmi_type_server,
    cmi_type_client
} cmi_type;

typedef struct {
    cmi_type        type;
    cmi_intf_type   intf_type;
    cmi_endian      endian_req;

    cmi_endian      endian_cur;
    int             sock_svr;

    pthread_mutex_t conn_lock;
    int             conn_sockfd;
    struct sockaddr_in conn_addr;
    int             conn_valid;
} cmi_intf_t;


extern const cmi_endian cmi_our_endian;

int cmi_module_init(const char* log_cat);

cmi_intf_t* cmi_intf_open(cmi_type type,
                          cmi_intf_type intf_type,
                          cmi_endian peer_endian);
int cmi_intf_connect(cmi_intf_t* intf, const char* ipaddr,int port);
void cmi_intf_disconnect(cmi_intf_t* intf);
int cmi_intf_is_connected(cmi_intf_t* intf);
void cmi_intf_close(cmi_intf_t* intf);

cmi_endian cmi_intf_get_endian(cmi_intf_t* intf);
ssize_t cmi_intf_read_msg(cmi_intf_t* intf, void* buf, size_t buf_size);
ssize_t cmi_intf_write_msg(cmi_intf_t* intf, void* buf, size_t bufsize);
int cmi_intf_write_flist(cmi_intf_t* intf, cmi_data_filelist_t* dfl,
                         uint32_t req_frag);
int cmi_intf_write_snapshot(cmi_intf_t* intf, char* buf_snap, size_t buf_snap_sz);
int cmi_intf_write_cmdresp(cmi_intf_t* intf,
                           uint16_t cmd_type,
                           uint16_t success);

void cmi_msg_build_hdr(cmi_msg_hdr_t* hdr, uint16_t msg_code, size_t msg_len);
void cmi_msg_build_datafrag(cmi_data_t* msg_buf, int data_type,
                            void* frag_buf, size_t frag_size,
                            uint32_t frag_id, int is_eof);
void cmi_msg_reform_hdr(cmi_msg_hdr_t* hdr, cmi_endian peer_endian);
void cmi_msg_reform_body(uint16_t msg_code, void* msg, cmi_endian peer_endian);
void cmi_msg_reform_flist(cmi_data_filelist_t* dfl, cmi_endian peer_endian);

int cmi_msg_is_same(cmi_msg_hdr_t* msg1, cmi_msg_hdr_t* msg2);
void cmi_msg_dump(zlog_level ll, void* msg, size_t size);

char* cmi_desc_cmdtype2str(int cmd_type);
char* cmi_desc_sysstate2str(int state);

#define MSG_CODE(M)         (((cmi_msg_hdr_t*)(M))->msg_code)
#define MSG_SIZE(M)         (((cmi_msg_hdr_t*)(M))->msg_len)
#define MSG_SYNCTAG(M)      (((cmi_msg_hdr_t*)(M))->sync_tag)
#define CMD_TYPE(M)         (((cmi_cmd_t*)(M))->cmd_type)
#define CMD_FRAGID(M)       (((cmi_cmd_t*)(M))->u.file.frag_id)
#define CMD_BLKSTART(M)     (((cmi_cmd_t*)(M))->u.file.blk_start)
#define CMD_BLKNUM(M)       (((cmi_cmd_t*)(M))->u.file.blk_num)
#define DATA_FRAGLEN(M)     (((cmi_data_t*)(M))->frag_len)
#define DATA_FRAGDATA(M)    (((cmi_data_t*)(M))->frag_data)
#define DATA_DATATYPE(M)    (((cmi_data_t*)(M))->data_type)
#define DATA_FRAGID(M)      (((cmi_data_t*)(M))->frag_id)
#define DATA_EOF(M)         (((cmi_data_t*)(M))->eof)

#endif
