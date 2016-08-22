#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stddef.h>
#include <inttypes.h>
#include <assert.h>
#include <pthread.h>
#include <errno.h>
#include "spark.h"
#include "cmi/cmi_intf.h"

#define SPK_MAX_WORKERS     (2)
typedef struct 
{
    pthread_mutex_t job_lock;
    int job_type;
    int sys_state;
} spk_wkr_ctx_t;

spk_wkr_ctx_t* spk_wkr_ctx_tbl[SPK_MAX_WORKERS] = {NULL};

void* __spk_wrk_thread(void* args)
{
    spk_wkr_ctx_t* ctx = (spk_wkr_ctx_t*)args;
    int cur_jobtype = 0;
    int i;
    ssize_t ret_size;
    
    while(1) {
        if (ctx->sys_state == sys_state_idle) {
            pthread_mutex_lock(&ctx->job_lock);
            cur_jobtype = ctx->job_type;
            pthread_mutex_unlock(&ctx->job_lock);
            if (cur_jobtype == cmd_type_filelist) {
                cmi_data_filelist_t fl;
                memset(&fl, 0, sizeof(cmi_data_filelist_t));
                for(i=0; i<CMI_MAX_FL_SLOTS; i++) {
                    fl.fl_tbl[i].slot_id = i;
                    fl.fl_tbl[i].file_sz = i;
                    fl.fl_tbl[i].file_sz = 0;
                }
                ret_size = cmi_data_send(msg_code_data_flist, &fl, sizeof(cmi_data_filelist_t));
                assert(ret_size == sizeof(cmi_data_filelist_t));
            }
        }
        usleep(100);
    }

    return(NULL);
}

int cmi_cmd_exec_inquiry(cmi_cmd_t* cmd)
{
    cmi_status_t status;
    cmi_msg_hdr_t* hdr = &status.hdr;
    ssize_t ret;

    memset(&status, 0, sizeof(status));
    cmi_msg_build_hdr(hdr, msg_code_status, sizeof(cmi_status_t));
    status.sys_state = sys_state_idle;
    
    ret = cmi_write(&status, sizeof(cmi_status_t));
    assert(ret == sizeof(cmi_status_t));
    
    return(0);
}

int cmi_cmd_exec_getfilelist(cmi_cmd_t* cmd)
{
    cmi_data_filelist_t dfl;
    ssize_t ret;

    memset(&dfl, 0, sizeof(cmi_data_filelist_t));
    for(int i=0; i<CMI_MAX_FL_SLOTS; i++) {
        dfl.fl_tbl[i].slot_id = i;
        dfl.fl_tbl[i].file_sz = i;
        dfl.fl_tbl[i].file_time = 0x1234567890abcef;
    }
    ret = cmi_data_send(msg_code_data_flist, &dfl, sizeof(cmi_data_filelist_t));
    assert(ret == sizeof(cmi_data_filelist_t));

    return(0);
}

int execute_cmd(cmi_cmd_t* cmd, size_t size, void* outbuf, size_t* respsize)
{
    int cmdtype = cmd->cmd_type;
    cmi_cmdresp_t* cmdresp = (cmi_cmdresp_t*)outbuf;
    int ret;
    
    assert(sizeof(cmi_cmdresp_t) <= CMI_MAX_MSGSIZE);
    
    printf("\n>>>>>> Executing cmd: cmd_type=%s\n", cmi_desc_cmdtype2str(cmdtype));
    switch(cmdtype) {
    case cmd_type_inquiry:
        ret = cmi_cmd_exec_inquiry(cmd);
        break;
    case cmd_type_filelist:
        ret = cmi_cmd_exec_getfilelist(cmd);
        break;
    }

    memset(cmdresp, 0, sizeof(cmi_cmdresp_t));
    cmi_msg_build_hdr((cmi_msg_hdr_t*)cmdresp, msg_code_cmdresp, sizeof(cmi_cmdresp_t));
   
    cmdresp->success = CMI_CMDEXEC_SUC;
    cmdresp->cmd_type = cmd->cmd_type;
    
    *respsize = sizeof(cmi_cmdresp_t);

    return(0);
}

int main()
{
    int ret;
    char* buf = malloc(CMI_MAX_MSGSIZE);
    char* o_buf = malloc(CMI_MAX_MSGSIZE);
    int i;
    
    zlog_init("./zlog.conf");
    ret = cmi_module_init(NULL);
    assert(!ret);
    
RECONN:
    ret = cmi_open(cmi_intf_tcp, 1234);
    assert(!ret);

    while(1) {
        ssize_t size = cmi_read(buf, CMI_MAX_MSGSIZE);
        if (size < 0) {
            cmi_close();
            goto RECONN;
        }
        if (size > 0) {
            cmi_msg_dump(ZLOG_LEVEL_NOTICE, buf, size);

            uint16_t msg_code = ((cmi_msg_hdr_t*)buf)->msg_code;
            size_t respsize = 0;
            if (msg_code == msg_code_cmd) {
                ret = execute_cmd((cmi_cmd_t*)buf, size, o_buf, &respsize);
                assert(!ret);
            } else if (msg_code == msg_code_cmdresp) {
                // impossible
                assert(0);
            } else if (msg_code == msg_code_status) {
                // impossible
                assert(0);
            } else if (((msg_code & 0xff00) >> 8) == 0xfe) {
            } else {
                ;
            }
            assert(respsize);
            if (respsize > 0) {
                size = cmi_write(o_buf, respsize);
                printf("Send ack\n");
                if (respsize != size) {
                    printf("respsize=%zd, size=%zd\n", respsize, size);
                    assert(0);
                }
            }
        }
    }

    return(0);
}
