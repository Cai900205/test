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
 * $Revision: 1.2 $
 */

#include <string.h>
#include <complib/cl_qmap.h>
#include <complib/cl_passivelock.h>
#include <complib/cl_debug.h>
#include <iba/ib_types.h>
#include "ibbbm.h"
#include <opensm/osm_madw.h>
#include <opensm/osm_log.h>
#include <opensm/osm_mad_pool.h>
#include <opensm/osm_msgdef.h>


/****g* IBBBM
 * NAME
 * ibbbm_mad p_ibbbm_mad
 *
 * DESCRIPTION
 * global IBBBM struct and a pointer to that struct
 *
 * SYNOPSIS
 */
ib_bbm_vpd_t *p_ibbbm_vpd_mad;
ib_bbm_vpd_t ibbbm_vpd_mad;
/*
 * SEE ALSO
 * IBBBM object, ibbbm_construct, ibbbm_destroy
 *********/


/**********************************************************************
 **********************************************************************/

ibbbm_t*
ibbbm_construct()
{
  ibbbm_t* p_ibbbm;

  OSM_LOG_ENTER(&(IbisObj.log));

  p_ibbbm = malloc(sizeof(ibbbm_t));
  if (p_ibbbm == NULL)
  {
    goto Exit;
  }

  memset (p_ibbbm, 0, sizeof(ibbbm_t));
  p_ibbbm->state = IBBBM_STATE_INIT;
  p_ibbbm->bm_sequence = 0;
  cl_event_construct(&p_ibbbm->wait_for_resp);

  Exit :
    OSM_LOG_EXIT(&(IbisObj.log));
  return(p_ibbbm);
}

/**********************************************************************
 **********************************************************************/
void
ibbbm_destroy(
  IN ibbbm_t* const p_ibbbm )
{
  OSM_LOG_ENTER(&(IbisObj.log));

  p_ibbbm->state = IBBBM_STATE_INIT;
  cl_event_destroy(&p_ibbbm->wait_for_resp);

  OSM_LOG_EXIT( &(IbisObj.log) );
}

/**********************************************************************
 **********************************************************************/
ib_api_status_t
ibbbm_init(
  IN ibbbm_t* const p_ibbbm )
{
  ib_api_status_t status = IB_SUCCESS;

  OSM_LOG_ENTER(&(IbisObj.log));

  cl_event_init(&p_ibbbm->wait_for_resp, FALSE); // FALSE: auto reset
  p_ibbbm->state = IBBBM_STATE_READY;

  OSM_LOG_EXIT( &(IbisObj.log) );
  return( status );
}

/**********************************************************************
 **********************************************************************/
static void
__ibbbm_space_rcv_callback(
  IN void* context,
  IN void* p_data)
{
  osm_madw_t* const p_madw = (osm_madw_t*)p_data;
  /* HACK : how do we get the context from the mad itself ??? */
  ibbbm_t* p_ibbbm = (ibbbm_t*)context;

  OSM_LOG_ENTER(&(IbisObj.log));

  memcpy(&ibbbm_vpd_mad,p_madw->p_mad,sizeof(ib_bbm_vpd_t));

  p_ibbbm_vpd_mad = (ib_bbm_vpd_t *)&ibbbm_vpd_mad;

  /* Signal for the waiter we have got some data */
  cl_event_signal(&p_ibbbm->wait_for_resp);

  OSM_LOG_EXIT( &(IbisObj.log) );
};


/**********************************************************************
 **********************************************************************/

ib_api_status_t
ibbbm_bind(
  IN ibbbm_t* const p_ibbbm )
{
  ib_api_status_t status;

  OSM_LOG_ENTER(&(IbisObj.log));

  status = ibis_gsi_mad_ctrl_bind(
    &(IbisObj.mad_ctrl),
    BBM_CLASS, 1,
    &p_ibbbm->h_bind
    );

  if( status != IB_SUCCESS )
  {
    goto Exit;
  }

  status = ibis_gsi_mad_ctrl_set_class_attr_cb(&(IbisObj.mad_ctrl),BBM_CLASS ,BBM_ATTR_WRITEVPD ,__ibbbm_space_rcv_callback, (void *)p_ibbbm);

  if( status != IB_SUCCESS )
  {
    goto Exit;
  }

  status = ibis_gsi_mad_ctrl_set_class_attr_cb(&(IbisObj.mad_ctrl),BBM_CLASS ,BBM_ATTR_READVPD ,__ibbbm_space_rcv_callback, (void *)p_ibbbm);

  Exit :

    OSM_LOG_EXIT( &(IbisObj.log) );
  return( status );
}


/**********************************************************************
 **********************************************************************/

