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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <complib/cl_debug.h>
#include "ibis.h"
#include "ibis_api.h"
#include "git_version.h"
#include <opensm/osm_log.h>
#include <opensm/osm_mad_pool.h>
#include <opensm/osm_helper.h>


/****g* IBIS/IbisObj
 * NAME
 * IbisObj
 *
 * DESCRIPTION
 * global IBIS Object
 *
 * SYNOPSIS
 */
ibis_t IbisObj;
/*
 * SEE ALSO
 * IBIS object, ibis_construct, ibis_destroy
 *********/

/**********************************************************************
 **********************************************************************/
void
ibis_construct(void) {
  memset( &IbisObj, 0, sizeof( ibis_t ) );
  IbisObj.initialized = FALSE;
  IbisObj.trans_id = 0x1;
  osm_log_construct( &(IbisObj.log) );
  osm_mad_pool_construct( &(IbisObj.mad_pool));
  cl_disp_construct( &(IbisObj.disp) );
  ibis_gsi_mad_ctrl_construct( &(IbisObj.mad_ctrl) );
}

/**********************************************************************
 **********************************************************************/
void
ibis_destroy(void) {

  if (! IbisObj.initialized) return;

  ibis_gsi_mad_ctrl_destroy( &(IbisObj.mad_ctrl) );
  cl_disp_destroy( &(IbisObj.disp) );

  if (IbisObj.p_vendor)
    osm_vendor_delete(&(IbisObj.p_vendor));

  osm_mad_pool_destroy( &(IbisObj.mad_pool));

  osm_log_destroy( &(IbisObj.log) );

  IbisObj.initialized = FALSE;
}

/**********************************************************************
 **********************************************************************/
ib_api_status_t
ibis_init(
  IN ibis_opt_t *p_opt,
  IN const osm_log_level_t log_flags
  )
{
  ib_api_status_t status;

  IbisObj.port_guid = 0;
  ibis_construct();
  status = osm_log_init( &(IbisObj.log),
                         p_opt->force_log_flush,0x01, p_opt->log_file
#ifdef OSM_BUILD_OPENIB
                         , FALSE /* do not accumulate log ... */
#endif
                         );
  if( status != IB_SUCCESS )
    return ( status ); // no log ....

  /* but we do not want any extra staff here */
  osm_log_set_level( &(IbisObj.log), log_flags );

  osm_log( &(IbisObj.log), OSM_LOG_FUNCS,"ibis_init: [\n" );

  IbisObj.p_opt = p_opt;

  status = osm_mad_pool_init( &(IbisObj.mad_pool) );
  if( status != IB_SUCCESS )
  {
    osm_log( &IbisObj.log, OSM_LOG_ERROR,
             "ibis_init: ERR 0002: "
             "Unable to allocate MAD pool\n");
    goto Exit;
  }

  IbisObj.p_vendor = osm_vendor_new( &(IbisObj.log),
                                     p_opt->transaction_timeout );


  if( IbisObj.p_vendor == NULL )
  {
    status = IB_INSUFFICIENT_RESOURCES;
    osm_log( &IbisObj.log, OSM_LOG_ERROR,
             "ibis_init: ERR 0001: "
             "Unable to allocate vendor object" );
    goto Exit;
  }

  if (IbisObj.p_opt->single_thread)
  {
    status = cl_disp_init(&(IbisObj.disp),1,"gsi" );
  }
  else
  {
    status = cl_disp_init(&(IbisObj.disp),0,"gsi" );
  }

  if( status != IB_SUCCESS )
  {
    osm_log( &IbisObj.log, OSM_LOG_ERROR,
             "ibis_init: ERR 0003: "
             "Unable to allocate GSI MADs Dispatcher\n");
    goto Exit;
  }

  status = ibis_gsi_mad_ctrl_init(&(IbisObj.mad_ctrl),
                                  &(IbisObj.mad_pool),
                                  IbisObj.p_vendor,
                                  &(IbisObj.log),
                                  &(IbisObj.disp));

  if( status != IB_SUCCESS )
  {
    osm_log( &IbisObj.log, OSM_LOG_ERROR,
             "ibis_init: ERR 0003: "
             "Unable to allocate GSI MADs Control\n");
    goto Exit;
  }

  IbisObj.initialized = TRUE;
 Exit:
  return ( status );
}

/**********************************************************************
 **********************************************************************/

const ibis_gsi_mad_ctrl_t*
ibis_get_gsi_mad_ctrl()
{
  return (&(IbisObj.mad_ctrl));
}

/**********************************************************************
 **********************************************************************/
ib_api_status_t
ibis_get_ports_status(
  IN OUT uint32_t *num_ports,
  IN OUT ibis_port_info_t ports_array[] )
{
  uint32_t i;
  ib_api_status_t status;
  ib_port_attr_t attr_array[MAX_LOCAL_IBPORTS];

  OSM_LOG_ENTER(&(IbisObj.log));

  for (i = 0; i < MAX_LOCAL_IBPORTS; i++)
  {
    attr_array[i].num_pkeys = 0;
    attr_array[i].p_pkey_table = NULL;
  }
  *num_ports = MAX_LOCAL_IBPORTS;
  status = osm_vendor_get_all_port_attr(
    IbisObj.p_vendor,
    attr_array,
    num_ports );

  if( status != IB_SUCCESS )
  {
    osm_log( &IbisObj.log, OSM_LOG_ERROR,
             "ibis_get_ports_status: ERR 0001: "
             "Unable to obtain ports information - got err code:%u.\n",
             status);
    goto Exit;
  }

  if( num_ports == 0 )
  {
    status = IB_ERROR;
    goto Exit;
  }

  for( i = 0; i < *num_ports; i++ )
  {
    ports_array[i].port_guid = cl_ntoh64(attr_array[i].port_guid);
    ports_array[i].lid = cl_ntoh16( attr_array[i].lid );
    ports_array[i].link_state = attr_array[i].link_state;
  }

  Exit :
    OSM_LOG_EXIT( &(IbisObj.log) );
  return (status);
}

/**********************************************************************
 **********************************************************************/
ib_net64_t
ibis_get_tid(void)
{
  return( cl_ntoh64(cl_atomic_inc(&IbisObj.trans_id)));
}


#ifndef IBIS_CODE_VERSION
#define IBIS_CODE_VERSION "undefined"
#endif
const char * ibisSourceVersion = IBIS_CODE_VERSION ;
