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

/****h* IBMgtSim/Messages
 * NAME
 *  Messages
 *
 * DESCRIPTION
 *  Declaration of messages between the client and simulator
 *
 * Environment:
 *  Linux User Mode
 *
 * $Revision: 1.7 $
 *
 * AUTHOR
 *  Eitan Zahavi, Mellanox
 *
 *********/

#ifndef IBMGTSIM_CLIENT_IFC
#define IBMGTSIM_CLIENT_IFC


#ifdef __cplusplus
extern "C"
{
#endif


#include <iba/ib_types.h>
#include "ib_types_extend.h"

/****s* IBMgtSim: ClientIfc/ibms_cli_msg_type_t
* NAME
*  ibms_cli_msg_type_t
*
* DESCRIPTION
*  The types of messages going between the client and simulator
*
* SYNOPSIS
*/
typedef enum _ibms_cli_msg_type {
    IBMS_CLI_MSG_CONN,
    IBMS_CLI_MSG_DISCONN,
    IBMS_CLI_MSG_BIND,
    IBMS_CLI_MSG_MAD,
    IBMS_CLI_MSG_CAP,
    IBMS_CLI_MSG_QUIT,
    IBMS_CLI_MSG_UNKOWN
} ibms_cli_msg_type_t;
/*
* IBMS_CLI_MSG_CONN
*   Establish the connection to the server by providing local node
*   physical info like port number and port guid.
*
* IBMS_CLI_MSG_BIND
*   Allows for binding to a specific MAD type and port (ala TS filter).
*
* IBMS_CLI_MSG_CAP
*   Setting/Clearing of port capability mask.
*
* IBMS_CLI_MSG_MAD
*   A mad being sent either from or to the simulator.
*
* IBMS_CLI_MSG_QUIT
*   Ask the server to quit
*
*********/


/****s* IBMgtSim: ClientIfc/ibms_conn_msg_t
* NAME
*   ibms_conn_msg_t
*
* DESCRIPTION
*   Connecting to the server message
*
* SYNOPSIS
*/
#include <complib/cl_packon.h>
typedef struct _ib_conn_msg {
    uint8_t   port_num;
    uint64_t  port_guid;
    char      host[128];
    unsigned short int in_msg_port;
} PACK_SUFFIX ibms_conn_msg_t;
#include <complib/cl_packoff.h>
/*
* FIELDS
*  port_num
*     The IB port number the client attaches to.
*
*  port_guid
*     The guid of the port to connect to.
*
*  host
*     Client host name (null terminated string)
*
*  in_msg_port
*     Client incoming messages port
*
* SEE ALSO
*********/


/****s* IBMgtSim: ClientIfc/ibms_disconn_msg_t
* NAME
*   ibms_disconn_msg_t
*
* DESCRIPTION
*   Disconnecting from the server message
*
* SYNOPSIS
*/
#include <complib/cl_packon.h>
typedef struct _ib_disconn_msg {
    uint8_t   port_num;
    uint64_t  port_guid;
} PACK_SUFFIX ibms_disconn_msg_t;
#include <complib/cl_packoff.h>
/*
* FIELDS
*   port_num
*     The IB port number the client attached to.
*
*   port_guid
*     The guid of the port to disconnect from.
*
* SEE ALSO
*********/


/****s* IBMgtSim: ClientIfc/ibms_mad_addr_t
* NAME
*   ibms_mad_addr_t
*
* DESCRIPTION
*   MAD Address
*
* SYNOPSIS
*/
#include <complib/cl_packon.h>
typedef struct _ibms_mad_addr {
    uint8_t    sl;
    int        pkey_index;
    uint16_t   slid;
    uint16_t   dlid;
    uint32_t   sqpn;
    uint32_t   dqpn;
} PACK_SUFFIX ibms_mad_addr_t;
#include <complib/cl_packoff.h>
/*
* FIELDS
*   sl
*
*   pkey_index
*
*   slid
*
*   dlid
*
*   sqpn
*
*   dqpn
*
* SEE ALSO
*********/


#define MAD_HEADER_SIZE 24

/****s* IBMgtSim: ClientIfc/ibms_mad_msg_t
* NAME
*   ibms_mad_msg_t
*
* DESCRIPTION
*   MAD Message
*
* SYNOPSIS
*/
#include <complib/cl_packon.h>
typedef struct _ibms_mad_msg {
    ibms_mad_addr_t  addr;
    ib_mad_t         header;
    uint8_t          payload[MAD_BLOCK_SIZE - MAD_HEADER_SIZE];
} PACK_SUFFIX ibms_mad_msg_t;
#include <complib/cl_packoff.h>
/*
* FIELDS
*  mad_hdr
*     MAD header
*
*  payload
*	  MAD data
*
*  address
*     Where do we sent it
*
* SEE ALSO
*********/


#define IBMS_BIND_MASK_PORT  1
#define IBMS_BIND_MASK_QP    2
#define IBMS_BIND_MASK_CLASS 4
#define IBMS_BIND_MASK_METH  8
#define IBMS_BIND_MASK_ATTR  16
#define IBMS_BIND_MASK_INPUT 32

/****s* IBMgtSim: ClientIfc/ibms_bind_msg_t
* NAME
*   ibms_bind_msg_t
*
* DESCRIPTION
*   Binding Message
*
* SYNOPSIS
*/
#include <complib/cl_packon.h>
typedef struct _ibms_bind_msg {
    uint8_t      port;
    uint32_t     qpn;
    uint8_t      mgt_class;
    uint8_t      method;
    uint16_t     attribute;
    uint8_t      only_input;
    uint8_t      mask;
    } PACK_SUFFIX ibms_bind_msg_t;
#include <complib/cl_packoff.h>
/*
* FIELDS
* port
*
* qpn
*
* mgmt_class
*
* method
*
* attribute
*
* only_input
*
* mask
*
* SEE ALSO
*********/


/****s* IBMgtSim: ClientIfc/ibms_cap_msg_t
* NAME
*   ibms_cap_msg_t
*
* DESCRIPTION
*   Capability Mask Message = set the port capabilities.
*   we simulate the VAPI set port capability mask with this API.
*
* SEE ALSO:
*   ib_types IB_PORT_CAP_* for the list of capabilities
*/
#include <complib/cl_packon.h>
typedef struct _ibms_cap_msg {
    uint32_t     capabilities;
    uint32_t     mask;
} PACK_SUFFIX ibms_cap_msg_t;
#include <complib/cl_packoff.h>
/*
* FIELDS
* capabilities
*   each bit might be set or cleared to mark a capability exists or not.
*
* mask
*   each bit might be 1 or 0 to mark the scope of the operation
*
* SEE ALSO
*********/


/****s* IBMgtSim: ClientIfc/ibms_client_msg_t
* NAME
*  ibms_client_msg_t
*
* DESCRIPTION
*  A message passed between the client and simulator
*
* SYNOPSIS
*/
#include <complib/cl_packon.h>
typedef struct _ibms_client_msg {
    ibms_cli_msg_type_t msg_type;
    union _msg {
        ibms_mad_msg_t     mad;
        ibms_bind_msg_t    bind;
        ibms_cap_msg_t     cap;
        ibms_conn_msg_t    conn;
        ibms_disconn_msg_t disc;
    } PACK_SUFFIX msg;
} PACK_SUFFIX ibms_client_msg_t;
#include <complib/cl_packoff.h>
/*
* FIELDS
*
* msg_type
*
* mad_msg
*
* bind_info
*
* conn_info
*
*********/


/****s* IBMgtSim: ClientIfc/ibms_response_t
* NAME
*  ibms_response_t
*
* DESCRIPTION
*  A response message passed between the client and server
*
* SYNOPSIS
*/
#include <complib/cl_packon.h>
typedef struct _ibms_response {
    int status;
} PACK_SUFFIX ibms_response_t;
#include <complib/cl_packoff.h>
/*
* FIELDS
*
* status
*   the status of the message execution
*
*********/


#ifdef __cplusplus
}
#endif


#endif          /* IBMGTSIM_CLIENT_IFC */
