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

#include <string.h>
#include "ibis.h"
#include <opensm/osm_helper.h>
#ifdef OSM_BUILD_OPENIB
#include <vendor/osm_vendor_sa_api.h>
#else
#include <opensm/osm_vendor_sa_api.h>
#endif

#define IB_MAD_STATUS_CLASS_MASK       (CL_HTON16(0xFF00))

static const char ib_mad_status_str_busy[] = "IB_MAD_STATUS_BUSY";
static const char ib_mad_status_str_redirect[] = "IB_MAD_STATUS_REDIRECT";
static const char ib_mad_status_str_unsup_class_ver[] =
"IB_MAD_STATUS_UNSUP_CLASS_VER";
static const char ib_mad_status_str_unsup_method[] =
"IB_MAD_STATUS_UNSUP_METHOD";
static const char ib_mad_status_str_unsup_method_attr[] =
"IB_MAD_STATUS_UNSUP_METHOD_ATTR";
static const char ib_mad_status_str_invalid_field[] =
"IB_MAD_STATUS_INVALID_FIELD";
static const char ib_mad_status_str_no_resources[] =
"IB_SA_MAD_STATUS_NO_RESOURCES";
static const char ib_mad_status_str_req_invalid[] =
"IB_SA_MAD_STATUS_REQ_INVALID";
static const char ib_mad_status_str_no_records[] =
"IB_SA_MAD_STATUS_NO_RECORDS";
static const char ib_mad_status_str_too_many_records[] =
"IB_SA_MAD_STATUS_TOO_MANY_RECORDS";
static const char ib_mad_status_str_invalid_gid[] =
"IB_SA_MAD_STATUS_INVALID_GID";
static const char ib_mad_status_str_insuf_comps[] =
"IB_SA_MAD_STATUS_INSUF_COMPS";
static const char generic_or_str[] = " | ";

/**********************************************************************
 **********************************************************************/
/* Global that is initialized to the bind handle of the SA client */
osm_bind_handle_t IbSacBindHndl;

int
ibsac_bind(
  IN ibis_t *   const p_ibis
  )
{
  IbSacBindHndl =
    osmv_bind_sa(p_ibis->p_vendor,
                 &p_ibis->mad_pool,
                 cl_hton64(p_ibis->port_guid));

  return (IbSacBindHndl == OSM_BIND_INVALID_HANDLE);
}

/**********************************************************************
 **********************************************************************/
