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

#ifndef _IBBBM_BASE_H_
#define _IBBBM_BASE_H_

#include <endian.h>

#define BBM_CLASS         0x05
#define BBM_ATTR_BKEYINFO 0x10
#define BBM_ATTR_WRITEVPD 0x20
#define BBM_ATTR_READVPD  0x21
#define VENDOR_GET        0x01
#define VENDOR_SET        0x02
#define VENDOR_SEND       0x03
#define VENDOR_GET_RESP   0x81
#define MAD_PAYLOAD_SIZE  256
#define IBBBM_INITIAL_TID_VALUE 0x5555
#define IBBBM_MOD_VPD_SIZE 68
#define IBBBM_MOD_VPD_OFFSET 0x177
#define IBBBM_MOD_VPD_DEV_SEL 0xA0
#define IBBBM_MOD_VPD_TEMP_SIZE 5
#define IBBBM_MOD_VPD_PWR_SIZE 12
#define IBBBM_CHA_VPD_SIZE 96
#define IBBBM_CHA_VPD_OFFSET 0x9e
#define IBBBM_CHA_VPD_DEV_SEL 0xA8
#define IBBBM_CHA_VPD_TEMP_SIZE 11
#define IBBBM_CHA_VPD_PWR_SIZE 12
#define IBBBM_CHA_VPD_FAN_SIZE 7
#define IBBBM_BSN_VPD_SIZE 32
#define IBBBM_BSN_VPD_OFFSET 0x3E
#define IBBBM_BSN_VPD_DEV_SEL 0xA0
#define IBBBM_VSD_VPD_SIZE 4
#define IBBBM_VSD_VPD_OFFSET 0x3E
#define IBBBM_VSD_VPD_DEV_SEL 0xA0
#define IBBBM_FW_VER_VPD_SIZE 3
#define IBBBM_FW_VER_VPD_OFFSET 0x20
#define IBBBM_FW_VER_VPD_DEV_SEL 0xA0


typedef enum _ibbbm_state
{
  IBBBM_STATE_INIT,
  IBBBM_STATE_READY
} ibbbm_state_t;

#include <complib/cl_packon.h>
typedef struct _ib_bbm_bkey_info
{
ib_mad_t mad_header;
ib_net64_t b_key;
ib_net32_t reserved0[8];
ib_net32_t bkey;
ib_net16_t protect_bit;
ib_net16_t lease_period;
ib_net16_t violation;
ib_net16_t reserved1;
ib_net32_t data[45];
}	PACK_SUFFIX ib_bbm_bkey_info_t;
#include <complib/cl_packoff.h>

#include <complib/cl_packon.h>
typedef struct _ib_bbm_vpd
{
ib_mad_t mad_header;
ib_net64_t b_key;
ib_net32_t reserved[8];
ib_net16_t bm_sequence;
uint8_t bm_source_device;
uint8_t bm_parm_count;
uint8_t vpd_device_selector;
ib_net16_t bytes_num;
ib_net16_t  offset;
uint8_t  data[183];
}	PACK_SUFFIX ib_bbm_vpd_t;
#include <complib/cl_packoff.h>

#include <complib/cl_packon.h>
typedef struct _ib_bbm_mod_vpd
{
ib_mad_t mad_header;
ib_net64_t b_key;
ib_net32_t reserved[8];
ib_net16_t bm_sequence;
uint8_t bm_source_device;
uint8_t bm_parm_count;
uint8_t vpd_device_selector;
ib_net16_t bytes_num;
ib_net16_t  offset;
uint8_t temp_sensor_count;
uint8_t res0;
ib_net16_t temp_sensor_record[IBBBM_MOD_VPD_TEMP_SIZE];
uint8_t power_sup_count;
uint8_t res2;
ib_net16_t res1;
ib_net32_t power_sup_record[IBBBM_MOD_VPD_PWR_SIZE];
ib_net32_t check_sum;
uint8_t  data[115];
}	PACK_SUFFIX ib_bbm_mod_vpd_t;
#include <complib/cl_packoff.h>

#include <complib/cl_packon.h>
typedef struct _ib_bbm_cha_vpd
{
ib_mad_t mad_header;
ib_net64_t b_key;
ib_net32_t reserved[8];
ib_net16_t bm_sequence;
uint8_t bm_source_device;
uint8_t bm_parm_count;
uint8_t vpd_device_selector;
ib_net16_t bytes_num;
ib_net16_t  offset;
uint8_t temp_sensor_count;
uint8_t res0;
ib_net16_t temp_sensor_record[IBBBM_CHA_VPD_TEMP_SIZE];
uint8_t power_sup_count;
uint8_t res2;
ib_net16_t res1;
ib_net32_t power_sup_record[IBBBM_CHA_VPD_PWR_SIZE];
uint8_t fan_count;
uint8_t res3;
ib_net16_t fan_record[IBBBM_CHA_VPD_PWR_SIZE];
ib_net16_t check_sum;
uint8_t  data[87];
}	PACK_SUFFIX ib_bbm_cha_vpd_t;
#include <complib/cl_packoff.h>

#include <complib/cl_packon.h>
typedef struct _ib_bbm_bsn_vpd
{
ib_mad_t mad_header;
ib_net64_t b_key;
ib_net32_t reserved[8];
ib_net16_t bm_sequence;
uint8_t bm_source_device;
uint8_t bm_parm_count;
uint8_t vpd_device_selector;
ib_net16_t bytes_num;
ib_net16_t  offset;
uint8_t bsn[32];
uint8_t data[151];
}	PACK_SUFFIX ib_bbm_bsn_vpd_t;
#include <complib/cl_packoff.h>

#include <complib/cl_packon.h>
typedef struct _ib_bbm_vsd_vpd
{
ib_mad_t mad_header;
ib_net64_t b_key;
ib_net32_t reserved[8];
ib_net16_t bm_sequence;
uint8_t bm_source_device;
uint8_t bm_parm_count;
uint8_t vpd_device_selector;
ib_net16_t bytes_num;
ib_net16_t  offset;
uint8_t bsn[4];
uint8_t data[179];
}	PACK_SUFFIX ib_bbm_vsd_vpd_t;
#include <complib/cl_packoff.h>

#include <complib/cl_packon.h>
typedef struct _ib_bbm_fw_ver_vpd
{
ib_mad_t mad_header;
ib_net64_t b_key;
ib_net32_t reserved[8];
ib_net16_t bm_sequence;
uint8_t bm_source_device;
uint8_t bm_parm_count;
uint8_t vpd_device_selector;
ib_net16_t bytes_num;
ib_net16_t  offset;
uint8_t  maj_fw_ver;
uint8_t  min_fw_ver;
uint8_t  sub_min_fw_ver;
uint8_t  data[180];
}	PACK_SUFFIX ib_bbm_fw_ver_vpd_t;
#include <complib/cl_packoff.h>



#endif /* _IBBBM_BASE_H_ */
