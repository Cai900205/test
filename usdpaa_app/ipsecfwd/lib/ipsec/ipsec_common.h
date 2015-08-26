/*
 * Copyright (C) 2011 - 2012 Freescale Semiconductor, Inc.
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

#ifndef LIB_IPSEC_IPSEC_COMMON_H
#define LIB_IPSEC_IPSEC_COMMON_H

#include <usdpaa/fsl_qman.h>
#include <usdpaa/fsl_bman.h>
#include <errno.h>
#include "ip/ip.h"
#include "ethernet/eth.h"
#include "ip/ip_common.h"
#include "net/annotations.h"
#include "statistics.h"

#define NUM_TO_SEC_FQ		8	/**< Num of FQs to-SEC */
#define IPSEC_TUNNEL_ENTRIES	1024	/**< Total Tunnel Count */
#define ESP_HDR_SIZE		8	/**< ESP Header Size */
#define PADLEN_OFFSET		2	/**< Offset of padlen from end after
						 decryption from SEC Block */
#define ANNOTATION_AREA_SIZE sizeof(struct annotations_t)
			/**< Annotation Field Size */
#define IP_HDR_OFFSET	(ANNOTATION_AREA_SIZE + ETHER_HDR_LEN)
			/**< IPSec Payload Offset */
#define ENCRYPT 0		/**< Mode is Encryption */
#define DECRYPT 1		/**< Mode is Decryption */

/**
 \brief ESP header is constructed when we perform decap test.
	For encap test SEC40 appends the ESP header for us. This
	structure is needed mainly for decap.
 */
struct ipsec_esp_hdr_t {
	uint32_t spi;		/**< SPI Field */
	uint32_t seq_num;	/**< Sequence Number */
};

struct ipsec_context_t;

/**
 \brief Function pointer used for Parsing of received frame
	confirmation of xmitted frame
 \param[in] ctxt ContextB came in Frame Descriptor
 \param[in] notes Pointer to annotations field in incoming buffer
 \param[in] data Pointer to data in buffer
 */
typedef void (*ipsec_cb) (const struct ipsec_context_t *ipsec_ctxt,
				const struct qm_fd *fd, void *data);

struct ipsec_context_t {
#ifdef STATS_TBD
	struct ipsec_pkt_statistics_t *ipsec_stats;
#endif
	struct qman_fq fq_from_sec;
	struct qman_fq fq_to_sec[NUM_TO_SEC_FQ];
	struct ppam_rx_hash ppam_ctxt;
	ipsec_cb ipsec_handler;
	int num_fq_to_sec;
};

#ifdef STATS_TBD
/**
 \brief Its a place holder for all IPsec stack related
	statistics. We determine how many time we have
	traversed through the ipsec encap pre and post
	processing routine.
 */
struct ipsec_pkt_statistics_t {
	/* outbound (encap) pre/post */
	stat32_t encap_pre_sec;	/**< Encap Packets before SEC processing */
	stat32_t encap_post_sec;	/**< Encap Packets after SEC processing */

	/* inbound (decap) pre/post */
	stat32_t decap_pre_sec;	/**< Decap packet before SEC processing */
	stat32_t decap_post_sec;	/**< Decap packets after SEC processing */

	/* pkt dropped due to inbound lookup failure */
	stat32_t dropped;		/**< Packet Dropped */

	/* for future use */
	stat32_t sec_err;		/**< Packets Dropped because of error */
};

/**
 \brief This statistics table is only for IPsec inbound SPI
	lookup.
 */
struct ipsec_statistics_t {
	stat32_t total_datagrams;	/**< Total packets */
	stat32_t new_entries;		/**< Total new tunnel entries */
	stat32_t entry_count;		/**< Total entries created */
	stat32_t removed_entries;	/**< Total entries removed */
	union stat64_t table_hits;	/**< Packets hit tunnel entries */
	union stat64_t table_misses;	/**< Packets missed tunnel entries */
};

/**
 \brief Print IPsec statistics.
	IPsec application uses this routine to print all IPsec
	related statistics including packet statistics, tunnel
	statistics and so on.
 \param[in] stats IPSec Stats structure pointer
 \return none
 */
void ipsec_print_statistics(const struct ipsec_pkt_statistics_t *stats,
			    bool print_zero);
#endif

/**
 \brief Helper routine to convert simple fd to compound fd.
 \param[in] fd Simple FD pointer
 \param[in] old_fd Incoming Simple FD pointer
 \param[in] ip_hdr IPv4 header pointer in frame
 \return void
 */
void ipsec_create_compound_fd(struct qm_fd *fd, const struct qm_fd *old_fd,
				 struct iphdr *ip_hdr, uint8_t);
/**
 \brief Helper routine to convert compound fd to simple fd.
 \param[in] simple_fd Simple FD pointer
 \param[in] compound_fd Incoming Compound FD pointer
 \param[in] mode of operation
 \return none
 */
void ipsec_create_simple_fd(struct qm_fd *simple_fd,
			    const struct qm_fd *compound_fd, uint8_t);
/**
 \brief Creates an IP header.
	This helper routine is used to prepare outer IP header
	that is fed to SEC40. When SEC40 gets encap request it
	uses this IP header and pre-pend that with the encrypted
	packet.
 \param[in] ip_hdr IP Header pointer
 \param[in] saddr Source IP Address
 \param[in] daddr Destination IP Address
 \return none
 */
void ipsec_build_outer_ip_hdr(struct iphdr *ip_hdr,
			      uint32_t *saddr, uint32_t *daddr);
/*
 * Frame gets freed to bman pool
 */
void ppac_drop_frameer(void *virt_addr, uint8_t bpid);

#endif /* ifndef LIB_IPSEC_IPSEC_COMMON_H */
