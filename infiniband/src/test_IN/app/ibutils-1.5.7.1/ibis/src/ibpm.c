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
 *    Implementation of ibpm_t.
 * This object represents the Subnet Performance Monitor object.
 * This object is part of the IBIS family of objects.
 *
 * Environment:
 *    Linux User Mode
 *
 * $Revision: 1.5 $
 */

#include <string.h>
#include <complib/cl_qmap.h>
#include <complib/cl_passivelock.h>
#include <complib/cl_debug.h>
#include <iba/ib_types.h>
#include "ibpm.h"
#include <opensm/osm_madw.h>
#include <opensm/osm_log.h>
#include <opensm/osm_mad_pool.h>
#include <opensm/osm_msgdef.h>

/**********************************************************************
 **********************************************************************/

ibpm_t*
ibpm_construct()
{
  ibpm_t* p_ibpm;

  OSM_LOG_ENTER(&(IbisObj.log));

  p_ibpm = malloc(sizeof(ibpm_t));
  if (p_ibpm == NULL)
  {
    goto Exit;
  }

  memset (p_ibpm, 0, sizeof(ibpm_t));
  Exit :
    OSM_LOG_EXIT(&(IbisObj.log));
  return(p_ibpm);
}

/**********************************************************************
 **********************************************************************/
void
ibpm_destroy(
  IN ibpm_t* const p_ibpm )
{
  OSM_LOG_ENTER(&(IbisObj.log));

  p_ibpm->state = IBPM_STATE_INIT;
  OSM_LOG_EXIT( &(IbisObj.log) );
}

/**********************************************************************
 **********************************************************************/
ib_api_status_t
ibpm_init(
  IN ibpm_t* const p_ibpm )
{
  ib_api_status_t status = IB_SUCCESS;

  OSM_LOG_ENTER(&(IbisObj.log));
  p_ibpm->state = IBPM_STATE_INIT;

  OSM_LOG_EXIT( &(IbisObj.log) );
  return( status );
}

/**********************************************************************
 **********************************************************************/

ib_api_status_t
ibpm_bind(
  IN ibpm_t* const p_ibpm )
{
  ib_api_status_t status;

  OSM_LOG_ENTER(&(IbisObj.log));

  status = ibis_gsi_mad_ctrl_bind(
    &(IbisObj.mad_ctrl),
    IB_MCLASS_PERF, 1,
    &p_ibpm->h_bind
    );

  if( status != IB_SUCCESS )
  {
    goto Exit;
  }

  status = ibis_gsi_mad_ctrl_set_class_attr_cb(
    &(IbisObj.mad_ctrl),
    PERF_CLASS ,
    PERF_PORTS_COUNTER ,
    ibis_gsi_sync_mad_batch_callback,
    (void *)p_ibpm);

  status = ibis_gsi_mad_ctrl_set_class_attr_cb(
    &(IbisObj.mad_ctrl),
    PERF_CLASS ,
    PERF_PORTS_COUNTER_EXTENDED ,
    ibis_gsi_sync_mad_batch_callback,
    (void *)p_ibpm);

  Exit :
    OSM_LOG_EXIT( &(IbisObj.log) );
  return( status );
}


/**********************************************************************
 **********************************************************************/

