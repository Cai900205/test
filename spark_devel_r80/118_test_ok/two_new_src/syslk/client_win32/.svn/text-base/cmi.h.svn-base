typedef unsigned char uint8_t;
typedef unsigned short int uint16_t;
typedef unsigned int uint32_t;
typedef unsigned long long int uint64_t;
typedef long long int int64_t;

#define CLIENT_APP_NAME		"采集设备终端控制台"
#define CLIENT_VER_STR		"1.0"

#define CMI_MAX_FL_SLOTS	(999)
#define CMI_MAX_DL_SIZE		(32*1024*1024)

#define CMI_SUCCESS			(0)
#define CMIERR_BASE			(1000)
#define CMIERR_UNKNOWN		(9999)
#define CMIERR_EINVAL		(-(CMIERR_BASE+1))
#define CMIERR_EAGAIN		(-(CMIERR_BASE+2))
#define CMIERR_EPACKET		(-(CMIERR_BASE+3))
#define CMIERR_BADSEQ		(-(CMIERR_BASE+4))
#define CMIERR_TIMEOUT		(-(CMIERR_BASE+5))

#define CMI_MAX_FRAGSIZE    (16*1024)
#define CMI_MAX_FCNUM       (4)

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
	cmd_type_sysdown,	// 0xFF10
	cmd_type_upgrade,	// 0xFF11
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
    sys_state_upgrade,
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
#define CMI_CMDEXEC_FAIL    (0xF0F0)
#define CMI_CMDEXEC_SUCC    (0xF1F1)
#define CMI_CMDEXEC_BUSY    (0xF2F2)
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
            uint32_t params[CMI_CMD_VAR_LEN];
        } config;
    }u;
} cmi_cmd_t;

typedef struct {
    cmi_msg_hdr_t hdr;
    uint16_t success;   // 0xF0F0  表示失败
    // 0xF1F1  表示成功
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
    uint64_t fb_recv_cnt[CMI_MAX_FCNUM]; // 4路光纤收/发计数
    uint64_t disk_size;     // 存储容量
    uint64_t disk_free;     // 存储设备剩余容量
    uint32_t disk_rspd;
    uint32_t disk_wspd;
    uint32_t rsvd2[16];
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


typedef struct
{
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

typedef struct
{
    uint16_t tag;        // 0x5a5a
    uint16_t machine;
    uint16_t slot_num;
    uint16_t rsvd;
    uint32_t total_sz_h;
    uint32_t total_sz_l;
    uint32_t free_sz_h;
    uint32_t free_sz_l;
} cmi_sysinfo_t;

typedef struct 
{
    cmi_sysinfo_t   sys_info;
    cmi_work_list_t work_list[CMI_MAX_FL_SLOTS];
} cmi_data_filelist_t;

#define CMI_TIMERID			(0xfe)
#define WM_CMI_UPDATE		(WM_USER + 1000)
#define WM_CMI_PROGRESS		(WM_USER + 1001)
#define WM_CMI_SNAPSHOT		(WM_USER + 1002)
#define CMI_UPDATE_STATUS	(1)
#define CMI_UPDATE_FILELIST	(2)

extern cmi_status_t g_status;
extern CMutex	g_fl_lock;
extern cmi_data_filelist_t g_filelist;
extern int g_cancel_req;
extern int g_quit_req;
extern HWND hwndProgress;
extern HWND hwndMain;

extern int cmi_init();
extern int cmi_cmd_disp_init();
extern int cmi_cmd_disp_inquiry();
extern int cmi_cmd_disp_snapshot();
extern int cmi_cmd_disp_sysdown();
extern int cmi_cmd_disp_getfilelist();
extern int cmi_cmd_disp_rec(int start);
extern int cmi_cmd_disp_format();
extern int cmi_cmd_disp_dl(int start, int slot_id, uint32_t blk_start, uint32_t blk_num);
extern int cmi_cmd_disp_delete(int slot_id);
extern int cmi_cmd_disp_synctime();
extern int cmi_cmd_disp_play(int slot_id);
extern int cmi_cmd_disp_stopplay();
extern int cmi_func_download(HANDLE hSaveFile, cmi_work_list_t* fl);
extern int cmi_func_upload(HANDLE hSaveFile);
extern int cmi_func_upgrade(HANDLE hSaveFile);
extern int cmi_func_getfilelist(void);

extern DWORD WINAPI cmi_recv_thread(LPVOID lpParameter);
#define SAFE_RELEASE(ptr)	if (ptr) {free(ptr);ptr=NULL;}
#define ALIGN(D, A)	(((D)+(A)-1) & (-(A)))
extern uint64_t sys_lktm_to_systm(uint32_t lktm);
extern uint32_t sys_systm_to_lktm(uint64_t systm);
extern CString format_number(int64_t number);
extern double format_number_mega(uint64_t byte);

extern CString g_svr_ip;
extern int g_svr_port;
extern int g_use_tcp;
extern uint32_t g_sys_version;
extern uint32_t g_agt_version;

#define LOW32(DATA)		((uint32_t)DATA)
#define HIGH32(DATA)		((uint32_t)(DATA>>32))