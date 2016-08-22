/*************************************************************************
    > File Name: main.c
    > Author:
    > Mail:
    > Created Time: Wed 12 Aug 2015 05:16:49 PM CST
 ************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <malloc.h>
#include <unistd.h>
#include <string.h>
#include <inttypes.h>
#include <pthread.h>
#include <assert.h>
#include <sys/time.h>
#include "syslk.h"

static void sys_job_kickoff(int wkr_id, int type, cmi_cmd_t* cmd, uint64_t arg)
{
    sys_wkr_ctx_t* wkr_ctx = sys_ctx.wkr_ctx_tbl[wkr_id];
    sys_jobq_node_t* job_node = NULL;

    job_node = malloc(sizeof(sys_jobq_node_t));
    assert(job_node);
    memset(job_node, 0, sizeof(sys_jobq_node_t));

    job_node->cmd_ref = cmd;
    job_node->job_type = type;
    job_node->arg = arg;
    job_node->resp = -1;

    sys_jobq_enqueue(&wkr_ctx->job_in, job_node);

    return;
}

static int sys_job_wait_resp(int wkr_id, int type)
{
    sys_wkr_ctx_t* wkr_ctx = sys_ctx.wkr_ctx_tbl[wkr_id];
    sys_jobq_node_t* job_node = NULL;
    int ret = SPKERR_TIMEOUT;

    while(1) {
        job_node = sys_jobq_dequeue(&wkr_ctx->job_out);
        if (job_node) {
            if (job_node->job_type == type) {
                break;
            }
            zlog_warn(sys_zc, "got a bad job resp: type=%s",
                      cmi_desc_cmdtype2str(job_node->job_type));
            SAFE_RELEASE(job_node);
        }
        // TODO: timeout
        usleep(100);
    }

    if (job_node) {
        ret = job_node->resp;
        SAFE_RELEASE(job_node);
    }
    return(ret);
}

static int sys_cmd_send_status(cmi_cmd_t* cmd)
{
    cmi_status_t status;
    cmi_msg_hdr_t* hdr = &status.hdr;
    int ret, i;

    // build status msg
    memset(&status, 0, sizeof(status));
    cmi_msg_build_hdr(hdr, msg_code_status, sizeof(cmi_status_t));

    // update sys_state
    status.sys_state = sys_ctx.sys_state;
    status.svr_time = sys_systm_to_lktm(time(NULL));

    if (!(sys_env.features & SYSFEA_USE_LOCALSTATS)) {
        // update fcstat from srio#0
        ips_fcstat_lk1* fcstat = &sys_ctx.fcstat[0];
        for (i=0; i<CMI_MAX_FCNUM; i++) {
            status.fb_link[i] = fcstat->fc_link_state & (0x01<<i);
            if (fcstat->work_mode == 0x01) {
                // record
                status.fb_speed[i] = fcstat->rec_spd_16b[i] << 1;
            } else if (fcstat->work_mode == 0x02) {
                status.fb_speed[i] = fcstat->play_spd_16b[i] << 1;
            }
            status.fb_count[i] = fcstat->recv_sz_16b[i] << 1;
        }
    } else {
        for (i=0; i<SYS_MAX_PIPES; i++) {
            sys_wkr_ctx_t* wkr_ctx = sys_ctx.wkr_ctx_tbl[i];
            if (wkr_ctx) {
                status.fb_count[i] = spk_stats_get_xfer_bytes(&wkr_ctx->stats) / 1024;
                status.fb_speed[i] = spk_stats_get_bps_wire(&wkr_ctx->stats) / 1024;
            }
        }
    }

    // update storage info
    status.disk_size = sys_ctx.diskcap;
    status.disk_free = dfv_vault_get_diskfree(sys_ctx.vault);

    ssize_t written = cmi_intf_write_msg(sys_ctx.cmi_intf,
                                         &status,
                                         sizeof(cmi_status_t));
    if (written != sizeof(cmi_status_t)) {
        ret = SPKERR_RESETSYS;
        goto out;
    }

    zlog_info(sys_zc, "> status sent: written=%ld", written);
    ret = SPK_SUCCESS;
out:
    return(ret);
}

int sys_cmd_send_filelist(cmi_cmd_t* cmd)
{
    cmi_data_filelist_t dfl;
    int ret;

    // generate filstlist
    memset(&dfl, 0, sizeof(cmi_data_filelist_t));
    // we use repo#0's meta as ref
    struct dfv_repo* repo = sys_ctx.vault->repo_tbl[0];
    assert(repo);
    int slot_num = 0;
    for(int i=0; i<CMI_MAX_SLOTS; i++) {
        dfv_fmeta_t* fmeta = dfv_repo_get_fmeta(repo, i);
        dfl.work_list[i].tag = 0x18efdc0a;
        dfl.work_list[i].slot_id = i;
        if (fmeta) {
            ssize_t slot_sz = dfv_vault_get_slotsize(sys_ctx.vault, i);
            if (slot_sz < 0) {
                // skip this file
                continue;
            }
            slot_sz /= 1024;
            dfl.work_list[i].file_sz_h = HGH32(slot_sz);
            dfl.work_list[i].file_sz_l = LOW32(slot_sz);
            dfl.work_list[i].begin_tm = sys_systm_to_lktm(fmeta->file_time);
            slot_num++;
        }
    }
    dfl.sys_info.tag = 0x5a5a;
    dfl.sys_info.slot_num = slot_num;

    cmi_msg_reform_flist(&dfl, cmi_intf_get_endian(sys_ctx.cmi_intf));

    uint32_t req_frag;
    if (cmd)
        req_frag = cmd->u.file.frag_id;
    else
        req_frag = (uint32_t)-1;

    ret = cmi_intf_write_flist(sys_ctx.cmi_intf, &dfl, req_frag);
    return(ret);
}

static int sys_cmd_send_snapshot(cmi_cmd_t* cmd)
{
    size_t buf_snap_sz_all = SYS_MAX_PIPES*SYS_SNAP_BUF_SZ;
    char* buf_snap = NULL;
    int ret = -1;

    buf_snap = malloc(buf_snap_sz_all);
    assert(buf_snap);

    for (int i=0; i<SYS_MAX_PIPES; i++) {
        sys_wkr_ctx_t* wkr_ctx = sys_ctx.wkr_ctx_tbl[i];
        if (wkr_ctx) {
            pthread_mutex_lock(&wkr_ctx->buf_snap_lock);
            memcpy(buf_snap + i*SYS_SNAP_BUF_SZ, wkr_ctx->buf_snap, SYS_SNAP_BUF_SZ);
            pthread_mutex_unlock(&wkr_ctx->buf_snap_lock);
        }
    }
    ret = cmi_intf_write_snapshot(sys_ctx.cmi_intf, buf_snap, buf_snap_sz_all);
    SAFE_RELEASE(buf_snap);

    return(ret);
}

static int sys_cmd_exec_stop(cmi_cmd_t* cmd, int pipe_num, int check_state)
{
    sys_ctx_t* ctx = &sys_ctx;
    int i;

    if (ctx->sys_state != check_state) {
        return(SPKERR_BADSEQ);
    }

    // drive worker
    for (i=0; i<pipe_num; i++) {
        sys_wkr_ctx_t* wkr_ctx = ctx->wkr_ctx_tbl[i];
        if (wkr_ctx) {
            wkr_ctx->reset_req = 1;
        }
    }

    for (i=0; i<pipe_num; i++) {
        sys_wkr_ctx_t* wkr_ctx = ctx->wkr_ctx_tbl[i];
        if (wkr_ctx) {
            while(wkr_ctx->wkr_state != sys_state_idle) {
                usleep(100);
            }
        }
    }

    // change system state
    sys_change_state(sys_state_idle);

    return(SPK_SUCCESS);
}

int sys_cmd_exec_startrec(cmi_cmd_t* cmd)
{
    sys_ctx_t* ctx = &sys_ctx;
    int i;
    int ret = -1;

    if (ctx->sys_state != sys_state_idle) {
        return(SPKERR_EAGAIN);
    }

    int slot_id = dfv_vault_get_freeslot(ctx->vault);
    if (slot_id < 0) {
        ret = SPKERR_BADRES;
        goto out;
    }

    for (i=0; i<SYS_MAX_PIPES; i++) {
        sys_job_kickoff(i, cmd_type_start_rec, cmd, slot_id);
    }

    ret = SPK_SUCCESS;
    for (i=0; i<SYS_MAX_PIPES; i++) {
        int ret2 = sys_job_wait_resp(i, cmd_type_start_rec);
        if (ret2 != SPK_SUCCESS) {
            ret = ret2;
        }
    }

    if (ret == SPK_SUCCESS) {
        sys_change_state(sys_state_rec);
    }

out:
    return(ret);
}

static int sys_cmd_exec_startplay(cmi_cmd_t* cmd)
{
    sys_ctx_t* ctx = &sys_ctx;
    int i;
    int ret = -1;

    if (ctx->sys_state != sys_state_idle) {
        return(SPKERR_EAGAIN);
    }

    int slot_id = cmd->u.file.index;
    uint64_t slot_sz = dfv_vault_get_slotsize(ctx->vault, slot_id);
    if (slot_sz <= 0) {
        ret = SPKERR_BADRES;
        goto out;
    }
    slot_sz /= ctx->vault->repo_num;

    // drive worker
    for (i=0; i<SYS_MAX_PIPES; i++) {
        sys_job_kickoff(i, cmd_type_start_play, cmd, slot_sz);
    }

    ret = SPK_SUCCESS;
    for (i=0; i<SYS_MAX_PIPES; i++) {
        int ret2 = sys_job_wait_resp(i, cmd_type_start_play);
        if (ret2 != SPK_SUCCESS) {
            ret = ret2;
        }
    }
    if (ret == SPK_SUCCESS) {
        sys_change_state(sys_state_play);
    }

out:
    return(ret);
}

static int sys_cmd_exec_startdl(cmi_cmd_t* cmd)
{
    sys_ctx_t* ctx = &sys_ctx;
    int ret = -1;

    if (ctx->sys_state != sys_state_idle) {
        return(SPKERR_EAGAIN);
    }

    // use worker#0
    sys_job_kickoff(0, cmd_type_start_dl, cmd, 0);
    ret = sys_job_wait_resp(0, cmd_type_start_dl);

    if ((cmd->u.file.frag_id == (uint32_t)-1) && ret == SPK_SUCCESS) {
        sys_change_state(sys_state_dl);
    }

    return(ret);
}

int sys_cmd_exec_stopul(cmi_cmd_t* cmd)
{
    int i;

    for (i=0; i<SYS_MAX_PIPES; i++) {
        sys_ul_ctx_t* ul_ctx = sys_ctx.ul_ctx_tbl[i];
        if (ul_ctx) {
            if (ul_ctx->dfvcm) {
                dfvcm_close(ul_ctx->dfvcm, 1/* free buf for me*/);
            }
            if (ul_ctx->work_node) {
                SAFE_RELEASE(ul_ctx->work_node->buf_ptr)
                SAFE_RELEASE(ul_ctx->work_node);
            }
            SAFE_RELEASE(ul_ctx);
            sys_ctx.ul_ctx_tbl[i] = NULL;
        }
    }

    sys_change_state(sys_state_idle);

    return(SPK_SUCCESS);
}

