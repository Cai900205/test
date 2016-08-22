#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stddef.h>
#include <inttypes.h>
#include <assert.h>
#include <errno.h>
#include <pthread.h>
#include "cmi.h"

static void __cmi_msg_dump_hdr(zlog_level ll, cmi_msg_hdr_t* hdr)
{
    CMI_LOG(ll, "  sync_tag   : 0x%x", hdr->sync_tag);
    CMI_LOG(ll, "  msg_len    : 0x%x", hdr->msg_len);
    CMI_LOG(ll, "  msg_code   : 0x%x", hdr->msg_code);
//    CMI_LOG(ll, "  msg_srcid  : 0x%x", hdr->msg_srcid);
//    CMI_LOG(ll, "  msg_destid : 0x%x", hdr->msg_destid);
//    CMI_LOG(ll, "  pcid       : 0x%x", hdr->pcid);
//    CMI_LOG(ll, "  msg_ts     : 0x%x", hdr->msg_ts);
//    CMI_LOG(ll, "  csum       : 0x%x", hdr->csum);

    return;
}

static void cmi_msg_dump_cmd(zlog_level ll, cmi_cmd_t* cmd, size_t size)
{
    CMI_LOG(ll, "dump MSG_CMD %zu@%p", size, cmd);
    __cmi_msg_dump_hdr(ll, &cmd->hdr);
    CMI_LOG(ll, "  cmd_type   : %s",   cmi_desc_cmdtype2str(cmd->cmd_type));
    for (int i=0; i<CMI_CMD_VAR_LEN; i++) {
        CMI_LOG(ll, "  word[%d]    : 0x%x", i, cmd->u.all.words[i]);
    }

    return;
}

static void cmi_msg_dump_cmdresp(zlog_level ll, cmi_cmdresp_t* cmdresp, size_t size)
{
    CMI_LOG(ll, "dump MSG_CMDRESP %zu@%p", size, cmdresp);
    __cmi_msg_dump_hdr(ll, &cmdresp->hdr);
    CMI_LOG(ll, " cmd_type   : %s",   cmi_desc_cmdtype2str(cmdresp->cmd_type));
    CMI_LOG(ll, " success    : 0x%x", cmdresp->success);

    return;
}

static void cmi_msg_dump_status(zlog_level ll, cmi_status_t* status, size_t size)
{
    CMI_LOG(ll, "dump MSG_STATUS %zu@%p", size, status);
    __cmi_msg_dump_hdr(ll, &status->hdr);
    CMI_LOG(ll, "  sys_state  : %s",  cmi_desc_sysstate2str(status->sys_state));
    for (int i=0; i<CMI_MAX_FCNUM; i++) {
        CMI_LOG(ll, "  fb_link#%d  : 0x%x", i, status->fb_link[i]);
        CMI_LOG(ll, "  fb_speed#%d : 0x%x", i, status->fb_speed[i]);
        CMI_LOG(ll, "  fb_count#%d : 0x%lx", i, status->fb_count[i]);
    }
    CMI_LOG(ll, "  disk_size  : 0x%lx", status->disk_size);
    CMI_LOG(ll, "  disk_free  : 0x%lx", status->disk_free);
    CMI_LOG(ll, "  disk_rspd  : 0x%x", status->disk_rspd);
    CMI_LOG(ll, "  disk_wspd  : 0x%x", status->disk_wspd);

    return;
}

static void cmi_msg_dump_data(zlog_level ll, cmi_data_t* data, size_t size)
{
    CMI_LOG(ll, "dump MSG_DATA %zu@%p", size, data);
    __cmi_msg_dump_hdr(ll, &data->hdr);

    CMI_LOG(ll, "  data_type  : 0x%x", data->data_type);
    CMI_LOG(ll, "  eof        : 0x%x", data->eof);
    CMI_LOG(ll, "  frag_len   : 0x%x", data->frag_len);

    char outstr[1024] = {0};
    for (int i=0; i<4; i++) {
        char temp[256];
        sprintf(temp, "0x%02x ", data->frag_data[i]);
        strcat(outstr, temp);
    }
    CMI_LOG(ll, "  frag_data  : %s", outstr);

    return;
}

