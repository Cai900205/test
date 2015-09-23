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
 * 	Declaration of ibis_gsi_mad_ctrl_t.
 *
 * Environment:
 * 	Linux User Mode
 *
 * $Revision: 1.3 $
 */


#ifndef _IBIS_GSI_MAD_CTRL_H_
#define _IBIS_GSI_MAD_CTRL_H_

#ifdef __cplusplus
#  define BEGIN_C_DECLS extern "C" {
#  define END_C_DECLS   }
#else /* !__cplusplus */
#  define BEGIN_C_DECLS
#  define END_C_DECLS
#endif /* __cplusplus */

BEGIN_C_DECLS

#include <complib/cl_types.h>
#include <iba/ib_types.h>
#include <opensm/osm_madw.h>
#include <opensm/osm_mad_pool.h>
#include <opensm/osm_log.h>
#include "ibis_api.h"
#include "ibis.h"


/****s* IBIS: GSI MAD Controller/ibis_gsi_cb_msg_pair_t
* NAME
*	ibis_gsi_cb_msg_pair_t
*
* DESCRIPTION
*	GSI MAD Controller Dispatcher CB and MsgId Pair
*
* SYNOPSIS
*/
typedef struct _ibis_gsi_cb_msg_pair
{
  cl_pfn_msgrcv_cb_t	    pfn_callback;
  cl_disp_msgid_t   	    msg_id;
  uint8_t                mgt_class;
  uint16_t               attr;
  cl_disp_reg_handle_t   h_disp;
} ibis_gsi_cb_msg_pair_t;
/*
* FIELDS
* pfn_callback
*     the call back registered for the class/attr
*
*  msg_id
*     dispatcher message id allocated
*
*  class
*     mad class.
*
*  attr
*     mad attribute
*
*  h_disp
*     dispatcher handler
*
* SEE ALSO
*	GSI MAD Controller object
*	GSI MAD object
*********/
/*
 * destroy an entry in the attribute vector.
 */


/****h* IBIS/GSI MAD Controller
* NAME
*	GSI MAD Controller
*
* DESCRIPTION
*	The GSI MAD Controller object encapsulates
*	the information needed to receive GSI MADs from the transport layer.
*
*	The GSI MAD Controller object is thread safe.
*
*	This object should be treated as opaque and should be
*	manipulated only through the provided functions.
*
* AUTHOR
*	Eitan Zahavi, Mellanox Technologies LTD
*
*********/



/****f* IBIS: GSI MAD Controller/ibis_gsi_mad_ctrl_construct
* NAME
*	ibis_gsi_mad_ctrl_construct
*
* DESCRIPTION
*	This function constructs a GSI MAD Controller object.
*
* SYNOPSIS
*/
void ibis_gsi_mad_ctrl_construct(
	IN ibis_gsi_mad_ctrl_t* const p_ctrl );
/*
* PARAMETERS
*	p_ctrl
*		[in] Pointer to a GSI MAD Controller
*		object to construct.
*
* RETURN VALUE
*	This function does not return a value.
*
* NOTES
*	Allows calling ibis_gsi_mad_ctrl_init, ibis_gsi_mad_ctrl_destroy,
*	and ibis_gsi_mad_ctrl_is_inited.
*
*	Calling ibis_gsi_mad_ctrl_construct is a prerequisite to calling any other
*	method except ibis_gsi_mad_ctrl_init.
*
* SEE ALSO
*	GSI MAD Controller object, ibis_gsi_mad_ctrl_init,
*	ibis_gsi_mad_ctrl_destroy, ibis_gsi_mad_ctrl_is_inited
*********/

/****f* IBIS: GSI MAD Controller/ibis_gsi_mad_ctrl_destroy
* NAME
*	ibis_gsi_mad_ctrl_destroy
*
* DESCRIPTION
*	The ibis_gsi_mad_ctrl_destroy function destroys the object, releasing
*	all resources.
*
* SYNOPSIS
*/
void ibis_gsi_mad_ctrl_destroy(
	IN ibis_gsi_mad_ctrl_t* const p_ctrl );
/*
* PARAMETERS
*	p_ctrl
*		[in] Pointer to the object to destroy.
*
* RETURN VALUE
*	This function does not return a value.
*
* NOTES
*	Performs any necessary cleanup of the specified
*	GSI MAD Controller object.
*	Further operations should not be attempted on the destroyed object.
*	This function should only be called after a call to
*	ibis_gsi_mad_ctrl_construct or ibis_gsi_mad_ctrl_init.
*
* SEE ALSO
*	GSI MAD Controller object, ibis_gsi_mad_ctrl_construct,
*	ibis_gsi_mad_ctrl_init
*********/

/****f* IBIS: GSI MAD Controller/ibis_gsi_mad_ctrl_init
* NAME
*	ibis_gsi_mad_ctrl_init
*
* DESCRIPTION
*	The ibis_gsi_mad_ctrl_init function initializes a
*	GSI MAD Controller object for use.
*
* SYNOPSIS
*/
ib_api_status_t
ibis_gsi_mad_ctrl_init(
	IN ibis_gsi_mad_ctrl_t* const p_ctrl,
	IN osm_mad_pool_t* const p_mad_pool,
	IN osm_vendor_t* const p_vendor,
	IN osm_log_t* const p_log,
	IN cl_dispatcher_t* const p_disp );
/*
* PARAMETERS
*	p_ctrl
*		[in] Pointer to an ibis_gsi_mad_ctrl_t object to initialize.
*
*	p_mad_pool
*		[in] Pointer to the MAD pool.
*
*	p_vendor
*		[in] Pointer to the vendor specific interfaces object.
*
*	p_log
*		[in] Pointer to the log object.
*
*	p_disp
*		[in] Pointer to the IBIS central Dispatcher.
*
* RETURN VALUES
*	IB_SUCCESS if the GSI MAD Controller object was initialized
*	successfully.
*
* NOTES
*	Allows calling other GSI MAD Controller methods.
*
* SEE ALSO
*	GSI MAD Controller object, ibis_gsi_mad_ctrl_construct,
*	ibis_gsi_mad_ctrl_destroy, ibis_gsi_mad_ctrl_is_inited
*********/
END_C_DECLS
#endif	/* _IBIS_GSI_MAD_CTRL_H_ */
