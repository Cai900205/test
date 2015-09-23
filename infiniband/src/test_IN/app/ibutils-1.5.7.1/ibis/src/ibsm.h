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
 * 	Definition of ibsm_t.
 *	This object represents the Subnet Management Packets Interface
 *	This object is part of the IBIS family of objects.
 *
 * Environment:
 * 	Linux User Mode
 *
 * $Revision: 1.2 $
 */

#ifndef _IBSM_H_
#define _IBSM_H_

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

typedef enum _ibsm_state
{
  IBSM_STATE_INIT,
  IBSM_STATE_READY,
  IBSM_STATE_BUSY,
} ibsm_state_t;

/****s* IBIS: ibsm/ibsm_t
* NAME  ibsm_t
*
*
* DESCRIPTION
*       ibsm structure.
*
* SYNOPSIS
*/
typedef struct _ibsm
{
  ibsm_state_t         state;
  osm_bind_handle_t    lid_route_bind;
  osm_bind_handle_t    dr_route_bind;
} ibsm_t;
/*
* FIELDS
*
*       state
*            The ibsm state: INIT, READ or BUSY
*
* SEE ALSO
*
*********/

/****s* IBIS: ibsm/ibsm_dr_path_t
* NAME  ibsm_dr_path_t
*
*
* DESCRIPTION
*       ibsm directed route structure.
*
* SYNOPSIS
*/
typedef struct _ibsm_dr_path {
  uint8_t count;
  uint8_t path[IB_SUBNET_PATH_HOPS_MAX];
} ibsm_dr_path_t;
/*
* FIELDS
*
*  path
*   The list of output ports to be used in the path going out (initial)
*
*  number of entries
*
* SEE ALSO
*
*********/

/****f* IBIS: ibsm/ibsm_construct
* NAME
*       ibsm_construct
*
* DESCRIPTION
*      Allocation of ibsm_t struct
*
* SYNOPSIS
*/

ibsm_t*
ibsm_construct(void);

/*
* PARAMETERS
*
*
* RETURN VALUE
*       Return a pointer to an ibsm struct. Null if fails to do so.
*
* NOTES
*       First step of the creation of ibsm_t
*
* SEE ALSO
*       ibsm_destroy ibsm_init
*********/

/****s* IBIS: ibsm/ibsm_destroy
* NAME
*       ibsm_destroy
*
* DESCRIPTION
*      release of ibsm_t struct
*
* SYNOPSIS
*/

void
ibsm_destroy(
  IN ibsm_t* const p_ibsm );

/*
* PARAMETERS
*       p_ibsm
*               A pointer to the ibsm_t struct that is about to be released
*
* RETURN VALUE
*
* NOTES
*       Final step of the releasing of ibsm_t
*
* SEE ALSO
*       ibsm_construct
*********/

/****f* IBIS: ibsm/ibsm_init
* NAME
*       ibsm_init
*
* DESCRIPTION
*      Initialization of an ibsm_t struct
*
* SYNOPSIS
*/
ib_api_status_t
ibsm_init(
  IN ibsm_t* const p_ibsm );

/*
* PARAMETERS
*       p_ibsm
*               A pointer to the ibsm_t struct that is about to be initialized
*
* RETURN VALUE
*       The status of the function.
*
* NOTES
*
* SEE ALSO
*       ibsm_construct
* *********/


/****f* IBIS: ibsm/ibsm_bind
* NAME
*       ibsm_bind
*
* DESCRIPTION
*      Binding the ibsm object to a lower level.
*
* SYNOPSIS
*/
ib_api_status_t
ibsm_bind(
  ibsm_t* p_ibsm );

/*
* PARAMETERS
*       p_ibsm
*               A pointer to the ibsm_t struct that is about to be binded
*
* RETURN VALUE
*       The status of the function.
*
* NOTES
*
* SEE ALSO
*       ibsm_construct
*********/

/****f* IBIS: ibsm/ibsm_send_mad_by_lid
* NAME
*     ibsm_send_mad_by_lid
*
* DESCRIPTION
*      Send a SMI mad to the given LID.
*
* SYNOPSIS
*   ibsm_send_mad_by_lid(p_ibsm, p_mad, lid, attr, mod, meth)
*   Note that all values are in host order.
*/
ib_api_status_t
ibsm_send_mad_by_lid(
  ibsm_t   *p_ibsm,
  uint8_t  *p_data,
  size_t    data_size,
  uint16_t  lid,
  uint16_t  attr,
  uint32_t  mod,
  uint16_t  meth);
/*
* PARAMETERS
*       p_ibsm
*               A pointer to the ibsm_t struct.
*
*       p_data
*               [in/out] A pointer to attribute data. Will be overwritten in case of response.
*
*       data_size
*               [in] The size of the attribute block
*
*       lid
*               The Destination lid of the MAD.
*
*       attr
*               The Attribute code
*
*       mod
*               Attribute modifier value
*
*       meth
*               The MAD method: Set/Get/Trap...
*
* RETURN VALUE
*       The status of the function or response status.
*
* NOTES
*
* SEE ALSO
*
*********/

/****f* IBIS: ibsm/ibsm_send_mad_by_dr
* NAME
*     ibsm_send_mad_by_dr
*
* DESCRIPTION
*      Send a SMI mad to the given a directed route
*
* SYNOPSIS
*   ibsm_send_mad_by_dr(p_ibsm, p_mad, dr[], attr, mod, meth)
*   Note that all values are in network order.
*/
ib_api_status_t
ibsm_send_mad_by_dr(
  ibsm_t   *p_ibsm,
  uint8_t  *p_data,
  size_t    data_size,
  ibsm_dr_path_t *dr,
  uint16_t  attr,
  uint32_t  mod,
  uint16_t  meth);
/*
* PARAMETERS
*       p_ibsm
*               A pointer to the ibsm_t struct.
*
*       p_data
*               [in/out] A pointer to attribute data. Will be overwritten in case of response.
*
*       data_size
*               [in] The size of the attribute block
*
*
*       dr
*               The directed route to the destination as an
*               array of bytes with last one is 0.
*
*       attr
*               The Attribute code
*
*       mod
*               Attribute modifier value
*
*       meth
*               The MAD method: Set/Get/Trap...
*
* RETURN VALUE
*       The status of the function or response status.
*
* NOTES
*
* SEE ALSO
*
*********/

#endif /* _IBSM_H_ */