static int sys_cmd_exec_startul(cmi_cmd_t* cmd)
{
    sys_ctx_t* ctx = &sys_ctx;
    int i;
    int ret = -1;

    if (ctx->sys_state != sys_state_idle) {
        return(SPKERR_EAGAIN);
    }

    for (i=0; i<SYS_MAX_PIPES; i++) {
        if (ctx->ul_ctx_tbl[i]) {
            assert(0);
        }
    }

    int slot_id = dfv_vault_get_freeslot(ctx->vault);
    if (slot_id < 0) {
        ret = SPKERR_BADRES;
        goto errout;
    }

    for (i=0; i<SYS_MAX_PIPES; i++) {
        sys_ul_ctx_t* ul_ctx = malloc(sizeof(sys_ul_ctx_t));
        assert(ul_ctx);
        memset(ul_ctx, 0, sizeof(sys_ul_ctx_t));

        struct dfvcm_ctx* dfvcm = NULL;
        dfv_slot_def_t slot_def;
        slot_def.repo = dfv_vault_get_repo(ctx->vault, i);
        slot_def.slot_id = slot_id;

        dfv_slice_def_t slice_def;
        slice_def.num = DFV_SLICE_NUM;
        slice_def.size = DFV_SLICE_SIZE;

        dfvcm = dfvcm_open(i, &slot_def, SPK_DIR_WRITE, &slice_def, 16+i*DFV_SLICE_NUM);
        if (!dfvcm) {
            ret = SPKERR_BADRES;
            goto errout;
        }

        void* buf_ptr = memalign(SYS_INTERLACE_SIZE, DFV_CHUNK_SIZE);
        if (!buf_ptr) {
            zlog_fatal(sys_zc, "not enough memroy: size_req=0x%x", DFV_CHUNK_SIZE);
            assert(0);
        }

        dfv_bufq_node_t* node = malloc(sizeof(dfv_bufq_node_t));
        assert(node);
        memset(node, 0, sizeof(dfv_bufq_node_t));
        node->buf_ptr = buf_ptr;
        node->buf_sz = DFV_CHUNK_SIZE;

        dfv_bufq_t* freeq = dfvcm_get_freeq(dfvcm);
        dfv_bufq_enqueue(freeq, node);

        ul_ctx->dfvcm = dfvcm;
        ul_ctx->work_node = NULL;
        ctx->ul_ctx_tbl[i] = ul_ctx;
    }

    sys_change_state(sys_state_ul);
    return(SPK_SUCCESS);

errout:
    sys_cmd_exec_stopul(NULL);

    return(ret);
}

