/*************************************************************************
    > File Name: main.c
    > Author:
    > Mail:
    > Created Time: Wed 12 Aug 2015 05:16:49 PM CST
 ************************************************************************/

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <malloc.h>
#include <unistd.h>
#include <string.h>
#include <inttypes.h>
#include <pthread.h>
#include <assert.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "syslk.h"

zlog_category_t* sys_zc = NULL;
sys_env_t sys_env;
sys_ctx_t sys_ctx;

uint64_t sys_fpga_stat[2];
ips_mode_t syslk_ips_mode = {
    .dma_use_nlwr = 0,
    .dma_use_chain = 0,
    .tx.use_wptr = 1,
    .tx.use_rptr = 1,
    .rx.use_wptr = 1,
    .rx.use_rptr = 1
};
// lktm
//  sec: [0: 5] - 6bit
//  min: [6:11] - 6bit
// hour: [12:16] - 5bit
// mday: [17:21] - 5bit
//  mon: [22:25] - 4bit
// year: [26:31] - 6bit
uint32_t sys_systm_to_lktm(uint64_t systm)
{
    uint32_t lk_tm;

    struct tm local_time;
    localtime_r((time_t*)&systm, &local_time);

    lk_tm = local_time.tm_sec;
    lk_tm |= ((local_time.tm_min)  << 6);
    lk_tm |= ((local_time.tm_hour) << 12);
    lk_tm |= ((local_time.tm_mday) << 17);
    lk_tm |= ((local_time.tm_mon + 1)  << 22);
    lk_tm |= ((local_time.tm_year-100) << 26);

    return(lk_tm);
}

uint64_t sys_lktm_to_systm(uint32_t lktm)
{
    uint64_t systm;

    struct tm local_time;
    memset(&local_time, 0, sizeof(struct tm));

    local_time.tm_sec  = (lktm >> 0) & 0x3f;
    local_time.tm_min  = (lktm >> 6) & 0x3f;
    local_time.tm_hour = (lktm >> 12) & 0x1f;
    local_time.tm_mday = (lktm >> 17) & 0x1f;
    local_time.tm_mon  = ((lktm >> 22) & 0x0f) - 1;
    local_time.tm_year = (lktm >> 26) & 0x3f;
    local_time.tm_year += 100;

    systm = mktime(&local_time);
    return(systm);
}

void sys_change_state(int new_state)
{
    if (sys_ctx.sys_state != new_state) {
        zlog_notice(sys_zc, "*** change state: %s -> %s",
                    cmi_desc_sysstate2str(sys_ctx.sys_state),
                    cmi_desc_sysstate2str(new_state));
        sys_ctx.sys_state = new_state;
    }
}

static int __sys_msg_parse_data_ul(cmi_data_t* msg, size_t msg_sz)
{
    int ret = -1;
    int i;

    if (DATA_DATATYPE(msg) != data_type_ul) {
        assert(0);
    }
    
    if (sys_ctx.sys_state != sys_state_ul) {
        // ignore
        zlog_warn(sys_zc, "found data msg while not in upload state");
        ret = SPKERR_BADSEQ;
        goto out;
    }

    assert(!(DATA_FRAGLEN(msg) % (SYS_MAX_PIPES*SYS_INTERLACE_SIZE)));
    for (i=0; i<SYS_MAX_PIPES; i++) {
        sys_ul_ctx_t* ul_ctx = sys_ctx.ul_ctx_tbl[i];
        assert(ul_ctx);
        assert(ul_ctx->dfvcm);
        dfv_bufq_node_t* node = NULL;
        uint8_t* data_ptr = DATA_FRAGDATA(msg);
        
        node = ul_ctx->work_node;
        if (!node) {
            do {
                node = dfv_bufq_dequeue(dfvcm_get_freeq(ul_ctx->dfvcm));
                if (node) {
                    ul_ctx->work_node = node;
                    node->buf_offset = 0l;
                    break;
                }
                usleep(100);
            } while(1);
        }

        assert(node);
        // do de-interlace
        data_ptr += i*SYS_INTERLACE_SIZE;
        while(data_ptr + SYS_INTERLACE_SIZE <= (DATA_FRAGDATA(msg)+DATA_FRAGLEN(msg))) {
            memcpy(node->buf_ptr + node->buf_offset,
                   data_ptr,
                   SYS_INTERLACE_SIZE);
            node->buf_offset += SYS_INTERLACE_SIZE;
            data_ptr += (SYS_MAX_PIPES*SYS_INTERLACE_SIZE);
        }
        if (node->buf_offset >= node->buf_sz || DATA_EOF(msg)) {
            // FIXME: payload_size & buf_size alignment check
            assert(node->buf_offset == node->buf_sz || DATA_EOF(msg));
            node->valid_sz = node->buf_offset;
            zlog_notice(sys_zc, "%zu bytes uploaded, write to dfvcm", node->buf_offset);
            dfv_bufq_enqueue(dfvcm_get_workq(ul_ctx->dfvcm), node);
            ul_ctx->work_node = NULL;
        }
    }
    
    if (DATA_EOF(msg)) {
        zlog_notice(sys_zc, "found upload EOF");
        sys_cmd_exec_stopul(NULL);
    }
    
    ret = SPK_SUCCESS;

out:        
    return(ret);
}

