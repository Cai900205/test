/**
 \file ipcsend.h
 \brief Basic IPSecfwd Config Tool defines and Data structures
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
#include <assert.h>

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
/**< Bit Mask for Default Gateway */
#define IPC_CTRL_PARAM_BIT_DEFGW	(1 << 12)
/**< High bandwidth tunnel */
#define IPC_CTRL_PARAM_BIT_HB_TUNNEL	(1 << 13)
#define IPC_CTRL_PARAM_MAX_IPSEC_BIT_NO	13


#define IPC_CTRL_PARAM_BMASK_SRCIP		(1 << 0)
/**< Bit Mask for Src IP */
#define IPC_CTRL_PARAM_BMASK_DESTIP	(1 << 1)
/**< Bit Mask for Dst IP */
#define IPC_CTRL_PARAM_BMASK_GWIP		(1 << 2)
/**< Bit Mask for Gateway IP */
#define IPC_CTRL_PARAM_BMASK_FLOWID	(1 << 4)
/**< Bit Mask for Flow ID */
#define IPC_CTRL_PARAM_MAX_IP_BIT_NO		5

#define IPC_CTRL_ROUTE_FLOWID_MIN				0
#define IPC_CTRL_ROUTE_FLOWID_MAX				1024
#define IPC_CTRL_ROUTE_FLOWID_DEF				0

/**< Mandatory Parameters needed for creating Route */
#define IPC_CTRL_ROUTE_ADD_MDTR_PARAM_MAP (IPC_CTRL_PARAM_BMASK_SRCIP | \
				IPC_CTRL_PARAM_BMASK_DESTIP | \
				IPC_CTRL_PARAM_BMASK_GWIP)

static struct argp_option route_add_options[] = {
	{"s", 's', "SRCIP", 0, "Source IP", 0},
	{"d", 'd', "DESTIP", 0, "Destination IP", 0},
	{"g", 'g', "GWIP", 0, "Gateway IP", 0},
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

/**< Mandatory Parameters needed for creating SA */
#define IPC_CTRL_SA_ADD_MDTR_PARAM_MAP (IPC_CTRL_PARAM_BIT_SRCIP | \
				IPC_CTRL_PARAM_BIT_DESTIP | \
				IPC_CTRL_PARAM_BIT_SRCGW | \
				IPC_CTRL_PARAM_BIT_DESTGW | \
				IPC_CTRL_PARAM_BIT_DIR | \
				IPC_CTRL_PARAM_BIT_SPI)

#define IPC_CTRL_SA_IPSEC_PROTO_MIN     0
#define IPC_CTRL_SA_IPSEC_PROTO_MAX     0
#define IPC_CTRL_SA_IPSEC_PROTO_DEF     0

#define IPC_CTRL_SA_ENCRIPTN_TYPE_MIN   0
#define IPC_CTRL_SA_ENCRIPTN_TYPE_MAX   1
#define IPC_CTRL_SA_ENCRIPTN_TYPE_DEF   0

#define IPC_CTRL_SA_AUTH_TYPE_MIN               0
#define IPC_CTRL_SA_AUTH_TYPE_MAX               0
#define IPC_CTRL_SA_AUTH_TYPE_DEF               0

static struct argp_option sa_add_options[] = {
	{"s", 's', "SRCIP", 0, "Source IP", 0},
	{"d", 'd', "DESTIP", 0, "Destination IP", 0},
	{"ss", 'g', "SRCGW", 0, "Source Gateway IP", 0},
	{"sd", 'G', "SRCGW", 0, "Destination Gateway IP", 0},
	{"dir", 'r', "DIR", 0, "DIR- in/ out", 0},
	{"dg", 'D', "DEFGW", 0, "Default Gateway", 0},
	{"a", 'a', "AKEY", 0, "Authentication Key", 0},
	{"e", 'e', "EKEY", 0, "Encryption Key", 0},
	{"spi", 'i', "SPI", 0, "SPI - 32 bit unsigned int", 0},
	{"is_esn", 'x', "ESN", 0, "Extended Sequence Number support", 0},
	{"seq_num", 'v', "SEQNUM", 0, "Sequence Number", 0},
	{"p", 'p', "PROTO", 0, "IPsec Proto type - ESP(0) {Default: 0}", 0},
	{"t", 't', "ETYPE", 0,
		"Encryption Type - AES-CBC(0), 3DES-CBC(1) {Default: 0}", 0},
	{"y", 'y', "ATYPE", 0,
		"Authentication Type - HMAC-SHA1(0) {Default: 0}", 0},
	{"b", 'b', "HBT", 0, "High bandwidth tunnel {Default: 0}", 0},
	{}
};

/**< Mandatory Parameters needed for deleting SA */
#define IPC_CTRL_SA_DEL_MDTR_PARAM_MAP (IPC_CTRL_PARAM_BIT_SRCIP |\
				IPC_CTRL_PARAM_BIT_DESTIP | \
				IPC_CTRL_PARAM_BIT_SRCGW | \
				IPC_CTRL_PARAM_BIT_DESTGW | \
				IPC_CTRL_PARAM_BIT_SPI)

static struct argp_option sa_del_options[] = {
	{"s", 's', "SRCIP", 0, "Source IP", 0},
	{"d", 'd', "DESTIP", 0, "Destination IP", 0},
	{"ss", 'g', "SRCGW", 0, "Source Gateway IP", 0},
	{"sd", 'G', "SRCGW", 0, "Destination Gateway IP", 0},
	{"spi", 'i', "SPI", 0, "SPI", 0},
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
