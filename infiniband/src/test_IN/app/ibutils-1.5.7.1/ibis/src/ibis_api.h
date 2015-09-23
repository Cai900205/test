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
 *    Declaration of IBIS interfaces
 *
 * Environment:
 *    Linux User Mode
 *
 * $Revision: 1.6 $
 */

#ifndef _IBIS_API_H_
#define _IBIS_API_H_

#ifdef __cplusplus
#  define BEGIN_C_DECLS extern "C" {
#  define END_C_DECLS   }
#else /* !__cplusplus */
#  define BEGIN_C_DECLS
#  define END_C_DECLS
#endif /* __cplusplus */

BEGIN_C_DECLS

#include <iba/ib_types.h>
#ifdef OSM_BUILD_OPENIB
#include <complib/cl_dispatcher.h>
//#include <vendor/osm_vendor_api.h>
#else
#include <opensm/cl_dispatcher.h>
//#include <opensm/osm_vendor_api.h>
#endif
#include <complib/cl_vector.h>
#include <opensm/osm_log.h>
#include <opensm/osm_mad_pool.h>

#include "ibis_base.h"

/****h* IBIS: API/IBIS Interface
* NAME
*  IBIS Interface
*
* DESCRIPTION
*  The file provides the IBIS interface definition.
*
* AUTHOR
*  Eitan Zahavi, Mellanox
*
*********/

/****f* IBIS: API/ibis_get_port
* NAME
*  ibis_get_port
*
* DESCRIPTION
*  The ibis_bind function should be called to attach the
*  IBIS to the given port guid.
*
* SYNOPSIS
*/
uint64_t
ibis_get_port(void);
/*
* PARAMETERS
*  NONE
*
* RETURN VALUE
*  the guid of the port to attach ibis to.
*
* SEE ALSO
*  IBIS object
*********/

/****f* IBIS: API/ibis_get_port
* NAME
*  ibis_get_port
*
* DESCRIPTION
*  The ibis_bind function should be called to attach the
*  IBIS to the given port guid.
*
* SYNOPSIS
*/
int
ibis_set_port(uint64_t guid);
/*
* PARAMETERS
*  NONE
*
* RETURN VALUE
*  the guid of the port to attach ibit to.
*
* SEE ALSO
*  IBIS object
*********/


/****f* IBIS: API/ibis_get_log
* NAME
*   ibis_get_log
*
* DESCRIPTION
*   Return the pointer to the ibis log
*
* SYNOPSIS
*/
const osm_log_t*
ibis_get_log(void);
/*
* PARAMETERS
*
* RETURN VALUE
*   The IBIS log
*
* NOTES
*
* SEE ALSO
*********/

/****s* IBIS: GSI MAD Controller/ibis_gsi_mad_ctrl_t
* NAME
*  ibis_gsi_mad_ctrl_t
*
* DESCRIPTION
*  GSI MAD Controller structure.
*
*  This object should be treated as opaque and should
*  be manipulated only through the provided functions.
*
* SYNOPSIS
*/
typedef struct _ibis_gsi_mad_ctrl
{
  osm_log_t             *p_log;
  osm_mad_pool_t        *p_mad_pool;
  struct _osm_vendor    *p_vendor;
  cl_dispatcher_t       *p_disp;
  struct _ibis          *p_ibis;
  cl_vector_t            class_vector;
  atomic32_t             msg_id;
} ibis_gsi_mad_ctrl_t;
/*
* FIELDS
*  p_log
*     Pointer to the log object.
*
*  p_mad_pool
*     Pointer to the MAD pool.
*
*  p_vendor
*     Pointer to the vendor specific interfaces object.
*
*  h_bind
*     Bind handle returned by the transport layer.
*
*  p_disp
*     Pointer to the Dispatcher.
*
*  class_vector
*     A vector holding vector of attribute fpn callbacks.
*
*  p_msg_id
*     A pointer to the msg id (semaphore) dynamic counter.
*
* SEE ALSO
*  GSI MAD Controller object
*  GSI MAD object
*********/

/****f* IBIS: API/ibis_get_mad_status
* NAME
*   ibis_get_mad_status
*
* DESCRIPTION
*   Get the status field of a mad
*
* SYNOPSIS
*/
static inline uint16_t
ibis_get_mad_status(
  IN ib_mad_t const *p_mad)
{
  return cl_ntoh16( (ib_net16_t)(p_mad->status & IB_SMP_STATUS_MASK) );
}
/*
* PARAMETERS
*   p_mad
*   [in] The mad to look for status into.
*
* RETURN VALUE
*   The status value in host order
*
* NOTES
*
* SEE ALSO
*********/