static int sys_cmd_exec_snapshot(cmi_cmd_t* cmd)
{
    sys_ctx_t* ctx = &sys_ctx;

    if (ctx->sys_state != sys_state_rec) {
        return(SPKERR_EAGAIN);
    }

    return(SPK_SUCCESS);
}

static int sys_cmd_exec_synctime(cmi_cmd_t* cmd)
{
    uint64_t synctime = sys_lktm_to_systm(cmd->u.tm.lktime);
    struct tm* local_time = localtime((time_t*)&synctime);
    char tmstr[256];
    strftime(tmstr, sizeof(tmstr), "%Y/%m/%d-%H:%M:%S", local_time);
    zlog_notice(sys_zc, "synctime: %s", tmstr);

    sprintf(tmstr, "date %02d%02d%02d%02d%04d.%02d",
            local_time->tm_mon + 1,
            local_time->tm_mday,
            local_time->tm_hour,
            local_time->tm_min,
            local_time->tm_year + 1900,
            local_time->tm_sec);
    zlog_notice(sys_zc, tmstr);
    spk_os_exec(tmstr);

    return(SPK_SUCCESS);
}

static int sys_cmd_exec_delete(cmi_cmd_t* cmd)
{
    int slot_id = cmd->u.file.index;
    dfv_vault_del_slot(sys_ctx.vault, slot_id);

    return(SPK_SUCCESS);
}

