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
 *    Implementation of ibcc_t.
 * This object represents the Congestion Control object.
 * This object is part of the IBIS family of objects.
 *
 * Environment:
 *    Linux User Mode
 *
 * $Revision: 1.0 $
 */

#include <string.h>
#include <complib/cl_qmap.h>
#include <complib/cl_passivelock.h>
#include <complib/cl_debug.h>
#include <iba/ib_types.h>
#include "ibcc.h"
#include <opensm/osm_madw.h>
#include <opensm/osm_log.h>
#include <opensm/osm_mad_pool.h>
#include <opensm/osm_msgdef.h>


/**********************************************************************
 **********************************************************************/

ibcc_t *
ibcc_construct()
{
	ibcc_t* p_ibcc;
	OSM_LOG_ENTER(&(IbisObj.log));

	p_ibcc = malloc(sizeof(ibcc_t));
	if (p_ibcc == NULL) {
		goto Exit;
	}
	memset(p_ibcc, 0, sizeof(ibcc_t));

    Exit :
	OSM_LOG_EXIT(&(IbisObj.log));
	return p_ibcc;
}

/**********************************************************************
 **********************************************************************/

void
ibcc_destroy(
	IN ibcc_t* const p_ibcc)
{
	OSM_LOG_ENTER(&(IbisObj.log));
	p_ibcc->state = IBCC_STATE_INIT;
	OSM_LOG_EXIT( &(IbisObj.log) );
}

/**********************************************************************
 **********************************************************************/

ib_api_status_t
ibcc_init(
	IN ibcc_t* const p_ibcc)
{
	ib_api_status_t status = IB_SUCCESS;
	OSM_LOG_ENTER(&(IbisObj.log));
	p_ibcc->state = IBCC_STATE_INIT;
	OSM_LOG_EXIT(&(IbisObj.log));
	return status;
}

/**********************************************************************
 **********************************************************************/

ib_api_status_t
ibcc_bind(
	IN ibcc_t* const p_ibcc)
{
	ib_api_status_t status = IB_SUCCESS;

	OSM_LOG_ENTER(&(IbisObj.log));

	/* Bind CongestionControl Management Class */
	if ((status = ibis_gsi_mad_ctrl_bind(
			&(IbisObj.mad_ctrl),
			IB_MCLASS_CC, 2,
			&p_ibcc->lid_route_bind)) != IB_SUCCESS)
		goto Exit;

	/* Set callback for ClassPortInfo attribute */
	if ((status = ibis_gsi_mad_ctrl_set_class_attr_cb(
			&(IbisObj.mad_ctrl),
			IB_MCLASS_CC,
			CL_NTOH16(IB_MAD_ATTR_CLASS_PORT_INFO),
			ibis_gsi_sync_mad_batch_callback,
			(void *)p_ibcc)) != IB_SUCCESS)
		goto Exit;

	/* Set callback for Notice attribute */
	if ((status = ibis_gsi_mad_ctrl_set_class_attr_cb(
			&(IbisObj.mad_ctrl),
			IB_MCLASS_CC,
			CL_NTOH16(IB_MAD_ATTR_NOTICE),
			ibis_gsi_sync_mad_batch_callback,
			(void *)p_ibcc)) != IB_SUCCESS)
		goto Exit;

	/* Set callback for CongestionInfo attribute */
	if ((status = ibis_gsi_mad_ctrl_set_class_attr_cb(
			&(IbisObj.mad_ctrl),
			IB_MCLASS_CC,
			CL_NTOH16(IB_MAD_ATTR_CONG_INFO),
			ibis_gsi_sync_mad_batch_callback,
			(void *)p_ibcc)) != IB_SUCCESS)
		goto Exit;

	/* Set callback for CongestionKeyInfo attribute */
	if ((status = ibis_gsi_mad_ctrl_set_class_attr_cb(
			&(IbisObj.mad_ctrl),
			IB_MCLASS_CC,
			CL_NTOH16(IB_MAD_ATTR_CONG_KEY_INFO),
			ibis_gsi_sync_mad_batch_callback,
			(void *)p_ibcc)) != IB_SUCCESS)
		goto Exit;

	/* Set callback for CongestionLog attribute */
	if ((status = ibis_gsi_mad_ctrl_set_class_attr_cb(
			&(IbisObj.mad_ctrl),
			IB_MCLASS_CC,
			CL_NTOH16(IB_MAD_ATTR_CONG_LOG),
			ibis_gsi_sync_mad_batch_callback,
			(void *)p_ibcc)) != IB_SUCCESS)
		goto Exit;

	/* Set callback for SwitchCongestionSetting attribute */
	if ((status = ibis_gsi_mad_ctrl_set_class_attr_cb(
			&(IbisObj.mad_ctrl),
			IB_MCLASS_CC,
			CL_NTOH16(IB_MAD_ATTR_SW_CONG_SETTING),
			ibis_gsi_sync_mad_batch_callback,
			(void *)p_ibcc)) != IB_SUCCESS)
		goto Exit;

	/* Set callback for SwitchPortCongestionSetting attribute */
	if ((status = ibis_gsi_mad_ctrl_set_class_attr_cb(
			&(IbisObj.mad_ctrl),
			IB_MCLASS_CC,
			CL_NTOH16(IB_MAD_ATTR_SW_PORT_CONG_SETTING),
			ibis_gsi_sync_mad_batch_callback,
			(void *)p_ibcc)) != IB_SUCCESS)
		goto Exit;

	/* Set callback for CACongestionSetting attribute */
	if ((status = ibis_gsi_mad_ctrl_set_class_attr_cb(
			&(IbisObj.mad_ctrl),
			IB_MCLASS_CC,
			CL_NTOH16(IB_MAD_ATTR_CA_CONG_SETTING),
			ibis_gsi_sync_mad_batch_callback,
			(void *)p_ibcc)) != IB_SUCCESS)
		goto Exit;

	/* Set callback for CongestionControlTable attribute */
	if ((status = ibis_gsi_mad_ctrl_set_class_attr_cb(
			&(IbisObj.mad_ctrl),
			IB_MCLASS_CC,
			CL_NTOH16(IB_MAD_ATTR_CC_TBL),
			ibis_gsi_sync_mad_batch_callback,
			(void *)p_ibcc)) != IB_SUCCESS)
		goto Exit;

	/* Set callback for TimeStamp attribute */
	if ((status = ibis_gsi_mad_ctrl_set_class_attr_cb(
			&(IbisObj.mad_ctrl),
			IB_MCLASS_CC,
			CL_NTOH16(IB_MAD_ATTR_TIME_STAMP),
			ibis_gsi_sync_mad_batch_callback,
			(void *)p_ibcc)) != IB_SUCCESS)
		goto Exit;

    Exit :
	OSM_LOG_EXIT( &(IbisObj.log) );
	return status;
}

