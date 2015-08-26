/**
 \file ip_appconf.h
 \brief Implements a simple, fast cache for looking up IPSec tunnels.
 */
/*
 * Copyright (C) 2012 Freescale Semiconductor, Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN
 * NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED
 * TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#ifndef LIB_IP_APPCONF_H
#define LIB_IP_APPCONF_H

#include <netinet/in.h>

/**
 \brief	Structure for configuring an Interface
 */
struct app_ctrl_intf_conf {
	in_addr_t ip_addr;			/**< IP Address */
	int ifnum;				/**< Interface number */
	char ifname[10];			/**< MACless Interface name */
#define IPC_CTRL_PARAM_BMASK_IFNUM		(1 << 0)
#define IPC_CTRL_PARAM_BMASK_IFNAME		(1 << 1)
#define IPC_CTRL_PARAM_BMASK_IPADDR		(1 << 2)
#define IPC_CTRL_PARAM_MAX_INTF_BIT_NO			3

#define IPC_CTRL_INTF_CONF_MDTR_PARAM_MAP (IPC_CTRL_PARAM_BMASK_IPADDR)

	uint32_t bitmask;
};

struct app_ctrl_ip_info {
	in_addr_t src_ipaddr;			/**<Source IP Address>*/
	in_addr_t dst_ipaddr;			/**<Destination IP Address>*/
	in_addr_t gw_ipaddr;			/**<Gateway IP Address>*/
	struct ether_addr mac_addr;		/**< Mac Address */
	unsigned int all;			/**< Show all enabled interfaces */
	unsigned int replace_entry;		/**< Used for overwriting an existing ARP entry */
	struct app_ctrl_intf_conf intf_conf;	/**< Interface Configuration */
	unsigned int fib_cnt;                     /**< Count for fib entries */
	unsigned int mask;                     /**< netmask */
};

/**
 \brief	Structure used for communicating with USDPAA process through
posix message queue.
 */
struct app_ctrl_op_info {

#define IPC_CTRL_CMD_STATE_IDLE 0
#define IPC_CTRL_CMD_STATE_BUSY 1
	unsigned int state;
	/**< State of Command */

#define IPC_CTRL_CMD_TYPE_ROUTE_ADD		1
#define IPC_CTRL_CMD_TYPE_ROUTE_DEL		2
#define IPC_CTRL_CMD_TYPE_INTF_CONF_CHNG	3
#define IPC_CTRL_CMD_TYPE_SHOW_INTF		4
#define IPC_CTRL_CMD_TYPE_ARP_ADD		5
#define IPC_CTRL_CMD_TYPE_ARP_DEL		6

	unsigned int msg_type;
	/**<Type of Request>*/

#define IPC_CTRL_RSLT_SUCCESSFULL		1
#define IPC_CTRL_RSLT_FAILURE		0
	unsigned int result;
	/**<Result - Successful, Failure>*/

	uint32_t pid;
	struct app_ctrl_ip_info ip_info;
	/**< IPfwd Info structure */
};

extern struct app_ctrl_op_info g_sLweCtrlSaInfo;

#endif	/* LIB_IP_APPCONF_H */