static int sys_cmd_exec_format(cmi_cmd_t* cmd)
{
    sys_ctx_t* ctx = &sys_ctx;
    int ret = -1;

    if (ctx->sys_state != sys_state_idle) {
        return(SPKERR_EAGAIN);
    }

    sys_job_kickoff(0, cmd_type_format, cmd, 0);
    ret = sys_job_wait_resp(0, cmd_type_format);

    if (ret == SPK_SUCCESS) {
        sys_change_state(sys_state_format);
    }

    return(ret);
}

static int sys_cmd_exec_config(cmi_cmd_t* cmd)
{
    sys_ctx_t* ctx = &sys_ctx;
    int ret = -1;
    int i;

    if (ctx->sys_state != sys_state_idle) {
        return(SPKERR_EAGAIN);
    }

    // drive worker
    for (i=0; i<SYS_MAX_PIPES; i++) {
        sys_job_kickoff(i, cmd_type_config, cmd, 0);
    }

    ret = SPK_SUCCESS;
    for (i=0; i<SYS_MAX_PIPES; i++) {
        int ret2 = sys_job_wait_resp(i, cmd_type_config);
        if (ret2 != SPK_SUCCESS) {
            ret = ret2;
        }
    }

    return(ret);
}

static int sys_cmd_exec_sysdown(cmi_cmd_t* cmd)
{
    if (sys_ctx.sys_state != sys_state_idle) {
        return(SPKERR_EAGAIN);
    }

    return(SPK_SUCCESS);
}