/**********************************************************************
 **********************************************************************/

static void
__ibcc_prep_cc_mad(
	IN ibcc_t       *p_ibcc,
	IN uint16_t      lid,
	IN uint8_t       method,
	IN uint16_t      attribute_id,
	IN uint32_t      attribute_mod,
	IN uint64_t      cc_key,
	IN uint8_t      *cc_log_data,
	IN size_t        cc_log_data_size,
	IN uint8_t      *cc_mgt_data,
	IN size_t        cc_mgt_data_size,
	OUT osm_madw_t **pp_madw)
{
	osm_mad_addr_t        mad_addr;
	osm_madw_t            *p_madw;

	OSM_LOG_ENTER(&(IbisObj.log));

	mad_addr.dest_lid = cl_hton16(lid);
	mad_addr.path_bits = 0;
	mad_addr.static_rate = 0;

	mad_addr.addr_type.gsi.remote_qp=cl_hton32(1);
	mad_addr.addr_type.gsi.remote_qkey = cl_hton32(0x80010000);
	mad_addr.addr_type.gsi.pkey_ix = 0;
	mad_addr.addr_type.gsi.service_level = 0;
	mad_addr.addr_type.gsi.global_route = FALSE;

	p_madw = osm_mad_pool_get(&(IbisObj.mad_pool),
				  p_ibcc->lid_route_bind,
				  MAD_BLOCK_SIZE,
				  &mad_addr);
	*pp_madw = p_madw;

	p_madw->resp_expected = TRUE;

	memset((char*)p_madw->p_mad, 0, MAD_BLOCK_SIZE);

	((ib_mad_t *)p_madw->p_mad)->base_ver = 1;
	((ib_mad_t *)p_madw->p_mad)->mgmt_class = IB_MCLASS_CC;
	((ib_mad_t *)p_madw->p_mad)->class_ver = 2;
	((ib_mad_t *)p_madw->p_mad)->method = method;
	((ib_mad_t *)p_madw->p_mad)->trans_id = ibis_get_tid();
	((ib_mad_t *)p_madw->p_mad)->attr_id = cl_hton16(attribute_id);
	((ib_mad_t *)p_madw->p_mad)->attr_mod = cl_hton32(attribute_mod);

	((ib_cc_mad_t *)p_madw->p_mad)->cc_key = cl_hton64(cc_key);

	memset(((ib_cc_mad_t *)p_madw->p_mad)->log_data,
	       0, IB_CC_LOG_DATA_SIZE);
	memset(((ib_cc_mad_t *)p_madw->p_mad)->mgt_data,
	       0, IB_CC_MGT_DATA_SIZE);

	if (method == IB_MAD_METHOD_GET) {
		/* In SubnGet() method we leave log and
		   management data clean - they should
		   be filled by the response MAD only.*/
	}
	else {
		if (cc_log_data_size > 0) {
			CL_ASSERT(cc_log_data_size <= IB_CC_LOG_DATA_SIZE);
			CL_ASSERT(cc_log_data);
			memcpy(((ib_cc_mad_t *)p_madw->p_mad)->log_data,
			       cc_log_data, cc_log_data_size);
		}

		if (cc_mgt_data_size > 0) {
			CL_ASSERT(cc_mgt_data_size <= IB_CC_MGT_DATA_SIZE);
			CL_ASSERT(cc_mgt_data);
			memcpy(((ib_cc_mad_t *)p_madw->p_mad)->mgt_data,
			       cc_mgt_data, cc_mgt_data_size);
		}
	}

	OSM_LOG_EXIT(&(IbisObj.log));
}  /* __ibcc_prep_cc_mad() */

