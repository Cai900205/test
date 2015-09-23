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
 *    Implementation of ibis_gsi_mad_ctrl_t.
 * This object is part of the GSI object.
 *
 * Environment:
 *    Linux User Mode
 *
 * $Revision: 1.15 $
 */

#include <stdlib.h>
#include <string.h>
#include <complib/cl_passivelock.h>
#include <complib/cl_debug.h>
#include <complib/cl_map.h>
#include <iba/ib_types.h>
#include "ibis_gsi_mad_ctrl.h"
#include "ibis.h"
#include <stdio.h>

/**********************************************************************
  FUNCTIONS HANDLING OF CLASS VECTOR AND ATTRIBUTE VECTOR
**********************************************************************/

static void
__destroy_attr_entry(
  IN           void* const             p_element,
  IN           void*                context )
{
  ibis_gsi_cb_msg_pair_t  *p_cb_msg_pair;

  p_cb_msg_pair = (ibis_gsi_cb_msg_pair_t  *)p_element;
  cl_disp_unregister( p_cb_msg_pair->h_disp);

}

/*
 * Initialize an entry in the attribute vector.
 */
static cl_status_t
__init_attr_entry(
  IN           void* const p_element,
  IN           void*       context )
{
  ibis_gsi_cb_msg_pair_t  *p_cb_msg_pair;

  p_cb_msg_pair = (ibis_gsi_cb_msg_pair_t *) p_element;
  p_cb_msg_pair->mgt_class = 0;
  p_cb_msg_pair->msg_id = CL_DISP_MSGID_NONE;
  p_cb_msg_pair->pfn_callback = NULL;
  p_cb_msg_pair->attr = 0;

  return( CL_SUCCESS );
}

/*
 * destroy an entry in the class vector.
 */
static void
__destroy_class_entry(
  IN           void* const             p_element,
  IN           void*                context )
{
  cl_vector_t  *p_attr_vector;

  p_attr_vector = (cl_vector_t *)p_element;

  cl_vector_destroy(p_attr_vector);
}

/*
 * Initialize an entry in the class vector.
 */
static cl_status_t
__init_class_entry(
  IN           void* const p_element,
  IN           void*       context )
{
  cl_vector_t  *p_attr_vector;
  cl_status_t  status;

  p_attr_vector = (cl_vector_t *)p_element;

  /* Initialize the attribute vector. */
  status = cl_vector_init(
    p_attr_vector,
    32, // start size
    1,  // grow size
    sizeof(ibis_gsi_cb_msg_pair_t), // element size (each one is a attr_vector)
    __init_attr_entry,    // element constructor
    __destroy_attr_entry, // element destructor
    NULL
    );

  return( CL_SUCCESS );
}

/****f* IBIS : GSI MAD /__ibis_gsi_mad_ctrl_disp_done_callback
 * NAME
 * __ibis_gsi_mad_ctrl_disp_done_callback
 *
 * DESCRIPTION
 * This function is the Dispatcher callback that indicates
 * a received MAD has been processed by the recipient.
 *
 * SYNOPSIS
 */
static void
__ibis_gsi_mad_ctrl_disp_done_callback(
  IN void* context,
  IN void* p_data )
{
  osm_madw_t* const p_madw = (osm_madw_t*)p_data;

  OSM_LOG_ENTER(&(IbisObj.log));

  CL_ASSERT( p_madw );
  /*
    Return the MAD & wrapper to the pool.
  */
  osm_mad_pool_put( &(IbisObj.mad_pool), p_madw );
  OSM_LOG_EXIT( &(IbisObj.log) );
}
/************/

/****f* IBIS: GSI MAD /__ibis_gsi_mad_ctrl_process
 * NAME
 * __ibis_gsi_mad_ctrl_process
 *
 * DESCRIPTION
 * This function handles known methods for received MADs.
 *
 * SYNOPSIS
 */