static ib_api_status_t
__ibbbm_vpd(
  IN ibbbm_t* p_ibbbm,
  IN uint16_t lid,
  IN uint16_t attr_id,
  IN uint64_t* p_trans_id,
  IN uint8_t vpd_device_selector,
  IN uint16_t bytes_num,
  IN uint16_t offset,
  IN uint8_t *p_data)
{

  ib_api_status_t       status;
  osm_mad_addr_t        mad_addr;
  osm_madw_t            *p_madw;
  uint64_t              trans_id;
  uint16_t              bm_sequence;

  OSM_LOG_ENTER(&(IbisObj.log));

  trans_id = ibis_get_tid();
  bm_sequence =(uint16_t)cl_atomic_inc(&p_ibbbm->bm_sequence);

  *p_trans_id = cl_ntoh64(trans_id);


  mad_addr.dest_lid = cl_hton16(lid);
  mad_addr.path_bits = 0;
  mad_addr.static_rate = 0;
  mad_addr.addr_type.gsi.remote_qp=cl_hton32(1);
  mad_addr.addr_type.gsi.remote_qkey = cl_hton32(0x80010000);
  mad_addr.addr_type.gsi.pkey_ix = 0;
  mad_addr.addr_type.gsi.service_level = 0;
  mad_addr.addr_type.gsi.global_route = FALSE;

  p_madw = osm_mad_pool_get(&(IbisObj.mad_pool),p_ibbbm->h_bind,MAD_PAYLOAD_SIZE,&mad_addr);
  p_madw->resp_expected = TRUE;

  if (attr_id == BBM_ATTR_READVPD)
  {
    ((ib_mad_t *)p_madw->p_mad)->method = VENDOR_GET;
  }
  else
  {
    ((ib_mad_t *)p_madw->p_mad)->method = VENDOR_SET;
  }

  ((ib_mad_t *)p_madw->p_mad)->class_ver = 1;
  ((ib_mad_t *)p_madw->p_mad)->mgmt_class = BBM_CLASS;
  ((ib_mad_t *)p_madw->p_mad)->base_ver = 1;


  ((ib_mad_t *)p_madw->p_mad)->attr_id = cl_hton16(attr_id);


  ((ib_mad_t *)p_madw->p_mad)->attr_mod = cl_hton32(0);
  ((ib_mad_t *)p_madw->p_mad)->trans_id = trans_id;

  ((ib_bbm_vpd_t *)p_madw->p_mad)->b_key = cl_hton64(0);
  ((ib_bbm_vpd_t *)p_madw->p_mad)->bm_sequence = cl_hton16(bm_sequence);
  ((ib_bbm_vpd_t *)p_madw->p_mad)->bm_source_device = 0xfe;
  ((ib_bbm_vpd_t *)p_madw->p_mad)->bm_parm_count = bytes_num+5;
  ((ib_bbm_vpd_t *)p_madw->p_mad)->vpd_device_selector = vpd_device_selector;
  ((ib_bbm_vpd_t *)p_madw->p_mad)->bytes_num = cl_hton16(bytes_num);
  ((ib_bbm_vpd_t *)p_madw->p_mad)->offset = cl_hton16(offset);

  if (p_data != NULL)
  {
    memcpy(((ib_bbm_vpd_t *)p_madw->p_mad)->data,p_data,bytes_num);
  };

  status = osm_vendor_send(p_ibbbm->h_bind,p_madw,TRUE);

  OSM_LOG_EXIT(&(IbisObj.log));

  return(status);
}

/**********************************************************************
 **********************************************************************/
ib_api_status_t
ibbbm_read_vpd(
  IN ibbbm_t* const p_ibbbm,
  IN uint16_t lid,
  IN uint8_t vpd_device_selector,
  IN uint16_t bytes_num,
  IN uint16_t offset,
  OUT ib_bbm_vpd_t *p_bbm_vpd_mad)
{

  ib_api_status_t status;
  uint64_t        trans_id;
  cl_status_t     wait_status;

  OSM_LOG_ENTER(&(IbisObj.log));

  status = __ibbbm_vpd(p_ibbbm,lid,BBM_ATTR_READVPD,&trans_id,vpd_device_selector,bytes_num,offset,NULL);

  if (status != IB_SUCCESS )
  {
    goto Exit;
  }

  /* wait for a second for the event. Allow interrupts also */
  wait_status = cl_event_wait_on(&p_ibbbm->wait_for_resp,
                                 IbisObj.p_opt->transaction_timeout*10000,
                                 TRUE);
  if ((p_ibbbm_vpd_mad-> mad_header.method != VENDOR_GET_RESP) || (wait_status != CL_SUCCESS))
  {
    status = IB_ERROR;
    goto Exit;
  };

  memcpy(p_bbm_vpd_mad,p_ibbbm_vpd_mad,sizeof(ib_bbm_vpd_t));

  if (p_bbm_vpd_mad->mad_header.status != 0)
  {
    status = IB_ERROR;
  };


 Exit:
  OSM_LOG_EXIT(&(IbisObj.log));
  return (status);
}

/**********************************************************************
 **********************************************************************/
ib_api_status_t
ibbbm_write_vpd(
  IN ibbbm_t* const p_ibbbm,
  IN uint16_t lid,
  IN uint8_t vpd_device_selector,
  IN uint16_t bytes_num,
  IN uint16_t offset,
  IN uint8_t *p_data)
{
  ib_api_status_t status;
  uint64_t        trans_id;
  cl_status_t     wait_status;

  OSM_LOG_ENTER(&(IbisObj.log));

  status = __ibbbm_vpd(p_ibbbm,lid,BBM_ATTR_WRITEVPD,&trans_id,vpd_device_selector,bytes_num,offset,p_data);

  if (status != IB_SUCCESS )
  {
    goto Exit;
  }

  /* wait for a second for the event. Allow interrupts also */
  wait_status = cl_event_wait_on(&p_ibbbm->wait_for_resp,
                                 IbisObj.p_opt->transaction_timeout*10000,
                                 TRUE);
  if ((p_ibbbm_vpd_mad-> mad_header.method != VENDOR_GET_RESP) ||
      (wait_status != CL_SUCCESS)) {
    status = IB_ERROR;
    goto Exit;
  };

  Exit :
    OSM_LOG_EXIT(&(IbisObj.log));
  return (status);
}
