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

/****h* IBMgtSim/ClientInterface
 * NAME
 *  IBIS
 *
 * DESCRIPTION
 *  Declaration of C API for clients
 *
 * Environment:
 *  Linux User Mode
 *
 * $Revision: 1.4 $
 *
 * AUTHOR
 *  Eitan Zahavi, Mellanox
 *
 *********/

#ifndef IBMGTSIM_CLIENT_API
#define IBMGTSIM_CLIENT_API


#include "simmsg.h"


#ifdef __cplusplus
extern "C"
{
#endif


/****f* IBMS Client API/ibms_pfn_receive_cb_t
* NAME
*   ibms_pfn_receive_cb_t
*
* DESCRIPTION
*   User-defined callback invoked on receiving new messages
*
* SYNOPSIS
*/
typedef void
(*ibms_pfn_receive_cb_t) (
  IN void* p_ctx,
  IN ibms_mad_msg_t *p_mad);
/*
* PARAMETERS
*   p_ctx
*     [in] The context provided during the client registration by ibms_connect
*
*  p_mad
*     [in] Pointer to the incoming mad message
*
*****/


/****f* IBMS Client API/ibms_conn_handle_t
* NAME
*	ibms_conn_handle_t
*
* DESCRIPTION
*	Abstract handle to connection objects
*
* SYNOPSIS
*/
typedef void * ibms_conn_handle_t;
/*
*
*****/


/* connect to the server to the port guid. Registering incoming messages callbacks */
ibms_conn_handle_t
ibms_connect(uint64_t portGuid,
        ibms_pfn_receive_cb_t receiveCb,
        void* context);

/* bind to a specific mad messages */
int
ibms_bind(ibms_conn_handle_t conHdl,
        ibms_bind_msg_t *pBindMsg);

/* set port capabilities */
int
ibms_set_cap(ibms_conn_handle_t conHdl,
        ibms_cap_msg_t *pCapMsg);

/* send a message to the simulator */
int
ibms_send(ibms_conn_handle_t conHdl,
        ibms_mad_msg_t *pMadMsg);

/* disconnect from the simulator */
int
ibms_disconnect(ibms_conn_handle_t conHdl);


#ifdef __cplusplus
/* extern "C" */
}
#endif


#endif /* IBMGTSIM_CLIENT_API */