static void
__ibis_gsi_mad_ctrl_process(
  IN ibis_gsi_mad_ctrl_t* const p_ctrl,
  IN osm_madw_t *p_madw )
{
  ib_mad_t*                 p_mad;
  ibis_gsi_cb_msg_pair_t*   p_cb_msg_pair;
  cl_vector_t*              p_attr_vector;
  cl_status_t               status;
  cl_disp_msgid_t           msg_id = CL_DISP_MSGID_NONE;
  cl_disp_reg_handle_t      h_disp = CL_DISP_INVALID_HANDLE;
  uint8_t                   mgmt_class;

  OSM_LOG_ENTER(&(IbisObj.log));

  p_mad = osm_madw_get_mad_ptr( p_madw );

  /*
    TODO: We need to have a dynamic and not a static
    mechanism for all GSM to register their MSG IDs in here.
  */

  osm_log( p_ctrl->p_log, OSM_LOG_DEBUG,
           "__ibis_gsi_mad_ctrl_process: "
           "Class-%x Attr-%x Status-%04x.\n",
           p_mad->mgmt_class,cl_ntoh16(p_mad->attr_id),cl_ntoh16(p_mad->status));

  /* we alias SUBN_DR to SUBN_LID callbacks */
  mgmt_class = p_mad->mgmt_class;
  if (mgmt_class == IB_MCLASS_SUBN_DIR) mgmt_class = IB_MCLASS_SUBN_LID;

  if (mgmt_class < cl_vector_get_size(&p_ctrl->class_vector))
    p_attr_vector = cl_vector_get_ptr(&p_ctrl->class_vector, mgmt_class);
  else
    p_attr_vector = NULL;

  if( p_attr_vector == NULL )
  {
    osm_log( p_ctrl->p_log, OSM_LOG_ERROR,
             "__ibis_gsi_mad_ctrl_process: ERR : "
             "Failed to find matching class.\n");

    osm_mad_pool_put( p_ctrl->p_mad_pool, p_madw );
    goto Exit;
  }

  if (cl_ntoh16(p_mad->attr_id) < cl_vector_get_size(p_attr_vector))
    p_cb_msg_pair = cl_vector_get_ptr(p_attr_vector,cl_ntoh16(p_mad->attr_id));
  else
    p_cb_msg_pair = NULL;

  /* make sure we have a client ... */
  if( (p_cb_msg_pair == NULL) ||
      (p_cb_msg_pair->msg_id == CL_DISP_MSGID_NONE))
  {
    osm_log( p_ctrl->p_log, OSM_LOG_ERROR,
             "__ibis_gsi_mad_ctrl_process: ERR : "
             "Failed to find matching attribute:%x class:%x.\n",
             cl_ntoh16(p_mad->attr_id),mgmt_class
             );

    osm_mad_pool_put( p_ctrl->p_mad_pool, p_madw );
    goto Exit;
  }


  msg_id = p_cb_msg_pair->msg_id;
  h_disp = p_cb_msg_pair->h_disp;


  /*
    Post this MAD to the dispatcher for asynchronous
    processing by the appropriate controller.
  */

  osm_log( p_ctrl->p_log, OSM_LOG_DEBUG,
           "__ibis_gsi_mad_ctrl_process: "
           "Posting Dispatcher message %s.\n",
           osm_get_disp_msg_str( msg_id ) );

  status = cl_disp_post( h_disp,
                         msg_id,
                         p_madw,
                         __ibis_gsi_mad_ctrl_disp_done_callback,
                         p_ctrl );

  if( status != CL_SUCCESS )
  {
    osm_log( p_ctrl->p_log, OSM_LOG_ERROR,
             "__ibis_gsi_mad_ctrl_process: ERR : "
             "Dispatcher post message failed (%s).\n",
             CL_STATUS_MSG( status ) );

    osm_mad_pool_put( p_ctrl->p_mad_pool, p_madw );
    goto Exit;
  }

 Exit:
  OSM_LOG_EXIT( p_ctrl->p_log );
}

/****f* IBIS: GSI MAD /__ibis_gsi_mad_ctrl_rcv_callback
 * NAME
 * __ibis_gsi_mad_ctrl_rcv_callback
 *
 * DESCRIPTION
 * This is the callback from the transport layer for received MADs.
 *
 * SYNOPSIS
 */
