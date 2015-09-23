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
 *    Implementation of ibcr_t.
 * This object represents the Subnet Performance Monitor object.
 * This object is part of the IBIS family of objects.
 *
 * Environment:
 *    Linux User Mode
 *
 * $Revision: 1.3 $
 */

#include <string.h>
#include <complib/cl_qmap.h>
#include <complib/cl_passivelock.h>
#include <complib/cl_debug.h>
#include <iba/ib_types.h>
#include "ibcr.h"
#include <opensm/osm_madw.h>
#include <opensm/osm_log.h>
#include <opensm/osm_mad_pool.h>
#include <opensm/osm_msgdef.h>

/**********************************************************************
 **********************************************************************/

ibcr_t*
ibcr_construct()
{
  ibcr_t* p_ibcr;

  OSM_LOG_ENTER(&(IbisObj.log));

  p_ibcr = malloc(sizeof(ibcr_t));
  if (p_ibcr == NULL)
  {
    goto Exit;
  }

  memset(p_ibcr, 0, sizeof(ibcr_t));
  p_ibcr->state = IBCR_STATE_INIT;

  Exit :
    OSM_LOG_EXIT(&(IbisObj.log));
  return(p_ibcr);
}

/**********************************************************************
 **********************************************************************/
void
ibcr_destroy(
  IN ibcr_t* const p_ibcr )
{
  OSM_LOG_ENTER(&(IbisObj.log));

  p_ibcr->state = IBCR_STATE_INIT;

  OSM_LOG_EXIT( &(IbisObj.log) );
}

/**********************************************************************
 **********************************************************************/
ib_api_status_t
ibcr_init(
  IN ibcr_t* const p_ibcr )
{
  ib_api_status_t status = IB_SUCCESS;

  OSM_LOG_ENTER(&(IbisObj.log));

  p_ibcr->state = IBCR_STATE_READY;

  OSM_LOG_EXIT( &(IbisObj.log) );
  return( status );
}

/**********************************************************************
 **********************************************************************/

ib_api_status_t
ibcr_bind(
  IN ibcr_t* const p_ibcr )
{
  ib_api_status_t status;

  OSM_LOG_ENTER(&(IbisObj.log));

  status = ibis_gsi_mad_ctrl_bind(
    &(IbisObj.mad_ctrl),
    CR_CLASS, 1,
    &p_ibcr->h_bind
    );

  if( status != IB_SUCCESS )
  {
    goto Exit;
  }

  status = ibis_gsi_mad_ctrl_set_class_attr_cb(
    &(IbisObj.mad_ctrl),
    CR_CLASS ,
    CR_ATTR_50 ,
    ibis_gsi_sync_mad_batch_callback,
    (void *)p_ibcr);

  if( status != IB_SUCCESS )
  {
    goto Exit;
  }

  status = ibis_gsi_mad_ctrl_set_class_attr_cb(
    &(IbisObj.mad_ctrl),
    CR_CLASS ,
    CR_ATTR_51 ,
    ibis_gsi_sync_mad_batch_callback,
    (void *)p_ibcr);

  Exit :

    OSM_LOG_EXIT( &(IbisObj.log) );
  return( status );
}


/**********************************************************************
 **********************************************************************/

static void
__ibcr_prep_cr_mad(
  IN ibcr_t* p_ibcr,
  IN uint16_t lid,
  IN uint8_t method,
  IN uint32_t data,
  IN uint32_t address,
  OUT osm_madw_t **pp_madw)
{

  osm_mad_addr_t        mad_addr;
  osm_madw_t            *p_madw;
  uint32_t              attr_mod=0;
  uint16_t              attr_id=0;

  OSM_LOG_ENTER(&(IbisObj.log));

  attr_mod = (((address & 0x00ff0000) << 8) | (0x01 << 16) | (address & 0xffff));

  attr_id = CR_ATTR_50;

  mad_addr.dest_lid = cl_hton16(lid);
  mad_addr.path_bits = 0;
  mad_addr.static_rate = 0;
  mad_addr.addr_type.gsi.remote_qp=cl_hton32(1);
  mad_addr.addr_type.gsi.remote_qkey = cl_hton32(0x80010000);
  mad_addr.addr_type.gsi.pkey_ix = 0;
  mad_addr.addr_type.gsi.service_level = 0;
  mad_addr.addr_type.gsi.global_route = FALSE;

  p_madw =
    osm_mad_pool_get(
      &(IbisObj.mad_pool),p_ibcr->h_bind,MAD_PAYLOAD_SIZE,&mad_addr);
  *pp_madw = p_madw;

  p_madw->resp_expected = TRUE;
  ((ib_mad_t *)p_madw->p_mad)->method = method;
  ((ib_mad_t *)p_madw->p_mad)->class_ver = 1;
  ((ib_mad_t *)p_madw->p_mad)->mgmt_class = CR_CLASS;
  ((ib_mad_t *)p_madw->p_mad)->base_ver = 1;
  ((ib_mad_t *)p_madw->p_mad)->attr_id = cl_hton16(attr_id);
  ((ib_mad_t *)p_madw->p_mad)->attr_mod = cl_hton32(attr_mod);
  ((ib_mad_t *)p_madw->p_mad)->trans_id = ibis_get_tid();

  ((ib_cr_space_t *)p_madw->p_mad)->vendor_key = cl_hton64(IbisObj.p_opt->v_key);
  ((ib_cr_space_t *)p_madw->p_mad)->data[0] = cl_hton32(data);

  OSM_LOG_EXIT(&(IbisObj.log));
}

