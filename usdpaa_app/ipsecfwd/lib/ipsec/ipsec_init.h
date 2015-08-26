/*
 * Copyright (C) 2011 Freescale Semiconductor, Inc.
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

#ifndef LIB_IPSEC_IPSEC_INIT_H
#define LIB_IPSEC_IPSEC_INIT_H 1

#include "ip/ip.h"
#include "ip/ip_common.h"
#include "ipsec/ipsec_common.h"
#include "ipsec/ipsec.h"
#include "net/annotations.h"
#include "ip/ip_appconf.h"
#include "ipsecfwd_stack.h"

#define IPSEC_MAX_ENC_KEY_SIZE (32 / 4)
#define IPSEC_MAX_HMAC_KEY_SIZE_BYTES 48
#define IPSEC_MAX_HMAC_KEY_SIZE (IPSEC_MAX_HMAC_KEY_SIZE_BYTES / 4)


/**
 \brief This structure allows the user to provide all IPsec
  tunnel configuration specific information. We adopted
  Openswan's IPsec tunnel bring up style. Since we are
  performing IPsec forwarding in tunnel mode, we identified
  each of the two systems involved in making the tunnel as
  left and right. This is the scheme Openswan follows to
  bring up the tunnel. So we assign ID to each system:
  LEFT_ROUTER or RIGHT_ROUTER. Each of these router protects
  a LAN. Based on these user provided information we populate
  our SAD, SPD-O and SPD-I.
 */
struct ipsec_tunnel_config_entry_t {
	uint32_t spi;  /**< Security Policy Index */
	bool is_esn;	/**< Extended Sequence Number support */
	uint64_t seq_num; /**< Sequence Number */
	uint32_t src_ip; /**< Source IP Address */
	uint32_t tunnel_src_ip_addr;	/**< Tunnel Source IP Address */
	uint32_t dst_ip;	/**< Destination IP Address */
	uint32_t tunnel_dst_ip_addr;  /**< Tunnel Destination IP Address */
	uint32_t tunnel_id;	/**< Tunnel Id */
	uint32_t defgw;	/** Default Gateway */
	bool hb_tunnel;
	struct app_ctrl_sa_algo *ealg; /**< Encryption Algo info */
	struct app_ctrl_sa_algo *aalg; /**< Authentication Algo info */
} __attribute__ ((aligned(64)));

/**
 \brief This structure would defined the cached memory
  objects needed to hold IPsec configuration information
 */
struct ipsec_tunnel_config_t {
	struct mem_cache_t *free_entries;
} __attribute__ ((aligned(64)));

/**
 \brief Creates cached memory object for IPsec config table.
 \return Pointer to the Ipsec config table
 */
int32_t ipsec_config_create(void);

/**
 \brief Obtains an entry from cached memory object for
	IPsec config table.
 \param[in] config Pointer to IPsec config table
 \return POinter to the Ipsec config entry
 */
struct ipsec_tunnel_config_entry_t *ipsec_config_entry_create
    (struct ipsec_tunnel_config_t *config);

/**
 \brief Adds the entry from cached memory object to
	IPsec config table.
 \param[in] config Pointer to IPsec config entry
 \param[in] ipsec_stack Ipsec Stack pointer
 \param[in] next_hop_addr Next Hop Address
 \param[in] mode mode encryption/ decryption
 \return Status
 */
int32_t ipsec_tunnel_create(struct ipsec_tunnel_config_entry_t *config,
			    struct ipsec_stack_t *ipsec_stack,
			    uint32_t *next_hop_addr, uint32_t mode);

/**
 \brief Adds entries to route cache.
	This routine can differentiate whether the entries to be
	added is process encap or decap processing. Decap requires
	one additional LOCAL_SCOPE entry to trigger IPsec Decap
	processing. And thus the need to differntiate between encap
	entries and decap entries to be added.
 \param[in] src_ip Source IP Address
 \param[in] dst_ip Destination IP Address
 \param[in] hop_ip Next Hop IP Address
 \param[in] ipsec_stack IPsec Stack Pointer
 \param[in] tunnel_route true/ false
 \param[in] tunnel tunnel info
 \return none
 */
void populate_route_cache(uint32_t src_ip,
			  uint32_t dst_ip,
			  uint32_t hop_ip,
			  struct ipsec_stack_t *ipsec_stack,
			  bool tunnel_route, struct ipsec_tunnel_t *tunnel,
			  bool, uint8_t);

extern struct ipsec_tunnel_config_t *g_ipsec_tunnel_config;
extern int32_t g_tunnel_id;
extern void *g_split_key;

#endif /* ifndef LIB_IPSEC_IPSEC_INIT_H */
