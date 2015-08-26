/* Copyright (c) 2011-2013 Freescale Semiconductor, Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright
 *	 notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *	 notice, this list of conditions and the following disclaimer in the
 *	 documentation and/or other materials provided with the distribution.
 *     * Neither the name of Freescale Semiconductor nor the
 *	 names of its contributors may be used to endorse or promote products
 *	 derived from this software without specific prior written permission.
 *
 *
 * ALTERNATIVELY, this software may be distributed under the terms of the
 * GNU General Public License ("GPL") as published by the Free Software
 * Foundation, either version 2 of that License or (at your option) any
 * later version.
 *
 * THIS SOFTWARE IS PROVIDED BY Freescale Semiconductor ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL Freescale Semiconductor BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef __NEIGH_SIZING_H
#define __NEIGH_SIZING_H
/* key number and size for neigh tables */
#define IPv4_KEY_SIZE		4
#define IPv6_KEY_SIZE		16
#define MAX_IP_KEY_SIZE		16
#if defined P4080
#define IPv4_NUM_KEYS		16
#define IPv6_NUM_KEYS		16
#define IPv4_NUM_ADDRS		16
#define IPv6_NUM_ADDRS		16

#elif defined B4860
#define IPv4_NUM_KEYS		64
#define IPv6_NUM_KEYS		64
#define IPv4_NUM_ADDRS		16
#define IPv6_NUM_ADDRS		16

#elif defined B4420
#define IPv4_NUM_KEYS		64
#define IPv6_NUM_KEYS		64
#define IPv4_NUM_ADDRS		16
#define IPv6_NUM_ADDRS		16

#elif defined T4240
#define IPv4_NUM_KEYS		64
#define IPv6_NUM_KEYS		64
#define IPv4_NUM_ADDRS		16
#define IPv6_NUM_ADDRS		16

#elif defined T2080
#define IPv4_NUM_KEYS		64
#define IPv6_NUM_KEYS		64
#define IPv4_NUM_ADDRS		16
#define IPv6_NUM_ADDRS		16

#else
#define IPv4_NUM_KEYS		16
#define IPv6_NUM_KEYS		16
#define IPv4_NUM_ADDRS		16
#define IPv6_NUM_ADDRS		16
#endif
#define NEIGH_TABLES_KEY_SIZE { IPv4_KEY_SIZE, IPv6_KEY_SIZE }
#define NEIGH_TABLES_NUM_KEYS { IPv4_NUM_KEYS, IPv6_NUM_KEYS }
#define LOCAL_TABLES_KEY_SIZE { IPv4_KEY_SIZE, IPv6_KEY_SIZE }
#define LOCAL_TABLES_NUM_KEYS { IPv4_NUM_ADDRS, IPv6_NUM_ADDRS }
#define VIPSEC_TABLES_KEY_SIZE { IPv4_KEY_SIZE, IPv6_KEY_SIZE }
#define VIPSEC_TABLES_NUM_KEYS { 1, 1 }
#endif /*__NEIGH_SIZING_H*/