static int __sys_msg_parse_data_upgrade(cmi_data_t* msg, size_t msg_sz)
{
    sys_ctx_t* ctx = &sys_ctx;
    int ret = -1;

    if (DATA_DATATYPE(msg) != data_type_upgrade) {
        assert(0);
    }
    
    if (ctx->sys_state != sys_state_upgrade) {
        // ignore
        zlog_warn(sys_zc, "found upgrade msg while not in upgrade state");
        ret = SPKERR_BADSEQ;
        goto errout;
    }

    assert(ctx->upgrd_ctx);
    if (sys_ctx.upgrd_ctx->buf_sz + DATA_FRAGLEN(msg) > SYS_MAX_UPGRD_SCRIPT_SZ) {
        ret = SPKERR_EACCESS;
        goto errout;
    }

    memcpy(ctx->upgrd_ctx->script_buf + ctx->upgrd_ctx->buf_sz,
           DATA_FRAGDATA(msg),
           DATA_FRAGLEN(msg));
    ctx->upgrd_ctx->buf_sz += DATA_FRAGLEN(msg);
    if (DATA_EOF(msg)) {
        zlog_notice(sys_zc, "found upgrade EOF: file_sz=%zu", ctx->upgrd_ctx->buf_sz);
        // execute
        FILE* file;
        file = fopen("/opt/sys_upgrade.tar.gz", "w+");
        if (!file) {
            zlog_error(sys_zc, "failed to open upgrade script file");
            ret = SPKERR_EACCESS;
            goto errout;
        }
        ssize_t xfer = fwrite(ctx->upgrd_ctx->script_buf,
                              1,
                              ctx->upgrd_ctx->buf_sz,
                              file);
        fclose(file);
        if (xfer != ctx->upgrd_ctx->buf_sz) {
            zlog_error(sys_zc, "failed to write upgrade script file");
            ret = SPKERR_EACCESS;
            goto errout;
        }
        zlog_notice(sys_zc, "upgrade script written: size=%ld", xfer);
        spk_os_exec("rm -Rf /opt/sys_upgrade");
        spk_os_exec("tar xzf /opt/sys_upgrade.tar.gz -C /opt");
        spk_os_exec("cd /opt/sys_upgrade ; ./install.sh");
        sys_change_state(sys_state_idle);
    }
    
    return(SPK_SUCCESS);
    
errout:
    if (ctx->upgrd_ctx) {
        if (ctx->upgrd_ctx->script_buf) {
            SAFE_RELEASE(ctx->upgrd_ctx->script_buf);
            ctx->upgrd_ctx->buf_sz = 0;    
        }
        SAFE_RELEASE(ctx->upgrd_ctx);
        sys_change_state(sys_state_idle);
    }
    return(ret);
}

