#ifndef __IPS_INTF_H__
#define __IPS_INTF_H__

#define IPS_MOD_VER     "0.9.151230"

#define IPS_MAX_FCNUM   (4)

#define IPS_MAX_PORTS_IN_DEV        (2)
#define IPS_IS_PORT_VALID(PORT_ID)  \
            (PORT_ID >= 0 && PORT_ID < IPS_MAX_PORTS_IN_DEV)
#define IPS_MAX_PCS_IN_EP           (4)
#define IPS_IS_PCID_VALID(PC_ID)  \
            (PC_ID >= 0 && PC_ID < IPS_MAX_PCS_IN_EP)

#define IPS_EPMODE_MASTER       (1)
#define IPS_EPMODE_SLAVE        (0)

typedef uint32_t IPS_EPID;
#define IPS_EPID_2_PORTID(ID)       (ID & 0x03)
#define IPS_EPID_2_DEVID(ID)        ((ID & 0x3C) >> 2)
#define IPS_EPID_2_MODE(ID)         ((ID & 0x40) >> 6)
#define IPS_MAKE_EPID(MODE, DID, PID)   ((MODE&0x01)<<6 | (DID&0x0f)<<4 | (PID&0x03))

typedef struct {
    int is_master;
    IPS_EPID mst_id;
    IPS_EPID slv_id;
    uint8_t mst_port;
    uint8_t slv_port;
} ips_linkdesc_t;

typedef struct ips_pcdesc {
    IPS_EPID    src_id;
    uint32_t    capacity;

    IPS_EPID    dest_id;
    uint32_t    sector_num;
    uint32_t    sector_sz;
} ips_pcdesc_t;

typedef struct ips_epdesc {
    uint32_t    capacity;
    int         pc_num;
    ips_pcdesc_t pcdesc_tbl[IPS_MAX_PCS_IN_EP];
} ips_epdesc_t;

typedef struct 
{
    uint64_t    recv_sz_16b[IPS_MAX_FCNUM];
    uint32_t    rec_spd_16b[IPS_MAX_FCNUM];
    uint32_t    play_spd_16b[IPS_MAX_FCNUM];
    uint8_t     work_mode; // 0x01: rec 0x02: playback
    uint8_t     fc_link_state;
    uint8_t     srio_link_state;
    uint8_t     rsvd;
} ips_fcstat_lk1;

struct ips_pcctx;

int ips_module_init(const char* log_cat);

int ips_ep_init(IPS_EPID epid, const ips_epdesc_t* epdesc);

struct ips_pcctx* ips_chan_open(IPS_EPID epid, int pc_id);
void ips_chan_close(struct ips_pcctx* pcctx);
int ips_chan_config(struct ips_pcctx* pcctx, char* config_buf, size_t buf_sz);
int ips_chan_start(struct ips_pcctx* pcctx, SPK_DIR dir);
int ips_chan_stop(struct ips_pcctx* pcctx);
int ips_chan_is_start(struct ips_pcctx* pcctx);

ssize_t ips_chan_write(struct ips_pcctx* pcctx, void* buf, size_t size);
ssize_t ips_chan_read(struct ips_pcctx* pcctx, void** pbuf, size_t min_size, size_t max_size);
int ips_chan_free_buf(struct ips_pcctx* pcctx, size_t buf_sz);

SPK_DIR ips_chan_get_dir(struct ips_pcctx* pcctx);
void* ips_chan_get_txbuf(struct ips_pcctx* pcctx, size_t* size);

int ips_chan_get_chstat(struct ips_pcctx* pcctx, void* chstat, size_t* stat_size);
int ips_chan_set_chstat(struct ips_pcctx* pcctx, void* chstat, size_t stat_size);
spk_stats_t* ips_chan_get_stats(struct ips_pcctx* pcctx);
#endif //