static int sys_cmd_exec_upgrade(cmi_cmd_t* cmd)
{
    sys_ctx_t* ctx = &sys_ctx;
    if (ctx->sys_state != sys_state_idle) {
        return(SPKERR_EAGAIN);
    }
    
    if (ctx->upgrd_ctx) {
        if (ctx->upgrd_ctx->script_buf) {
            SAFE_RELEASE(ctx->upgrd_ctx->script_buf);
        }
        SAFE_RELEASE(ctx->upgrd_ctx);
    }
    ctx->upgrd_ctx = malloc(sizeof(sys_upgrd_ctx_t));
    assert(ctx->upgrd_ctx);
    ctx->upgrd_ctx->script_buf = malloc(SYS_MAX_UPGRD_SCRIPT_SZ);
    assert(ctx->upgrd_ctx->script_buf);
    ctx->upgrd_ctx->buf_sz = 0;
    
    sys_change_state(sys_state_upgrade);

    return(SPK_SUCCESS);
}

int sys_cmd_exec(cmi_cmd_t* cmd, size_t size)
{
    int cmdtype = cmd->cmd_type;
    int ret = -1;

    zlog_info(sys_zc, "> executing cmd: type=%s", cmi_desc_cmdtype2str(cmdtype));

    // pre-exec
    switch(cmdtype) {
    case cmd_type_start_rec:
        ret = sys_cmd_exec_startrec(cmd);
        break;
    case cmd_type_stop_rec:
        ret = sys_cmd_exec_stop(cmd, SYS_MAX_PIPES, sys_state_rec);
        break;
    case cmd_type_start_play:
        ret = sys_cmd_exec_startplay(cmd);
        break;
    case cmd_type_stop_play:
        ret = sys_cmd_exec_stop(cmd, SYS_MAX_PIPES, sys_state_play);
        break;
    case cmd_type_snapshot:
        ret = sys_cmd_exec_snapshot(cmd);
        break;
    case cmd_type_stop_dl:
        ret = sys_cmd_exec_stop(cmd, 1, sys_state_dl);
        break;
    case cmd_type_start_ul:
        ret = sys_cmd_exec_startul(cmd);
        break;
    case cmd_type_stop_ul:
        ret = sys_cmd_exec_stopul(cmd);
        break;
    case cmd_type_sync_time:
        ret = sys_cmd_exec_synctime(cmd);
        break;
    case cmd_type_delete:
        ret = sys_cmd_exec_delete(cmd);
        break;
    case cmd_type_config:
        ret = sys_cmd_exec_config(cmd);
        break;
    case cmd_type_sysdown:
        ret = sys_cmd_exec_sysdown(cmd);
        break;
    case cmd_type_upgrade:
        ret = sys_cmd_exec_upgrade(cmd);
        break;
    case cmd_type_init:
    case cmd_type_filelist:
    case cmd_type_inquiry:
    case cmd_type_format:
    case cmd_type_start_dl:
        // nothing to do
        ret = SPK_SUCCESS;
        break;
    default:
        assert(0);
        break;
    }

    // send resp
    cmi_cmdresp_t cmdresp;
    memset(&cmdresp, 0, sizeof(cmi_cmdresp_t));
    cmi_msg_build_hdr((cmi_msg_hdr_t*)&cmdresp,
                      msg_code_cmdresp,
                      sizeof(cmi_cmdresp_t));
    cmdresp.cmd_type = cmd->cmd_type;
    if (ret == SPK_SUCCESS) {
        cmdresp.success = CMI_CMDEXEC_SUCC;
        if (cmd->cmd_type == cmd_type_init) {
            cmdresp.u.version.sys_ver = SYS_VERSION_INT;
        }
    } else {
        zlog_error(sys_zc, "failed to exec cmd: type=%s, ret=%d",
                   cmi_desc_cmdtype2str(cmd->cmd_type), ret);
        cmdresp.success = CMI_CMDEXEC_FAIL;
    }
    ssize_t written = cmi_intf_write_msg(sys_ctx.cmi_intf,
                                         &cmdresp,
                                         sizeof(cmi_cmdresp_t));
    zlog_info(sys_zc, "> cmdresp sent: written=%ld", written);
    if (written != sizeof(cmi_cmdresp_t)) {
        ret = SPKERR_RESETSYS;
        goto out;
    }

    // post-exec
    if (ret == SPK_SUCCESS) {
        switch(cmdtype) {
        case cmd_type_inquiry:
            ret = sys_cmd_send_status(cmd);
            break;
        case cmd_type_filelist:
            ret = sys_cmd_send_filelist(cmd);
            break;
        case cmd_type_start_dl:
            ret = sys_cmd_exec_startdl(cmd);
            break;
        case cmd_type_snapshot:
            ret = sys_cmd_send_snapshot(cmd);
            break;
        case cmd_type_format:
            ret = sys_cmd_exec_format(cmd);
            break;
        case cmd_type_sysdown:
            sys_ctx.sysdown_req = 1;
            break;
        case cmd_type_init:
        case cmd_type_delete:
        case cmd_type_sync_time:
        case cmd_type_start_rec:
        case cmd_type_stop_rec:
        case cmd_type_start_play:
        case cmd_type_stop_play:
        case cmd_type_stop_dl:
        case cmd_type_start_ul:
        case cmd_type_stop_ul:
        case cmd_type_config:
        case cmd_type_upgrade:
            // nothing to do
            break;
        default:
            assert(0);
            break;
        }
    }

out:
    zlog_info(sys_zc, "> cmd executed: type=%s, frag_id=%u, ret=%d",
              cmi_desc_cmdtype2str(cmdtype),
              cmd->u.all.words[0], ret);
    return(ret);
}

void* __sys_wkr_autorec(void* args)
{
    zlog_notice(sys_zc, "autorec> spawn ...");

    while(sys_ctx.auto_rec) {
        if (sys_ctx.sys_state == sys_state_idle) {
            sys_cmd_exec_startrec(NULL);
        }
        usleep(100);
        int wkr_state = sys_state_idle;
        for (int i=0; i<SYS_MAX_PIPES; i++) {
            sys_wkr_ctx_t* wkr_ctx = sys_ctx.wkr_ctx_tbl[i];
            if (wkr_ctx && wkr_ctx->wkr_state != sys_state_idle) {
                wkr_state = wkr_ctx->wkr_state;
            }
        }
        sys_change_state(wkr_state);
    }
    zlog_notice(sys_zc, "autorec> terminated");
    return(NULL);
}

