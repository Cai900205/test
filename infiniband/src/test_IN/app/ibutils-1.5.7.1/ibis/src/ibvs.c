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
 *    Implementation of ibvs_t.
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
#include "ibvs.h"
#include <opensm/osm_madw.h>
#include <opensm/osm_log.h>
#include <opensm/osm_mad_pool.h>
#include <opensm/osm_msgdef.h>

/**********************************************************************
 **********************************************************************/

ibvs_t*
ibvs_construct()
{
  ibvs_t* p_ibvs;

  OSM_LOG_ENTER(&(IbisObj.log));

  p_ibvs = malloc(sizeof(ibvs_t));
  if (p_ibvs == NULL)
  {
    goto Exit;
  }

  memset (p_ibvs, 0, sizeof(ibvs_t));
  p_ibvs->state = IBVS_STATE_INIT;

  Exit :
    OSM_LOG_EXIT(&(IbisObj.log));
  return(p_ibvs);
}

/**********************************************************************
 **********************************************************************/
void
ibvs_destroy(
  IN ibvs_t* const p_ibvs )
{
  OSM_LOG_ENTER(&(IbisObj.log));

  p_ibvs->state = IBVS_STATE_INIT;

  OSM_LOG_EXIT( &(IbisObj.log) );
}

/**********************************************************************
 **********************************************************************/
ib_api_status_t
ibvs_init(
  IN ibvs_t* const p_ibvs )
{
  ib_api_status_t status = IB_SUCCESS;

  OSM_LOG_ENTER(&(IbisObj.log));

  p_ibvs->state = IBVS_STATE_READY;

  OSM_LOG_EXIT( &(IbisObj.log) );
  return( status );
}

/**********************************************************************
 **********************************************************************/

ib_api_status_t
ibvs_bind(
  IN ibvs_t* const p_ibvs )
{
  ib_api_status_t status;

  OSM_LOG_ENTER(&(IbisObj.log));

  status = ibis_gsi_mad_ctrl_bind(
    &(IbisObj.mad_ctrl),
    VS_CLASS, 1,
    &p_ibvs->h_bind
    );

  if( status != IB_SUCCESS )
  {
    goto Exit;
  }

  status = ibis_gsi_mad_ctrl_bind(
    &(IbisObj.mad_ctrl),
    IB_MCLASS_SUBN_LID, 1,
    &p_ibvs->h_smp_bind
    );

  if( status != IB_SUCCESS )
  {
    goto Exit;
  }

  status = ibis_gsi_mad_ctrl_set_class_attr_cb(
    &(IbisObj.mad_ctrl),
    VS_CLASS ,
    VS_EXT_PORT_ACCESS ,
    ibis_gsi_sync_mad_batch_callback,
    (void *)p_ibvs);

  status = ibis_gsi_mad_ctrl_set_class_attr_cb(
    &(IbisObj.mad_ctrl),
    VS_CLASS ,
    VS_DEVICE_SOFT_RESET ,
    ibis_gsi_sync_mad_batch_callback,
    (void *)p_ibvs);

  status = ibis_gsi_mad_ctrl_set_class_attr_cb(
    &(IbisObj.mad_ctrl),
    VS_CLASS ,
    VS_FLASH_OPEN ,
    ibis_gsi_sync_mad_batch_callback,
    (void *)p_ibvs);

  status = ibis_gsi_mad_ctrl_set_class_attr_cb(
    &(IbisObj.mad_ctrl),
    VS_CLASS ,
    VS_FLASH_CLOSE ,
    ibis_gsi_sync_mad_batch_callback,
    (void *)p_ibvs);

  status = ibis_gsi_mad_ctrl_set_class_attr_cb(
    &(IbisObj.mad_ctrl),
    VS_CLASS ,
    VS_FLASH_BANK_SET ,
    ibis_gsi_sync_mad_batch_callback,
    (void *)p_ibvs);

  status = ibis_gsi_mad_ctrl_set_class_attr_cb(
    &(IbisObj.mad_ctrl),
    VS_CLASS ,
    VS_FLASH_ERASE_SECTOR,
    ibis_gsi_sync_mad_batch_callback,
    (void *)p_ibvs);

  status = ibis_gsi_mad_ctrl_set_class_attr_cb(
    &(IbisObj.mad_ctrl),
    VS_CLASS ,
    VS_FLASH_READ_SECTOR,
    ibis_gsi_sync_mad_batch_callback,
    (void *)p_ibvs);

  status = ibis_gsi_mad_ctrl_set_class_attr_cb(
    &(IbisObj.mad_ctrl),
    VS_CLASS ,
    VS_FLASH_WRITE_SECTOR,
    ibis_gsi_sync_mad_batch_callback,
    (void *)p_ibvs);

  status = ibis_gsi_mad_ctrl_set_class_attr_cb(
    &(IbisObj.mad_ctrl),
    VS_CLASS ,
    VS_MIRROR,
    ibis_gsi_sync_mad_batch_callback,
    (void *)p_ibvs);

  status = ibis_gsi_mad_ctrl_set_class_attr_cb(
    &(IbisObj.mad_ctrl),
    VS_CLASS ,
    VS_GENERAL_INFO_ATTR,
    ibis_gsi_sync_mad_batch_callback,
    (void *)p_ibvs);

  status = ibis_gsi_mad_ctrl_set_class_attr_cb(
    &(IbisObj.mad_ctrl),
    IB_MCLASS_SUBN_LID,
    VS_SM_PLFT_MAP_ATTR,
    ibis_gsi_sync_mad_batch_callback,
    (void *)p_ibvs);

  if( status != IB_SUCCESS )
  {
    goto Exit;
  }

  Exit :

    OSM_LOG_EXIT( &(IbisObj.log) );
  return( status );
}