void cmi_msg_dump(zlog_level ll, void* msg, size_t size)
{
    uint16_t msg_code = ((cmi_msg_hdr_t*)msg)->msg_code;

    if (msg_code == msg_code_cmd) {
        cmi_msg_dump_cmd(ll, msg, size);
    } else if (msg_code == msg_code_cmdresp) {
        cmi_msg_dump_cmdresp(ll, msg, size);
    } else if (msg_code == msg_code_status) {
        cmi_msg_dump_status(ll, msg, size);
    } else if (msg_code == msg_code_data) {
        cmi_msg_dump_data(ll ,msg, size);
    } else {
        ;
    }

    return;
}

static int cmi_msg_is_valid_code(uint16_t msgcode)
{
    if (msgcode == msg_code_cmd ||
        msgcode == msg_code_cmdresp ||
        msgcode == msg_code_status ||
        msgcode == msg_code_data)
        return 1;

    return(0);
}

static int cmd_msg_is_valid_cmd(cmi_cmd_t* cmd, size_t size)
{
    if (cmd->cmd_type < cmd_type_base ||
        cmd->cmd_type >= cmd_type_max) {
        return(0);
    }
    return(1);
}

static int cmd_msg_is_valid_cmdresp(cmi_cmdresp_t* cmdresp, size_t size)
{
    if (cmdresp->cmd_type < cmd_type_base ||
        cmdresp->cmd_type >= cmd_type_max) {
        return(0);
    }
    return(1);
}

static int cmd_msg_is_valid_status(cmi_status_t* status, size_t size)
{
    if (status->sys_state < sys_state_base ||
        status->sys_state >= sys_state_max) {
        return(0);
    }

    return(1);
}

static int cmd_msg_is_valid_data(cmi_data_t* data, size_t size)
{

    return(1);
}

int cmi_msg_is_valid(void* msg, size_t size)
{
    cmi_msg_hdr_t* hdr =  (cmi_msg_hdr_t*)msg;
    uint16_t    msg_code = hdr->msg_code;

    // common check
    if (hdr->sync_tag != CMI_SYNC_TAG) {
        zlog_warn(cmi_zc, "Invalid message tag: tag=0x%0x", hdr->sync_tag);
        return(0);
    }
    if (hdr->msg_len != size) {
        zlog_warn(cmi_zc, "Invalid message size: size=%zu, msg_len=%u",
                  size, hdr->msg_len);
        return(0);
    }
    if (!cmi_msg_is_valid_code(msg_code)) {
        zlog_warn(cmi_zc, "Invalid message code: code=0x%x",
                  msg_code);
        return(0);
    }

    // check by msg_code
    switch(msg_code) {
    case msg_code_cmd:
        if (!cmd_msg_is_valid_cmd(msg, size)) {
            zlog_warn(cmi_zc, "Illegal message: type=cmd");
            return(0);
        }
        break;
    case msg_code_cmdresp:
        if (!cmd_msg_is_valid_cmdresp(msg, size)) {
            zlog_warn(cmi_zc, "Illegal message: type=cmdresp");
            return(0);
        }
        break;
    case msg_code_status:
        if (!cmd_msg_is_valid_status(msg, size)) {
            zlog_warn(cmi_zc, "Illegal message: type=status");
            return(0);
        }
        break;
    case msg_code_data:
        if (!cmd_msg_is_valid_data(msg, size)) {
            zlog_warn(cmi_zc, "Illegal message: type=data");
            return(0);
        }
        break;
    default:
        assert(0);
        break;
    }

    return(1);
}