static void
__ibpm_prep_port_counter_mad(
  IN ibpm_t*       p_ibpm,
  IN uint16_t      lid,
  IN uint8_t       method,
  IN uint8_t       port_select,
  IN uint16_t      counter_select,
  IN uint16_t      attr,
  OUT osm_madw_t **pp_madw
  )
{
  osm_mad_addr_t        mad_addr;
  osm_madw_t            *p_madw;

  OSM_LOG_ENTER(&(IbisObj.log));

  osm_log(&(IbisObj.log), OSM_LOG_DEBUG,
          "__ibpm_prep_port_counter_mad: "
          " Sending method:0x%X lid:0x%04X port:0x%X counters:0x%X\n",
          method, lid, port_select, counter_select);


  mad_addr.dest_lid = cl_hton16(lid);
  mad_addr.path_bits = 0;
  mad_addr.static_rate = 0;
  mad_addr.addr_type.gsi.remote_qp=cl_hton32(1);
  mad_addr.addr_type.gsi.remote_qkey = cl_hton32(0x80010000);
  mad_addr.addr_type.gsi.pkey_ix = 0;
  mad_addr.addr_type.gsi.service_level = 0;
  mad_addr.addr_type.gsi.global_route = FALSE;

  p_madw = osm_mad_pool_get(
    &(IbisObj.mad_pool), p_ibpm->h_bind, MAD_PAYLOAD_SIZE, &mad_addr);
  *pp_madw = p_madw;
  p_madw->resp_expected = TRUE;

  ((ib_mad_t *)p_madw->p_mad)->method = method;
  ((ib_mad_t *)p_madw->p_mad)->class_ver = 1;
  ((ib_mad_t *)p_madw->p_mad)->mgmt_class = PERF_CLASS;
  ((ib_mad_t *)p_madw->p_mad)->base_ver = 1;
  ((ib_mad_t *)p_madw->p_mad)->attr_id = cl_hton16(attr);
  ((ib_mad_t *)p_madw->p_mad)->attr_mod = cl_hton32(0);
  ((ib_mad_t *)p_madw->p_mad)->trans_id = ibis_get_tid();

  ((ib_pm_port_counter_t *)p_madw->p_mad)->port_select = port_select;
  ((ib_pm_port_counter_t *)p_madw->p_mad)->counter_select = cl_hton16(counter_select);

  /* NOTE: we do not send the mads - just prepare them */
  OSM_LOG_EXIT(&(IbisObj.log));
}

/**********************************************************************
 **********************************************************************/
ib_api_status_t
ibpm_get_counters(
  IN ibpm_t* const p_ibpm,
  IN uint16_t lid,
  IN uint8_t port_select,
  OUT ib_pm_port_counter_t* p_ibpm_port_counter_mad)
{

  osm_madw_t          *p_madw_arr[1];
  ib_api_status_t      status;
  OSM_LOG_ENTER(&(IbisObj.log));

  /* prepare the mad */
  __ibpm_prep_port_counter_mad(
    p_ibpm,
    lid,
    VENDOR_GET,
    port_select,
    0xffff, /* counter select */
    PERF_PORTS_COUNTER,
    &p_madw_arr[0]
    );

  status = ibis_gsi_send_sync_mad_batch(
    &(IbisObj.mad_ctrl),
    p_ibpm->h_bind,
    1,
    p_madw_arr,
    sizeof(ib_pm_port_counter_t),
    (uint8_t*)p_ibpm_port_counter_mad);

  OSM_LOG_EXIT(&(IbisObj.log));
  return (status);
}

/**********************************************************************
 **********************************************************************/
ib_api_status_t
ibpm_get_multi_counters(
  IN ibpm_t* const p_ibpm,
  IN uint8_t num,
  IN uint16_t lid_list[],
  IN uint8_t port_select_list[],
  OUT ib_pm_port_counter_t *p_ibpm_port_counter_mad_list)
{

  osm_madw_t           *p_madw_arr[IBPM_MULTI_MAX];
  unsigned int          i;
  ib_api_status_t       status;

  OSM_LOG_ENTER(&(IbisObj.log));

  if (num > IBPM_MULTI_MAX)
  {
    status = IB_ERROR;
    goto Exit;
  }

  /* prepare the mads */
  for (i = 0; i < num; i++)
  {
    __ibpm_prep_port_counter_mad(
      p_ibpm,
      lid_list[i],
      VENDOR_GET,
      port_select_list[i],
      0xffff, /* counter select */
      PERF_PORTS_COUNTER,
      &p_madw_arr[i]
      );
  }

  /* send */
  status = ibis_gsi_send_sync_mad_batch(
    &(IbisObj.mad_ctrl),
    p_ibpm->h_bind,
    num,
    p_madw_arr,
    sizeof(ib_pm_port_counter_t),
    (uint8_t*)p_ibpm_port_counter_mad_list);

 Exit:
  OSM_LOG_EXIT(&(IbisObj.log));
  return (status);
}

/**********************************************************************
 **********************************************************************/