/**********************************************************************
 **********************************************************************/
void
__ibvs_init_mad_addr(
  IN uint16_t lid,
  OUT osm_mad_addr_t *p_mad_addr)
{
  p_mad_addr->dest_lid = cl_hton16(lid);
  p_mad_addr->path_bits = 0;
  p_mad_addr->static_rate = 0;
  p_mad_addr->addr_type.gsi.remote_qp=cl_hton32(1);
  p_mad_addr->addr_type.gsi.remote_qkey = cl_hton32(0x80010000);
  p_mad_addr->addr_type.gsi.pkey_ix = 0;
  p_mad_addr->addr_type.gsi.service_level = 0;
  p_mad_addr->addr_type.gsi.global_route = FALSE;
}


/**********************************************************************
 **********************************************************************/
void
__ibvs_init_mad_hdr(
  IN uint8_t method,
  IN uint16_t attr_id,
  IN uint32_t attr_mod,
  OUT osm_madw_t *p_madw
  )
{
  p_madw->resp_expected = TRUE;
  ((ib_mad_t *)p_madw->p_mad)->method = method;
  ((ib_mad_t *)p_madw->p_mad)->class_ver = 1;
  ((ib_mad_t *)p_madw->p_mad)->mgmt_class = VS_CLASS;
  ((ib_mad_t *)p_madw->p_mad)->base_ver = 1;
  ((ib_mad_t *)p_madw->p_mad)->attr_id = cl_hton16(attr_id);
  ((ib_mad_t *)p_madw->p_mad)->attr_mod = cl_hton32(attr_mod);
  ((ib_mad_t *)p_madw->p_mad)->trans_id = ibis_get_tid();
}

/**********************************************************************
 **********************************************************************/

static void
__ibvs_prep_ext_port_access_mad(
  IN ibvs_t* p_ibvs,
  IN uint8_t ext_port,
  IN uint16_t lid,
  IN uint8_t method,
  IN uint8_t size,
  IN uint8_t cpu_traget_size, /* used only in cpu */
  IN uint8_t device_id, /* used only in i2c */
  IN uint32_t data[],
  IN uint32_t address,
  IN uint64_t gpio_mask, /* used only in gpio */
  IN uint64_t gpio_data,  /* used only in gpio */
  OUT osm_madw_t **pp_madw
  )
{
  osm_mad_addr_t        mad_addr;
  osm_madw_t            *p_madw;
  uint8_t               i,dword_size;

  OSM_LOG_ENTER(&(IbisObj.log));

  osm_log(&(IbisObj.log), OSM_LOG_DEBUG,
          "__ibvs_prep_ext_port_access_mad: "
          " Sending VS to lid:0x%04X method:0x%X ext_port:0x%X address:0x%X\n",
          lid, method, ext_port, address);

  dword_size = size / 4;


  __ibvs_init_mad_addr(lid, &mad_addr);

  p_madw =
    osm_mad_pool_get(&(IbisObj.mad_pool),
                     p_ibvs->h_bind, MAD_PAYLOAD_SIZE, &mad_addr);

  *pp_madw = p_madw;

  __ibvs_init_mad_hdr(method, VS_EXT_PORT_ACCESS, ext_port, p_madw);
  ((ib_vs_t *)p_madw->p_mad)->vendor_key = cl_hton64(IbisObj.p_opt->v_key);

  if (ext_port == EXT_CPU_PORT)
  {
    ((ib_vs_t *)p_madw->p_mad)->data[0] = cl_hton32(cpu_traget_size << 28 | size);
    ((ib_vs_t *)p_madw->p_mad)->data[1] = cl_hton32(address);
    if (method == VENDOR_SET)
    {
      for (i=0;i<dword_size;i++) {
        ((ib_vs_t *)p_madw->p_mad)->data[2+i] = cl_hton32(data[i]);
      };
    };
  };

  if (ext_port == EXT_I2C_PORT_1 || ext_port == EXT_I2C_PORT_2)
  {
    ((ib_vs_t *)p_madw->p_mad)->data[0] = cl_hton32(size);
    ((ib_vs_t *)p_madw->p_mad)->data[1] = cl_hton32(device_id);
    ((ib_vs_t *)p_madw->p_mad)->data[2] = cl_hton32(address);
    if (method == VENDOR_SET)
    {
      for (i=0;i<dword_size;i++) {
        ((ib_vs_t *)p_madw->p_mad)->data[3+i] = cl_hton32(data[i]);
      };
    };
  };

  if (ext_port == EXT_GPIO_PORT && method == VENDOR_SET)
  {
    ((ib_vs_t *)p_madw->p_mad)->data[0] = cl_hton32(gpio_mask & 0xffffffff);
    ((ib_vs_t *)p_madw->p_mad)->data[1] = cl_hton32(gpio_mask & 0xffffffff00000000ULL);
  };

  OSM_LOG_EXIT(&(IbisObj.log));
}

