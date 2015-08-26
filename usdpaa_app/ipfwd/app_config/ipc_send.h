/**
 \file ipfwd_cp.h
 \brief Basic IPfwd Config Tool defines and Data structures
 */
/*
 * Copyright (C) 2010,2011 Freescale Semiconductor, Inc.
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

#include <argp.h>

const char *argp_program_version = "argex 1.0";

#define IPC_CTRL_PARAM_BIT_SRCIP	(1 << 0)
/**< Bit Mask for Src IP */
#define IPC_CTRL_PARAM_BIT_DESTIP	(1 << 1)
/**< Bit Mask for Dst IP */
#define IPC_CTRL_PARAM_BIT_SRCGW	(1 << 2)
/**< Bit Mask for Tunnel Src IP */
#define IPC_CTRL_PARAM_BIT_DESTGW	(1 << 3)
/**< Bit Mask for Tunnel Dst IP */
#define IPC_CTRL_PARAM_BIT_PROTO	(1 << 4)
/**< Bit Mask for Protocol */
#define IPC_CTRL_PARAM_BIT_EKEY	(1 << 5)
/**< Bit Mask for Encryption Key */
#define IPC_CTRL_PARAM_BIT_AKEY	(1 << 6)
/**< Bit Mask for Authentication Key */
#define IPC_CTRL_PARAM_BIT_SPI	(1 << 7)
/**< Bit Mask for Security Policy Index */
#define IPC_CTRL_PARAM_BIT_DIR	(1 << 8)
/**< Bit Mask for Direction */
#define IPC_CTRL_PARAM_BIT_ETYPE	(1 << 9)
/**< Bit Mask for Encryption type */
#define IPC_CTRL_PARAM_BIT_ATYPE	(1 << 10)
/**< Bit Mask for Authentication type */
#define IPC_CTRL_PARAM_BIT_FLOWID	(1 << 11)
#define IPC_CTRL_PARAM_MAX_IPSEC_BIT_NO	12

/**< Mandatory Parameters needed for creating SA */
#define IPC_CTRL_ADD_MDTR_PARAM_MAP (IPC_CTRL_PARAM_BIT_SRCIP | \
				IPC_CTRL_PARAM_BIT_DESTIP | \
				IPC_CTRL_PARAM_BIT_SRCGW | \
				IPC_CTRL_PARAM_BIT_DESTGW | \
				IPC_CTRL_PARAM_BIT_DIR | \
				IPC_CTRL_PARAM_BIT_SPI)

#define IPC_CTRL_PARAM_BMASK_SRCIP		(1 << 0)
/**< Bit Mask for Src IP */
#define IPC_CTRL_PARAM_BMASK_DESTIP	(1 << 1)
/**< Bit Mask for Dst IP */
#define IPC_CTRL_PARAM_BMASK_GWIP		(1 << 2)
/**< Bit Mask for Gateway IP */
#define IPC_CTRL_PARAM_BMASK_FLOWID	(1 << 4)
/**< Bit Mask for Flow ID */
#define LWE_CTRL_PARAM_BMASK_SRCCNT    (1 << 5)        /**< Bit Mask for Src Count */
#define LWE_CTRL_PARAM_BMASK_DSTCNT    (1 << 6)        /**< Bit Mask for Dest Count */
#define IPC_CTRL_PARAM_MAX_IP_BIT_NO		6

#define IPC_CTRL_ROUTE_FLOWID_MIN				0
#define IPC_CTRL_ROUTE_FLOWID_MAX				1024
#define IPC_CTRL_ROUTE_FLOWID_DEF				0

/**< Mandatory Parameters needed for creating Route */
#define IPC_CTRL_ROUTE_ADD_MDTR_PARAM_MAP (IPC_CTRL_PARAM_BMASK_SRCIP | \
				IPC_CTRL_PARAM_BMASK_DESTIP | \
				IPC_CTRL_PARAM_BMASK_GWIP | \
				LWE_CTRL_PARAM_BMASK_SRCCNT | \
				LWE_CTRL_PARAM_BMASK_DSTCNT)

static struct argp_option route_add_options[] = {
	{"s", 's', "SRCIP", 0, "Source IP", 0},
	{"c", 'c', "SRCCNT", 0, "SRC CNT", 0},
	{"d", 'd', "DESTIP", 0, "Destination IP", 0},
	{"n", 'n', "DESTCNT", 0, "Destination COUNT", 0},
	{"g", 'g', "GWIP", 0, "Gateway IP", 0},
	{"f", 'f', "FLOWID", 0, "Flow ID - (0 - 64) {Default: 0}", 0},
	{}
};

/**< Mandatory Parameters needed for deleting Route */
#define IPC_CTRL_ROUTE_DEL_MDTR_PARAM_MAP (IPC_CTRL_PARAM_BMASK_SRCIP | \
				IPC_CTRL_PARAM_BMASK_DESTIP)

static struct argp_option route_del_options[] = {
	{"s", 's', "SRCIP", 0, "Source IP", 0},
	{"d", 'd', "DESTIP", 0, "Destination IP", 0},
	{}
};

#define IPC_CTRL_PARAM_BMASK_ARP_IPADDR		(1 << 0)
/**< Bit Mask for ARP IP Address */
#define IPC_CTRL_PARAM_BMASK_ARP_MACADDR		(1 << 1)
/**< Bit Mask for MAC Address */
#define IPC_CTRL_PARAM_BMASK_ARP_REPLACE		(1 << 2)
/**< Bit Mask for Replace variable */
#define IPC_CTRL_PARAM_ARP_MAX_BIT_NO		3

/**< Mandatory Parameters needed for creating ARP */
#define IPC_CTRL_ARP_ADD_MDTR_PARAM_MAP (IPC_CTRL_PARAM_BMASK_ARP_IPADDR | \
				IPC_CTRL_PARAM_BMASK_ARP_MACADDR)

static struct argp_option arp_add_options[] = {
	{"s", 's', "IPADDR", 0, "IP Address", 0},
	{"m", 'm', "MACADDR", 0, "MAC Address", 0},
	{"r", 'r', "Replace", 0,
	 "Replace Exiting Entry - true/ false {Default: false}", 0},
	{}
};

/**< Mandatory Parameters needed for deleting ARP */
#define IPC_CTRL_ARP_DEL_MDTR_PARAM_MAP (IPC_CTRL_PARAM_BMASK_ARP_IPADDR)

static struct argp_option arp_del_options[] = {
	{"s", 's', "IPADDR", 0, "IP Address", 0},
	{}
};

#define IPC_CTRL_IFNUM_MIN				0
#define IPC_CTRL_IFNUM_MAX				19
static struct argp_option intf_conf_options[] = {
	{"i", 'i', "IFNAME", 0, "If Name", 0},
	{"a", 'a', "IPADDR", 0, "IP Address", 0},
	{}
};

static struct argp_option show_intf_options[] = {
	{"a", 'a', "ALL", 0, "All interfaces", 0},
	{}
};
