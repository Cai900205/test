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

#ifndef _IBPM_BASE_H_
#define _IBPM_BASE_H_

#include <endian.h>

#define PERF_CLASS      0x04
#define PERF_CLASS_PORT_INFO 0x01
#define PERF_PORT_SAMPLES_CONTROL 0x10
#define PERF_PORT_SAMPLES_RESULTS 0x11
#define PERF_PORTS_COUNTER 0x12
#define PERF_PORTS_COUNTER_EXTENDED 0x1D
#define VENDOR_GET      0x01
#define VENDOR_SET      0x02
#define VENDOR_GET_RESP 0x81
#define MAD_PAYLOAD_SIZE 256
#define IBPM_INITIAL_TID_VALUE 0x4444
#define IBPM_MULTI_MAX 64

typedef enum _ibpm_state
{
  IBPM_STATE_INIT,
  IBPM_STATE_READY,
  IBPM_STATE_BUSY,
} ibpm_state_t;

#include <complib/cl_packon.h>
typedef struct _ib_pm_port_counter
{
  ib_mad_t mad_header;
  ib_net32_t reserved0[10];
  uint8_t reserved1;
  uint8_t port_select;
  ib_net16_t counter_select;
  ib_net16_t symbol_error_counter;
  uint8_t link_error_recovery_counter;
  uint8_t link_down_counter;
  ib_net16_t port_rcv_errors;
  ib_net16_t port_rcv_remote_physical_errors;
  ib_net16_t port_rcv_switch_relay_errors;
  ib_net16_t port_xmit_discard;
  /* uint4_t excessive_buffer_overrun_errors;
     uint4_t local_link_integrity_errors; */
  uint8_t port_xmit_constraint_errors;
  uint8_t port_rcv_constraint_errors;
  uint8_t reserved2;
  uint8_t lli_errors_exc_buf_errors;
  ib_net16_t reserved3;
  ib_net16_t vl15_dropped;
  ib_net32_t port_xmit_data;
  ib_net32_t port_rcv_data;
  ib_net32_t port_xmit_pkts;
  ib_net32_t port_rcv_pkts;
  ib_net32_t reserved5[38];

}  PACK_SUFFIX ib_pm_port_counter_t;
#include <complib/cl_packoff.h>

#include <complib/cl_packon.h>
typedef struct _ib_pm_port_counter_extended
{
  ib_mad_t mad_header;
  ib_net32_t reserved0[10];
  uint8_t reserved1;
  uint8_t port_select;
  ib_net16_t counter_select;
  uint32_t reserved2;
  ib_net64_t port_xmit_data;
  ib_net64_t port_rcv_data;
  ib_net64_t port_xmit_pkts;
  ib_net64_t port_rcv_pkts;
  ib_net64_t port_ucast_xmit_pkts;
  ib_net64_t port_ucast_rcv_pkts;
  ib_net64_t port_mcast_xmit_pkts;
  ib_net64_t port_mcast_rcv_pkts;
}  PACK_SUFFIX ib_pm_port_counter_extended_t;
#include <complib/cl_packoff.h>


#endif /* _IBPM_BASE_H_ */
