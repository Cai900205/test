# Copyright (c) 2011 - 2012 Freescale Semiconductor, Inc.
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

lib_LIBRARIES = usdpaa_ppac
dist_DATA =						\
	usdpaa_config_p2_p3_p5_14g.xml			\
	usdpaa_config_p2_p3_p5_14g_shared_mac.xml	\
	usdpaa_config_p3_p5_serdes_0x04.xml		\
	usdpaa_config_p3_p5_serdes_0x33.xml		\
	usdpaa_config_p4_serdes_0xe_offline_host.xml	\
	usdpaa_config_p4_serdes_0x10.xml		\
	usdpaa_config_p4_serdes_0xe.xml			\
	usdpaa_config_p4_serdes_0x16.xml		\
	usdpaa_config_p5_serdes_0x02.xml		\
	usdpaa_config_b4_serdes_0x2a_0x98.xml		\
	usdpaa_config_b4_serdes_0x18_0x9e.xml		\
	usdpaa_config_t4_serdes_1_1_6_6.xml		\
	usdpaa_config_t4_48g.xml			\
	usdpaa_config_t2_serdes_66_16.xml		\
	usdpaa_config_t2_serdes_66_16_22g.xml		\
	usdpaa_config_t1_serdes_0x66.xml		\
	usdpaa_policy_hash_ipv4.xml			\
	usdpaa_policy_hash_ipv4_arp_coarse_classify.xml \
	usdpaa_policy_hash_shared_mac_ipv4.xml		\
	usdpaa_custom_coarse_classify_netpdl.xml	\
	usdpaa_policer.xml

AM_CFLAGS := -I$(TOP_LEVEL)/apps/include -I$(TOP_LEVEL)/apps/lib
AM_CFLAGS += $(LIBRTA_CFLAGS)

usdpaa_ppac_SOURCES = main.c
usdpaa_ppac_CFLAGS :=								\
	$(LIBEDIT_CFLAGS)							\
	-DDEF_PCD_PATH="/$(INSTALL_OTHER)/usdpaa_policy_hash_ipv4.xml"		\
	-DDEF_CFG_PATH="/$(INSTALL_OTHER)/usdpaa_config_p4_serdes_0xe.xml"