static int sys_msg_parse_data(cmi_data_t* msg, size_t msg_sz)
{
    int ret = -1;

    if (DATA_DATATYPE(msg) == data_type_ul) {
        ret = __sys_msg_parse_data_ul(msg, msg_sz);
    } else if (DATA_DATATYPE(msg) == data_type_upgrade) {
        ret = __sys_msg_parse_data_upgrade(msg, msg_sz);
    } else {
        assert(0);
    }
    return(ret);
}

static void l_trim(char* output, char* input)
{
    assert(input != NULL);
    assert(output != NULL);
    assert(output != input);
    
    for(; (*input != '\0' && isspace(*input)); ++input) {
        ;
    }
    strcpy(output, input); 
}

static void lr_trim(char* output, char* input)
{
    char *p = NULL;
    assert(input != NULL);
    assert(output != NULL);
    l_trim(output, input);
    for(p = output + strlen(output) - 1; p >= output && isspace(*p); --p) {
        ;
    }
    *(++p) = '\0';
}

int  syslk_parse_keyval(FILE *fp, char* KeyName, char* value)
{
    char buf_o[KEYVALLEN], buf_i[KEYVALLEN];
    char *buf = NULL;
    char *c;
    char keyname[KEYVALLEN];
    char KeyVal[KEYVALLEN];
    int ret = -1;
    
    while( !feof(fp) && fgets(buf_i,KEYVALLEN,fp) != NULL) {
        l_trim(buf_o, buf_i);
        if( strlen(buf_o) <= 0 ) {
            continue;	
        }
        buf = NULL;
        buf = buf_o;
        if ( buf[0] == '#') {
            continue;	
        } else {
            if( (c = (char*)strchr(buf, '=')) == NULL ) {
                continue;
            }	
            sscanf( buf, "%[^=|^ |^\t]", keyname );
            if( strcmp(keyname, KeyName) == 0 ) {
                sscanf( ++c, "%[^\n]", KeyVal );
                char *KeyVal_o = (char *)malloc(strlen(KeyVal) + 1);
                if(KeyVal_o != NULL) {
                    memset((void *)KeyVal_o, 0, sizeof(KeyVal_o));
                    lr_trim(KeyVal_o, KeyVal);
                    if(KeyVal_o && strlen(KeyVal_o) > 0) {
                        strcpy(value, KeyVal_o);
                    }
                    free(KeyVal_o);
                    KeyVal_o = NULL;
                    ret = 0;
                    break;
                }
            }
        }
        
    }
    return ret;
}

static void syslk_parse_ips_mode(char *filename)
{
    FILE *fp = NULL;
    uint8_t value = 0;
    char data[KEYVALLEN];
    int ret = -1;

    if ((fp = fopen(filename, "r")) == NULL) {
        fprintf(stderr, "can not open %s", filename);
        return;
    }
    fseek(fp, 0, SEEK_SET);
    ret = syslk_parse_keyval(fp, "dma_use_nlwr", data);
    if (ret == 0) {
        value = atoi(data);
        syslk_ips_mode.dma_use_nlwr = value; 
    }
    
    fseek(fp, 0, SEEK_SET);
    ret = syslk_parse_keyval(fp, "dma_use_chain", data);
    if (ret == 0) {
        value = atoi(data);
        syslk_ips_mode.dma_use_chain = value; 
    }
    
    fseek(fp, 0, SEEK_SET);
    ret = syslk_parse_keyval(fp, "tx_use_wptr", data);
    if (ret == 0) {
        value = atoi(data);
        syslk_ips_mode.tx.use_wptr = value; 
    }
    
    fseek(fp, 0, SEEK_SET);
    ret = syslk_parse_keyval(fp,"tx_use_rptr", data);
    if (ret == 0) {
        value = atoi(data);
        syslk_ips_mode.tx.use_rptr = value; 
    }
    
    fseek(fp, 0, SEEK_SET);
    ret = syslk_parse_keyval(fp, "rx_use_wptr", data);
    if (ret == 0) {
        value = atoi(data);
        syslk_ips_mode.rx.use_wptr = value; 
    }
    
    fseek(fp, 0, SEEK_SET);
    ret = syslk_parse_keyval(fp, "rx_use_rptr", data);
    if (ret == 0) {
        value = atoi(data);
        syslk_ips_mode.rx.use_rptr = value; 
    }

    fclose(fp);
}

