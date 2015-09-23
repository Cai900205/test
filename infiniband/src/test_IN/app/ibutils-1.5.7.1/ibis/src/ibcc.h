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

/*
 * Abstract:
 *	Definition of ibcc_t.
 *	This object represents the Congestion Control Packets Interface
 *	This object is part of the IBIS family of objects.
 *
 * Environment:
 * 	Linux User Mode
 *
 * $Revision: 1.0 $
 */

#ifndef _IBCC_H_
#define _IBCC_H_

#include <complib/cl_qmap.h>
#include <complib/cl_passivelock.h>
#include <complib/cl_debug.h>
#include <iba/ib_types.h>
#include <opensm/osm_madw.h>
#include <opensm/osm_log.h>
#include <opensm/osm_mad_pool.h>
#include <opensm/osm_msgdef.h>
#include "ibis_api.h"
#include "ibis.h"

typedef enum _ibcc_state
{
	IBCC_STATE_INIT,
	IBCC_STATE_READY,
	IBCC_STATE_BUSY,
} ibcc_state_t;

#define MAD_PAYLOAD_SIZE 256

#define IBCC_DEAFULT_KEY 0

/****s* IBIS: ibcc/ibcc_t
* NAME  ibcc_t
*
*
* DESCRIPTION
*	ibcc structure
*
* SYNOPSIS
*/
typedef struct _ibcc
{
	ibcc_state_t       state;
	osm_bind_handle_t  lid_route_bind;
} ibcc_t;
/*
* FIELDS
*
*	state
*		The ibcc state: INIT, READ or BUSY
*
*	lid_route_bind
*		The handle to bind with the lower level for lid routed packets
*
* SEE ALSO
*
*********/

/****f* IBIS: ibcc/ibcc_construct
* NAME
*	ibcc_construct
*
* DESCRIPTION
*	Allocation of ibcc_t struct
*
* SYNOPSIS
*/
ibcc_t*
ibcc_construct(void);
/*
* PARAMETERS
*
*
* RETURN VALUE
*	Return a pointer to an ibcc struct. Null if fails to do so.
*
* NOTES
*	First step of the creation of ibcc_t
*
* SEE ALSO
*	ibcc_destroy ibcc_init
*********/

/****s* IBIS: ibcc/ibcc_destroy
* NAME
*	ibcc_destroy
*
* DESCRIPTION
*	release of ibcc_t struct
*
* SYNOPSIS
*/
void
ibcc_destroy(
	IN ibcc_t* const p_ibcc);
/*
* PARAMETERS
*	p_ibcc
*		A pointer to the ibcc_t struct that is about to be released
*
* RETURN VALUE
*
* NOTES
*	Final step of the releasing of ibcc_t
*
* SEE ALSO
*	ibcc_construct
*********/

/****f* IBIS: ibcc/ibcc_init
* NAME
*	ibcc_init
*
* DESCRIPTION
*	Initialization of an ibcc_t struct
*
* SYNOPSIS
*/
ib_api_status_t
ibcc_init(
	IN ibcc_t* const p_ibcc);
/*
* PARAMETERS
*	p_ibcc
*		A pointer to the ibcc_t struct that is about to be initialized
*
* RETURN VALUE
*	The status of the function.
*
* NOTES
*
* SEE ALSO
*	ibcc_construct
* *********/


/****f* IBIS: ibcc/ibcc_bind
* NAME
*	ibcc_bind
*
* DESCRIPTION
*	Binding the ibcc object to a lower level.
*
* SYNOPSIS
*/
ib_api_status_t
ibcc_bind(
	IN ibcc_t* const p_ibcc);
/*
* PARAMETERS
*	p_ibcc
*		A pointer to the ibcc_t struct that is about to be binded
*
* RETURN VALUE
*	The status of the function.
*
* NOTES
*
* SEE ALSO
*	ibcc_construct
*********/

/****f* IBIS: ibcc/ibcc_send_mad_by_lid
* NAME
*	ibcc_send_mad_by_lid
*
* DESCRIPTION
*	Send a CC mad to the given LID.
*
* SYNOPSIS
*	ibcc_send_mad_by_lid(p_ibcc, p_mad, lid, attr, mod, meth)
*	Note that all values are in host order.
*/
ib_api_status_t
ibcc_send_mad_by_lid (
	ibcc_t   *p_ibcc,
	uint64_t  cc_key,
	uint8_t  *cc_log_data,
	size_t    cc_log_data_size,
	uint8_t  *cc_mgt_data,
	size_t    cc_mgt_data_size,
	uint16_t  lid,
	uint16_t  attribute_id,
	uint32_t  attribute_mod,
	uint16_t  method);