/**********************************************************************
 **********************************************************************/
ib_api_status_t
ibvs_cpu_read(
  IN ibvs_t* const p_ibvs,
  IN uint16_t lid,
  IN uint8_t size,
  IN uint8_t cpu_traget_size,
  IN uint32_t address,
  OUT ib_vs_t *p_vs_mad
  )
{

  osm_madw_t          *p_madw_arr[1];
  ib_api_status_t status;

  OSM_LOG_ENTER(&(IbisObj.log));

  __ibvs_prep_ext_port_access_mad(
    p_ibvs,
    EXT_CPU_PORT,
    lid,
    VENDOR_GET,
    size,
    cpu_traget_size,
    0,
    0,
    address,
    0,
    0,
    &p_madw_arr[0]);

  status = ibis_gsi_send_sync_mad_batch(
    &(IbisObj.mad_ctrl),
    p_ibvs->h_bind,
    1,
    p_madw_arr,
    sizeof(ib_vs_t),
    (uint8_t*)p_vs_mad);

  OSM_LOG_EXIT(&(IbisObj.log));
  return (status);
}

/**********************************************************************
 **********************************************************************/
ib_api_status_t
ibvs_cpu_write(
  IN ibvs_t* const p_ibvs,
  IN uint16_t lid,
  IN uint8_t size,
  IN uint8_t cpu_traget_size,
  IN uint32_t data[],
  IN uint32_t address)
{
  ib_api_status_t status;
  osm_madw_t     *p_madw_arr[1];
  ib_vs_t         res_mad;

  OSM_LOG_ENTER(&(IbisObj.log));

  __ibvs_prep_ext_port_access_mad(
    p_ibvs,
    EXT_CPU_PORT,
    lid,
    VENDOR_SET,
    size,
    cpu_traget_size,
    0,
    data,
    address,
    0,
    0,
    &p_madw_arr[0]);

  status = ibis_gsi_send_sync_mad_batch(
    &(IbisObj.mad_ctrl),
    p_ibvs->h_bind,
    1,
    p_madw_arr,
    sizeof(ib_vs_t),
    (uint8_t*)&res_mad);

  if (status == IB_SUCCESS)
    status = ibis_get_mad_status((ib_mad_t*)&res_mad);

  OSM_LOG_EXIT(&(IbisObj.log));
  return (status);
}

/**********************************************************************
 **********************************************************************/
ib_api_status_t
ibvs_i2c_read(
  IN ibvs_t* const p_ibvs,
  IN uint16_t lid,
  IN uint8_t port_num,
  IN uint8_t size,
  IN uint8_t device_id,
  IN uint32_t address,
  OUT ib_vs_t *p_vs_mad)
{

  ib_api_status_t status;
  osm_madw_t          *p_madw_arr[1];

  OSM_LOG_ENTER(&(IbisObj.log));

  osm_log(&(IbisObj.log), OSM_LOG_DEBUG,
          "ibvs_i2c_read: "
          "Reading lid:0x%04X method:0x%X "
          "ext_port:0x%X dev:0x%X address:0x%X size:0x%X \n",
          lid, VENDOR_GET ,
          EXT_I2C_PORT+port_num, device_id, address, size);

  __ibvs_prep_ext_port_access_mad(
    p_ibvs,
    EXT_I2C_PORT+port_num,
    lid,
    VENDOR_GET,
    size,
    0,
    device_id,
    0,
    address,
    0,
    0,
    &p_madw_arr[0]);

  status = ibis_gsi_send_sync_mad_batch(
    &(IbisObj.mad_ctrl),
    p_ibvs->h_bind,
    1,
    p_madw_arr,
    sizeof(ib_vs_t),
    (uint8_t*)p_vs_mad);

  OSM_LOG_EXIT(&(IbisObj.log));
  return (status);
}

/**********************************************************************
 **********************************************************************/