static void
__ibis_gsi_mad_ctrl_rcv_callback(
  IN osm_madw_t *p_madw,
  IN void *bind_context,
  IN osm_madw_t *p_req_madw )
{
  ibis_gsi_mad_ctrl_t* p_ctrl = (ibis_gsi_mad_ctrl_t*)bind_context;
  ib_mad_t* p_mad;

  OSM_LOG_ENTER(p_ctrl->p_log);

  CL_ASSERT( p_madw );

  /*
    A MAD was received from the wire, possibly in response to a request.
  */

  /* copy the request mad context if exists */
  if (p_req_madw)
  {
    p_madw->context = p_req_madw->context;
  }

  p_mad = osm_madw_get_mad_ptr( p_madw );

  __ibis_gsi_mad_ctrl_process( p_ctrl, p_madw );

  if (p_req_madw)
  {
    osm_mad_pool_put( p_ctrl->p_mad_pool, p_req_madw );
  }

  OSM_LOG_EXIT( p_ctrl->p_log );
}

/****f* IBIS: GSI MAD /__ibis_gsi_mad_ctrl_send_err_callback
 * NAME
 * __ibis_gsi_mad_ctrl_send_err_callback
 *
 * DESCRIPTION
 * This is the callback from the transport layer for send errors
 * on MADs that were expecting a response.
 *
 * SYNOPSIS
 */
void
__ibis_gsi_mad_ctrl_send_err_callback(
  IN void *bind_context,
  IN osm_madw_t *p_madw )
{
  ib_mad_t* p_mad;
  ibis_gsi_mad_ctrl_t* p_ctrl = (ibis_gsi_mad_ctrl_t*)bind_context;

  OSM_LOG_ENTER(p_ctrl->p_log);

  // TODO . General call_back for errors.

  CL_ASSERT( p_madw );

  /*
    An error occurred.  No response was received to a request MAD.
    Retire the original request MAD.
  */

  p_mad = osm_madw_get_mad_ptr( p_madw );

  osm_log( p_ctrl->p_log, OSM_LOG_ERROR,
           "__ibis_gsi_mad_ctrl_send_err_callback: ERR 1A06: "
           "MAD transaction completed in error.\n" );

  __ibis_gsi_mad_ctrl_process( p_ctrl, p_madw );

  /* osm_mad_pool_put( p_ctrl->p_mad_pool, p_madw ); */

  OSM_LOG_EXIT( p_ctrl->p_log );
}

/****s* IBIS: GSI/ibis_gsi_mad_batch_context_t
 * NAME
 * ibis_gsi_mad_batch_context_t
 *
 * DESCRIPTION
 * Context used for tracking of synchronous mad batches
 *  we only have one such context for each batch.
 *
 * SYNOPSIS
 */
typedef struct _ibis_gsi_mad_batch_context
{
  ibis_gsi_mad_ctrl_t *p_ctrl;
  uint64_t             id;
  uint16_t             num_outstanding;
  boolean_t            all_sent;
  size_t               res_size;
  uint8_t              *res_arr;
  cl_event_t           wait_for_resp;
  cl_spinlock_t        lock;
} ibis_gsi_mad_batch_context_t;
/*
 * FIELDS
 *
 *  p_ctrl
 *     The pointer to the manager so we can log etc.
 *
 *  id
 *     A unique id used to track the context activity and
 *     during mad receives.
 *
 *  num_outstanding
 *     Number of mads still outstanding in this query.
 *
 *  all_sent
 *     Flags that all the batch mads were sent - so if num_outstanding is 0
 *     we can signal we are done.
 *
 *  res_size
 *     The size of a single result. The num of bytes copied.
 *
 *  res_arr
 *     Results array.
 *
 *  wait_for_resp
 *     Event for signaling we got all responses
 *
 *  lock
 *     A lock object for preventing collisions ?
 *
 * SEE ALSO
 *     ibis_gsi_mad_batch_madw_context_t
 *********/

/****g* IBIS: GSI/g_ibis_active_batch_contexts_map
 * NAME
 * g_ibis_active_batch_contexts_qmap
 *
 * DESCRIPTION
 * Global variable holding all active Batch Context IDs
 *  used for tracking of synchronous mad batches.
 *  We use this qmap under the global lock and make sure
 *  to install new entries when batch context is created
 *  and remove when it is destructed.
 *
 * SYNOPSIS
 */
static cl_map_t g_ibis_active_batch_contexts_map;
/*
*********/

/****g* IBIS: GSI/g_ibis_next_batch_context_id
 * NAME
 * g_ibis_next_batch_context_id
 *
 * DESCRIPTION
 * Global variable holding the id of the next batch context
 *
 * SYNOPSIS
 */
