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
 * 	Implementation of ibbbm_t.
 *	This object represents the Subnet Performance Monitor object.
 *	This object is part of the IBIS family of objects.
 *
 * Environment:
 * 	Linux User Mode
 *
 * $Revision: 1.2 $
 */

#ifndef _IBBBM_H_
#define _IBBBM_H_

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
#include "ibbbm_base.h"


/****s* IBIS: ibbbm/ibbbm_t
* NAME  ibbbm_t
*
*
* DESCRIPTION
*       ibbbm structure.
*
* SYNOPSIS
*/


typedef struct _ibbbm
{
  ibbbm_state_t      state;
  atomic32_t         bm_sequence;
  osm_bind_handle_t  h_bind;
  cl_event_t         wait_for_resp;
} ibbbm_t;


/*
* FIELDS
*
*       state
*            The ibbbm condition state.
*
*       h_bind
*            The handle to bind with the lower level.
*
*      wait_for_resp
*            An event to signal the return of a MAD.
*
* SEE ALSO
*
*********/


/****f* IBIS: ibbbm/ibbbm_construct
* NAME
*       ibbbm_construct
*
* DESCRIPTION
*      Allocation of ibbbm_t struct
*
* SYNOPSIS
*/

ibbbm_t*
ibbbm_construct(void);

/*
* PARAMETERS
*
*
* RETURN VALUE
*       Return a pointer to an ibbbm struct. Null if fails to do so.
*
* NOTES
*       First step of the creation of ibbbm_t
*
* SEE ALSO
*       ibbbm_destroy ibbbm_init
*********/

/****s* IBIS: ibbbm/ibbbm_destroy
* NAME
*       ibbbm_destroy
*
* DESCRIPTION
*      release of ibbbm_t struct
*
* SYNOPSIS
*/

void
ibbbm_destroy(
  IN ibbbm_t* const p_ibbbm );

/*
* PARAMETERS
*       p_ibbbm
*               A pointer to the ibbbm_t struct that is joining to be released
*
* RETURN VALUE
*
* NOTES
*       Final step of the releasing of ibbbm_t
*
* SEE ALSO
*       ibbbm_construct
*********/

/****f* IBIS: ibbbm/ibbbm_init
* NAME
*       ibbbm_init
*
* DESCRIPTION
*      Initialization of an ibbbm_t struct
*
* SYNOPSIS
*/
ib_api_status_t
ibbbm_init(
  IN ibbbm_t* const p_ibbbm );

/*
* PARAMETERS
*       p_ibbbm
*               A pointer to the ibbbm_t struct that is joining to be initialized
*
* RETURN VALUE
*       The status of the function.
*
* NOTES
*
* SEE ALSO
*       ibbbm_construct
* *********/


/****f* IBIS: ibbbm/ibbbm_bind
* NAME
*       ibbbm_bind
*
* DESCRIPTION
*      Binding the ibbbm object to a lower level.
*
* SYNOPSIS
*/
ib_api_status_t
ibbbm_bind(
  IN ibbbm_t* const p_ibbbm );

/*
* PARAMETERS
*       p_ibbbm
*               A pointer to the ibbbm_t struct that is joining to be binded
*
* RETURN VALUE
*       The status of the function.
*
* NOTES
*
* SEE ALSO
*       ibbbm_construct
*********/

/****f* IBIS: ibbbm/ibbbm_get_counters
* NAME
*     ibbbm_get_counters
*
* DESCRIPTION
*      Send a BBM MAD (port counters) and wait for the reply.
*
* SYNOPSIS
*/
ib_api_status_t
ibbbm_read_vpd(
  IN ibbbm_t* const p_ibbbm,
  IN uint16_t lid,
  IN uint8_t vpd_device_selector,
  IN uint16_t bytes_num,
  IN uint16_t offset,
  OUT ib_bbm_vpd_t *p_bbm_vpd_mad);

/*
* PARAMETERS
*       p_ibbbm
*               A pointer to the ibbbm_t struct.
*
*       lid
*               The Destination lid of the MAD.
*
*       p_bbm_vpd_mad
*               A pointer to a Baseboard Management MAD that was received.
*
* RETURN VALUE
*       The status of the function.
*
* NOTES
*
* SEE ALSO
*       ibbbm_write
*********/

/****f* IBIS: ibbbm/ibbbm_write
* NAME
*     ibbbm_write
*
* DESCRIPTION
*      Send a BBM MAD  that writes to a vpd address.
*
* SYNOPSIS
*/
ib_api_status_t
ibbbm_write_vpd(
  IN ibbbm_t* const p_ibbbm,
  IN uint16_t lid,
  IN uint8_t vpd_device_selector,
  IN uint16_t bytes_num,
  IN uint16_t offset,
  IN uint8_t *p_data);

#endif /* _IBBBM_H_ */
/*
* PARAMETERS
*       p_ibbbm
*               A pointer to the ibbbm_t struct.
*
*       lid
*               The Destination lid of the MAD.
*
*       p_bbm_vpd_mad
*               A pointer to a Baseboard Management MAD that will be sent.
*
* RETURN VALUE
*       The status of the function.
*
* NOTES
*
* SEE ALSO
*       ibbb_read
*********/
