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
 * 	Implementation of ibpm_t.
 *	This object represents the Subnet Performance Monitor object.
 *	This object is part of the IBIS family of objects.
 *
 * Environment:
 * 	Linux User Mode
 *
 * $Revision: 1.3 $
 */

#ifndef _IBPM_H_
#define _IBPM_H_

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
#include "ibpm_base.h"


/****s* IBIS: ibpm/ibpm_t
 * NAME  ibpm_t
 *
 *
 * DESCRIPTION
 *       ibpm structure.
 *
 * SYNOPSIS
 */
typedef struct _ibpm
{
  ibpm_state_t         state;
  osm_bind_handle_t    h_bind;
} ibpm_t;
/*
 * FIELDS
 *
 *       state
 *            The ibpm state: INIT, READ or BUSY
 *
 * SEE ALSO
 *
 *********/


/****f* IBIS: ibpm/ibpm_construct
 * NAME
 *       ibpm_construct
 *
 * DESCRIPTION
 *      Allocation of ibpm_t struct
 *
 * SYNOPSIS
 */

ibpm_t*
ibpm_construct(void);

/*
 * PARAMETERS
 *
 *
 * RETURN VALUE
 *       Return a pointer to an ibpm struct. Null if fails to do so.
 *
 * NOTES
 *       First step of the creation of ibpm_t
 *
 * SEE ALSO
 *       ibpm_destroy ibpm_init
 *********/

/****s* IBIS: ibpm/ibpm_destroy
 * NAME
 *       ibpm_destroy
 *
 * DESCRIPTION
 *      release of ibpm_t struct
 *
 * SYNOPSIS
 */

void
ibpm_destroy(
  IN ibpm_t* const p_ibpm );

/*
 * PARAMETERS
 *       p_ibpm
 *               A pointer to the ibpm_t struct that is about to be released
 *
 * RETURN VALUE
 *
 * NOTES
 *       Final step of the releasing of ibpm_t
 *
 * SEE ALSO
 *       ibpm_construct
 *********/

/****f* IBIS: ibpm/ibpm_init
 * NAME
 *       ibpm_init
 *
 * DESCRIPTION
 *      Initialization of an ibpm_t struct
 *
 * SYNOPSIS
 */
ib_api_status_t
ibpm_init(
  IN ibpm_t* const p_ibpm );

/*
 * PARAMETERS
 *       p_ibpm
 *               A pointer to the ibpm_t struct that is about to be initialized
 *
 * RETURN VALUE
 *       The status of the function.
 *
 * NOTES
 *
 * SEE ALSO
 *       ibpm_construct
 * *********/


/****f* IBIS: ibpm/ibpm_bind
 * NAME
 *       ibpm_bind
 *
 * DESCRIPTION
 *      Binding the ibpm object to a lower level.
 *
 * SYNOPSIS
 */
ib_api_status_t
ibpm_bind(
  IN ibpm_t* const p_ibpm );

/*
 * PARAMETERS
 *       p_ibpm
 *               A pointer to the ibpm_t struct that is about to be binded
 *
 * RETURN VALUE
 *       The status of the function.
 *
 * NOTES
 *
 * SEE ALSO
 *       ibpm_construct
 *********/

/****f* IBIS: ibpm/ibpm_get_counters
 * NAME
 *     ibpm_get_counters
 *
 * DESCRIPTION
 *      Send a PM MAD (port counters) and wait for the reply.
 *
 * SYNOPSIS
 */
ib_api_status_t
ibpm_get_counters(
  IN ibpm_t* const p_ibpm,
  IN uint16_t lid,
  IN uint8_t port_select,
  OUT ib_pm_port_counter_t *p_pm_port_counter_mad);

/*
 * PARAMETERS
 *       p_ibpm
 *               A pointer to the ibpm_t struct.
 *
 *       lid
 *               The Destination lid of the MAD.
 *
 *       port_select
 *               The Destination port of the MAD.
 *
 *       p_pm_port_counter_mad
 *               A pointer to a pm port counter MAD that was received.
 *
 * RETURN VALUE
 *       The status of the function.
 *
 * NOTES
 *
 * SEE ALSO
 *       ibpm_clr_all_counters
 *********/