static uint64_t g_ibis_next_batch_context_id;
/*
*********/

/****g* IBIS: GSI/g_ibis_batch_contexts_map_lock
 * NAME
 * g_ibis_batch_contexts_map_lock
 *
 * DESCRIPTION
 * Global variable holding a lock object for handling the
 *  Map of batch contexts.
 *
 * SYNOPSIS
 */
static cl_spinlock_t g_ibis_batch_contexts_map_lock;
/*
*********/

/**********************************************************************
 * allocate a new batch context and track it in the global map.
 **********************************************************************/
ibis_gsi_mad_batch_context_t *
__gsi_new_mad_batch_context(void)
{
  ibis_gsi_mad_batch_context_t *p_batch_ctx;
  ibis_gsi_mad_batch_context_t *insert_res;

  /* allocate a new batch */
  p_batch_ctx = (ibis_gsi_mad_batch_context_t *)
    malloc(sizeof(ibis_gsi_mad_batch_context_t));

  /* construct and initialize the spinlock and mutex on the batch */
  cl_event_construct(&p_batch_ctx->wait_for_resp);
  cl_spinlock_construct(&p_batch_ctx->lock);

  cl_event_init(&p_batch_ctx->wait_for_resp, FALSE); // FALSE: auto reset
  cl_spinlock_init(&p_batch_ctx->lock);

  /* everything done in the active batches is under lock */
  cl_spinlock_acquire(&g_ibis_batch_contexts_map_lock);

  p_batch_ctx->id =
    g_ibis_next_batch_context_id++;

  /* insert into the map making sure no previous same pointer exists */
  insert_res =
    cl_map_insert( &g_ibis_active_batch_contexts_map,
                   p_batch_ctx->id, p_batch_ctx);

  cl_spinlock_release(&g_ibis_batch_contexts_map_lock);

  /* if we did not get the pointer we have put in - there must have been
     a previous entry by that key!!! */
  CL_ASSERT(insert_res == p_batch_ctx);

  return p_batch_ctx;
}

/**********************************************************************
 * deallocate a batch context and remove it from the global map.
 **********************************************************************/
void
__gsi_delete_mad_batch_context(
  IN ibis_gsi_mad_batch_context_t *p_batch_ctx)
{
  ibis_gsi_mad_batch_context_t *p_rem_res;

  /* find the context in the map or assert */
  cl_spinlock_acquire( &g_ibis_batch_contexts_map_lock );
  p_rem_res =
    cl_map_remove( &g_ibis_active_batch_contexts_map, p_batch_ctx->id );
  cl_spinlock_release( &g_ibis_batch_contexts_map_lock );

  /* is it possible it was already deleted ??? */
  if (p_rem_res != NULL)
  {
    /* need to cleanup the allocated event and lock */
    cl_event_destroy(&p_batch_ctx->wait_for_resp);
    cl_spinlock_destroy(&p_batch_ctx->lock);

    memset(p_batch_ctx, 0, sizeof(ibis_gsi_mad_batch_context_t));

    /* finally */
    free(p_batch_ctx);
  }
}

/**********************************************************************
 * Return the batch context if is still valid.
 * NOTE: the g_ibis_active_batch_contexts_map should be  locked.
 **********************************************************************/
ibis_gsi_mad_batch_context_t *
__gsi_get_valid_mad_batch_context(
  IN uint64_t id)
{
  void * res;
  /* NOTE THE LOCK SHOULD BE PERFORMED DURING THE ENTIRE USAGE */
  res = cl_map_get( &g_ibis_active_batch_contexts_map, id);
  return res;
}

/**********************************************************************
 **********************************************************************/
void
ibis_gsi_mad_ctrl_construct(
  IN ibis_gsi_mad_ctrl_t* const p_ctrl )
{
  CL_ASSERT( p_ctrl );
  memset( p_ctrl, 0, sizeof(*p_ctrl) );
  cl_vector_construct( &p_ctrl->class_vector );
  cl_map_construct( & g_ibis_active_batch_contexts_map );
}

/**********************************************************************
 **********************************************************************/