/**********************************************************************
 **********************************************************************/
ib_api_status_t
ibcr_read(
  IN ibcr_t* const p_ibcr,
  IN uint16_t lid,
  IN uint32_t address,
  OUT ib_cr_space_t *p_cr_space_mad)
{

  ib_api_status_t status;
  osm_madw_t     *p_madw_arr[1];

  OSM_LOG_ENTER(&(IbisObj.log));

  /* prepare the mad */
  __ibcr_prep_cr_mad(
    p_ibcr,
    lid,
    VENDOR_GET,
    0x0,
    address,
    &p_madw_arr[0]);

  status = ibis_gsi_send_sync_mad_batch(
    &(IbisObj.mad_ctrl),
    p_ibcr->h_bind,
    1,
    p_madw_arr,
    sizeof(ib_cr_space_t),
    (uint8_t*)p_cr_space_mad);

  OSM_LOG_EXIT(&(IbisObj.log));
  return (status);
}

/**********************************************************************
 **********************************************************************/
ib_api_status_t
ibcr_write(
  IN ibcr_t* const p_ibcr,
  IN uint16_t lid,
  IN uint32_t data,
  IN uint32_t address)
{
  ib_api_status_t status;

  ib_cr_space_t res_mad;
  osm_madw_t   *p_madw_arr[1];

  OSM_LOG_ENTER(&(IbisObj.log));

  __ibcr_prep_cr_mad(
    p_ibcr,
    lid,
    VENDOR_SET,
    data,
    address,
    &p_madw_arr[0]);

  status = ibis_gsi_send_sync_mad_batch(
    &(IbisObj.mad_ctrl),
    p_ibcr->h_bind,
    1,
    p_madw_arr,
    sizeof(ib_cr_space_t),
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
ibcr_multi_read(
  IN ibcr_t* const p_ibcr,
  IN uint8_t num,
  IN uint16_t lid_list[],
  IN uint32_t address,
  OUT ib_cr_space_t *p_cr_space_mad_list)
{

  ib_api_status_t status;
  uint8_t i;
  osm_madw_t          *p_madw_arr[IBCR_MULTI_MAX];

  OSM_LOG_ENTER(&(IbisObj.log));

  if (num > IBCR_MULTI_MAX)
  {
    status = IB_ERROR;
    goto Exit;
  }

  for (i=0 ; i <num ; i++ ) {
    __ibcr_prep_cr_mad(
      p_ibcr,
      lid_list[i],
      VENDOR_GET,
      0x0,
      address,
      &p_madw_arr[i]);
  }

  status = ibis_gsi_send_sync_mad_batch(
    &(IbisObj.mad_ctrl),
    p_ibcr->h_bind,
    num,
    p_madw_arr,
    sizeof(ib_cr_space_t),
    (uint8_t*)p_cr_space_mad_list);

 Exit:
  OSM_LOG_EXIT(&(IbisObj.log));
  return (status);
}

/**********************************************************************
 **********************************************************************/
ib_api_status_t
ibcr_multi_write(
  IN ibcr_t* const p_ibcr,
  IN uint8_t num,
  IN uint16_t lid_list[],
  IN uint32_t data,
  IN uint32_t address)
{
  ib_api_status_t status;
  uint8_t         i;
  osm_madw_t     *p_madw_arr[IBCR_MULTI_MAX];
  ib_cr_space_t   res_mads[IBCR_MULTI_MAX];

  OSM_LOG_ENTER(&(IbisObj.log));

  if (num > IBCR_MULTI_MAX)
  {
    status = IB_ERROR;
    goto Exit;
  }

  for (i=0 ; i <num ; i++ ) {
    __ibcr_prep_cr_mad(
      p_ibcr,
      lid_list[i],
      VENDOR_SET,
      data,
      address,
      &p_madw_arr[i]);
  }

  status = ibis_gsi_send_sync_mad_batch(
    &(IbisObj.mad_ctrl),
    p_ibcr->h_bind,
    num,
    p_madw_arr,
    sizeof(ib_cr_space_t),
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

