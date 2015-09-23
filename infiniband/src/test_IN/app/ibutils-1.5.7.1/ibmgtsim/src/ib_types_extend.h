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

#ifndef IB_TYPES_EXTEND
#define IB_TYPES_EXTEND

#include <complib/cl_packon.h>
typedef struct _ib_pm_counters {
  ib_mad_t mad_header;
  ib_net32_t reserved0[10];
  uint8_t reserved1;
  uint8_t port_select;
  ib_net16_t counter_select;
  ib_net16_t symbol_error_counter;
  uint8_t link_error_recovery_counter;
  uint8_t link_down_counter;
  ib_net16_t port_rcv_errors;
  ib_net16_t port_rcv_remote_physical_errors;
  ib_net16_t port_rcv_switch_relay_errors;
  ib_net16_t port_xmit_discard;
  uint8_t port_xmit_constraint_errors;
  uint8_t port_rcv_constraint_errors;
  uint8_t reserved2;
  uint8_t lli_errors_exc_buf_errors;
  ib_net16_t reserved3;
  ib_net16_t vl15_dropped;
  ib_net32_t port_xmit_data;
  ib_net32_t port_rcv_data;
  ib_net32_t port_xmit_pkts;
  ib_net32_t port_rcv_pkts;
  ib_net32_t reserved5[38];
} PACK_SUFFIX ib_pm_counters_t;
#include <complib/cl_packoff.h>

/****f* IBA Base: Types_extend/ib_node_info_set_local_port_num
* NAME
*	ib_node_info_set_local_port_num
*
* DESCRIPTION
*	Sets a the local port number In the NodeInfo attribute.
*
* SYNOPSIS
*/
static inline void
ib_node_info_set_local_port_num(
	IN      ib_node_info_t* 	p_ni,
    IN      uint8_t             inPort)
{
    p_ni->port_num_vendor_id = cl_hton32(((cl_ntoh32(p_ni->port_num_vendor_id) & 0x00ffffff) |
                                (inPort << 24)));
}
/*
* PARAMETERS
*	p_ni
*		[in] Pointer to a NodeInfo attribute.
*   inPort
*       [in] Port which SMA came on
*
* RETURN VALUES
*
* NOTES
*
* SEE ALSO
*	ib_node_info_t
*********/

/****f* IBA Base: Types_extend/ib_port_info_get_smsl
* NAME
*	ib_port_info_get_smsl
*
* DESCRIPTION
*	Returns the encoded value for the Master SM SL at this port.
*
* SYNOPSIS
*/
static inline uint8_t
ib_port_info_get_smsl(
	IN const ib_port_info_t* const p_pi )
{
	return( (uint8_t)(p_pi->mtu_smsl & 0x0F));
}
/*
* PARAMETERS
*	p_pi
*		[in] Pointer to a PortInfo attribute.
*
* RETURN VALUES
*	Returns the encoded value for the Master SM SL at this port.
*
* NOTES
*
* SEE ALSO
*********/

/****f* IBA Base: Types/ib_port_info_set_smsl
* NAME
*	ib_port_info_set_smsl
*
* DESCRIPTION
*	Sets the Master SM SL value in the PortInfo attribute.
*
* SYNOPSIS
*/
static inline void
ib_port_info_set_smsl(
	IN				ib_port_info_t* const		p_pi,
	IN		const	uint8_t						mSmSl )
{
	p_pi->mtu_smsl = (uint8_t)((p_pi->mtu_smsl & 0xF0) | mSmSl);
}
/*
* PARAMETERS
*	p_pi
*		[in] Pointer to a PortInfo attribute.
*
*	mSmSl
*		[in] Encoded Master SM SL value to set
*
* RETURN VALUES
*	None.
*
* NOTES
*
* SEE ALSO
*********/

/****f* IBA Base: Types_extend/ib_port_info_set_vl_cap
* NAME
*	ib_port_info_set_vl_cap
*
* DESCRIPTION
*	Sets the VL Capability of a port.
*
* SYNOPSIS
*/
static inline void
ib_port_info_set_vl_cap(
	IN  ib_port_info_t*     p_pi,
    IN	const	uint8_t 	vlCap )
{
    p_pi->vl_cap = (p_pi->vl_cap & 0x0F) | (vlCap << 4);
}
/*
* PARAMETERS
*	p_pi
*		[in] Pointer to a PortInfo attribute.
*
* RETURN VALUES
*	VL_CAP field
*
* NOTES
*
* SEE ALSO
*********/

/****s* IBA Base: Types extend /ib_mft_table_t
* NAME
*	ib_mft_table_t
*
* DESCRIPTION
*	IBA defined MFT table.
*
* SYNOPSIS
*/

#include <complib/cl_packon.h>
typedef struct _ib_mft_table
{
	ib_net16_t			mft_entry[IB_MCAST_BLOCK_SIZE];

}	PACK_SUFFIX ib_mft_table_t;
#include <complib/cl_packoff.h>
/************/
#endif