void
ibis_gsi_mad_ctrl_destroy(
  IN ibis_gsi_mad_ctrl_t* const p_ctrl )
{

  CL_ASSERT(p_ctrl );
  /* do not deallocate to prevent case of rush in mad ... */
  /* cl_vector_destroy( &p_ctrl->class_vector ); */
  cl_map_destroy( & g_ibis_active_batch_contexts_map );
}


/**********************************************************************
 **********************************************************************/
ib_api_status_t
ibis_gsi_mad_ctrl_init(
  IN ibis_gsi_mad_ctrl_t* const p_ctrl,
  IN osm_mad_pool_t* const p_mad_pool,
  IN osm_vendor_t* const p_vendor,
  IN osm_log_t* const p_log,
  IN cl_dispatcher_t* const p_disp )
{
  ib_api_status_t status = IB_SUCCESS;
  cl_status_t cl_status;
  cl_disp_reg_handle_t      h_disp;

  ibis_gsi_mad_ctrl_construct( p_ctrl);
  p_ctrl->p_log = p_log;
  p_ctrl->p_disp = p_disp;
  p_ctrl->p_mad_pool = p_mad_pool;
  p_ctrl->p_vendor = p_vendor;

  h_disp = cl_disp_register(
    p_ctrl->p_disp,
    CL_DISP_MSGID_NONE,
    NULL,
    p_ctrl );

  if( h_disp == CL_DISP_INVALID_HANDLE )
  {
    osm_log( p_log, OSM_LOG_ERROR,
             "ibis_gsi_mad_ctrl_init: ERR 1A08: "
             "Dispatcher registration failed.\n" );
    status = IB_INSUFFICIENT_RESOURCES;
    goto Exit;
  }

  cl_disp_unregister(h_disp);

  p_ctrl->msg_id = (cl_disp_msgid_t ) 0;

  /* Initialize the class vector. */
  cl_status = cl_vector_init(
    &p_ctrl->class_vector,
    11, // start size
    1,  // grow size
    sizeof(cl_vector_t), // element size (each one is a attr_vector)
    __init_class_entry,    // element constructor
    __destroy_class_entry, // element destructor
    NULL
    );
  if( cl_status != CL_SUCCESS )
  {
    status = IB_ERROR;
    goto Exit;
  }

  /* initialize the map of all active batch sends contexts */
  cl_map_init( & g_ibis_active_batch_contexts_map, 10 );

  /* initialize the first batch id */
  g_ibis_next_batch_context_id = 1;

  /* initialize the lock object */
  cl_spinlock_init(&g_ibis_batch_contexts_map_lock);

 Exit:
  return( status );
}

/**********************************************************************
 **********************************************************************/
ib_api_status_t
ibis_gsi_mad_ctrl_bind(
  IN ibis_gsi_mad_ctrl_t* const p_ctrl,
  IN const uint8_t mad_class,
  IN const uint8_t class_version,
  IN osm_bind_handle_t *p_h_bind)
{
  osm_bind_info_t bind_info;
  ib_api_status_t status = IB_SUCCESS;

  OSM_LOG_ENTER(p_ctrl->p_log);

  if( *p_h_bind != OSM_BIND_INVALID_HANDLE )
  {
    osm_log( p_ctrl->p_log, OSM_LOG_ERROR,
             "ibis_gsi_mad_ctrl_bind: ERR 1A09: "
             "Multiple binds not allowed.  Call unbind first. " );
    status = IB_ERROR;
    goto Exit;
  }

  memset(&bind_info, 0, sizeof(osm_bind_info_t));
  bind_info.class_version = class_version;
  bind_info.is_responder = FALSE;
  bind_info.is_report_processor = FALSE;
  bind_info.is_trap_processor = FALSE;
  bind_info.mad_class = mad_class;
  bind_info.port_guid = cl_hton64(IbisObj.port_guid);
  bind_info.recv_q_size = OSM_SM_DEFAULT_QP1_RCV_SIZE;
  bind_info.send_q_size = OSM_SM_DEFAULT_QP1_SEND_SIZE;

  osm_log( p_ctrl->p_log, OSM_LOG_VERBOSE,
           "ibis_gsi_mad_ctrl_bind: "
           "Binding to port GUID = 0x%" PRIx64 ".\n",
           cl_ntoh64( bind_info.port_guid ) );

  *p_h_bind = osm_vendor_bind( p_ctrl->p_vendor,
                               &bind_info,
                               p_ctrl->p_mad_pool,
                               __ibis_gsi_mad_ctrl_rcv_callback,
                               __ibis_gsi_mad_ctrl_send_err_callback,
                               p_ctrl);

  if( *p_h_bind == OSM_BIND_INVALID_HANDLE )
  {
    status = IB_ERROR;
    osm_log( p_ctrl->p_log, OSM_LOG_ERROR,
             "ibis_gsi_mad_ctrl_bind: ERR 1A10: "
             "Vendor specific bind() failed (%s).\n",
             ib_get_err_str(status) );
    goto Exit;
  }

 Exit:
  OSM_LOG_EXIT( p_ctrl->p_log );
  return( status );
}


