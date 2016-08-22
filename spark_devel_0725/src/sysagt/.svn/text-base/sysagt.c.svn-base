/*************************************************************************
    > File Name: sysagt.c
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
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <assert.h>

#include "spark.h"
#include "cmi/cmi_intf.h"
#include "sysagt.h"

#define AGT_MAX_MERGEOUT_NODES  (16)

static agt_env_t agt_env;
static agt_ctx_t agt_ctx;
zlog_category_t* agt_zc = NULL;

static void agt_sys_change_state(int new_state)
{
    if (agt_ctx.sys_state != new_state) {
        zlog_notice(agt_zc, "*** change state: %s -> %s",
                    cmi_desc_sysstate2str(agt_ctx.sys_state),
                    cmi_desc_sysstate2str(new_state));
        agt_ctx.sys_state = new_state;
    }
}

agt_msgq_node_t* agt_msgq_dequeue_type(agt_msgq_t* q, agt_msgq_node_t* node_sample)
{
    int found = 0;
    agt_msgq_node_t* node = NULL;
    agt_msgq_node_t* node_tmp = NULL;

    pthread_mutex_lock(&q->lock);
    if (!list_empty_careful(&q->listhead)) {
        list_for_each_entry_safe(node, node_tmp, &q->listhead, list) {
            if (cmi_msg_is_same((cmi_msg_hdr_t*)node->msg,
                (cmi_msg_hdr_t*)node_sample->msg)) {
                found = 1;
                break;
            }
        }
        if (found) {
            list_del_init(&node->list);
            q->count--;
        } else {
            node = NULL;
        }
    }
    pthread_mutex_unlock(&q->lock);

    return(node);
}

static int agt_msg_should_drop(char* msg)
{
    int msg_code = MSG_CODE(msg);
    int drop = 0;
    if (msg_code == msg_code_data) {
        int data_type = DATA_DATATYPE(msg);
        if (data_type == data_type_dl && agt_ctx.sys_state != sys_state_dl) {
            zlog_notice(agt_zc, "drop msg: data_type=0x%x, sys_state=%s",
                        data_type, cmi_desc_sysstate2str(agt_ctx.sys_state));
            drop = 1;
        }
    }

    return(drop);
}

static void agt_msgq_cleanup(agt_msgq_t* q, agt_msgq_t* q2)
{
    agt_msgq_node_t* node = NULL;
    agt_msgq_node_t* node_tmp = NULL;

    pthread_mutex_lock(&q->lock);
    if (!list_empty_careful(&q->listhead)) {
        list_for_each_entry_safe(node, node_tmp, &q->listhead, list) {
            if (agt_msg_should_drop(node->msg)) {
                list_del_init(&node->list);
                q->count--;
                agt_msgq_enqueue(q2, node);
            }
        }
    }
    pthread_mutex_unlock(&q->lock);

    return;
}

void* __agt_svr_worker(void* arg)
{
    int svr_id = (int)(intptr_t)arg;
    agt_svr_ctx_t* svr_ctx = agt_ctx.svr_ctx_tbl[svr_id];
    agt_msgq_node_t* msg_node = NULL;
    cmi_intf_t* svr_cmi = svr_ctx->svr_cmi;
    ssize_t xfer_len;
    char msg_buffer[CMI_MAX_MSGSIZE];

    assert(svr_cmi);

    zlog_notice(agt_zc, "svr#%d> spawned", svr_id);

    while(!svr_ctx->svrt_quit_req) {
        // receive from server
        xfer_len = cmi_intf_read_msg(svr_cmi, &msg_buffer, CMI_MAX_MSGSIZE);
        if (xfer_len == 0) {
            usleep(100);
            continue;
        }
        if (xfer_len < 0) {
            zlog_error(agt_zc, "[RESET_REQ] cmi_intf_read_msg: ret=%ld", xfer_len);
            goto out;
        }
        // msg arrived
        // check
        if (!agt_msg_should_drop(msg_buffer)) {
            do {
                msg_node = agt_msgq_dequeue(&svr_ctx->wkr_fq);
                if (msg_node)
                    break;
                if (svr_ctx->svrt_quit_req)
                    goto out;
                usleep(100);
            } while(1);

            // push to oqueue (to upper node)
            memcpy(msg_node->msg, msg_buffer, xfer_len);
            msg_node->msg_code = MSG_CODE(msg_node->msg);
            msg_node->msg_size = MSG_SIZE(msg_node->msg);
            agt_msgq_enqueue(&svr_ctx->wkr_oq, msg_node);
        }
    };

out:
    agt_ctx.reset_flag = 1;

    zlog_notice(agt_zc, "svr#%d> terminated", svr_id);
    return NULL;
}

static cmi_data_filelist_t dfl_out;
static int dfl_ready = 0;
static int __agt_ob_merge_data_flist(agt_msgq_node_t* node_in_ary[], int num,
                      agt_msgq_node_t* node_out_ary[AGT_MAX_MERGEOUT_NODES])
{
    static cmi_data_filelist_t dfl_in[AGT_MAX_CONNS];
    static int dfl_recved = 0;
    int i, j;

    agt_msgq_node_t* master = node_in_ary[0];
    cmi_data_t* msg_data = (cmi_data_t*)master->msg;
    uint32_t frag_len = msg_data->frag_len;
    int eof = msg_data->eof;

    if (msg_data->frag_id == 0) {
        // start
        memset(dfl_in, 0, sizeof(dfl_in));
        dfl_recved = 0;
        dfl_ready = 0;
    }

    assert(dfl_recved + frag_len <=  sizeof(cmi_data_filelist_t));

    for (i=0; i<num; i++) {
        cmi_data_t* msg = (cmi_data_t*)node_in_ary[i]->msg;
        memcpy(((char*)&dfl_in[i])+dfl_recved, msg->frag_data, msg->frag_len);
        if (eof) {
            // reform from server endian to ours
            cmi_msg_reform_flist(&dfl_in[i], agt_ctx.svr_ctx_tbl[i]->svr_cmi->endian_cur);
        }
    }

    dfl_recved += frag_len;

    if (eof) {
        assert(dfl_recved == sizeof(cmi_data_filelist_t));

        // merge work/file list
        // copy from
        memcpy(&dfl_out, &dfl_in[0], sizeof(cmi_data_filelist_t));
        for (i=0; i<CMI_MAX_SLOTS; i++) {
            cmi_work_list_t* fl_out = &dfl_out.work_list[i];
            cmi_work_list_t* fl_master = &dfl_in[0].work_list[i];
            uint64_t file_sz_master = MAKE64(fl_master->file_sz_h,
                                             fl_master->file_sz_l);
            for (j=1; j<num; j++) {
                cmi_work_list_t* fl_cur = &dfl_in[j].work_list[i];
                uint64_t file_sz_cur = MAKE64(fl_cur->file_sz_h,
                                              fl_cur->file_sz_l);
                assert(fl_master->slot_id == fl_cur->slot_id);
                if (file_sz_master != file_sz_cur) {
                    // file_sz is different
                    // clear it to -
                    zlog_warn(agt_zc, "filesz in list not sync: slot=%d, master=%lu, repo#%d=%lu",
                                       i, file_sz_master, j, file_sz_cur);
                    fl_out->file_sz_h = HGH32((uint64_t)-1);
                    fl_out->file_sz_l = LOW32((uint64_t)-1);
                    break;
                }
                uint64_t file_sz_out = MAKE64(fl_out->file_sz_h,
                                              fl_out->file_sz_l);
                file_sz_out += file_sz_master;
                fl_out->file_sz_h = HGH32(file_sz_out);
                fl_out->file_sz_l = LOW32(file_sz_out);
            }
        }

        // reform from our endian to client
        cmi_msg_reform_flist(&dfl_out, agt_ctx.cli_cmi->endian_cur);

        // flist ready
        dfl_ready = 1;
    }

    return(0);
}

static int __agt_ob_merge_data_snap(agt_msgq_node_t* node_in_ary[], int num,
                      agt_msgq_node_t* node_out_ary[AGT_MAX_MERGEOUT_NODES])
{
    agt_msgq_node_t* node = malloc(sizeof(agt_msgq_node_t));
    assert(node);

    memcpy(node, node_in_ary[0], sizeof(agt_msgq_node_t));
    node_out_ary[0] = node;
    return(1);
}

static int __agt_ob_merge_data_dl(agt_msgq_node_t* node_in_ary[], int num,
                      agt_msgq_node_t* node_out_ary[AGT_MAX_MERGEOUT_NODES])
{
    int i, j;

    agt_msgq_node_t* master = node_in_ary[0];
    int data_len = num*DATA_FRAGLEN(master->msg);
    int deint_len = AGT_INTERLACE_SIZE;
    assert(!(data_len % deint_len));

    int pkt_count = num; // FIXME

    for (i=0; i<pkt_count; i++) {
        agt_msgq_node_t* node = malloc(sizeof(agt_msgq_node_t));
        assert(node);
        // we use the master's hdr & frag_id/eof etc.
        memcpy(node, master, sizeof(agt_msgq_node_t));
        for (j=0; j<num; j++) {
            memcpy(DATA_FRAGDATA(node->msg) + j*deint_len,
                   DATA_FRAGDATA(node_in_ary[j]->msg)+ i*deint_len,
                   deint_len);
        }
        DATA_FRAGID(node->msg) *= pkt_count;
        DATA_FRAGID(node->msg) += i;
        // only last will raise eof
        DATA_EOF(node->msg) &= (i == (pkt_count-1));
        node_out_ary[i] = node;
    }
    if (DATA_EOF(master->msg)) {
        agt_sys_change_state(sys_state_idle);
    }
    return(pkt_count);
}

static int __agt_ob_merge_data(agt_msgq_node_t* node_in_ary[], int num,
                      agt_msgq_node_t* node_out_ary[AGT_MAX_MERGEOUT_NODES])
{
    agt_msgq_node_t* master = node_in_ary[0];
    cmi_data_t* msg_data = (cmi_data_t*)master->msg;
    int ret = -1;

    switch(msg_data->data_type) {
    case data_type_flist:
        ret = __agt_ob_merge_data_flist(node_in_ary, num, node_out_ary);
        break;
    case data_type_snap:
        ret = __agt_ob_merge_data_snap(node_in_ary, num, node_out_ary);
        break;
    case data_type_dl:
        ret = __agt_ob_merge_data_dl(node_in_ary, num, node_out_ary);
        break;
    default:
    case data_type_ul:
        assert(0);
        break;
    }

    return(ret);
}

static int __agt_ob_merge_cmdresp(agt_msgq_node_t* node_in_ary[], int num,
                      agt_msgq_node_t* node_out_ary[AGT_MAX_MERGEOUT_NODES])
{
    int i;
    uint16_t success = 0;
    uint32_t sys_ver = 0;

    for (i=0; i<num; i++) {
        agt_msgq_node_t* node = node_in_ary[i];
        cmi_cmdresp_t* msg_cmdresp = (cmi_cmdresp_t*)node->msg;
        if (success == 0) {
            success = msg_cmdresp->success;
        } else if (success != msg_cmdresp->success) {
            success = CMI_CMDEXEC_UNKNOWN;
        }
        if (sys_ver == 0) {
            sys_ver = msg_cmdresp->u.version.sys_ver;
        } else if (sys_ver != msg_cmdresp->u.version.sys_ver) {
            sys_ver = 0xffffffff;
        }
    }

    agt_msgq_node_t* mst_node = node_in_ary[0];
    cmi_cmdresp_t* mst_node_msg = (cmi_cmdresp_t*)mst_node->msg;
    if (mst_node_msg->cmd_type == cmd_type_filelist &&
        mst_node_msg->success == CMI_CMDEXEC_SUCC) {
        // fetch filelist successfully
        // filter this response
        // we will construct fake data/cmdresp msg in inbound thread
        return(0);
    }

    // lazy to build whole agt_msgq_node_t
    // just copy it from master(#0)
    agt_msgq_node_t* node = malloc(sizeof(agt_msgq_node_t));
    assert(node);
    memcpy(node, mst_node, sizeof(agt_msgq_node_t));
    // set success flag
    ((cmi_cmdresp_t*)node->msg)->success = success;
    if (mst_node_msg->cmd_type == cmd_type_init) {
        // merge version, etc.
        ((cmi_cmdresp_t*)node->msg)->u.version.sys_ver = sys_ver;    
        ((cmi_cmdresp_t*)node->msg)->u.version.agt_ver = AGT_VERSION_INT;    
    }
//    zlog_notice(agt_zc, "merged cmdresp: type=0x%x", mst_node_msg->cmd_type);

    // output 1 node
    node_out_ary[0] = node;
    return(1);
}

static int __agt_ob_merge_status(agt_msgq_node_t* node_in_ary[], int num,
                      agt_msgq_node_t* node_out_ary[AGT_MAX_MERGEOUT_NODES])
{
    int i, j;
    agt_msgq_node_t* status_node;
    cmi_status_t* status_out = NULL;

    status_node = malloc(sizeof(agt_msgq_node_t));
    assert(status_node);
    memset(status_node, 0, sizeof(agt_msgq_node_t));

    cmi_msg_build_hdr((cmi_msg_hdr_t*)status_node->msg,
                        msg_code_status,
                        sizeof(cmi_status_t));
    status_node->msg_code = MSG_CODE(status_node->msg);
    status_node->msg_size = MSG_SIZE(status_node->msg);
    status_out = (cmi_status_t*)status_node->msg;

    for (i=0; i<num; i++) {
        cmi_status_t* status_in = (cmi_status_t*)(node_in_ary[i]->msg);
        status_out->disk_size += status_in->disk_size;
        status_out->disk_free += status_in->disk_free;
        // TODO: speed need divisor
        status_out->disk_rspd += status_in->disk_rspd;
        status_out->disk_wspd += status_in->disk_wspd;

        for (j=0; j<CMI_MAX_FCNUM; j++) {
            status_out->fb_link[0] &= status_in->fb_link[j];
            status_out->fb_count[0] += status_in->fb_count[j];
            status_out->fb_speed[0] += status_in->fb_speed[j];
        }

        // we use maskter(#0) status if item can not be merged
        if (i == 0) {
            status_out->sys_state = status_in->sys_state;
            status_out->svr_time = status_in->svr_time;
        }
    }

    // output 1 node
    node_out_ary[0] = status_node;

    return(1);
}

int __agt_ob_merge_msg(agt_msgq_node_t* node_in_ary[], int num,
                      agt_msgq_node_t* node_out_ary[AGT_MAX_MERGEOUT_NODES])
{
    assert(num > 0);

    agt_msgq_node_t* master = node_in_ary[0];
    int ret = -1;

    uint16_t msg_code = MSG_CODE(master->msg);
    switch(msg_code) {
        case msg_code_cmdresp:
            ret = __agt_ob_merge_cmdresp(node_in_ary, num, node_out_ary);
            break;
        case msg_code_status:
            ret = __agt_ob_merge_status(node_in_ary, num, node_out_ary);
            break;
        case msg_code_data:
            ret = __agt_ob_merge_data(node_in_ary, num, node_out_ary);
            break;
        case msg_code_cmd:
        default:
            assert(0);
            break;
    }

    return(ret);
}

int __agt_ib_dispatch_msg_clone(agt_msgq_node_t* node_in)
{
    int i;
    int ret = 0;

    // make copies of msg_node
    // and push to each worker's iqueue
    for (i=0; i<agt_env.conn_num; i++) {
        agt_svr_ctx_t* svr_ctx = agt_ctx.svr_ctx_tbl[i];
        ssize_t xfer_len = cmi_intf_write_msg(svr_ctx->svr_cmi,
                                              node_in->msg,
                                              MSG_SIZE(node_in->msg));
        if (xfer_len <= 0) {
            zlog_error(agt_zc, "[RESET_REQ] cmi_intf_write_msg: ret=%ld", xfer_len);
            ret = SPKERR_RESETSYS;
            goto out;
        }
    }

    ret = agt_env.conn_num;
out:
    return(ret);
}

int __agt_ib_dispatch_msg_uldata(agt_msgq_node_t* node_in)
{
    int i;
    int ret = 0;

    assert((MSG_CODE(node_in->msg)) == msg_code_data);
    assert((DATA_DATATYPE(node_in->msg)) == data_type_ul);

    cmi_data_t msg_data;
    memset(&msg_data, 0, sizeof(cmi_data_t));
    // duplicate msg
    memcpy(&msg_data, node_in->msg, sizeof(cmi_data_t));

    uint8_t* data_ptr = DATA_FRAGDATA(node_in->msg);
    int copy_len = 0;
    int copy_count = 0;
    int data_len = DATA_FRAGLEN(node_in->msg);
    int deint_len = AGT_INTERLACE_SIZE;
    assert(!(data_len % deint_len));

    while(copy_len < data_len) {
        // make copies of msg_node
        // and push to each worker's iqueue
        for (i=0; i<agt_env.conn_num; i++) {
            agt_svr_ctx_t* svr_ctx = agt_ctx.svr_ctx_tbl[i];

            memcpy(DATA_FRAGDATA(&msg_data), data_ptr + copy_len, deint_len);
            DATA_FRAGLEN(&msg_data) = deint_len;
            DATA_FRAGID(&msg_data) = DATA_FRAGID(node_in->msg) * agt_env.conn_num + i;
            DATA_EOF(&msg_data) = DATA_EOF(node_in->msg) & (copy_len + (deint_len * agt_env.conn_num) >= data_len);
            copy_len += deint_len;
            copy_count++;
            ssize_t xfer_len = cmi_intf_write_msg(svr_ctx->svr_cmi,
                                                  &msg_data,
                                                  MSG_SIZE(&msg_data));
            if (xfer_len <= 0) {
                zlog_error(agt_zc, "[RESET_REQ] cmi_intf_write_msg: ret=%ld", xfer_len);
                ret = SPKERR_RESETSYS;
                goto out;
            }
        }
    }

    ret = copy_count;
out:
    return(ret);
}

static int __agt_ib_handle_msg(agt_msgq_node_t* node_in)
{
    int ret = -1;

    int is_flist_frag0 = 0;

    int msg_code = 0;
    int cmd_type = 0;

    msg_code = MSG_CODE(node_in->msg);
    if (msg_code == msg_code_cmd) {
        cmd_type = CMD_TYPE(node_in->msg);
    }

    if (msg_code == msg_code_cmd) {
        if (cmd_type == cmd_type_filelist) {
            // handle cmd_type_filelist
            // cmd_type_filelist is very special
            // because it has to be reformed even the payload
            uint32_t frag_id = CMD_FRAGID(node_in->msg);
            if (frag_id == 0) {
                // override to fast mode
                CMD_FRAGID(node_in->msg) = (uint32_t)-1;
                is_flist_frag0 = 1;
                frag_id = CMD_FRAGID(node_in->msg);
            }
            if (frag_id != (uint32_t)-1) {
                // wait flist arrived and send to upper directly
                goto skip_dispatch;
            }
            // reset ready flag
            // to ensure every byte we use is brand-new
            dfl_ready = 0;
        } else if (cmd_type == cmd_type_start_dl) {
            assert(!(CMD_BLKSTART(node_in->msg) % agt_env.conn_num));
            assert(!(CMD_BLKNUM(node_in->msg) % agt_env.conn_num));
            CMD_BLKSTART(node_in->msg) /= agt_env.conn_num;
            CMD_BLKNUM(node_in->msg) /= agt_env.conn_num;
            agt_sys_change_state(sys_state_dl);
        } else if (cmd_type == cmd_type_start_ul) {
            agt_sys_change_state(sys_state_ul);
        }
        // cmd, cloen
        ret = __agt_ib_dispatch_msg_clone(node_in);
    } else {
        // upload data, split
        assert(msg_code == msg_code_data);
        if ((DATA_DATATYPE(node_in->msg) == data_type_ul)) {
            ret = __agt_ib_dispatch_msg_uldata(node_in);
        } else if ((DATA_DATATYPE(node_in->msg) == data_type_upgrade)) {
            ret = __agt_ib_dispatch_msg_clone(node_in);
        }
        if (DATA_EOF(node_in->msg)) {
            agt_sys_change_state(sys_state_idle);
        }
    }

    if (ret <= 0) {
        zlog_error(agt_zc, "dispatch msg failed: msgcode=0x%x, ret=%d", msg_code, ret);
        ret = SPKERR_RESETSYS;
        goto out;
    }

skip_dispatch:
    if (msg_code == msg_code_cmd) {
        if (cmd_type == cmd_type_filelist) {
            // wait until server sent the whole flist
            uint64_t tm_timeout = spk_get_tick_count() + 5*1000;
            do  {
                if (spk_get_tick_count() > tm_timeout) {
                    ret = SPKERR_TIMEOUT;
                    goto out;
                }
                usleep(100);
            } while(!dfl_ready);

            // flist arrived from lower node
            uint32_t req_frag = CMD_FRAGID(node_in->msg);
            if (is_flist_frag0) {
                // restore real frad_id
                req_frag = 0;
            }

            // send cmdresp
            ret = cmi_intf_write_cmdresp(agt_ctx.cli_cmi,
                                         cmd_type_filelist,
                                         CMI_CMDEXEC_SUCC);
            if (ret < 0) {
                zlog_error(agt_zc, "cmi_intf_write_cmdresp: ret=%d", ret);
                goto out;
            }

            // send flist body
            ret = cmi_intf_write_flist(agt_ctx.cli_cmi, &dfl_out, req_frag);
            if (ret < 0) {
                zlog_error(agt_zc, "cmi_intf_write_flist: ret=%d", ret);
                goto out;
            }
        } else if (cmd_type == cmd_type_stop_dl) {
            // change state to prevent outbound from
            // receiving any dl_data msg
            agt_sys_change_state(sys_state_idle);
            // clear all dl_data in outbound
            for (int i=0; i<agt_env.conn_num; i++) {
                agt_svr_ctx_t* svr_ctx = agt_ctx.svr_ctx_tbl[i];
                agt_msgq_cleanup(&svr_ctx->wkr_oq, &svr_ctx->wkr_fq);
            }
        }
    }

    ret = SPK_SUCCESS;

out:
    return(ret);
}

static int __agt_ob_handle_msg(agt_msgq_node_t* mst_node, int* quit_flag)
{
    agt_svr_ctx_t* svr_ctx = NULL;
    agt_msgq_node_t* node_in_ary[AGT_MAX_CONNS] = {NULL};
    agt_msgq_node_t* node_out_ary[AGT_MAX_MERGEOUT_NODES] = {NULL};
    agt_msgq_node_t* msg_node = NULL;
    int i;
    int ret = -1;

    memset(node_in_ary, 0, sizeof(node_in_ary));
    memset(node_out_ary, 0, sizeof(node_out_ary));
    node_in_ary[0] = mst_node;

    // pickup from each svr for the same type as master node
    for (i=1; i<agt_env.conn_num; i++) {
        uint64_t tm_timeout = spk_get_tick_count() + 5*1000;
        // popup the same type from each workers
        svr_ctx = agt_ctx.svr_ctx_tbl[i];
        do {
            msg_node = agt_msgq_dequeue_type(&svr_ctx->wkr_oq, mst_node);
            if (msg_node) {
                // got a same type node
                break;
            }
            if (agt_msgq_get_count(&svr_ctx->wkr_oq) >= AGT_MAX_QUEUE_DEPTH) {
                // queue full, but still can not found match node
                zlog_warn(agt_zc, "outbound queue full");
                cmi_msg_dump(ZLOG_LEVEL_WARN, mst_node->msg, MSG_SIZE(mst_node->msg));
                ret = SPKERR_RESETSYS;
                break;
            }
            if (*quit_flag) {
                // quit request
                ret = SPK_SUCCESS;
                break;
            }
            if (spk_get_tick_count() > tm_timeout) {
                if (agt_msg_should_drop(mst_node->msg)) {
                    // confirm again
                    goto out;
                }
                // timeout
                zlog_warn(agt_zc, "matched node not found");
                cmi_msg_dump(ZLOG_LEVEL_WARN, mst_node->msg, MSG_SIZE(mst_node->msg));
                ret = SPKERR_TIMEOUT;
                break;
            }
            usleep(100);
        } while(1);

        if (!msg_node) {
            goto out;
        }

        node_in_ary[i] = msg_node;
    }

    // got every pkts from svrs
    // merge them all
    ret = __agt_ob_merge_msg(node_in_ary, agt_env.conn_num, node_out_ary);
    if (ret < 0) {
        zlog_error(agt_zc, "__agt_ob_merge_msg: ret=%d", ret);
        ret = SPKERR_RESETSYS;
        goto out;
    }

    // send every merged-out nodes to our client
    for (i=0; i<ret; i++) {
        assert(node_out_ary[i]);
        assert(node_out_ary[i]->msg_size);
        ssize_t written;
        int msg_size = MSG_SIZE(node_out_ary[i]->msg);
        written = cmi_intf_write_msg(agt_ctx.cli_cmi,
                                     node_out_ary[i]->msg,
                                     msg_size);
        if (written != msg_size) {
            zlog_error(agt_zc, "cmi_intf_write_msg: ret=%ld", written);
            ret = SPKERR_RESETSYS;
            goto out;
        }
    }

    ret = SPK_SUCCESS;

out:
    for (i=0; i<AGT_MAX_MERGEOUT_NODES; i++) {
        if (node_out_ary[i]) {
            SAFE_RELEASE(node_out_ary[i]);
        }
    }
    // recycle all nodes, include master's
    for (i=0; i<agt_env.conn_num; i++) {
        if (node_in_ary[i]) {
            svr_ctx = agt_ctx.svr_ctx_tbl[i];
            agt_msgq_enqueue(&svr_ctx->wkr_fq, node_in_ary[i]);
        }
    }
    return(ret);
}

void* __agt_ob_worker(void* arg)
{
    agt_svr_ctx_t* svr_ctx = agt_ctx.svr_ctx_tbl[0];
    agt_msgq_node_t* mst_node = NULL;
    assert(svr_ctx);

    zlog_notice(agt_zc, "obt> spawned");
    while(!agt_ctx.obt_quit_req) {
        // handle msgs from svr
        // we use master(#0)'s oqueue
        mst_node = agt_msgq_dequeue(&svr_ctx->wkr_oq);
        if (mst_node) {
            if (agt_msg_should_drop(mst_node->msg)){
                agt_msgq_enqueue(&svr_ctx->wkr_fq, mst_node);
            } else {
                int ret = __agt_ob_handle_msg(mst_node, &agt_ctx.obt_quit_req);
                if (ret < 0) {
                    zlog_error(agt_zc, "[RESET_REQ] __agt_ob_handle_msg: ret=%d", ret);
                    break;
                }
            }
        }
        usleep(10);
    };

    agt_ctx.reset_flag = 1;
    zlog_notice(agt_zc, "obt> terminated");

    return(NULL);
}

static void agt_svr_destroy_svr(agt_svr_ctx_t* ctx)
{
    if (ctx->svrt) {
        ctx->svrt_quit_req = 1;
        pthread_join(*ctx->svrt, NULL);
        SAFE_RELEASE(ctx->svrt);
    }

    cmi_intf_disconnect(ctx->svr_cmi);
    cmi_intf_close(ctx->svr_cmi);
    ctx->svr_cmi = NULL;

    agt_msgq_node_t* node = NULL;
    while((node = agt_msgq_dequeue(&ctx->wkr_oq))) {
        agt_msgq_enqueue(&ctx->wkr_fq, node);
    }
    assert(agt_msgq_get_count(&ctx->wkr_fq) == AGT_MAX_QUEUE_DEPTH);
    while((node = agt_msgq_dequeue(&ctx->wkr_fq))) {
        SAFE_RELEASE(node);
    }

    SAFE_RELEASE(ctx);

    return;
}

static int agt_svr_connect_svr(int svr_id)
{
    int j;
    int ret = -1;

    // initialize each svr threads
    agt_svr_ctx_t* svr_ctx = malloc(sizeof(agt_svr_ctx_t));
    assert(svr_ctx);

    memset(svr_ctx, 0, sizeof(agt_svr_ctx_t));
    svr_ctx->svr_id = svr_id;
    svr_ctx->svr_cmi = cmi_intf_open(agt_env.svr_type,
                                     agt_env.svr_intftype,
                                     agt_env.svr_endian);
    assert(svr_ctx->svr_cmi);

    agt_msgq_init(&svr_ctx->wkr_oq);
    agt_msgq_init(&svr_ctx->wkr_fq);
    for (j=0; j<AGT_MAX_QUEUE_DEPTH; j++) {
        agt_msgq_node_t* node = malloc(sizeof(agt_msgq_node_t));
        assert(node);
        agt_msgq_enqueue(&svr_ctx->wkr_fq, node);
    }

RECONN:
    zlog_notice(agt_zc, "svr#%d> connecting: addr=%s:%d",
                        svr_id,
                        agt_env.conn_tbl[svr_id].svr_ipaddr,
                        agt_env.conn_tbl[svr_id].svr_port);
    ret = cmi_intf_connect(svr_ctx->svr_cmi,
                           agt_env.conn_tbl[svr_id].svr_ipaddr,
                           agt_env.conn_tbl[svr_id].svr_port);
    if (ret < 0) {
        zlog_notice(agt_zc, "svr#%d> failed to connect to server, "
                           "retry after 5 second", svr_id);
        sleep(5);
        goto RECONN;
    }
    zlog_notice(agt_zc, "svr#%d> server connected", svr_id);

    svr_ctx->svrt = malloc(sizeof(pthread_t));
    assert(svr_ctx->svrt);
    agt_ctx.svr_ctx_tbl[svr_id] = svr_ctx;

    pthread_create(svr_ctx->svrt, NULL, __agt_svr_worker, (void*)(intptr_t)svr_id);

    return(SPK_SUCCESS);
}

int main(int argc, char **argv)
{
    int ret;
    int i;

    // initialize log system
    zlog_init("./zlog.conf");
    agt_zc = zlog_get_category("AGT");
    assert(agt_zc);

    ret = cmi_module_init(NULL);
    assert(!ret);

    memset(&agt_env, 0, sizeof(agt_env_t));
    memset(&agt_ctx, 0, sizeof(agt_ctx_t));

    // initialize env
    // TBD: should be read from .conf
    zlog_notice(agt_zc, "> loading conf ...");
    memset(&agt_env, 0, sizeof(agt_env));
    agt_env.type = cmi_type_server;
    agt_env.intftype = cmi_intf_tcp;
    agt_env.port = 1234;
    agt_env.endian = cmi_endian_auto;

    agt_env.svr_type = cmi_type_client;
    agt_env.svr_intftype = cmi_intf_tcp;
    agt_env.svr_endian = cmi_endian_big;

    agt_env.conn_num = 2;
    agt_env.conn_tbl[0].svr_ipaddr = "192.168.133.13";
    agt_env.conn_tbl[0].svr_port = 1235;
    agt_env.conn_tbl[1].svr_ipaddr = "192.168.133.14";
    agt_env.conn_tbl[1].svr_port = 1235;

RECONN:
    zlog_notice(agt_zc, "> ---------- SERVER START ----------");
    if (agt_ctx.obt) {
        zlog_notice(agt_zc, "> cleanup ob thread ...");
        agt_ctx.obt_quit_req = 1;
        pthread_join(*agt_ctx.obt, NULL);
        SAFE_RELEASE(agt_ctx.obt);
        agt_ctx.obt_quit_req = 0;
    }

    if (agt_ctx.cli_cmi) {
        zlog_notice(agt_zc, "> disconnect from client...");
        cmi_intf_disconnect(agt_ctx.cli_cmi);
        cmi_intf_close(agt_ctx.cli_cmi);
        agt_ctx.cli_cmi = NULL;
    }

    for (i=0; i<agt_env.conn_num; i++) {
        if (agt_ctx.svr_ctx_tbl[i]) {
            zlog_notice(agt_zc, "> destroy svr#%d ...", i);
            agt_svr_destroy_svr(agt_ctx.svr_ctx_tbl[i]);
            agt_ctx.svr_ctx_tbl[i] = NULL;
        }
    }
    agt_ctx.reset_flag = 0;
    agt_ctx.sys_state = sys_state_idle;

    // open cmi
    agt_ctx.cli_cmi = cmi_intf_open(agt_env.type,
                                    agt_env.intftype,
                                    agt_env.endian);
    assert(agt_ctx.cli_cmi);

    zlog_notice(agt_zc, "> wait for client to connect ...");
    ret = cmi_intf_connect(agt_ctx.cli_cmi, NULL, agt_env.port);
    if (ret != SPK_SUCCESS) {
        assert(0);
        exit(-1);
    }

    for (i=0; i<agt_env.conn_num; i++) {
        zlog_notice(agt_zc, "> connecting svr#%d ...", i);
        ret = agt_svr_connect_svr(i);
        assert(ret == SPK_SUCCESS);
        assert(agt_ctx.svr_ctx_tbl[i]);
    }

    agt_ctx.obt = malloc(sizeof(pthread_t));
    pthread_create(agt_ctx.obt, NULL, __agt_ob_worker, NULL);

    while(1) {
        // main loop
        if (agt_ctx.reset_flag) {
            goto RECONN;
        }

        // handle msgs from upper node
        agt_msgq_node_t msg_node;
        ssize_t msg_size = cmi_intf_read_msg(agt_ctx.cli_cmi,
                                             &msg_node.msg,
                                             CMI_MAX_MSGSIZE);
        if (msg_size < 0) {
            zlog_error(agt_zc, "[RESET_REQ] cmi_intf_read_msg: ret=%ld", msg_size);
            agt_ctx.reset_flag = 1;
            continue;
        }

        if (msg_size > 0) {
//            cmi_msg_dump(ZLOG_LEVEL_NOTICE, msg_node.msg, MSG_SIZE(msg_node.msg));
            msg_node.msg_code = MSG_CODE(msg_node.msg);
            msg_node.msg_size = MSG_SIZE(msg_node.msg);
            ret = __agt_ib_handle_msg(&msg_node);
            if (ret < 0) {
                zlog_error(agt_zc, "[RESET_REQ] __agt_ib_handle_msg: ret=%d", ret);
                agt_ctx.reset_flag = 1;
                continue;
            }
        }
        usleep(10);
    }

    // FIXME: resources not cleanup
    assert(0);
    return 0;
}
