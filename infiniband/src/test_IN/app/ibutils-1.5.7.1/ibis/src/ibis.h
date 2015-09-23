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
 * 	Declaration of ibis_t.
 *	This object represents the IBIS Platform object.
 *
 * Environment:
 * 	Linux User Mode
 *
 * $Revision: 1.4 $
 */

#ifndef _IBIS_H_
#define _IBIS_H_

#include <opensm/osm_log.h>
#include <opensm/osm_mad_pool.h>
#include <opensm/osm_helper.h>
#include "ibis_api.h"
#include "ibis_gsi_mad_ctrl.h"

#ifdef OSM_BUILD_OPENIB
#include <vendor/osm_vendor_api.h>
#else
#include <opensm/osm_vendor_api.h>
#endif

/****h* IBIS/IBIS
 * NAME
 *	IBIS
 *
 * DESCRIPTION
 *	The IBIS object provides a simplified API to a vendor specific MAD
 *  processing and SA query facilities.
 *
 * AUTHOR
 *	Eitan Zahavi, Mellanox
 *
 *********/

/****f* IBIS/ibis_construct
* NAME
*	ibis_construct
*
* DESCRIPTION
*	This function constructs an IBIS object.
*
* SYNOPSIS
*/
void ibis_construct(void);

/*
* PARAMETERS
*
* RETURN VALUE
*	This function does not return a value.
*
* NOTES
*	Allows calling ibis_init, ibis_destroy.
*
*	Calling ibis_construct is a prerequisite to calling any other
*	method except ibis_init.
*
* SEE ALSO
*	IBIS object, ibis_init, ibis_destroy
*********/

/****f* IBIS/ibis_destroy
* NAME
*	ibis_destroy
*
* DESCRIPTION
*	The ibis_destroy function destroys an ibis object, releasing
*	all resources.
*
* SYNOPSIS
*/
void
ibis_destroy(void);
/*
* PARAMETERS
*
* RETURN VALUE
*	This function does not return a value.
*
* NOTES
*	Performs any necessary cleanup of the specified IBIS object.
*	Further operations should not be attempted on the destroyed object.
*	This function should only be called after a call to ibis_construct or
*	ibis_init.
*
* SEE ALSO
*	IBIS object, ibis_construct, ibis_init
*********/

/****f* IBIS/ibis_init
* NAME
*	ibis_init
*
* DESCRIPTION
*	The ibis_init function initializes a IBIS object for use.
*
* SYNOPSIS
*/
ib_api_status_t
ibis_init(
  IN ibis_opt_t *p_opt,
  IN const osm_log_level_t log_flags
  );

/*
* PARAMETERS
*
*	p_opt
*		[in] Pointer to the options structure.
*
*	log_flags
*		[in] Log level flags to set.
*
* RETURN VALUES
*	IB_SUCCESS if the IBIS object was initialized successfully.
*
* NOTES
*	Allows calling other IBIS methods.
*
* SEE ALSO
*	IBIS object, ibis_construct, ibis_destroy
*********/

/****f* IBIS/ibis_get_mad_status_str
* NAME
*	ibis_get_mad_status_str
*
* DESCRIPTION
*	return the string representing the given mad status
*
* SYNOPSIS
*/
const char *
ibis_get_mad_status_str( IN const ib_mad_t * const p_mad );
/*
* PARAMETERS
*	p_mad
*		[in] Pointer to the mad payload
*
* RETURN VALUES
*	NONE
*
* NOTES
*
* SEE ALSO
*********/

/****f* IBIS/ibis_get_tid
* NAME
*	ibis_get_tid
*
* DESCRIPTION
*	return a transaction ID for sending MAD
*
* SYNOPSIS
*/
ib_net64_t
ibis_get_tid(void);
/*
* PARAMETERS
*
* RETURN VALUES
*	a transaction ID not used by previous mads.
*
* NOTES
*
* SEE ALSO
*********/

#endif /* _IBIS_H_ */
