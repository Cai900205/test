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

#ifndef _IBCR_BASE_H_
#define _IBCR_BASE_H_

#include <endian.h>

#define CR_CLASS        0x09
#define CR_ATTR_50      0x50
#define CR_ATTR_51      0x51
#define VENDOR_GET      0x01
#define VENDOR_SET      0x02
#define VENDOR_GET_RESP 0x81
#define MAD_PAYLOAD_SIZE 256
#define IBCR_INITIAL_TID_VALUE 0x9999
#define IBCR_DWORD_MAX_GET 1
#define IBCR_MULTI_MAX 64

typedef enum _ibcr_state
{
  IBCR_STATE_INIT,
  IBCR_STATE_READY
} ibcr_state_t;

#include <complib/cl_packon.h>
typedef struct _ib_cr_space
{
ib_mad_t mad_header;
ib_net64_t vendor_key;
ib_net32_t data[56];
}	PACK_SUFFIX ib_cr_space_t;
#include <complib/cl_packoff.h>

#include <complib/cl_packon.h>
typedef struct _ib_cr_space_attr_mod
{
uint8_t reserved;
uint8_t mode_count;
ib_net16_t burst_addr;
}PACK_SUFFIX ib_cr_space_attr_mod_t;

#endif /* _IBCR_BASE_H_ */
