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
 * 	Implementation of ibcr_t.
 *	This object represents the Subnet Performance Monitor object.
 *	This object is part of the IBIS family of objects.
 *
 * Environment:
 * 	Linux User Mode
 *
 * $Revision: 1.2 $
 */

#ifndef _IBCR_H_
#define _IBCR_H_

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
#include "ibcr_base.h"


/****s* IBIS: ibcr/ibcr_t
* NAME  ibcr_t
*
*
* DESCRIPTION
*       ibcr structure.
*
* SYNOPSIS
*/


typedef struct _ibcr
{
  ibcr_state_t       state;
  osm_bind_handle_t  h_bind;
} ibcr_t;


/*
* FIELDS
*
*       state
*            The ibcr condition state.
*
*       h_bind
*            The handle to bind with the lower level.
*
* SEE ALSO
*
*********/


/****f* IBIS: ibcr/ibcr_construct
* NAME
*       ibcr_construct
*
* DESCRIPTION
*      Allocation of ibcr_t struct
*
* SYNOPSIS
*/

ibcr_t*
ibcr_construct(void);

/*
* PARAMETERS
*
*
* RETURN VALUE
*       Return a pointer to an ibcr struct. Null if fails to do so.
*
* NOTES
*       First step of the creation of ibcr_t
*
* SEE ALSO
*       ibcr_destroy ibcr_init
*********/

/****s* IBIS: ibcr/ibcr_destroy
* NAME
*       ibcr_destroy
*
* DESCRIPTION
*      release of ibcr_t struct
*
* SYNOPSIS
*/

void
ibcr_destroy(
  IN ibcr_t* const p_ibcr );

/*
* PARAMETERS
*       p_ibcr
*               A pointer to the ibcr_t struct that is about to be released
*
* RETURN VALUE
*
* NOTES
*       Final step of the releasing of ibcr_t
*
* SEE ALSO
*       ibcr_construct
*********/

/****f* IBIS: ibcr/ibcr_init
* NAME
*       ibcr_init
*
* DESCRIPTION
*      Initialization of an ibcr_t struct
*
* SYNOPSIS
*/
ib_api_status_t
ibcr_init(
  IN ibcr_t* const p_ibcr );

/*
* PARAMETERS
*       p_ibcr
*               A pointer to the ibcr_t struct that is about to be initialized
*
* RETURN VALUE
*       The status of the function.
*
* NOTES
*
* SEE ALSO
*       ibcr_construct
* *********/


/****f* IBIS: ibcr/ibcr_bind
* NAME
*       ibcr_bind
*
* DESCRIPTION
*      Binding the ibcr object to a lower level.
*
* SYNOPSIS
*/
ib_api_status_t
ibcr_bind(
  IN ibcr_t* const p_ibcr );

/*
* PARAMETERS
*       p_ibcr
*               A pointer to the ibcr_t struct that is about to be binded
*
* RETURN VALUE
*       The status of the function.
*
* NOTES
*
* SEE ALSO
*       ibcr_construct
*********/

/****f* IBIS: ibcr/ibcr_read
* NAME
*     ibcr_read
*
* DESCRIPTION
*      Send a CR MAD  and wait for the reply.
*
* SYNOPSIS
*/
ib_api_status_t
ibcr_read(
  IN ibcr_t* const p_ibcr,
  IN uint16_t lid,
  IN uint32_t address,
  OUT ib_cr_space_t *p_cr_space_mad);

/*
* PARAMETERS
*       p_ibcr
*               A pointer to the ibcr_t struct.
*
*       lid
*               The Destination lid of the MAD.
*
*       address
*               The cr-space address in which to read from.
*
*       p_cr_space_mad
*               A pointer to a cr space MAD that was received.
*
* RETURN VALUE
*       The status of the function.
*
* NOTES
*
* SEE ALSO
*       ibcr_write
*********/

/****f* IBIS: ibcr/ibcr_write
* NAME
*     ibcr_write
*
* DESCRIPTION
*      Send a CR MAD  that writes to a cr-space address.
*
* SYNOPSIS
*/
ib_api_status_t
ibcr_write(
  IN ibcr_t* const p_ibcr,
  IN uint16_t lid,
  IN uint32_t data,
  IN uint32_t address);

/*
* PARAMETERS
*       p_ibcr
*               A pointer to the ibcr_t struct.
*
*       lid
*               The Destination lid of the MAD.
*
*       data
*               The cr-space data.
*
*       address
*               The cr-space address in which to write to.
*

* RETURN VALUE
*       The status of the function.
*
* NOTES
*
* SEE ALSO
*       ibcr_read
*********/

/****f* IBIS: ibcr/ibcr_multi_read
* NAME
*     ibcr_multi_read
*
* DESCRIPTION
*      Send a CR MAD  and wait for the reply.
*
* SYNOPSIS
*/
ib_api_status_t
ibcr_multi_read(
  IN ibcr_t* const p_ibcr,
  IN uint8_t num,
  IN uint16_t lid_list[],
  IN uint32_t address,
  OUT ib_cr_space_t *p_cr_space_mad_list);

/*
* PARAMETERS
*       p_ibcr
*               A pointer to the ibcr_t struct.
*
*       num
*               number of MAD to be send.
*
*       lid_list
*               The list of Destination lid of the MAD.
*
*       address
*               The cr-space address in which to read from.
*
*       p_cr_space_mad
*               A pointer to a cr space MAD that was received.
*
* RETURN VALUE
*       The status of the function.
*
* NOTES
*
* SEE ALSO
*       ibcr_multi_write
*********/

/****f* IBIS: ibcr/ibcr_multi_write
* NAME
*     ibcr_multi_write
*
* DESCRIPTION
*      Send a CR MAD  that writes to a cr-space address.
*
* SYNOPSIS
*/
ib_api_status_t
ibcr_multi_write(
  IN ibcr_t* const p_ibcr,
  IN uint8_t num,
  IN uint16_t lid_list[],
  IN uint32_t data,
  IN uint32_t address);

/*
* PARAMETERS
*       p_ibcr
*               A pointer to the ibcr_t struct.
*
*       num
*               number of MAD to be send.
*
*       lid_list
*               The list of Destination lid of the MAD.
*
*       data
*               The cr-space data.
*
*       address
*               The cr-space address in which to write to.
*

* RETURN VALUE
*       The status of the function.
*
* NOTES
*
* SEE ALSO
*       ibcr_multi_read
*********/

#endif /* _IBCR_H_ */