void cmi_msg_build_hdr(cmi_msg_hdr_t* hdr, uint16_t msg_code, size_t msg_len)
{
    hdr->sync_tag = CMI_SYNC_TAG;
    hdr->msg_len = msg_len;
    hdr->msg_code = msg_code;
    hdr->msg_srcid = 0;
    hdr->msg_destid = 0;
    hdr->msg_ts = 0x0;
    hdr->csum = 0;

    return;
}

void cmi_msg_build_datafrag(cmi_data_t* msg_buf, int data_type,
                            void* frag_buf, size_t frag_size,
                            uint32_t frag_id, int is_eof)
{
    assert(frag_size <= CMI_MAX_FRAGSIZE);
    memset(msg_buf, 0, sizeof(cmi_data_t));

    cmi_msg_build_hdr(&msg_buf->hdr, msg_code_data, sizeof(cmi_data_t));
    msg_buf->data_type = data_type;
    msg_buf->eof = is_eof;
    msg_buf->frag_id = frag_id;
    msg_buf->frag_len = frag_size;
    memcpy(msg_buf->frag_data, frag_buf, frag_size);

    return;
}

void cmi_msg_reform_hdr(cmi_msg_hdr_t* hdr, cmi_endian endian)
{
    if (cmi_our_endian != endian) {
        hdr->sync_tag   = byteswaps(hdr->sync_tag);
        hdr->msg_len    = byteswaps(hdr->msg_len);
        hdr->msg_code   = byteswaps(hdr->msg_code);
        hdr->msg_srcid  = byteswaps(hdr->msg_srcid);
        hdr->msg_destid = byteswaps(hdr->msg_destid);
        hdr->pcid       = byteswaps(hdr->pcid);
        hdr->msg_ts     = byteswapl(hdr->msg_ts);
        hdr->csum       = byteswapl(hdr->csum);
    }
    return;
}

void cmi_msg_reform_body(uint16_t msg_code, void* msg, cmi_endian endian)
{
    if (cmi_our_endian != endian) {
        switch(msg_code) {
        case msg_code_data: {
            cmi_data_t* msg_data = (cmi_data_t*)msg;
            msg_data->data_type  = byteswaps(msg_data->data_type);
            msg_data->eof        = byteswaps(msg_data->eof);
            msg_data->frag_id    = byteswapl(msg_data->frag_id);
            msg_data->frag_len   = byteswapl(msg_data->frag_len);
        }
        break;
        case msg_code_cmd: {
            cmi_cmd_t* cmd = (cmi_cmd_t*)msg;
            cmd->cmd_type = byteswaps(cmd->cmd_type);
            for (int i=0; i<CMI_CMD_VAR_LEN; i++) {
                cmd->u.all.words[i]  = byteswapl(cmd->u.all.words[i]);
            }
        }
        break;
        case msg_code_cmdresp: {
            cmi_cmdresp_t* cmdresp = (cmi_cmdresp_t*)msg;
            cmdresp->success       = byteswaps(cmdresp->success);
            cmdresp->cmd_type      = byteswaps(cmdresp->cmd_type);
            for (int i=0; i<CMI_CMDRESP_VAR_LEN; i++) {
                cmdresp->u.all.words[i]  = byteswapl(cmdresp->u.all.words[i]);
            }
        }
        break;
        case msg_code_status: {
            cmi_status_t* status = (cmi_status_t*)msg;
            status->sys_state    = byteswapl(status->sys_state);
            status->svr_time     = byteswapl(status->svr_time);
            for (int i=0; i<CMI_MAX_FCNUM; i++) {
                status->fb_link[i]  = byteswaps(status->fb_link[i]);
                status->fb_speed[i] = byteswapl(status->fb_speed[i]);
                status->fb_count[i] = byteswapll(status->fb_count[i]);
            }
            status->disk_size  = byteswapll(status->disk_size);
            status->disk_free  = byteswapll(status->disk_free);
            status->disk_rspd  = byteswapl(status->disk_size);
            status->disk_wspd  = byteswapl(status->disk_free);
        }
        break;
        default:
            zlog_fatal(cmi_zc, "unknown msg_code: code=0x%x", msg_code);
            assert(0);
            break;
        }
    }
}