int main(int argc, char **argv)
{
    int ret;
    int i;
    char msg_ibuf[CMI_MAX_MSGSIZE];

    umask(022);

    // build assert
    assert(DFV_MAX_SLOTS == CMI_MAX_SLOTS);
    assert(IPS_MAX_FCNUM == CMI_MAX_FCNUM);
    assert(!(SYS_CACHE_SIZE % CMI_MAX_FRAGSIZE));

    printf("\n");
    printf("SSSSSSSSSS\n");
    printf("SSSSSSSSSS\n");
    printf("SSS    SSS  PPPPP      AA     RRRRR    KK  KK\n");
    printf("SS  SS  SS  PP  PP    AAAA    RR  RR   KK  KK\n");
    printf("SS  SSSSSS  PP  PP   AA  AA   RR  RR   KK KK\n");
    printf("SSS  SSSSS  PP  PP   AA  AA   RR  RR   KK KK\n");
    printf("SSSS  SSSS  PPPPP    AA  AA   RRRRR    KKKK\n");
    printf("SSSSS  SSS  PP       AAAAAA   RR RR    KK KK\n");
    printf("SSSSSS  SS  PP       AA  AA   RR  RR   KK KK\n");
    printf("SS  SS  SS  PP       AA  AA   RR  RR   KK  KK\n");
    printf("SSS    SSS  PP       AA  AA   RR  RR   KK  KK\n");
    printf("SSSSSSSSSS  \n");
    printf("SSSSSSSSSS  ==SYS: LK  ==VER: %s\n", SYS_VERSION);
    printf("\n");

    // initialize log system
    zlog_init("./zlog.conf");
    sys_zc = zlog_get_category("SYS");
    assert(sys_zc);

    zlog_notice(sys_zc, "------------------------------------------");
    zlog_notice(sys_zc, "==> system starting ...");
    zlog_notice(sys_zc, "==> dump system versions ...");
    zlog_notice(sys_zc, "  SYS: %s", SYS_VERSION);
    zlog_notice(sys_zc, "  IPS: %s", IPS_MOD_VER);
    zlog_notice(sys_zc, "  DFV: %s", DFV_MOD_VER);
    zlog_notice(sys_zc, "  IDT: %s", IDT_MOD_VER);
    zlog_notice(sys_zc, "  CMI: %s", CMI_MOD_VER);

    // initialize env
    // TBD: should be read from .conf
    zlog_notice(sys_zc, "> loading conf ...");
    memset(&sys_env, 0, sizeof(sys_env));
    
    sys_env.features = SYSFEA_USE_LOCALSTATS;

    sys_env.intf_type = cmi_intf_tcp;
    sys_env.endian = cmi_endian_auto;
    sys_env.ipaddr = NULL;
    sys_env.port = 1235;

    sys_env.dfv_desc_tbl[0].mnt_path = "sdb";
    sys_env.dfv_desc_tbl[0].dev_path = "sdb";
    sys_env.dfv_desc_tbl[0].flag = 0;
    
    sys_env.ips_linkdesc_tbl[0].mst_id = IPS_MAKE_EPID(IPS_EPMODE_MASTER, 0, 0);
    sys_env.ips_linkdesc_tbl[0].slv_id = IPS_MAKE_EPID(IPS_EPMODE_SLAVE, 0, 0);
    sys_env.ips_linkdesc_tbl[0].mst_port = 1;
    sys_env.ips_linkdesc_tbl[0].slv_port = 6;
    sys_env.ips_linkdesc_tbl[0].is_master = 1;

    if (SYS_MAX_PIPES == 2) {
        sys_env.dfv_desc_tbl[1].mnt_path = "sdf";
        sys_env.dfv_desc_tbl[1].dev_path = "sdf";
        sys_env.dfv_desc_tbl[1].flag = 0;
        sys_env.ips_linkdesc_tbl[1].mst_id = IPS_MAKE_EPID(IPS_EPMODE_MASTER, 0, 1);
        sys_env.ips_linkdesc_tbl[1].slv_id = IPS_MAKE_EPID(IPS_EPMODE_SLAVE, 0, 1);
        sys_env.ips_linkdesc_tbl[1].mst_port = 4;
        sys_env.ips_linkdesc_tbl[1].slv_port = 3;
        sys_env.ips_linkdesc_tbl[1].is_master = 1;
        assert(sys_env.ips_linkdesc_tbl[1].is_master); // I am master
        sys_env.ips_desc_tbl[1].capacity = 0;
        sys_env.ips_desc_tbl[1].pc_num = 1;
        for (i = 0; i < 1; i++) {
            ips_pcdesc_t* pcdesc = &sys_env.ips_desc_tbl[1].pcdesc_tbl[i];
            pcdesc->src_id = sys_env.ips_linkdesc_tbl[1].mst_id;
            pcdesc->dest_id = sys_env.ips_linkdesc_tbl[1].slv_id;
            pcdesc->sector_sz = IPS_CLS_SECTOR_SIZE;
            pcdesc->sector_num = IPS_CLS_SECTOR_NUM;
        }
    }

    assert(sys_env.ips_linkdesc_tbl[0].is_master); // I am master

    sys_env.ips_desc_tbl[0].capacity = 0;
    sys_env.ips_desc_tbl[0].pc_num = 1;
    for (i = 0; i < 1; i++) {
        ips_pcdesc_t* pcdesc = &sys_env.ips_desc_tbl[0].pcdesc_tbl[i];
        pcdesc->src_id = sys_env.ips_linkdesc_tbl[0].mst_id;
        pcdesc->dest_id = sys_env.ips_linkdesc_tbl[0].slv_id;
        pcdesc->sector_sz = IPS_CLS_SECTOR_SIZE;
        pcdesc->sector_num = IPS_CLS_SECTOR_NUM;
    }

    // initialize each module
    zlog_notice(sys_zc, "==> initializing modules ...");
#ifdef ARCH_ppc64
    ret = ips_module_init(NULL);
    assert(!ret);
    ret = idt_module_init(NULL);
    assert(!ret);
#endif
    ret = dfv_module_init(NULL);
    assert(!ret);
    ret = cmi_module_init(NULL);
    assert(!ret);

    //init ips mode
    syslk_parse_ips_mode("./syslk_ips.conf");
    ips_init_mode(syslk_ips_mode);
    zlog_notice(sys_zc,"dma_use_nlwr:%d dma_use_chain:%d tx_wmd:%d tx_rmd:%d rx_wmd:%d rx_rmd:%d",
                syslk_ips_mode.dma_use_nlwr, syslk_ips_mode.dma_use_chain, syslk_ips_mode.tx.use_wptr,
                syslk_ips_mode.tx.use_rptr, syslk_ips_mode.rx.use_wptr, syslk_ips_mode.rx.use_rptr);    
    // initialize ctxs
    memset(&sys_ctx, 0, sizeof(sys_ctx));

    zlog_notice(sys_zc, "==> initializing storage ...");
    const char* repo_mnt_tbl[DFV_MAX_REPOS] = {NULL};
    const char* repo_dev_tbl[DFV_MAX_REPOS] = {NULL};
    for (i=0; i<SYS_MAX_PIPES; i++) {
        repo_mnt_tbl[i] = sys_env.dfv_desc_tbl[i].mnt_path;
        repo_dev_tbl[i] = sys_env.dfv_desc_tbl[i].dev_path;
    }
    sys_ctx.vault = dfv_vault_open(SYS_MAX_PIPES, DFV_SLICE_NUM, repo_mnt_tbl, repo_dev_tbl, 0);
    if (!sys_ctx.vault) {
        zlog_fatal(sys_zc, "failed to initializing storage, quit");
        exit(-1);
    }
    sys_ctx.diskcap = dfv_vault_get_diskcap(sys_ctx.vault);
    // check freeslot
    int slot_id = dfv_vault_get_freeslot(sys_ctx.vault);
    if (slot_id < 0) {
        zlog_fatal(sys_zc, "no spece left on vault, quit");
        exit(-1);
    }

#ifdef ARCH_ppc64
    zlog_notice(sys_zc, "==> initializing switch route table ...");
    int idt_fd = idt_dev_open(2, 0x67);
    assert(idt_fd > 0);
    for (i=0; i<SYS_MAX_PIPES; i++) {
        ips_linkdesc_t* linkdesc = &sys_env.ips_linkdesc_tbl[i];
        // reset link parnter
        idt_port_recovery(idt_fd, linkdesc->slv_port);
        // reset routetbl entries
        idt_routetbl_set(idt_fd, linkdesc->mst_port,
                         linkdesc->slv_id,
                         linkdesc->slv_port);
        idt_routetbl_set(idt_fd, linkdesc->slv_port,
                         linkdesc->mst_id,
                         linkdesc->mst_port);
    }
    idt_dev_close(idt_fd);

    zlog_notice(sys_zc, "==> initializing srio ...");
    for (i=0; i<SYS_MAX_PIPES; i++) {
        // init repo
        zlog_notice(sys_zc, "  initializing ips_srio: pipe=%d, id=0x%x",
                    i,
                    sys_env.ips_linkdesc_tbl[i].mst_id);
        ret = ips_ep_init(sys_env.ips_linkdesc_tbl[i].mst_id, &sys_env.ips_desc_tbl[i]);
        assert(!ret);
    }
#endif
    sys_ctx.file_cache = malloc(sizeof(sys_cache_t));
    assert(sys_ctx.file_cache);
    memset(sys_ctx.file_cache, 0, sizeof(sys_cache_t));
    sys_ctx.file_cache->slot_id = -1;
    sys_ctx.file_cache->data = malloc(SYS_CACHE_SIZE);
    assert(sys_ctx.file_cache->data);

    zlog_notice(sys_zc, "==> initializing job workers ...");
    for (i=0; i<SYS_MAX_PIPES; i++) {
        sys_wkr_ctx_t* wkr_ctx = malloc(sizeof(sys_wkr_ctx_t));
        assert(wkr_ctx);
        memset(wkr_ctx, 0, sizeof(sys_wkr_ctx_t));

        wkr_ctx->wkr_state = sys_state_idle;
        wkr_ctx->wkr_id = i;
        pthread_mutex_init(&wkr_ctx->buf_snap_lock, NULL);
        sys_jobq_init(&wkr_ctx->job_in);
        sys_jobq_init(&wkr_ctx->job_out);
        wkr_ctx->wkr_thread = malloc(sizeof(pthread_t));
        assert(wkr_ctx->wkr_thread);
        sys_ctx.wkr_ctx_tbl[i] = wkr_ctx;

        pthread_create(wkr_ctx->wkr_thread, NULL, __sys_wkr_job, (void*)wkr_ctx);
    }

    sys_ctx.auto_rec = 0;
    if (sys_ctx.auto_rec) {
        pthread_t thread_autorec;
        pthread_create(&thread_autorec, NULL, __sys_wkr_autorec, NULL);
    }

RECONN:
    zlog_notice(sys_zc, "==> ---------- SERVER START ----------");
    // stop all workers
    for (i=0; i<SYS_MAX_PIPES; i++) {
        sys_wkr_ctx_t* wkr_ctx = sys_ctx.wkr_ctx_tbl[i];
        if (wkr_ctx->wkr_state != sys_state_idle) {
            zlog_notice(sys_zc, "==> stopping job worker#%d ...", i);
            wkr_ctx->reset_req = 1;
            while(wkr_ctx->wkr_state != sys_state_idle) {
                usleep(100);
            }
        }
    }
    
    sys_cmd_exec_stopul(NULL);
    
    if (sys_ctx.cmi_intf) {
        zlog_notice(sys_zc, "==> closing client cmi ...");
        cmi_intf_close(sys_ctx.cmi_intf);
        sys_ctx.cmi_intf = NULL;
    }

    if (sys_ctx.sysdown_req) {
        goto out;
    }

    // open cmi
    zlog_notice(sys_zc, "==> opening client cmi ...");
    sys_ctx.cmi_intf = cmi_intf_open(cmi_type_server,
                                     sys_env.intf_type,
                                     sys_env.endian);
    if (!sys_ctx.cmi_intf) {
        assert(0);
        exit(-1);
    }    
    
    zlog_notice(sys_zc, "==> connecting client cmi ...");
    ret = cmi_intf_connect(sys_ctx.cmi_intf,
                           sys_env.ipaddr,
                           sys_env.port);
    if (ret != SPK_SUCCESS) {
        assert(0);
        exit(-1);
    }

    while(1) {
        // main loop
        // update sys_state
        if (!sys_ctx.auto_rec) {
            // some jobs are done by job workers
            // we do not known when they finished
            // so we inquiry workers state in some states
            switch(sys_ctx.sys_state) {
            case sys_state_rec:
            case sys_state_play:
            case sys_state_dl:
            case sys_state_format:
                {
                    int all_idle = 1;
                    for (int i=0; i<SYS_MAX_PIPES; i++) {
                        sys_wkr_ctx_t* wkr_ctx = sys_ctx.wkr_ctx_tbl[i];
                        if (wkr_ctx && wkr_ctx->wkr_state != sys_state_idle) {
                            all_idle = 0;
                        }
                    }
                    if (all_idle) {
                        sys_change_state(sys_state_idle);
                    }
                }
                break;
            }
        }

        // get message from cmi
        ssize_t msg_size = cmi_intf_read_msg(sys_ctx.cmi_intf,
                                             msg_ibuf,
                                             CMI_MAX_MSGSIZE);
        if (msg_size < 0) {
            zlog_warn(sys_zc, "failed to read from socket: ret=%ld", msg_size);
            goto RECONN;
        }
        if (msg_size == 0) {
            usleep(100);
            continue;
        }

        // msg arrived
//        cmi_msg_dump(ZLOG_LEVEL_NOTICE, msg_ibuf, msg_size);
//        zlog_notice(sys_zc, "> read msg: code=0x%x, size=%ld", MSG_CODE(msg_ibuf), msg_size);

        // notify auto_rec_thread() to quit
        sys_ctx.auto_rec = 0;
        
        ret = SPK_SUCCESS;
        // parse msg
        switch(MSG_CODE(msg_ibuf)) {
        case msg_code_cmd:
            ret = sys_cmd_exec((cmi_cmd_t*)msg_ibuf,
                               msg_size);
            break;
        case msg_code_data:
            ret = sys_msg_parse_data((cmi_data_t*)msg_ibuf, msg_size);
            break;
        case msg_code_cmdresp:
        case msg_code_status:
        default:
            // impossible
            assert(0);
            break;
        }
        if (ret == SPKERR_RESETSYS) {
            zlog_warn(sys_zc, "socket error, restart");
            goto RECONN;
        }
        
        if (sys_ctx.sysdown_req) {
            zlog_notice(sys_zc, "################# SYSTEM SHUTDOWN");
            spk_os_exec("poweroff");
            goto RECONN;
        }
        // TODO: other ret code
    }

out:
    for (i=0; i<SYS_MAX_PIPES; i++) {
        sys_wkr_ctx_t* wkr_ctx = sys_ctx.wkr_ctx_tbl[i];
        if (wkr_ctx) {
            wkr_ctx->quit_req = 1;
            pthread_join(*wkr_ctx->wkr_thread, NULL);
            SAFE_RELEASE(wkr_ctx->wkr_thread);
            SAFE_RELEASE(wkr_ctx);
        }
    }

    return 0;
}