static void
__ibcc_dump_raw_mad(
        ib_cc_mad_t * p_cc_mad)
{
      int mad_size = (int)sizeof(ib_cc_mad_t);
      uint8_t * p_raw_mad = (uint8_t*)p_cc_mad;
      int i;
      int last_data_idx; // last non-zero byte of the MAD

      for (i = (mad_size-1); i > 0; i -= 1)
              if (p_raw_mad[i] != 0)
                      break;
      last_data_idx = i;

      for (i = 0; i < mad_size; i += 4) {
              if (i == 0)
                      printf("  Header |---------------------------|\n");
              else if (i == 24)
                      printf("  V key  |---------------------------|\n");
              else if (i == 32)
                      printf("  RESV0  |---------------------------|\n");
              else if (i == 64)
                      printf("  Data   |---------------------------|\n");
              else if (i == 192)
                      printf("  RESV1  |---------------------------|\n");

              if (i <= last_data_idx)
                      printf("          [%03d..%03d]   %02x  %02x  %02x  %02x\n",
                             i+3,i,
                             //p_raw_mad[i+3], p_raw_mad[i+2], p_raw_mad[i+1], p_raw_mad[i+0]);
							 p_raw_mad[i+0], p_raw_mad[i+1], p_raw_mad[i+2], p_raw_mad[i+3]);
      }
      printf("     end |---------------------------|\n");
}

/**********************************************************************
 **********************************************************************/

ib_api_status_t
ibcc_send_mad_by_lid (
	ibcc_t   *p_ibcc,
	uint64_t  cc_key,
	uint8_t  *cc_log_data,
	size_t    cc_log_data_size,
	uint8_t  *cc_mgt_data,
	size_t    cc_mgt_data_size,
	uint16_t  lid,
	uint16_t  attribute_id,
	uint32_t  attribute_mod,
	uint16_t  method)
{
	osm_madw_t      *p_madw;
	ib_cc_mad_t      response_mad;
	ib_api_status_t  status;
	int debug_mode = 0;

	OSM_LOG_ENTER(&(IbisObj.log));

	osm_log(&(IbisObj.log), OSM_LOG_DEBUG,
		"ibcc_send_mad_by_lid: "
		"Sending to lid:0x%04X method:0x%02X "
		"attr:0x%04X mod:0x%08x\n",
		lid, method, attribute_id, attribute_mod);

	memset(&response_mad, 0, sizeof(ib_cc_mad_t));

	__ibcc_prep_cc_mad(p_ibcc,
			   lid,
			   method,
			   attribute_id,
			   attribute_mod,
			   cc_key,
			   cc_log_data,
			   cc_log_data_size,
			   cc_mgt_data,
			   cc_mgt_data_size,
			   &p_madw);
	if (debug_mode == 1)
	{
		/* print mad */
		printf("\n\n>>> Sending MAD:\n\n" );
		__ibcc_dump_raw_mad((ib_cc_mad_t *)(p_madw->p_mad));
	}

	/* send and wait */
	status = ibis_gsi_send_sync_mad_batch(
				&(IbisObj.mad_ctrl),
				p_ibcc->lid_route_bind,
				1,
				&p_madw,
				sizeof(ib_cc_mad_t),
				(uint8_t*)&response_mad);

	if (!response_mad.header.method)
		status = IB_TIMEOUT;

	if (status == IB_SUCCESS) {
		if (debug_mode == 1)
		{
			/* print mad */
			printf("\n\nResponse MAD:\n\n" );
			__ibcc_dump_raw_mad((ib_cc_mad_t *)&response_mad);
		}

		if (cc_log_data)
			memcpy(cc_log_data,
			       response_mad.log_data,
			       cc_log_data_size);

		if (cc_mgt_data)
			memcpy(cc_mgt_data,
			       response_mad.mgt_data,
			       cc_mgt_data_size);

		if (cl_ntoh16(response_mad.header.status) & 0x7fff)
			status = cl_ntoh16(response_mad.header.status);
	}

	OSM_LOG_EXIT(&(IbisObj.log));
	return (status);
}  /* ibcc_send_mad_by_lid() */

