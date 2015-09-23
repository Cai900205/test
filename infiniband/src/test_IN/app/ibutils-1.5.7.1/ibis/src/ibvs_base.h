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

#ifndef _IBVS_BASE_H_
#define _IBVS_BASE_H_

#include <endian.h>

#define VS_CLASS             0x0a
#define VS_CLASS_PORT_INFO   0x01
#define VS_PRIVATE_LFT       0x10
#define VS_PORT_ON_OFF       0x11
#define VS_DEVICE_SOFT_RESET 0x12
#define VS_EXT_PORT_ACCESS   0x13
#define VS_PHY_CONFIG        0x14
#define VS_MFT               0x15
#define VS_IB_PORT_CONFIG    0x16
#define VS_MIRROR            0x18
#define VENDOR_GET           0x01
#define VENDOR_SET           0x02
#define VENDOR_GET_RESP      0x81
#define SWITCH_PORT          0x0
#define EXT_CPU_PORT         0x01
#define EXT_I2C_PORT         0x01
#define EXT_I2C_PORT_1       0x02
#define EXT_I2C_PORT_2       0x03
#define EXT_GPIO_PORT        0x04
#define MAD_PAYLOAD_SIZE     256
#define IBVS_INITIAL_TID_VALUE 0xaaaa
#define IBVS_MULTI_MAX 64
#define IBVS_DATA_MAX 64
#define VS_FLASH_OPEN 0x0A
#define VS_FLASH_CLOSE 0x0B
#define VS_FLASH_BANK_SET 0x0C
#define VS_FLASH_ERASE_SECTOR 0x0F
#define VS_FLASH_READ_SECTOR 0x0D
#define VS_FLASH_WRITE_SECTOR 0x0E
#define ATTR_ID 0x0
#define ATTR_MOD 0x0
#define ATTR_MOD_LAST 0x1
#define VS_CPU_DATA_OFFSET 0
#define VS_GPIO_DATA_OFFSET 0
#define VS_MIRROR_DATA_OFFSET 0
#define VS_I2C_DATA_OFFSET 3
#define VS_FLASH_DATA_OFFSET 2
#define VS_GENERAL_INFO_ATTR 0x17
#define VS_SM_PLFT_MAP_ATTR 0xff10

typedef enum _ibvs_state
{
  IBVS_STATE_INIT,
  IBVS_STATE_READY,
  IBVS_STATE_BUSY,
} ibvs_state_t;

#include <complib/cl_packon.h>
typedef struct _ib_vs
{
ib_mad_t mad_header;
ib_net64_t vendor_key;
ib_net32_t data[56];
}	PACK_SUFFIX ib_vs_t;
#include <complib/cl_packoff.h>

#include <complib/cl_packon.h>
typedef struct _ib_vs_i2c
{
ib_mad_t mad_header;
ib_net64_t vendor_key;
ib_net32_t size;
ib_net32_t device_select;
ib_net32_t offset;
ib_net32_t data[53];
}	PACK_SUFFIX ib_vs_i2c_t;
#include <complib/cl_packoff.h>

#include <complib/cl_packon.h>
typedef struct _ib_vs_flash
{
ib_mad_t mad_header;
ib_net64_t vendor_key;
ib_net32_t size;
ib_net32_t offset;
ib_net32_t data[54];
}	PACK_SUFFIX ib_vs_flash_t;
#include <complib/cl_packoff.h>

#include <complib/cl_packon.h>
typedef struct _ib_vs_plft_map
{
	ib_mad_t mad_header;
	ib_net64_t m_key;
	ib_net16_t dr_slid;
	ib_net16_t dr_dlid;
	uint32_t	  resv1[7];

	uint8_t ib_port0;
	uint8_t shared_plft_port0;
	uint8_t reserved0;
	uint8_t size0;

	uint8_t ib_port1;
	uint8_t shared_plft_port1;
	uint8_t reserved1;
	uint8_t size1;

	uint8_t ib_port2;
	uint8_t shared_plft_port2;
	uint8_t reserved2;
	uint8_t size2;

	uint8_t ib_port3;
	uint8_t shared_plft_port3;
	uint8_t reserved3;
	uint8_t size3;

	uint8_t ib_port4;
	uint8_t shared_plft_port4;
	uint8_t reserved4;
	uint8_t size4;

	uint8_t ib_port5;
	uint8_t shared_plft_port5;
	uint8_t reserved5;
	uint8_t size5;

	uint8_t ib_port6;
	uint8_t shared_plft_port6;
	uint8_t reserved6;
	uint8_t size6;

	uint8_t ib_port7;
	uint8_t shared_plft_port7;
	uint8_t reserved7;
	uint8_t size7;

	uint8_t ib_port8;
	uint8_t shared_plft_port8;
	uint8_t reserved8;
	uint8_t size8;

	uint8_t ib_port9;
	uint8_t shared_plft_port9;
	uint8_t reserved9;
	uint8_t size9;

	uint8_t ib_port10;
	uint8_t shared_plft_port10;
	uint8_t reserved10;
	uint8_t size10;

	uint8_t ib_port11;
	uint8_t shared_plft_port11;
	uint8_t reserved11;
	uint8_t size11;

	uint8_t ib_port12;
	uint8_t shared_plft_port12;
	uint8_t reserved12;
	uint8_t size12;
}	PACK_SUFFIX ib_vs_plft_map_t;
#include <complib/cl_packoff.h>

#include <complib/cl_packon.h>
typedef struct _ib_vs_gen_info
{
	ib_mad_t mad_header;
	ib_net64_t vendor_key;
	uint16_t hw_rev;
	uint16_t hw_devid;
	uint8_t  hw_reserved[24];
	uint32_t hw_uptime;
        uint8_t  reserved0;
	uint8_t  fw_major;
	uint8_t  fw_minor;
	uint8_t  fw_sub_minor;
	uint32_t fw_build_id;
	uint8_t  fw_month;
	uint8_t  fw_day;
	uint16_t fw_year;
	uint16_t reserved3;
	uint16_t fw_hour;
	uint8_t  fw_psid[16];
	uint32_t fw_ini_ver;
	uint32_t fw_reserved[7];
	uint8_t  sw_reserved;
	uint8_t  sw_major;
	uint8_t  sw_minor;
	uint8_t  sw_sub_minor;
}	PACK_SUFFIX ib_vs_gen_info_t;
#include <complib/cl_packoff.h>

#endif /* _IBVS_BASE_H_ */