/**********************************************************************
 **********************************************************************/
ib_api_status_t
ibis_gsi_mad_ctrl_set_class_attr_cb(
  IN ibis_gsi_mad_ctrl_t* const p_ctrl,
  IN const uint8_t mad_class,
  IN const uint16_t attr,
  IN cl_pfn_msgrcv_cb_t class_attr_cb,
  IN void *context)
{
  ib_api_status_t         status = IB_SUCCESS;
  cl_status_t             cl_status = CL_SUCCESS;
  cl_disp_msgid_t         mid;
  cl_vector_t*            p_attr_vector;
  size_t                  size;
  ibis_gsi_cb_msg_pair_t  dummy_cb_msg_pair; /* see later hack */
  ibis_gsi_cb_msg_pair_t *p_cb_msg_pair;
  cl_disp_reg_handle_t    disp_reg_hdl;

  OSM_LOG_ENTER(p_ctrl->p_log);

  mid = (cl_disp_msgid_t)cl_atomic_inc( &p_ctrl->msg_id);

  /* register the new message in the dispatcher */
  disp_reg_hdl = cl_disp_register(
    p_ctrl->p_disp,
    mid,
    class_attr_cb,
    context);

  if( disp_reg_hdl == CL_DISP_INVALID_HANDLE )
  {
    osm_log(p_ctrl->p_log, OSM_LOG_ERROR,
            "ibis_gsi_mad_ctrl_set_class_attr_cb: "
            "Dispatcher registration failed.\n" );
    status = IB_INSUFFICIENT_RESOURCES;
    goto Exit;
  }

  /* make sure the vector is big enough */
  size = cl_vector_get_size(&p_ctrl->class_vector);
  if (size <= mad_class)
  {
    cl_status = cl_vector_set_size(&p_ctrl->class_vector,mad_class+1);

    if( cl_status != CL_SUCCESS)
    {
      osm_log(p_ctrl->p_log, OSM_LOG_ERROR,
              "ibis_gsi_mad_ctrl_set_class_attr_cb: "
              "cl_vector_set on class Failed.\n" );
      status = IB_INSUFFICIENT_RESOURCES;
      goto Exit;
    }
  }

  p_attr_vector = cl_vector_get_ptr (&p_ctrl->class_vector,mad_class);

  /*
    HACK: due to a bug in cl_vector_set (copy over the given data)
    we need to set an empty entry and then directly write into the
    returned element .
    We keep this hack for backward compatibility with old complib
  */

  /* we first set the empty value */
  cl_status = cl_vector_set(p_attr_vector, attr, (void *)&dummy_cb_msg_pair);
  if( cl_status != CL_SUCCESS)
  {
    osm_log(p_ctrl->p_log, OSM_LOG_ERROR,
            "ibis_gsi_mad_ctrl_set_class_attr_cb: "
            "cl_vector_set on attribute Failed.\n" );
    status = IB_INSUFFICIENT_RESOURCES;
    goto Exit;
  }

  /* now we get the pointer to the vector */
  p_cb_msg_pair =
    (ibis_gsi_cb_msg_pair_t *)cl_vector_get_ptr(p_attr_vector, attr);
  p_cb_msg_pair->mgt_class = mad_class;
  p_cb_msg_pair->attr = attr;
  p_cb_msg_pair->msg_id = mid;
  p_cb_msg_pair->pfn_callback = class_attr_cb;
  p_cb_msg_pair->h_disp = disp_reg_hdl;

  osm_log(p_ctrl->p_log, OSM_LOG_ERROR,
          "ibis_gsi_mad_ctrl_set_class_attr_cb: "
          "Setting CB for class:0x%x attr:0x%x on vectors: %p,%p.\n",
          mad_class, attr, &p_ctrl->class_vector, p_attr_vector
          );

 Exit:

  OSM_LOG_EXIT(p_ctrl->p_log );
  return (status);

}