/****f* IBIS: API/ibis_get_gsi_mad_ctrl
* NAME
*   ibis_get_gsi_mad_ctrl
*
* DESCRIPTION
*   Return the pointer to the ibis gsi_mad_ctrl
*
* SYNOPSIS
*/
const ibis_gsi_mad_ctrl_t*
ibis_get_gsi_mad_ctrl(void);
/*
* PARAMETERS
*
* RETURN VALUE
*   The IBIS gsi_mad_ctrl
*
* NOTES
*
* SEE ALSO
*********/

/****s* IBIS: API/ibis_port_info_t
* NAME
*  ibis_port_info_t
*
* DESCRIPTION
*  Available HCA ports info.
*
* SYNOPSIS
*/
typedef struct _ibis_port_info
{
  uint64_t   port_guid;
  uint16_t   lid;
  uint8_t    port_num;
  uint8_t    link_state;
} ibis_port_info_t;
/*
* FIELDS
*
* hca_guid
*   The CA Guid of this port
*
* port_guid
*   The port GUID
*
* port_num
*   The index in the CA ports.
*
* link_state
*    Actual link state.
*
* SEE ALSO
*********/

/****f* IBIS: API/ibis_get_ports_status
* NAME
*   ibis_get_ports_status
*
* DESCRIPTION
*   Return the list of available CA ports and their status.
*
* SYNOPSIS
*/
ib_api_status_t
ibis_get_ports_status(
    IN OUT uint32_t *num_ports,
    IN OUT ibis_port_info_t ports_array[] );
/*
* PARAMETERS
*   num_ports
*   [in out] as input number of ports provided, updated by the code
*            to reflect the number of available ports
*
*   ports_array
*   [in out] to be filled by the routine if not null
*
* RETURN VALUE
*   IB_SUCCESS if all fine
*   IB_ERROR otherwise.
*
* NOTES
*
* SEE ALSO
*********/




/****f* IBIS: GSI/ibis_gsi_mad_ctrl_bind
* NAME
*  ibis_gsi_mad_ctrl_bind
*
* DESCRIPTION
*  Binds the GSI MAD Controller object to a class
*
* SYNOPSIS
*/
ib_api_status_t
ibis_gsi_mad_ctrl_bind(
  IN ibis_gsi_mad_ctrl_t* const p_ctrl,
  IN const uint8_t mad_class,
  IN const uint8_t class_version,
  IN osm_bind_handle_t *p_h_bind);
/*
* PARAMETERS
*  p_ctrl
*     [in] Pointer to an ibis_gsi_mad_ctrl_t object to initialize.
*
*  mad_class
*     [in] The specific mad class we register for.
*
*  class_version
*     [in] The specific class version we register for.
*
*
* RETURN VALUES
*  None
*
* NOTES
*  A given GSI MAD Controller object can only be bound to one
*  port at a time.
*
* SEE ALSO
*********/

/****f* IBIS: GSI/ibis_gsi_mad_ctrl_set_class_attr_cb
* NAME
*  ibis_gsi_mad_ctrl_set_class_attr_cb
*
* DESCRIPTION
*  Register the given callback function as the handler for
*  the given Class,Attr pair..
*
* SYNOPSIS
*/
ib_api_status_t
ibis_gsi_mad_ctrl_set_class_attr_cb(
  IN ibis_gsi_mad_ctrl_t* const p_ctrl,
  IN const uint8_t mad_class,
  IN const uint16_t attr,
  IN cl_pfn_msgrcv_cb_t class_attr_cb,
  IN void *context);
/*
* PARAMETERS
*  p_ctrl
*     [in] Pointer to an ibis_gsi_mad_ctrl_t object.
*
*  mad_class
*     [in] The mad class involved.
*
*  attr
*     [in] The attribute
*
*  class_attr_cb
*     [in] The call back function the dispatcher should call.
*
*  context
*     [in] The context to be provided to the class_attr_cb
*
* RETURN VALUES
*  IB_SUCCESS if OK
*  IB_ERROR otherwise.
*
* SEE ALSO
*********/

/****f* IBIS: GSI/ibis_gsi_sync_mad_batch_callback
* NAME
*  ibis_gsi_sync_mad_batch_callback
*
* DESCRIPTION
*  The callback function to be used during bind if batch mad sends
*  are to be used for the class and attr.
*
* SYNOPSIS
*/
void
ibis_gsi_sync_mad_batch_callback(
  IN void* context,
  IN void* p_data);
/*
* PARAMETERS
*  context
*     [in] The context provided during the bind.
*
*  p_data
*     [in] The madw of the incoming mad
*
*
* SEE ALSO
*********/