const char *
ib_get_mad_status_str( IN const ib_mad_t * const p_mad )
{
  static char line[512];
  uint32_t offset = 0;
  ib_net16_t status;
  boolean_t first = TRUE;

  line[offset] = '\0';

  status = ( ib_net16_t ) ( p_mad->status & IB_SMP_STATUS_MASK );

  if( status == 0 )
  {
    strcat( &line[offset], "IB_SUCCESS" );
    return ( line );
  }

  if( status & IB_MAD_STATUS_BUSY )
  {
    strcat( &line[offset], ib_mad_status_str_busy );
    offset += sizeof( ib_mad_status_str_busy );
  }
  if( status & IB_MAD_STATUS_REDIRECT )
  {
    if( !first )
    {
      strcat( &line[offset], generic_or_str );
      offset += sizeof( generic_or_str ) - 1;
    }
    first = FALSE;
    strcat( &line[offset], ib_mad_status_str_redirect );
    offset += sizeof( ib_mad_status_str_redirect ) - 1;
  }
  if( status & IB_MAD_STATUS_UNSUP_CLASS_VER )
  {
    if( !first )
    {
      strcat( &line[offset], generic_or_str );
      offset += sizeof( generic_or_str ) - 1;
    }
    first = FALSE;
    strcat( &line[offset], ib_mad_status_str_unsup_class_ver );
    offset += sizeof( ib_mad_status_str_unsup_class_ver ) - 1;
  }
  if( status & IB_MAD_STATUS_UNSUP_METHOD )
  {
    if( !first )
    {
      strcat( &line[offset], generic_or_str );
      offset += sizeof( generic_or_str ) - 1;
    }
    first = FALSE;
    strcat( &line[offset], ib_mad_status_str_unsup_method );
    offset += sizeof( ib_mad_status_str_unsup_method ) - 1;
  }
  if( status & IB_MAD_STATUS_UNSUP_METHOD_ATTR )
  {
    if( !first )
    {
      strcat( &line[offset], generic_or_str );
      offset += sizeof( generic_or_str ) - 1;
    }
    first = FALSE;
    strcat( &line[offset], ib_mad_status_str_unsup_method_attr );
    offset += sizeof( ib_mad_status_str_unsup_method_attr ) - 1;
  }
  if( status & IB_MAD_STATUS_INVALID_FIELD )
  {
    if( !first )
    {
      strcat( &line[offset], generic_or_str );
      offset += sizeof( generic_or_str ) - 1;
    }
    first = FALSE;
    strcat( &line[offset], ib_mad_status_str_invalid_field );
    offset += sizeof( ib_mad_status_str_invalid_field ) - 1;
  }
  if( ( status & IB_MAD_STATUS_CLASS_MASK ) ==
      IB_SA_MAD_STATUS_NO_RESOURCES )
  {
    if( !first )
    {
      strcat( &line[offset], generic_or_str );
      offset += sizeof( generic_or_str ) - 1;
    }
    first = FALSE;
    strcat( &line[offset], ib_mad_status_str_no_resources );
    offset += sizeof( ib_mad_status_str_no_resources ) - 1;
  }
  if( ( status & IB_MAD_STATUS_CLASS_MASK ) ==
      IB_SA_MAD_STATUS_REQ_INVALID )
  {
    if( !first )
    {
      strcat( &line[offset], generic_or_str );
      offset += sizeof( generic_or_str ) - 1;
    }
    first = FALSE;
    strcat( &line[offset], ib_mad_status_str_req_invalid );
    offset += sizeof( ib_mad_status_str_req_invalid ) - 1;
  }
  if( ( status & IB_MAD_STATUS_CLASS_MASK ) == IB_SA_MAD_STATUS_NO_RECORDS )
  {
    if( !first )
    {
      strcat( &line[offset], generic_or_str );
      offset += sizeof( generic_or_str ) - 1;
    }
    first = FALSE;
    strcat( &line[offset], ib_mad_status_str_no_records );
    offset += sizeof( ib_mad_status_str_no_records ) - 1;
  }
  if( ( status & IB_MAD_STATUS_CLASS_MASK ) ==
      IB_SA_MAD_STATUS_TOO_MANY_RECORDS )
  {
    if( !first )
    {
      strcat( &line[offset], generic_or_str );
      offset += sizeof( generic_or_str ) - 1;
    }
    first = FALSE;
    strcat( &line[offset], ib_mad_status_str_too_many_records );
    offset += sizeof( ib_mad_status_str_too_many_records ) - 1;
  }
  if( ( status & IB_MAD_STATUS_CLASS_MASK ) ==
      IB_SA_MAD_STATUS_INVALID_GID )
  {
    if( !first )
    {
      strcat( &line[offset], generic_or_str );
      offset += sizeof( generic_or_str ) - 1;
    }
    first = FALSE;
    strcat( &line[offset], ib_mad_status_str_invalid_gid );
    offset += sizeof( ib_mad_status_str_invalid_gid ) - 1;
  }
  if( ( status & IB_MAD_STATUS_CLASS_MASK ) ==
      IB_SA_MAD_STATUS_INSUF_COMPS )
  {
    if( !first )
    {
      strcat( &line[offset], generic_or_str );
      offset += sizeof( generic_or_str ) - 1;
    }
    first = FALSE;
    strcat( &line[offset], ib_mad_status_str_insuf_comps );
    offset += sizeof( ib_mad_status_str_insuf_comps ) - 1;
  }

  return ( line );
}

typedef struct _ibsac_req_context
{
  ibis_t *p_ibis;
  osmv_query_res_t result;
} ibsac_req_context_t;

/**********************************************************************
 **********************************************************************/
void
ibsac_query_res_cb( IN osmv_query_res_t * p_rec )
{
  ibsac_req_context_t *const p_ctxt =
    ( ibsac_req_context_t * ) p_rec->query_context;
  ibis_t *const p_ibis = p_ctxt->p_ibis;

  OSM_LOG_ENTER(&p_ibis->log);

  p_ctxt->result = *p_rec;

  if( p_rec->status != IB_SUCCESS )
  {
    if ( p_rec->status != IB_INVALID_PARAMETER )
    {
      osm_log( &p_ibis->log, OSM_LOG_ERROR,
               "ibsac_query_res_cb: ERR 0003: "
               "Error on query (%s).\n",
               ib_get_err_str(p_rec->status) );
    }
  }

  OSM_LOG_EXIT( &p_ibis->log );
}

/**********************************************************************
 **********************************************************************/