ib_api_status_t
ibvs_multi_i2c_read(
  IN ibvs_t* const p_ibvs,
  IN uint8_t num,
  IN uint16_t lid_list[],
  IN uint8_t port_num,
  IN uint8_t size,
  IN uint8_t device_id,
  IN uint32_t address,
  OUT ib_vs_t *vs_mad_arr)
{

  ib_api_status_t status;
  osm_madw_t     *p_madw_arr[IBVS_MULTI_MAX];
  uint16_t        i;

  OSM_LOG_ENTER(&(IbisObj.log));

  if (num > IBVS_MULTI_MAX)
  {
    status = IB_ERROR;
    goto Exit;
  }

  osm_log(&(IbisObj.log), OSM_LOG_DEBUG,
          "ibvs_multi_i2c_read: "
          "Reading %d lids method:0x%X "
          "ext_port:0x%X dev:0x%X address:0x%X size:%X \n",
          num, VENDOR_GET ,
          EXT_I2C_PORT+port_num, device_id, address, size);

  for (i = 0; i < num; i++)
  {
    __ibvs_prep_ext_port_access_mad(
      p_ibvs,
      EXT_I2C_PORT+port_num,
      lid_list[i],
      VENDOR_GET,
      size,
      0,
      device_id,
      0,
      address,
      0,
      0,
      &p_madw_arr[i]);
  }

  status = ibis_gsi_send_sync_mad_batch(
    &(IbisObj.mad_ctrl),
    p_ibvs->h_bind,
    num,
    p_madw_arr,
    sizeof(ib_vs_t),
    (uint8_t*)vs_mad_arr);

 Exit:
  OSM_LOG_EXIT(&(IbisObj.log));
  return (status);
}


/**********************************************************************
 **********************************************************************/
ib_api_status_t
ibvs_i2c_write(
  IN ibvs_t* const p_ibvs,
  IN uint16_t lid,
  IN uint8_t port_num,
  IN uint8_t size,
  IN uint8_t device_id,
  IN uint32_t data[],
  IN uint32_t address)
{
  ib_api_status_t status;
  osm_madw_t     *p_madw_arr[1];
  ib_vs_t         res_mad;

  OSM_LOG_ENTER(&(IbisObj.log));

  __ibvs_prep_ext_port_access_mad(
    p_ibvs,
    EXT_I2C_PORT+port_num,
    lid,
    VENDOR_SET,
    size,
    0,
    device_id,
    data,
    address,
    0,
    0,
    &p_madw_arr[0]);

  status = ibis_gsi_send_sync_mad_batch(
    &(IbisObj.mad_ctrl),
    p_ibvs->h_bind,
    1,
    p_madw_arr,
    sizeof(ib_vs_t),
    (uint8_t*)&res_mad);

  OSM_LOG_EXIT(&(IbisObj.log));
  return (status);
}

/**********************************************************************
 **********************************************************************/
ib_api_status_t
ibvs_multi_i2c_write(
  IN ibvs_t* const p_ibvs,
  IN uint8_t num,
  IN uint16_t lid_list[],
  IN uint8_t port_num,
  IN uint8_t size,
  IN uint8_t device_id,
  IN uint32_t data[],
  IN uint32_t address,
  OUT ib_vs_t *vs_mad_arr)
{
  ib_api_status_t status;
  uint8_t         i;
  osm_madw_t     *p_madw_arr[IBVS_MULTI_MAX];

  OSM_LOG_ENTER(&(IbisObj.log));

  if (num > IBVS_MULTI_MAX)
  {
    status = IB_ERROR;
    goto Exit;
  }

  for (i = 0; i < num; i++)
  {
    __ibvs_prep_ext_port_access_mad(
      p_ibvs,
      EXT_I2C_PORT+port_num,
      lid_list[i],
      VENDOR_SET,
      size,
      0,
      device_id,
      data,
      address,
      0,
      0,
      &p_madw_arr[i]);
  }

  status = ibis_gsi_send_sync_mad_batch(
    &(IbisObj.mad_ctrl),
    p_ibvs->h_bind,
    num,
    p_madw_arr,
    sizeof(ib_vs_t),
    (uint8_t*)vs_mad_arr);

  /* check some commands passed in success */
  if (status == IB_SUCCESS)
  {
    for (i = 0; i < num; i++)
    {
      status = ibis_get_mad_status((ib_mad_t*)&vs_mad_arr[i]);
      if (status == IB_SUCCESS)
      {
        break;
      }
    }
  }
  else
  {
    osm_log(&(IbisObj.log), OSM_LOG_ERROR,
            "ibvs_multi_i2c_write: "
            " Fail to send mad batch status:%d\n",
            status);
  }

 Exit:
  OSM_LOG_EXIT(&(IbisObj.log));
  return (status);
}


/**********************************************************************
 **********************************************************************/

ib_api_status_t
ibvs_gpio_read(
  IN ibvs_t* const p_ibvs,
  IN uint16_t lid,
  OUT ib_vs_t *p_vs_mad)
{
  ib_api_status_t status;
  osm_madw_t     *p_madw_arr[1];

  OSM_LOG_ENTER(&(IbisObj.log));

  __ibvs_prep_ext_port_access_mad(
    p_ibvs,
    EXT_GPIO_PORT,
    lid,
    VENDOR_GET,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    &p_madw_arr[0]);

  status = ibis_gsi_send_sync_mad_batch(
    &(IbisObj.mad_ctrl),
    p_ibvs->h_bind,
    1,
    p_madw_arr,
    sizeof(ib_vs_t),
    (uint8_t*)p_vs_mad);

  OSM_LOG_EXIT(&(IbisObj.log));
  return (status);
}


/**********************************************************************
 **********************************************************************/