/****f* IBIS: GSI/ibis_gsi_send_sync_mad_batch
* NAME
*  ibis_gsi_send_sync_mad_batch
*
* DESCRIPTION
*  Send the given set of mads wrappers and wait for their
*  completion, using the given timeout. The result MADs will be copied
*  into the given pre-allocated array of buffers with the given size.
*
* SIDE EFFECTS
*  The context of the pre-allocated mad wrappers is over written
*  by the local context.
*
* RETURN
*  IB_SUCCESS if OK IB_TIMEOUT if ALL mads has timeout.
*  The resulting array will be cleaned up so you can count on TID and
*  method to know if the result was obtained.
*
* SYNOPSIS
*/
ib_api_status_t
ibis_gsi_send_sync_mad_batch(
  IN ibis_gsi_mad_ctrl_t       *p_ctrl,
  IN osm_bind_handle_t          h_bind,
  IN uint16_t                   num,
  IN osm_madw_t                *p_madw_arr[],
  IN size_t                     res_size,
  OUT uint8_t                   res_arr[]);
/*
* PARAMETERS
*  p_ctrl
*     [in] Pointer to an ibis_gsi_mad_ctrl_t object
*
*  h_bind
*     [in] A binding handle obtained from calling the ibis_gsi_mad_ctrl_bind
*
*  num
*     [in] The number of MADs to be sent
*
*  p_madw_arr
*     [in] array of mad wrapper pointers
*
*  res_size
*     [in] The size of each entry in the the results array
*
*  res_arr
*     [out] The array of results - should be pre-allocated.
*
* RETURN VALUES
*  IB_SUCCESS if OK (any MAD was returned - not ALL).
*  IB_ERROR otherwise.
*
* SEE ALSO
*********/

/****s* IBIS: API/ibis_opt_t
* NAME
*  ibis_opt_t
*
* DESCRIPTION
*  IBIS options structure. This structure contains the various
*  specific configuration parameters for ibis.
*
* SYNOPSIS
*/
typedef struct _ibis_opt
{
  uint32_t          transaction_timeout;
  boolean_t         single_thread;
  boolean_t         force_log_flush;
  osm_log_level_t   log_flags;
  char              log_file[1024];
  uint64_t          sm_key;
  uint64_t          m_key;
  uint64_t          v_key;
} ibis_opt_t;
/*
 * FIELDS
 *
 * transaction_timeout
 *   Transaction timeout before retry in msec
 *
 * single_thread
 *   Control the number of dispatcher threads to be created. If set to TRUE only
 *   one thread will be used. The dispatcher threads pull MADs from the incomming
 *   MADs FIFO and invoke the appropriate callback for handling the MAD.
 *
 * force_log_flush
 *   Forces log file flush every logged event.
 *
 * log_flags
 *    The log levels to be used
 *
 * log_file
 *   The name of the log file to be used.
 *
 * sm_key
 *   The SM_Key to be used when sending SubnetMgt and SubnetAdmin MADs
 *
 * m_key
 *   The M_Key to be used when sending SubnetMgt
 *
 * v_key
 *   The Vendor Key to be used when sending Vendor Specific MADs.
 *
 * SEE ALSO
 *********/

/****s* IBIS/ibis_t
 * NAME
 * ibis_t
 *
 * DESCRIPTION
 * IBIS main structure.
 *
 * This object should be treated as opaque and should
 * be manipulated only through the provided functions.
 *
 * SYNOPSIS
 */
typedef struct _ibis
{
  boolean_t             initialized;
  ibis_opt_t            *p_opt;
  osm_log_t             log;
  osm_mad_pool_t        mad_pool;
  ibis_gsi_mad_ctrl_t   mad_ctrl;
  cl_dispatcher_t       disp;
  struct _osm_vendor   *p_vendor;
  uint64_t              port_guid;
  atomic32_t            trans_id;
} ibis_t;
/*
* FIELDS
*  p_opt
*     ibis options structure
*
*  log
*     Log facility used by all IBIS components.
*
*  mad_pool
*     The layer mad pool instance.
*
*  mad_ctrl
*     The layer mad control instance.
*
*  disp
*     The layer dispatcher instance.
*
*  p_vendor
*     Pointer to the vendor transport layer.
*
*  port_guid
*     The port GUID ibis is attached to.
*
*  trans_id
*     Transaction ID
*
* SEE ALSO
*********/


/****g* IBIS/IbisObj
* NAME
*  IbisObj
*
* DESCRIPTION
*  global IBIS Object
*
* SYNOPSIS
*/
extern ibis_t IbisObj;
/*
* SEE ALSO
*  IBIS object, ibis_construct, ibis_destroy
*********/

END_C_DECLS

#endif /* _IBIS_API_H_ */