/*
* PARAMETERS
*	p_ibcc
*		A pointer to the ibcc_t struct
*
*	cc_key
*		Congestion Control key
*
*	cc_log_data
*		[in/out] A pointer to CC log data.
*		Will be overwritten in case of response.
*
*	cc_log_data_size
*		[in] The size of the log data block
*
*	cc_mgt_data
*		[in/out] A pointer to CC management data.
*		Will be overwritten in case of response.
*
*	cc_mgt_data_size
*		[in] The size of the mgt data block
*
*	lid
*		The Destination lid of the MAD
*
*	attribute_id
*		The Attribute ID
*
*	attribute_mod
*		Attribute modifier value
*
*	method
*		The MAD method: Set/Get/Trap...
*
* RETURN VALUE
*	The status of the function or response status.
*
* NOTES
*
* SEE ALSO
*
*********/

/****s* IBA Base: Types/ibcc_notice_attr_t
* NAME
*	ibcc_notice_attr_t
*
* DESCRIPTION
*	IBA defined Notice attribute (13.4.8) defines
*	many types of notices, so it has many unions.
*	Instead of dealing with the union in SWIG, the
*	following struct is defined to deal only with
*	CC notice.
*	For more details, please see ib_mad_notice_attr_t
*	definition in ib_types.h
*
* SYNOPSIS
*/
#include <complib/cl_packon.h>
typedef struct _ibcc_notice
{
	uint8_t    generic_type;

	uint8_t    generic__prod_type_msb;
	ib_net16_t generic__prod_type_lsb;
	ib_net16_t generic__trap_num;

	ib_net16_t issuer_lid;
	ib_net16_t toggle_count;

	ib_net16_t ntc0__source_lid;   // Source LID from offending packet LRH
	uint8_t    ntc0__method;       // Method, from common MAD header
	uint8_t    ntc0__resv0;
	ib_net16_t ntc0__attr_id;      // Attribute ID, from common MAD header
	ib_net16_t ntc0__resv1;
	ib_net32_t ntc0__attr_mod;     // Attribute Modif, from common MAD header
	ib_net32_t ntc0__qp;           // 8b pad, 24b dest QP from BTH
	ib_net64_t ntc0__cc_key;       // CC key of the offending packet
	ib_gid_t   ntc0__source_gid;   // GID from GRH of the offending packet
	uint8_t    ntc0__padding[14];  // Padding - ignored on read

	ib_gid_t      issuer_gid;
} PACK_SUFFIX ibcc_notice_attr_t;
#include <complib/cl_packoff.h>
/*********/

/****s* IBA Base: Types/ibcc_ca_cong_log_t
* NAME
*	ibcc_ca_cong_log_t
*
* DESCRIPTION
*	IBA defined CongestionLog attribute (A10.4.3.5)
*	has a union that includes Congestion Log for
*	switches and CAs.
*	Instead of dealing with the union in SWIG, the
*	following struct is defined to deal only with
*	CA congestion log.
*	For more details, please see ib_cong_log_t
*	definition in ib_types.h
*
* SYNOPSIS
*/
#include <complib/cl_packon.h>
typedef struct _ib_ca_cong_log {
	uint8_t log_type;
	uint8_t cong_flags;
	ib_net16_t event_counter;
	ib_net16_t event_map;
	ib_net16_t resv;
	ib_net32_t time_stamp;
	ib_cong_log_event_ca_t log_event[13];
} PACK_SUFFIX ibcc_ca_cong_log_t;
#include <complib/cl_packoff.h>
/*********/

/****s* IBA Base: Types/ibcc_sw_cong_log_t
* NAME
*	ibcc_sw_cong_log_t
*
* DESCRIPTION
*	IBA defined CongestionLog attribute (A10.4.3.5)
*	has a union that includes Congestion Log for
*	switches and CAs.
*	Instead of dealing with the union in SWIG, the
*	following struct is defined to deal only with
*	switch congestion log.
*	For more details, please see ib_cong_log_t
*	definition in ib_types.h
*
* SYNOPSIS
*/
#include <complib/cl_packon.h>
typedef struct _ib_sw_cong_log {
	uint8_t log_type;
	uint8_t cong_flags;
	ib_net16_t event_counter;
	ib_net32_t time_stamp;
	uint8_t port_map[32];
	ib_cong_log_event_sw_t entry_list[15];
} PACK_SUFFIX ibcc_sw_cong_log_t;
#include <complib/cl_packoff.h>
/*********/

#endif /* _IBCC_H_ */