ib_api_status_t
ibpm_get_multi_counters_extended(
  IN ibpm_t* const p_ibpm,
  IN uint8_t num,
  IN uint16_t lid_list[],
  IN uint8_t port_select_list[],
  OUT ib_pm_port_counter_extended_t *p_ibpm_port_counter_mad_list)
{

  osm_madw_t           *p_madw_arr[IBPM_MULTI_MAX];
  unsigned int          i;
  ib_api_status_t       status;

  OSM_LOG_ENTER(&(IbisObj.log));

  if (num > IBPM_MULTI_MAX)
  {
    status = IB_ERROR;
    goto Exit;
  }

  /* prepare the mads */
  for (i = 0; i < num; i++)
  {
    __ibpm_prep_port_counter_mad(
      p_ibpm,
      lid_list[i],
      VENDOR_GET,
      port_select_list[i],
      0xffff, /* counter select */
      PERF_PORTS_COUNTER_EXTENDED,
      &p_madw_arr[i]
      );
  }

  /* send */
  status = ibis_gsi_send_sync_mad_batch(
    &(IbisObj.mad_ctrl),
    p_ibpm->h_bind,
    num,
    p_madw_arr,
    sizeof(ib_pm_port_counter_t),
    (uint8_t*)p_ibpm_port_counter_mad_list);

 Exit:
  OSM_LOG_EXIT(&(IbisObj.log));
  return (status);
}

/**********************************************************************
 **********************************************************************/
ib_api_status_t
ibpm_clr_all_counters(
  IN ibpm_t* const p_ibpm,
  IN uint16_t lid,
  IN uint8_t port_select)
{

  ib_pm_port_counter_t res_mad;
  osm_madw_t          *p_madw_arr[1];
  ib_api_status_t      status;

  OSM_LOG_ENTER(&(IbisObj.log));

  /* prepare the mad */
  __ibpm_prep_port_counter_mad(
    p_ibpm,
    lid,
    VENDOR_SET,
    port_select,
    0xffff, /* counter select */
    PERF_PORTS_COUNTER,
    &p_madw_arr[0]
    );

  status = ibis_gsi_send_sync_mad_batch(
    &(IbisObj.mad_ctrl),
    p_ibpm->h_bind,
    1,
    p_madw_arr,
    sizeof(ib_pm_port_counter_t),
    (uint8_t*)&res_mad);

  if (status == IB_SUCCESS)
    status = ibis_get_mad_status((ib_mad_t*)&res_mad);

  if (! ((ib_mad_t *)&res_mad)->trans_id)
    status = IB_ERROR;

  OSM_LOG_EXIT(&(IbisObj.log));
  return (status);
}

/**********************************************************************
 **********************************************************************/
ib_api_status_t
ibpm_clr_all_multi_counters(
  IN ibpm_t* const p_ibpm,
  IN uint8_t num,
  IN uint16_t lid_list[],
  IN uint8_t port_select_list[])
{

  ib_pm_port_counter_t res_mads[IBPM_MULTI_MAX];
  osm_madw_t          *p_madw_arr[IBPM_MULTI_MAX];
  ib_api_status_t      status;
  uint8_t              i;

  OSM_LOG_ENTER(&(IbisObj.log));

  if (num > IBPM_MULTI_MAX)
  {
    status = IB_ERROR;
    goto Exit;
  }

  /* prepare the mads */
  for (i = 0; i < num; i++)
  {
    __ibpm_prep_port_counter_mad(
      p_ibpm,
      lid_list[i],
      VENDOR_SET,
      port_select_list[i],
      0xffff, /* counter select */
      PERF_PORTS_COUNTER,
      &p_madw_arr[i]
      );
  }

  status = ibis_gsi_send_sync_mad_batch(
    &(IbisObj.mad_ctrl),
    p_ibpm->h_bind,
    num,
    p_madw_arr,
    sizeof(ib_pm_port_counter_t),
    (uint8_t*)res_mads);

  /* check some commands passed in success */
  if (status == IB_SUCCESS)
  {
    for (i = 0; i < num; i++)
    {
      status = ibis_get_mad_status((ib_mad_t*)&res_mads[i]);
      if (status == IB_SUCCESS)
      {
        break;
      }
    }
  }

 Exit:
  OSM_LOG_EXIT(&(IbisObj.log));
  return (status);

}
