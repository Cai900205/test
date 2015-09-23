/*
 * Copyright (c) 2004-2010 Mellanox Technologies LTD. All rights reserved.
 *
 * This software is available to you under a choice of one of two
 * licenses.  You may choose to be licensed under the terms of the GNU
 * General Public License (GPL) Version 2, available from the file
 * COPYING in the main directory of this source tree, or the
 * OpenIB.org BSD license below:
 *
 *     Redistribution and use in source and binary forms, with or
 *     without modification, are permitted provided that the following
 *     conditions are met:
 *
 *      - Redistributions of source code must retain the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer.
 *
 *      - Redistributions in binary form must reproduce the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer in the documentation and/or other materials
 *        provided with the distribution.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 */

#include <inttypes.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <stdio.h>
#include <unistd.h>
#include <string>
#include "simmsg.h"
#include <inttypes.h>


#ifndef PRIx64
# if __WORDSIZE == 64
#  define __PRI64_PREFIX   "l"
#  define __PRIPTR_PREFIX  "l"
# else
#  define __PRI64_PREFIX   "ll"
#  define __PRIPTR_PREFIX
# endif
# define PRIx64 __PRI64_PREFIX "x"
#endif


std::string
__ibms_dump_conn_msg(IN const ibms_client_msg_t *p_msg)
{
    char buff[32];
    char msg[512];
    strncpy(buff,p_msg->msg.conn.host,31);
    buff[31] = '\0';

    sprintf(msg,"MSG: CONN"
            " Port:%u Guid:0x%016" PRIx64 " Host:%s InPort:%u",
            p_msg->msg.conn.port_num,
            p_msg->msg.conn.port_guid,
            p_msg->msg.conn.host,
            p_msg->msg.conn.in_msg_port);
    return msg;
}


std::string
__ibms_dump_disconn_msg(IN const ibms_client_msg_t *p_msg)
{
    char buff[32];
    char msg[512];
    strncpy(buff,p_msg->msg.conn.host,31);
    buff[31] = '\0';

    sprintf(msg,"MSG: DISCONNECT "
            " Port:%u Guid:0x%016" PRIx64 "",
            p_msg->msg.disc.port_num,
            p_msg->msg.disc.port_guid);
    return msg;
}


std::string
__ibms_dump_cap_msg(IN const ibms_client_msg_t *p_msg)
{
    char msg[512];
    sprintf(msg, "MSG: CAP");
    return msg;
}


std::string
__ibms_dump_bind_msg(IN const ibms_client_msg_t *p_msg)
{
    char msg[512];
    sprintf(msg, "MSG: BIND");
    if (p_msg->msg.bind.mask & IBMS_BIND_MASK_PORT)
        sprintf(msg,"%s Port:%u ",msg, p_msg->msg.bind.port);
    if (p_msg->msg.bind.mask & IBMS_BIND_MASK_QP)
        sprintf(msg, "%s QP:%u ", msg, p_msg->msg.bind.qpn);
    if (p_msg->msg.bind.mask & IBMS_BIND_MASK_CLASS)
        sprintf(msg, "%s Class:0x%X ", msg, p_msg->msg.bind.mgt_class);
    if (p_msg->msg.bind.mask & IBMS_BIND_MASK_METH)
        sprintf(msg, "%s Method:0x%X ", msg, p_msg->msg.bind.method);
    if (p_msg->msg.bind.mask & IBMS_BIND_MASK_ATTR)
        sprintf(msg, "%s Attribute:0x%X ", msg, p_msg->msg.bind.attribute);
    if (p_msg->msg.bind.mask & IBMS_BIND_MASK_INPUT) {
        if (p_msg->msg.bind.only_input)
            sprintf(msg, "%s Direction:IN",msg);
        else
            sprintf(msg, "%s Direction:IN/OUT", msg);
    }
    return msg;
}


