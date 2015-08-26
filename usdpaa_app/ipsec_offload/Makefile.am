# Copyright (c) 2011-2013 Freescale Semiconductor, Inc.
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are met:
#     * Redistributions of source code must retain the above copyright
#	notice, this list of conditions and the following disclaimer.
#     * Redistributions in binary form must reproduce the above copyright
#	notice, this list of conditions and the following disclaimer in the
#	documentation and/or other materials provided with the distribution.
#     * Neither the name of Freescale Semiconductor nor the
#	names of its contributors may be used to endorse or promote products
#	derived from this software without specific prior written permission.
#
#
# ALTERNATIVELY, this software may be distributed under the terms of the
# GNU General Public License ("GPL") as published by the Free Software
# Foundation, either version 2 of that License or (at your option) any
# later version.
#
# THIS SOFTWARE IS PROVIDED BY Freescale Semiconductor ``AS IS'' AND ANY
# EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL Freescale Semiconductor BE LIABLE FOR ANY
# DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
# (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
# LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
# ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
# SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

bin_PROGRAMS = ipsec_offload

dist_DATA = ipsec_offload_config_p4080.xml ipsec_offload_config_p4080_1p.xml \
            ipsec_offload_config_p2041.xml ipsec_offload_config_p2041_1p.xml \
            ipsec_offload_config_t4240.xml ipsec_offload_config_t4240_1p.xml \
            ipsec_offload_config_t2080.xml ipsec_offload_config_t2080_1p.xml \
            ipsec_offload_pcd_p2.xml ipsec_offload_pcd_p2_1p.xml \
            ipsec_offload_config_b4860.xml ipsec_offload_config_b4860_1p.xml \
            ipsec_offload_config_b4420.xml \
            ipsec_offload_pcd_p4.xml ipsec_offload_pcd_p4_1p.xml \
            ipsec_offload_pcd_b4.xml ipsec_offload_pcd_b4_1p.xml \
            ipsec_offload_pcd_t4.xml ipsec_offload_pcd_t4_1p.xml \
            ipsec_offload_pcd_t2.xml ipsec_offload_pcd_t2_1p.xml \
            ipsec_offload_policy.xml ipsec_offload_policy_1p.xml ipsec_offload_swp.xml \
            ipsec_offload_config_p4080_1p_fman0_instance.xml \
            ipsec_offload_config_p4080_1p_fman1_instance.xml \
            ipsec_offload_policy_1p_fman0_instance.xml \
            ipsec_offload_policy_1p_fman1_instance.xml \
            ipsec_offload_config_t4240_1p_fman0_instance.xml \
            ipsec_offload_config_t4240_1p_fman1_instance.xml

AM_CFLAGS := -I$(TOP_LEVEL)/apps/include \
	     $(FMC_CFLAGS) $(FMLIB_CFLAGS) $(LIBRTA_CFLAGS)

ipsec_offload_SOURCES := ipsec_offload.c pcd_init.c \
			ipsec_init.c xfrm_km.c \
			pfkey_utils.c neigh_utils.c \
			route_utils.c stats_init.c

ipsec_offload_LDADD := usdpaa_ppac usdpaa_syscfg usdpaa_qbman usdpaa_fman \
		      usdpaa_dma_mem usdpaa_of usdpaa_process \
		      usdpaa_dpa_offload fifo_queue

ipsec_offload_LDFLAGS := $(LIBXML2_LDFLAGS) $(LIBEDIT_LDFLAGS) \
			$(FMC_LDFLAGS) $(FMLIB_LDFLAGS) \
			-T $(TOP_LEVEL)/apps/ppac/ppac.lds