/****s* IBIS: GSI/ibis_gsi_mad_batch_madw_context_t
 * NAME
 * ibis_gsi_mad_batch_madw_context_t
 *
 * DESCRIPTION
 * Context used for tracking of synchronous each mad in the batch.
 *  It it stored in the madw->context .
 *
 * SYNOPSIS
 */
typedef struct _ibis_gsi_mad_batch_madw_context
{
  uint16_t                      idx;
  uint64_t                      batch_ctx_id;
} ibis_gsi_mad_batch_madw_context_t;
/*
 * FIELDS
 *
 *  magic
 *     Provides a way to internally calculate (in the callback) if the
 *     mad received context is still valid. So we avoid the need for lookup.
 *
 *  idx
 *     The index of the mad. Used to get the index of the response in the
 *     results array.
 *
 *  p_batch_ctx
 *     Points to a mads batch context
 *
 * SEE ALSO
 *     ibis_gsi_mad_batch_context_t
 *********/

/**********************************************************************
 **********************************************************************/
void
ibis_gsi_sync_mad_batch_callback(
  IN void* context,
  IN void* p_data)
{
  osm_madw_t* const p_madw = (osm_madw_t*)p_data;
  ibis_gsi_mad_batch_madw_context_t *p_madw_ctx =
    (ibis_gsi_mad_batch_madw_context_t*)(&p_madw->context);
  ibis_gsi_mad_batch_context_t *p_batch_ctx;

  /* grab a lock on the global map of batch contexts */
  cl_spinlock_acquire(&g_ibis_batch_contexts_map_lock);

  if ((p_batch_ctx =
       __gsi_get_valid_mad_batch_context(p_madw_ctx->batch_ctx_id)))
  {
    ibis_gsi_mad_ctrl_t* const p_ctrl = p_batch_ctx->p_ctrl;
    uint8_t  *p_result;
    ib_mad_t *p_mad;
    OSM_LOG_ENTER(p_ctrl->p_log);

    /* obtain the lock */
    cl_spinlock_acquire(&p_batch_ctx->lock);

    /* no need for the global lock since we got a lock on the context */
    cl_spinlock_release(&g_ibis_batch_contexts_map_lock);

    /* ignore non GET RESP mads */
    if (!p_madw->p_mad || p_madw->p_mad->method != IB_MAD_METHOD_GET_RESP)
    {
      osm_log(p_ctrl->p_log, OSM_LOG_DEBUG,
              "ibis_gsi_sync_mad_batch_callback: "
              "Ignoring non GetResp\n");
      // goto Exit;
    }

    /* the size of each result is known so we simply use the buffer */
    if (p_batch_ctx->res_arr)
    {
      p_result = &(p_batch_ctx->res_arr[p_madw_ctx->idx * p_batch_ctx->res_size]);
      p_mad = (ib_mad_t*)p_result;
    }
    else
    {
      /* some mads do not require a result ... */
      p_result = NULL;
    }

    /* make sure we did not already got this mad */
    if (p_madw->p_mad && p_madw->p_mad->method != IB_MAD_METHOD_GET_RESP)
    {
      osm_log(p_ctrl->p_log, OSM_LOG_DEBUG,
              "ibis_gsi_sync_mad_batch_callback: "
              "Ignoring data of non GetResp mad for TID 0x%016" PRIx64 "\n",
              p_madw->p_mad->trans_id);
    }
    else
    {
      /* store the result */
      if (p_result && p_madw->p_mad)
        memcpy(p_result, p_madw->p_mad, p_batch_ctx->res_size);
    }

    /* decrement the number of outstanding mads */
    /* NOTE: MADS that failed on timeout will be return with A GET/SET method */

    if (! p_batch_ctx->num_outstanding)
    {
      printf("ERROR: Got a MAD after the outstanding is 0!\n");
    }
    p_batch_ctx->num_outstanding--;

    osm_log(p_ctrl->p_log, OSM_LOG_DEBUG,
            "ibis_gsi_sync_mad_batch_callback: "
            "Updated MAD Index:%u Num Outstanding 0x%u\n",
            p_madw_ctx->idx, p_batch_ctx->num_outstanding);

    if ((p_batch_ctx->num_outstanding == 0) && p_batch_ctx->all_sent)
    {
      /* Signal for the waiter we have got all data */
      osm_log(p_ctrl->p_log, OSM_LOG_DEBUG,
              "ibis_gsi_sync_mad_batch_callback: "
              "No outstanding mads - signal the waiting thread\n");
      cl_event_signal(&p_batch_ctx->wait_for_resp);
    }

    cl_spinlock_release(&p_batch_ctx->lock);
    OSM_LOG_EXIT( p_ctrl->p_log );
  }
  else
  {
    /* the batch context was invalid */
    cl_spinlock_release(&g_ibis_batch_contexts_map_lock);
  }
}