int cmi_msg_is_same(cmi_msg_hdr_t* msg1, cmi_msg_hdr_t* msg2)
{
    if (MSG_CODE(msg1) != MSG_CODE(msg2) ||
        MSG_SIZE(msg1) != MSG_SIZE(msg2)) {
        return(0);
    }
    switch(msg1->msg_code) {
    case msg_code_cmd: {
        cmi_cmd_t* cmd1 = (cmi_cmd_t*)msg1;
        cmi_cmd_t* cmd2 = (cmi_cmd_t*)msg2;
        if (cmd1->u.all.words[0] != cmd2->u.all.words[0] ||
            cmd1->u.all.words[1] != cmd2->u.all.words[1] ||
            cmd1->u.all.words[2] != cmd2->u.all.words[2] ||
            cmd1->u.all.words[3] != cmd2->u.all.words[3]) {
            return(0);
        }
    }
    break;
    case msg_code_cmdresp: {
        cmi_cmdresp_t* cmd1 = (cmi_cmdresp_t*)msg1;
        cmi_cmdresp_t* cmd2 = (cmi_cmdresp_t*)msg2;
        if (cmd1->cmd_type != cmd2->cmd_type) {
            return(0);
        }
    }
    break;
    case msg_code_status:
        // must be the same
        break;
    case msg_code_data: {
        cmi_data_t* data1 = (cmi_data_t*)msg1;
        cmi_data_t* data2 = (cmi_data_t*)msg2;
        if (data1->data_type != data2->data_type ||
            data1->eof != data2->eof ||
            data1->frag_len != data2->frag_len ||
            data1->frag_id != data2->frag_id) {
            return(0);
        }
    }
    break;
    default:
        assert(0);
        break;
    }

    return(1);
}

void cmi_msg_reform_flist(cmi_data_filelist_t* dfl,
                          cmi_endian peer_endian)
{
    if (cmi_our_endian != peer_endian) {
        dfl->sys_info.tag      = byteswaps(dfl->sys_info.tag);
        dfl->sys_info.machine  = byteswaps(dfl->sys_info.machine);
        dfl->sys_info.slot_num = byteswaps(dfl->sys_info.slot_num);
        dfl->sys_info.rsvd     = byteswaps(dfl->sys_info.rsvd);
        dfl->sys_info.total_sz_h = byteswapl(dfl->sys_info.total_sz_h);
        dfl->sys_info.total_sz_l = byteswapl(dfl->sys_info.total_sz_l);
        dfl->sys_info.free_sz_h  = byteswapl(dfl->sys_info.free_sz_h);
        dfl->sys_info.free_sz_l  = byteswapl(dfl->sys_info.free_sz_l);

        int slot;
        for (slot = 0; slot < CMI_MAX_SLOTS; slot++) {
            cmi_work_list_t* wl = &dfl->work_list[slot];
            wl->tag       = byteswapl(wl->tag);
            wl->slot_id   = byteswaps(wl->slot_id);
            wl->data_src  = byteswaps(wl->data_src);
            wl->work_mark = byteswapl(wl->work_mark);
            wl->begin_tm  = byteswapl(wl->begin_tm);
            wl->end_tm    = byteswapl(wl->end_tm);
            for (int i=0; i<8; i++) {
                wl->task_cmd[i] = byteswaps(wl->task_cmd[i]);
            }
            for (int i=0; i<8; i++) {
                wl->work_place[i] = byteswaps(wl->work_place[i]);
            }
            for (int i=0; i<14; i++) {
                wl->file_desc[i] = byteswaps(wl->file_desc[i]);
            }
            wl->file_sz_h = byteswapl(wl->file_sz_h);
            wl->file_sz_l = byteswapl(wl->file_sz_l);
        }
    }

    return;
}