/****f* IBIS: ibpm/ibpm_get_multi_counters
 * NAME
 *     ibpm_get_multi_counters
 *
 * DESCRIPTION
 *      Sends a number of PM MADs (port counters) and wait for the reply.
 *
 * SYNOPSIS
 */
ib_api_status_t
ibpm_get_multi_counters(
  IN ibpm_t* const p_ibpm,
  IN uint8_t num,
  IN uint16_t lid_list[],
  IN uint8_t port_select_list[],
  OUT ib_pm_port_counter_t *p_pm_port_counter_mad_list);

/*
 * PARAMETERS
 *       p_ibpm
 *               A pointer to the ibpm_t struct.
 *
 *       num
 *              The number of requested MADs.
 *
 *       lid_list
 *               List of destination lids of the MADs.
 *
 *       port_select_list
 *               List of destination ports of the MADs.
 *
 *       p_pm_port_counter_mad_list
 *               A pointer to a list of pm port counter MADs that was received.
 *
 * RETURN VALUE
 *       The status of the function.
 *
 * NOTES
 *
 * SEE ALSO
 *       ibpm_get_counters
 *********/

/****f* IBIS: ibpm/ibpm_get_multi_counters_extended
 * NAME
 *     ibpm_get_multi_counters_extended
 *
 * DESCRIPTION
 *      Sends a number of PM MADs (port counters) and wait for the reply.
 *
 * SYNOPSIS
 */
ib_api_status_t
ibpm_get_multi_counters_extended(
  IN ibpm_t* const p_ibpm,
  IN uint8_t num,
  IN uint16_t lid_list[],
  IN uint8_t port_select_list[],
  OUT ib_pm_port_counter_extended_t *p_pm_port_counter_mad_list);

/*
 * PARAMETERS
 *       p_ibpm
 *               A pointer to the ibpm_t struct.
 *
 *       num
 *              The number of requested MADs.
 *
 *       lid_list
 *               List of destination lids of the MADs.
 *
 *       port_select_list
 *               List of destination ports of the MADs.
 *
 *       p_pm_port_counter_mad_list
 *               A pointer to a list of pm port counter MADs that was received.
 *
 * RETURN VALUE
 *       The status of the function.
 *
 * NOTES
 *
 * SEE ALSO
 *       ibpm_get_counters
 *********/

/****f* IBIS: ibpm/ibpm_clr_all_counters
 * NAME
 *     ibpm_clr_all_counters
 *
 * DESCRIPTION
 *      Send a PM MAD (port counters) that resets all the counters..
 *
 * SYNOPSIS
 */
ib_api_status_t
ibpm_clr_all_counters(
  IN ibpm_t* const p_ibpm,
  IN uint16_t lid,
  IN uint8_t port_select);

/*
 * PARAMETERS
 *       p_ibpm
 *               A pointer to the ibpm_t struct.
 *
 *       lid
 *               The Destination lid of the MAD.
 *
 *       port_select
 *               The Destination port of the MAD.
 *

 * RETURN VALUE
 *       The status of the function.
 *
 * NOTES
 *
 * SEE ALSO
 *       ibpm_get_counters
 *********/

/****f* IBIS: ibpm/ibpm_clr_all_multi_counters
 * NAME
 *     ibpm_clr_all_multi_counters
 *
 * DESCRIPTION
 *      Send a number of PM MADs (port counters) that resets all the counters..
 *
 * SYNOPSIS
 */
ib_api_status_t
ibpm_clr_all_multi_counters(
  IN ibpm_t* const p_ibpm,
  IN uint8_t num,
  IN uint16_t lid_list[],
  IN uint8_t port_select_list[]);

/*
 * PARAMETERS
 *       p_ibpm
 *               A pointer to the ibpm_t struct.
 *
 *       num
 *              The number of requested MADs.
 *
 *       lid_list
 *               List of destination lids of the MADs.
 *
 *       port_select_list
 *               List of destination ports of the MADs.
 *
 *
 * RETURN VALUE
 *       The status of the function.
 *
 * NOTES
 *
 * SEE ALSO
 *       ibpm_get_multi_counters
 *********/
#endif /* _IBPM_H_ */