ib_api_status_t
ibsac_query( IN ibis_t *   const p_ibis,
             IN ib_net16_t const attr_id,
             IN void *     const p_attr,
             IN ib_net64_t const comp_mask,
             IN uint8_t    const method,
             OUT uint32_t       *result_count,
             OUT osm_madw_t    **pp_result_madw)
{
  ib_api_status_t status = IB_SUCCESS;
  osmv_user_query_t user;
  osmv_query_req_t req;
  ibsac_req_context_t context;
  ibsac_req_context_t *p_context = &context;

  OSM_LOG_ENTER(&p_ibis->log);

  if( osm_log_is_active( &p_ibis->log, OSM_LOG_DEBUG ) )
  {
    osm_log( &p_ibis->log, OSM_LOG_DEBUG,
             "ibsac_query: "
             "Getting matching %s records.\n",
             ib_get_sa_attr_str( attr_id ) );
  }

  /*
   * Do a blocking query for the requested
   * The result is returned in the result field of the caller's
   * context structure.
   *
   * The query structures are locals.
   */
  memset( &req, 0, sizeof( req ) );
  memset( &user, 0, sizeof( user ) );

  p_context->p_ibis = p_ibis;
  user.method = method;
  user.attr_id = attr_id;
  user.comp_mask = comp_mask;
  user.attr_mod = 0;
  user.p_attr = p_attr;
  switch (attr_id) {
  case IB_MAD_ATTR_NODE_RECORD:
    user.attr_offset = ib_get_attr_offset( sizeof(ib_node_record_t) );
    break;
  case IB_MAD_ATTR_PORTINFO_RECORD:
    user.attr_offset = ib_get_attr_offset( sizeof(ib_portinfo_record_t) );
    break;
  case IB_MAD_ATTR_GUIDINFO_RECORD:
    user.attr_offset = ib_get_attr_offset( sizeof( ib_guidinfo_record_t ) );
    break;
  case IB_MAD_ATTR_SERVICE_RECORD:
    user.attr_offset = ib_get_attr_offset( sizeof( ib_service_record_t ) );
    break;
  case IB_MAD_ATTR_LINK_RECORD:
    user.attr_offset = ib_get_attr_offset( sizeof( ib_link_record_t ) );
    break;
  case IB_MAD_ATTR_LFT_RECORD:
    user.attr_offset = ib_get_attr_offset( sizeof( ib_lft_record_t ) );
    break;
  case IB_MAD_ATTR_MCMEMBER_RECORD:
    user.attr_offset = ib_get_attr_offset( sizeof( ib_member_rec_t ) );
    break;
  case IB_MAD_ATTR_SMINFO_RECORD:
    user.attr_offset = ib_get_attr_offset( sizeof( ib_sminfo_record_t ) );
    break;
  case IB_MAD_ATTR_PATH_RECORD:
    user.attr_offset = ib_get_attr_offset( sizeof( ib_path_rec_t ) );
    break;
  case IB_MAD_ATTR_VLARB_RECORD:
    user.attr_offset = ib_get_attr_offset( sizeof( ib_vl_arb_table_record_t ) );
    break;
  case IB_MAD_ATTR_SLVL_RECORD:
    user.attr_offset = ib_get_attr_offset( sizeof( ib_slvl_table_record_t ) );
    break;
  case IB_MAD_ATTR_PKEY_TBL_RECORD:
    user.attr_offset = ib_get_attr_offset( sizeof( ib_pkey_table_record_t ) );
    break;
  case IB_MAD_ATTR_TRACE_RECORD:
  case IB_MAD_ATTR_MULTIPATH_RECORD:
  case IB_MAD_ATTR_SVC_ASSOCIATION_RECORD:
    osm_log( &p_ibis->log, OSM_LOG_ERROR,
             "ibsac_query: ERR 0004: "
             "Unsupported attribute (%u).\n", attr_id );
    status = IB_ERROR;
    goto Exit;
  }

  req.query_type = OSMV_QUERY_USER_DEFINED;
  req.timeout_ms = p_ibis->p_opt->transaction_timeout;
  /* req.retry_cnt = p_ibis->p_opt->retry_count; */
  req.retry_cnt = 5; /* HACK we currently always use retry of 5 */
  req.flags = OSM_SA_FLAGS_SYNC;
  req.query_context = p_context;
  req.pfn_query_cb = ibsac_query_res_cb;
  req.p_query_input = &user;
  req.sm_key = cl_ntoh64(p_ibis->p_opt->sm_key);

  *result_count = 0;
  *pp_result_madw = p_context->result.p_result_madw = NULL;

  status = osmv_query_sa( IbSacBindHndl, &req );

  if( status != IB_SUCCESS )
  {
    osm_log( &p_ibis->log, OSM_LOG_ERROR,
             "ibsac_query: ERR 0004: "
             "ib_query failed (%s).\n", ib_get_err_str( status ) );
    goto Exit;
  }

  status = p_context->result.status;

  if( status != IB_SUCCESS )
  {
    osm_log( &p_ibis->log, OSM_LOG_ERROR,
             "ibsac_query: ERR 0064: "
             "ib_query failed (%s).\n", ib_get_err_str( status ) );

    if( status == IB_REMOTE_ERROR )
    {
      osm_log( &p_ibis->log, OSM_LOG_ERROR,
               "ibsac_query: "
               "Remote error = %s.\n",
               ib_get_mad_status_str(
                 osm_madw_get_mad_ptr
                 ( p_context->result.p_result_madw ) ) );
    }
    goto Exit;
  }

  *result_count = p_context->result.result_cnt;

 Exit:
  /* we must return the mad pointer even on error so we can return
     it to the pool */
  *pp_result_madw = p_context->result.p_result_madw;
  OSM_LOG_EXIT( &p_ibis->log );
  return ( status );
}