ib_api_status_t
ibvs_gpio_write(
  IN ibvs_t* const p_ibvs,
  IN uint16_t lid,
  IN uint64_t gpio_mask,
  IN uint64_t gpio_data)
{

  ib_api_status_t status;
  osm_madw_t     *p_madw_arr[1];
  ib_vs_t        res_mad;

  OSM_LOG_ENTER(&(IbisObj.log));

  __ibvs_prep_ext_port_access_mad(
    p_ibvs,
    EXT_GPIO_PORT,
    lid,
    VENDOR_SET,
    0,
    0,
    0,
    0,
    0,
    gpio_mask,
    gpio_data,
    &p_madw_arr[0]);

  status = ibis_gsi_send_sync_mad_batch(
    &(IbisObj.mad_ctrl),
    p_ibvs->h_bind,
    1,
    p_madw_arr,
    sizeof(ib_vs_t),
    (uint8_t*)&res_mad);

  if (status == IB_SUCCESS)
    status = ibis_get_mad_status((ib_mad_t*)&res_mad);

  OSM_LOG_EXIT(&(IbisObj.log));
  return (status);
}


/**********************************************************************
 **********************************************************************/
static void
__ibvs_prep_sw_reset_mad(
  IN ibvs_t* p_ibvs,
  IN uint16_t lid,
  IN uint8_t method,
  OUT osm_madw_t **pp_madw)
{
  osm_mad_addr_t        mad_addr;
  osm_madw_t            *p_madw;

  OSM_LOG_ENTER(&(IbisObj.log));

  osm_log(&(IbisObj.log), OSM_LOG_DEBUG,
          "__ibvs_prep_sw_reset_mad: "
          " Sending VS RST to lid:0x%04X method:0x:%X\n",
          lid, method);

  __ibvs_init_mad_addr(lid, &mad_addr);

  p_madw = osm_mad_pool_get(
    &(IbisObj.mad_pool), p_ibvs->h_bind, MAD_PAYLOAD_SIZE, &mad_addr);

  *pp_madw = p_madw;

  __ibvs_init_mad_hdr(method, VS_DEVICE_SOFT_RESET, 0, p_madw);
  ((ib_vs_t *)p_madw->p_mad)->vendor_key = cl_hton64(IbisObj.p_opt->v_key);

  OSM_LOG_EXIT(&(IbisObj.log));
}

/**********************************************************************
 **********************************************************************/