std::string
__ibms_dump_mad_msg(IN const ibms_client_msg_t *p_msg)
{
    char msg[1024];
    sprintf(msg,
            "MSG: MAD"
            " ADDRESS:"
            " SLID:0x%04X"
            " DLID:0x%04X"
            " SQPN:%u"
            " DQPN:%u"
            " PKEY:%u"
            " SL:%u"
            " CLASS:0x%02X"
            " METHOD:0x%02X"
            " STATUS:0x%04X"
            " TID:0x%016"PRIx64"",
            p_msg->msg.mad.addr.slid,
            p_msg->msg.mad.addr.dlid,
            p_msg->msg.mad.addr.sqpn,
            p_msg->msg.mad.addr.dqpn,
            p_msg->msg.mad.addr.pkey_index,
            p_msg->msg.mad.addr.sl,
            p_msg->msg.mad.header.mgmt_class,
            p_msg->msg.mad.header.method,
            p_msg->msg.mad.header.status,
            p_msg->msg.mad.header.trans_id);
    return msg;
}


std::string
ibms_get_msg_str(IN const ibms_client_msg_t *p_msg)
{
    std::string msgStr;

    switch (p_msg->msg_type) {
    case IBMS_CLI_MSG_CONN:
        msgStr = __ibms_dump_conn_msg(p_msg);
        break;
    case IBMS_CLI_MSG_DISCONN:
        msgStr = __ibms_dump_disconn_msg(p_msg);
        break;
    case IBMS_CLI_MSG_BIND:
        msgStr = __ibms_dump_bind_msg(p_msg);
        break;
    case IBMS_CLI_MSG_CAP:
        msgStr = __ibms_dump_cap_msg(p_msg);
        break;
    case IBMS_CLI_MSG_MAD:
        msgStr = __ibms_dump_mad_msg(p_msg);
        break;
    default:
        msgStr = std::string("MSG: UNDEFINED");
    }
    return msgStr;
}


void
ibms_dump_msg(IN const ibms_client_msg_t *p_msg)
{
    printf("%s", ibms_get_msg_str(p_msg).c_str());
}


const char *
ibms_get_resp_str(IN const ibms_response_t *p_response)
{
    if (p_response->status)
        return "REMOTE_ERROR";
    else
        return "SUCCESS";
}


std::string
ibms_get_mad_header_str(ib_mad_t madHeader)
{
    char msg[1024];

    sprintf(msg,
            "--------------------------------------------------------\n"
            "  base_ver                 0x%x\n"
            "  mgmt_class               0x%x\n"
            "  class_ver                0x%x\n"
            "  method                   0x%x\n"
            "  status                   0x%x\n"
            "  class_spec               0x%x\n"
            "  trans_id                 0x%016"PRIx64"\n"
            "  attr_id                  0x%x\n"
            "  attr_mod                 0x%x\n"
            "--------------------------------------------------------\n",
            madHeader.base_ver,
            madHeader.mgmt_class,
            madHeader.class_ver,
            madHeader.method,
            CL_NTOH16(madHeader.status),
            CL_NTOH16(madHeader.class_spec),
            CL_NTOH64(madHeader.trans_id),
            CL_NTOH16(madHeader.attr_id),
            CL_NTOH32(madHeader.attr_mod));
    return (std::string)msg;
};


std::string
ibms_get_node_info_str(ib_node_info_t*     pNodeInfo)
{
    char msg[1024];

    sprintf(msg,
            "--------------------------------------------------------\n"
            "  node_type                0x%x\n"
            "  num_ports                0x%x\n"
            "  local port number        0x%x\n"
            "--------------------------------------------------------\n",
            pNodeInfo->node_type,
            pNodeInfo->num_ports,
            ib_node_info_get_local_port_num(pNodeInfo));
    return (std::string)msg;
}


std::string
ibms_get_port_info_str(ib_port_info_t*     pPortInfo)
{
    char msg[1024];

    sprintf(msg,
            "--------------------------------------------------------\n"
            "  lid                      0x%x\n"
            "  Port State               0x%x\n"
            "  Port Phy State           0x%x\n"
            "  nMTU                     0x%x\n"
            "  VL Cap                   0x%x\n"
            "  LMC                      0x%x\n"
            "--------------------------------------------------------\n",
            CL_NTOH16(pPortInfo->base_lid),
            ib_port_info_get_port_state(pPortInfo),
            ib_port_info_get_port_phys_state(pPortInfo),
            ib_port_info_get_neighbor_mtu(pPortInfo),
            ib_port_info_get_vl_cap(pPortInfo),
            ib_port_info_get_lmc(pPortInfo)
            );
    return (std::string)msg;
};

