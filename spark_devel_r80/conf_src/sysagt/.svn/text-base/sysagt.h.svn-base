
#ifndef __SYSAGT_H__
#define __SYSAGT_H__

#include "fica_list.h"

#define AGT_VERSION_MAJOR   0
#define AGT_VERSION_MINOR   9
#define AGT_VERSION_DATE    151230

#define AGT_VERSION         MAKE_VER_STR(AGT_VERSION_MAJOR, AGT_VERSION_MINOR, AGT_VERSION_DATE)
#define AGT_VERSION_INT     MAKE_VER_INT(AGT_VERSION_MAJOR, AGT_VERSION_MINOR, AGT_VERSION_DATE)

#define AGT_MAX_CONNS   (2)
#define AGT_MAX_QUEUE_DEPTH (32)

#define AGT_INTERLACE_SIZE  (4*1024) * 2 //each server has 2 pipes

typedef struct
{
    const char* svr_ipaddr;
    int svr_port;
} agt_conn_t;

typedef struct
{
    cmi_type        type;
    cmi_intf_type   intftype;
    int             port;
    cmi_endian      endian;

    cmi_type        svr_type;
    cmi_intf_type   svr_intftype;
    cmi_endian      svr_endian;

    int             conn_num;
    agt_conn_t      conn_tbl[AGT_MAX_CONNS];
} agt_env_t;

typedef struct
{
    struct list_head list;
    int msg_code;
    int msg_size;
    char msg[CMI_MAX_MSGSIZE];
} agt_msgq_node_t;

DECLARE_QUEUE(agt_msgq, agt_msgq_node_t)

typedef struct
{
    int svr_id;

    cmi_intf_t* svr_cmi;

    agt_msgq_t  wkr_fq;
    agt_msgq_t  wkr_oq;
    
    pthread_t*  svrt;
    int         svrt_quit_req;
} agt_svr_ctx_t;

typedef struct
{
    cmi_intf_t* cli_cmi;
    pthread_t*  obt;
    int         obt_quit_req;

    agt_svr_ctx_t* svr_ctx_tbl[AGT_MAX_CONNS];
    int         reset_flag;
    int         sys_state;
} agt_ctx_t;


#endif