/**********************************************************************
 Send a batch of MADs
**********************************************************************/
ib_api_status_t
ibis_gsi_send_sync_mad_batch(
  IN ibis_gsi_mad_ctrl_t       *p_ctrl,
  IN osm_bind_handle_t          h_bind,
  IN uint16_t                   num,
  IN osm_madw_t                *p_madw_arr[],
  IN size_t                     res_size,
  OUT uint8_t                   res_arr[])
{
  uint16_t                           i;
  uint16_t                           num_send_failed = 0;
  ibis_gsi_mad_batch_context_t      *p_batch_ctx;
  osm_madw_t                        *p_madw;
  ibis_gsi_mad_batch_madw_context_t *p_madw_ctx;
  cl_status_t                        wait_status;
  ib_api_status_t                    status;

  OSM_LOG_ENTER(p_ctrl->p_log);

  /* initialize the batch context */
  p_batch_ctx = __gsi_new_mad_batch_context();

  p_batch_ctx->p_ctrl          = p_ctrl;
  p_batch_ctx->num_outstanding = 0;
  p_batch_ctx->all_sent        = FALSE;
  p_batch_ctx->res_size        = res_size;
  p_batch_ctx->res_arr         = res_arr;

  /* start by taking the lock */
  cl_spinlock_acquire(&p_batch_ctx->lock);

  /* cleanup the results array */
  if (res_arr) memset(res_arr, 0, res_size*num);

  /* send the mads */
  for (i = 0; i < num; i++)
  {
    /* set the madw context */
    p_madw = p_madw_arr[i];
    p_madw_ctx = (ibis_gsi_mad_batch_madw_context_t*)(&p_madw->context);
    p_madw_ctx->idx = i;
    p_madw_ctx->batch_ctx_id = p_batch_ctx->id;

    status = osm_vendor_send(h_bind, p_madw, TRUE);
    /* increment the number of outstanding mads if sent */
    if (status == IB_SUCCESS)
    {
      p_batch_ctx->num_outstanding++;
    }
    else
    {
      num_send_failed++;
    }
  }

  /* this flag is used by the callback to make sure we got all mads
     sent before even looking on zero outstanding mads */
  p_batch_ctx->all_sent = TRUE;


  /* we only globally failed if we could not send any */
  if (num_send_failed == num)
  {
    status = IB_ERROR;
    goto Exit;
  }

  /* we ignore any send failures */
  status = IB_SUCCESS;

  /* we must unlock to let the callbacks work */
  cl_spinlock_release(&p_batch_ctx->lock);

  /*
    Wait for all mads to retire:

    NOTE:
    we will block only in size of batch * timeout,
    this is done because callback function will not be called
    on timeout
  */
  wait_status = cl_event_wait_on(&p_batch_ctx->wait_for_resp,
                                  p_batch_ctx->num_outstanding * ((&IbisObj)->p_opt->transaction_timeout * 1000),    //in usec
                                  TRUE);
  if (wait_status != CL_SUCCESS)
  {
    status = IB_ERROR;
  }

  /* take the lock again */
  cl_spinlock_acquire(&p_batch_ctx->lock);
 Exit:
  cl_spinlock_release(&p_batch_ctx->lock);

  __gsi_delete_mad_batch_context(p_batch_ctx);
  OSM_LOG_EXIT(p_ctrl->p_log);
  return(status);
}