ib_api_status_t
ibvs_multi_sw_reset(
  IN ibvs_t* const p_ibvs,
  IN uint8_t num,
  IN uint16_t lid_list[])
{
  ib_api_status_t status;
  uint8_t         i;
  osm_madw_t     *p_madw_arr[IBVS_MULTI_MAX];
  ib_vs_t         res_mads[IBVS_MULTI_MAX];

  OSM_LOG_ENTER(&(IbisObj.log));

  if (num > IBVS_MULTI_MAX)
  {
    status = IB_ERROR;
    goto Exit;
  }

  for (i = 0; i < num; i++)
  {
    __ibvs_prep_sw_reset_mad(
      p_ibvs,
      lid_list[i],
      VENDOR_SET,
      &p_madw_arr[i]);
  }

  status = ibis_gsi_send_sync_mad_batch(
    &(IbisObj.mad_ctrl),
    p_ibvs->h_bind,
    num,
    p_madw_arr,
    sizeof(ib_vs_t),
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

/**********************************************************************
 **********************************************************************/

static void
__ibvs_prep_flash_access_mad(
  IN ibvs_t* p_ibvs,
  IN uint16_t lid,
  IN uint8_t method,
  IN uint16_t attr_id,
  IN uint32_t attr_mod,
  IN uint8_t size,
  IN uint32_t data[],
  IN uint32_t address,
  OUT osm_madw_t **pp_madw)
{

  osm_mad_addr_t        mad_addr;
  osm_madw_t            *p_madw;
  uint8_t               i,dword_size;

  OSM_LOG_ENTER(&(IbisObj.log));

  osm_log(&(IbisObj.log), OSM_LOG_DEBUG,
          "__ibvs_prep_flash_access_mad: "
          " Sending VS Flash to lid:0x%04X method:0x:%X attr:0x%X mod:0x%X\n",
          lid, method, attr_id, attr_mod);

  dword_size = size / 4;

  __ibvs_init_mad_addr(lid, &mad_addr);

  p_madw = osm_mad_pool_get(
    &(IbisObj.mad_pool), p_ibvs->h_bind, MAD_PAYLOAD_SIZE, &mad_addr);

  *pp_madw = p_madw;

  __ibvs_init_mad_hdr(method, attr_id, attr_mod, p_madw);
  ((ib_vs_t *)p_madw->p_mad)->vendor_key = cl_hton64(IbisObj.p_opt->v_key);

  ((ib_vs_flash_t *)p_madw->p_mad)->size = cl_hton32(size);
  ((ib_vs_flash_t *)p_madw->p_mad)->offset = cl_hton32(address);

  if ((method == VENDOR_SET) || (attr_id == VS_FLASH_OPEN))
  {
    for (i=0;i<dword_size;i++) {
      ((ib_vs_flash_t *)p_madw->p_mad)->data[i] = cl_hton32(data[i]);
    };
  };

  OSM_LOG_EXIT(&(IbisObj.log));
}

/**********************************************************************
 **********************************************************************/

ib_api_status_t
ibvs_multi_flash_open(
  IN ibvs_t* const p_ibvs,
  IN uint8_t num,
  IN uint16_t lid_list[],
  IN uint32_t last,
  IN uint8_t size,
  IN uint32_t data[],
  IN uint32_t address,
  OUT ib_vs_t vs_mad_arr[])
{

  ib_api_status_t status;
  osm_madw_t     *p_madw_arr[IBVS_MULTI_MAX];
  uint8_t         i;

  OSM_LOG_ENTER(&(IbisObj.log));

  if (num > IBVS_MULTI_MAX)
  {
    status = IB_ERROR;
    goto Exit;
  }

  for (i = 0; i < num; i++)
  {
    __ibvs_prep_flash_access_mad(
      p_ibvs,
      lid_list[i],
      VENDOR_GET,
      VS_FLASH_OPEN,
      last,
      size,
      data,
      address,
      &p_madw_arr[i]);
  }

  status = ibis_gsi_send_sync_mad_batch(
    &(IbisObj.mad_ctrl),
    p_ibvs->h_bind,
    num,
    p_madw_arr,
    sizeof(ib_vs_t),
    (uint8_t*)vs_mad_arr);

 Exit:
  OSM_LOG_EXIT(&(IbisObj.log));
  return (status);
}


/**********************************************************************
 **********************************************************************/

ib_api_status_t
ibvs_multi_flash_close(
  IN ibvs_t* const p_ibvs,
  IN uint8_t num,
  IN uint16_t lid_list[],
  IN uint32_t force,
  OUT ib_vs_t vs_mad_arr[])
{

  ib_api_status_t status;
  osm_madw_t     *p_madw_arr[IBVS_MULTI_MAX];
  uint8_t         i;

  OSM_LOG_ENTER(&(IbisObj.log));

  if (num > IBVS_MULTI_MAX)
  {
    status = IB_ERROR;
    goto Exit;
  }

  for (i = 0; i < num; i++)
  {
    __ibvs_prep_flash_access_mad(
      p_ibvs,
      lid_list[i],
      VENDOR_GET,
      VS_FLASH_CLOSE,
      force,
      0,
      0,
      0,
      &p_madw_arr[i]);
  }

  status = ibis_gsi_send_sync_mad_batch(
    &(IbisObj.mad_ctrl),
    p_ibvs->h_bind,
    num,
    p_madw_arr,
    sizeof(ib_vs_t),
    (uint8_t*)vs_mad_arr);

 Exit:
  OSM_LOG_EXIT(&(IbisObj.log));
  return (status);
}


/**********************************************************************
 **********************************************************************/


ib_api_status_t
ibvs_multi_flash_set_bank(
  IN ibvs_t* const p_ibvs,
  IN uint8_t num,
  IN uint16_t lid_list[],
  IN uint32_t address,
  OUT ib_vs_t vs_mad_arr[])
{

  ib_api_status_t status;
  osm_madw_t     *p_madw_arr[IBVS_MULTI_MAX];
  uint8_t         i;

  OSM_LOG_ENTER(&(IbisObj.log));

  if (num > IBVS_MULTI_MAX)
  {
    status = IB_ERROR;
    goto Exit;
  }

  for (i = 0; i < num; i++)
  {
    __ibvs_prep_flash_access_mad(
      p_ibvs,
      lid_list[i],
      VENDOR_SET,
      VS_FLASH_BANK_SET,
      ATTR_MOD,
      0,
      0,
      address,
      &p_madw_arr[i]);
  }

  status = ibis_gsi_send_sync_mad_batch(
    &(IbisObj.mad_ctrl),
    p_ibvs->h_bind,
    num,
    p_madw_arr,
    sizeof(ib_vs_t),
    (uint8_t*)vs_mad_arr);

 Exit:
  OSM_LOG_EXIT(&(IbisObj.log));
  return (status);
}


/**********************************************************************
 **********************************************************************/

ib_api_status_t
ibvs_multi_flash_erase(
  IN ibvs_t* const p_ibvs,
  IN uint8_t num,
  IN uint16_t lid_list[],
  IN uint32_t address,
  OUT ib_vs_t vs_mad_arr[])
{

  ib_api_status_t status;
  osm_madw_t     *p_madw_arr[IBVS_MULTI_MAX];
  uint8_t         i;

  OSM_LOG_ENTER(&(IbisObj.log));

  if (num > IBVS_MULTI_MAX)
  {
    status = IB_ERROR;
    goto Exit;
  }

  for (i = 0; i < num; i++)
  {
    __ibvs_prep_flash_access_mad(
      p_ibvs,
      lid_list[i],
      VENDOR_SET,
      VS_FLASH_ERASE_SECTOR,
      ATTR_MOD,
      0,
      0,
      address,
      &p_madw_arr[i]);
  }

  status = ibis_gsi_send_sync_mad_batch(
    &(IbisObj.mad_ctrl),
    p_ibvs->h_bind,
    num,
    p_madw_arr,
    sizeof(ib_vs_t),
    (uint8_t*)vs_mad_arr);

 Exit:
  OSM_LOG_EXIT(&(IbisObj.log));
  return (status);
}

/**********************************************************************
 **********************************************************************/

ib_api_status_t
ibvs_multi_flash_read(
  IN ibvs_t* const p_ibvs,
  IN uint8_t num,
  IN uint16_t lid_list[],
  IN uint8_t size,
  IN uint32_t address,
  OUT ib_vs_t vs_mad_arr[])
{

  ib_api_status_t status;
  osm_madw_t     *p_madw_arr[IBVS_MULTI_MAX];
  uint8_t         i;

  OSM_LOG_ENTER(&(IbisObj.log));

  if (num > IBVS_MULTI_MAX)
  {
    status = IB_ERROR;
    goto Exit;
  }

  for (i = 0; i < num; i++)
  {
    __ibvs_prep_flash_access_mad(
      p_ibvs,
      lid_list[i],
      VENDOR_GET,
      VS_FLASH_READ_SECTOR,
      ATTR_MOD,
      size,
      0,
      address,
      &p_madw_arr[i]);
  }

  status = ibis_gsi_send_sync_mad_batch(
    &(IbisObj.mad_ctrl),
    p_ibvs->h_bind,
    num,
    p_madw_arr,
    sizeof(ib_vs_t),
    (uint8_t*)vs_mad_arr);

 Exit:
  OSM_LOG_EXIT(&(IbisObj.log));
  return (status);
}

/**********************************************************************
 **********************************************************************/


ib_api_status_t
ibvs_multi_flash_write(
  IN ibvs_t* const p_ibvs,
  IN uint8_t num,
  IN uint16_t lid_list[],
  IN uint8_t size,
  IN uint32_t data[],
  IN uint32_t address)
{
  ib_api_status_t status;
  uint8_t         i;
  osm_madw_t     *p_madw_arr[IBVS_MULTI_MAX];
  ib_vs_t         res_mads[IBVS_MULTI_MAX];

  OSM_LOG_ENTER(&(IbisObj.log));

  if (num > IBVS_MULTI_MAX)
  {
    status = IB_ERROR;
    goto Exit;
  }

  for (i = 0; i < num; i++)
  {
    __ibvs_prep_flash_access_mad(
      p_ibvs,
      lid_list[i],
      VENDOR_SET,
      VS_FLASH_WRITE_SECTOR,
      ATTR_MOD,
      size,
      data,
      address,
      &p_madw_arr[i]);
  }

  status = ibis_gsi_send_sync_mad_batch(
    &(IbisObj.mad_ctrl),
    p_ibvs->h_bind,
    num,
    p_madw_arr,
    sizeof(ib_vs_t),
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


/**********************************************************************
 **********************************************************************/


ib_api_status_t
ibvs_mirror_read(
  IN ibvs_t* const p_ibvs,
  IN uint16_t lid,
  OUT ib_vs_t *p_vs_mad)
{
  osm_mad_addr_t        mad_addr;
  osm_madw_t            *p_madw;
  ib_api_status_t status;
  osm_madw_t     *p_madw_arr[1];

  OSM_LOG_ENTER(&(IbisObj.log));

  osm_log(&(IbisObj.log), OSM_LOG_DEBUG,
          "ibvs_mirror_read: "
          " Sending VS mirror get to lid:0x%04X",
		  lid);

  __ibvs_init_mad_addr(lid, &mad_addr);

  p_madw =
    osm_mad_pool_get(&(IbisObj.mad_pool),
                     p_ibvs->h_bind, MAD_PAYLOAD_SIZE, &mad_addr);

  *p_madw_arr = p_madw;

  __ibvs_init_mad_hdr(VENDOR_GET, VS_MIRROR, SWITCH_PORT, p_madw);
 ((ib_vs_t *)p_madw->p_mad)->vendor_key =
 cl_hton64(IbisObj.p_opt->v_key);

  status = ibis_gsi_send_sync_mad_batch(
    &(IbisObj.mad_ctrl),
    p_ibvs->h_bind,
    1,
    p_madw_arr,
    sizeof(ib_vs_t),
    (uint8_t*)p_vs_mad);

  OSM_LOG_EXIT(&(IbisObj.log));
  return (status);
}


/**********************************************************************

**********************************************************************/


ib_api_status_t
ibvs_mirror_write(
  IN ibvs_t* const p_ibvs,
  IN uint16_t lid,
  IN uint32_t rx_mirror,
  IN uint32_t tx_mirror)
{
  osm_mad_addr_t        mad_addr;
  osm_madw_t            *p_madw;
  ib_api_status_t status;
  osm_madw_t     *p_madw_arr[1];
  ib_vs_t        res_mad;

  OSM_LOG_ENTER(&(IbisObj.log));

  osm_log(&(IbisObj.log), OSM_LOG_DEBUG,
          "ibvs_mirror_write: "
          " Sending VS mirror set to lid:0x%04X",
		  lid);

  __ibvs_init_mad_addr(lid, &mad_addr);

  p_madw =
    osm_mad_pool_get(&(IbisObj.mad_pool),
                     p_ibvs->h_bind, MAD_PAYLOAD_SIZE, &mad_addr);

  *p_madw_arr = p_madw;

  __ibvs_init_mad_hdr(VENDOR_SET, VS_MIRROR, SWITCH_PORT, p_madw);
 ((ib_vs_t *)p_madw->p_mad)->vendor_key =
 cl_hton64(IbisObj.p_opt->v_key);

  ((ib_vs_t *)p_madw->p_mad)->data[0] = cl_hton32(rx_mirror);
 ((ib_vs_t *)p_madw->p_mad)->data[1] = cl_hton32(tx_mirror);


  status = ibis_gsi_send_sync_mad_batch(
    &(IbisObj.mad_ctrl),
    p_ibvs->h_bind,
    1,
    p_madw_arr,
    sizeof(ib_vs_t),
    (uint8_t*)&res_mad);

  if (status == IB_SUCCESS)
    status = ibis_get_mad_status((ib_mad_t*)&res_mad);

  OSM_LOG_EXIT(&(IbisObj.log));
  return (status);
}


ib_api_status_t
ibvs_plft_map_get(
  IN ibvs_t* const p_ibvs,
  IN uint16_t lid,
  IN uint8_t upper_ports,
  OUT ib_vs_t *p_vs_mad)
{
  osm_mad_addr_t   mad_addr;
  osm_madw_t      *p_madw;
  ib_api_status_t  status;
  osm_madw_t      *p_madw_arr[1];
  uint32_t         attr_mod = 0;

  OSM_LOG_ENTER(&(IbisObj.log));

  osm_log(&(IbisObj.log), OSM_LOG_DEBUG,
          "ibvs_plft_map_get: "
          " Sending VS PrivateLFT Map lid:0x%04X",
			 lid);

  mad_addr.dest_lid = cl_hton16(lid);
  mad_addr.path_bits = 0;
  mad_addr.static_rate = 0;
  mad_addr.addr_type.gsi.remote_qp=cl_hton32(0);
  mad_addr.addr_type.gsi.remote_qkey = 0;
  mad_addr.addr_type.gsi.pkey_ix = 0;
  mad_addr.addr_type.gsi.service_level = 0;
  mad_addr.addr_type.gsi.global_route = FALSE;

  p_madw =
    osm_mad_pool_get(&(IbisObj.mad_pool),
                     p_ibvs->h_bind, MAD_PAYLOAD_SIZE, &mad_addr);

  *p_madw_arr = p_madw;

  if (upper_ports)
	  attr_mod = 1<<16;

  p_madw->resp_expected = TRUE;
  ((ib_mad_t *)p_madw->p_mad)->method = IB_MAD_METHOD_GET;
  ((ib_mad_t *)p_madw->p_mad)->class_ver = 1;
  ((ib_mad_t *)p_madw->p_mad)->mgmt_class = IB_MCLASS_SUBN_LID;
  ((ib_mad_t *)p_madw->p_mad)->base_ver = 1;
  ((ib_mad_t *)p_madw->p_mad)->attr_id = cl_hton16(VS_SM_PLFT_MAP_ATTR);
  ((ib_mad_t *)p_madw->p_mad)->attr_mod = cl_hton32(attr_mod);
  ((ib_mad_t *)p_madw->p_mad)->trans_id = ibis_get_tid();

  status = ibis_gsi_send_sync_mad_batch(
    &(IbisObj.mad_ctrl),
    p_ibvs->h_smp_bind,
    1,
    p_madw_arr,
    sizeof(ib_smp_t),
    (uint8_t*)p_vs_mad);

  OSM_LOG_EXIT(&(IbisObj.log));
  return (status);
}

ib_api_status_t
ibvs_general_info_get(
  IN ibvs_t* const p_ibvs,
  IN uint16_t lid,
  OUT ib_vs_t *p_vs_mad)
{
  osm_mad_addr_t        mad_addr;
  osm_madw_t            *p_madw;
  ib_api_status_t status;
  osm_madw_t     *p_madw_arr[1];

  OSM_LOG_ENTER(&(IbisObj.log));

  osm_log(&(IbisObj.log), OSM_LOG_DEBUG,
          "ibvs_general_info_get: "
          " Sending VS GeneralInfo to lid:0x%04X",
		  lid);

  __ibvs_init_mad_addr(lid, &mad_addr);

  p_madw =
    osm_mad_pool_get(&(IbisObj.mad_pool),
                     p_ibvs->h_bind, MAD_PAYLOAD_SIZE, &mad_addr);

  *p_madw_arr = p_madw;

  __ibvs_init_mad_hdr(VENDOR_GET, VS_GENERAL_INFO_ATTR, 0, p_madw);
 ((ib_vs_t *)p_madw->p_mad)->vendor_key =
	 cl_hton64(IbisObj.p_opt->v_key);

  status = ibis_gsi_send_sync_mad_batch(
    &(IbisObj.mad_ctrl),
    p_ibvs->h_bind,
    1,
    p_madw_arr,
    sizeof(ib_vs_t),
    (uint8_t*)p_vs_mad);

  OSM_LOG_EXIT(&(IbisObj.log));
  return (status);
}
