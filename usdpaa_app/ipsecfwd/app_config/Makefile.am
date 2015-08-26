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

bin_PROGRAMS = ipsecfwd_config
dist_DATA = ipsecfwd_enc_20G.sh ipsecfwd_enc_22G.sh ipsecfwd_dec_20G.sh \
	ipsecfwd_dec_22G.sh ipsecfwd_mix_20G.sh ipsecfwd_mix_15G.sh \
	ipsecfwd_mix_40G.sh ipsecfwd_enc_40G.sh ipsecfwd_enc_42G.sh \
	ipsecfwd_dec_40G.sh ipsecfwd_dec_42G.sh ipsecfwd_t1040qds_enc_7G.sh \
	ipsecfwd_t1040qds_dec_7G.sh ipsecfwd_t2080_enc_42G.sh \
	ipsecfwd_t2080_dec_42G.sh ipsecfwd_t2080_mix_40G.sh \
	ipsecfwd_t1040qds_mix_7G.sh ipsecfwd_t4240_dec_48G.sh \
	ipsecfwd_t4240_enc_48G.sh ipsecfwd_t4240_mix_48G.sh \
	ipsecfwd_b4860qds_mix_20G.sh ipsecfwd_t2080_enc_22G.sh \
	ipsecfwd_t2080_mix_22G.sh

AM_CFLAGS := -I$(TOP_LEVEL)/apps/ipsecfwd/lib -I$(TOP_LEVEL)/apps/ipsecfwd/include
AM_CFLAGS += $(LIBRTA_CFLAGS)

ipsecfwd_config_SOURCES := ipc_send.c

ipsecfwd_config_LDADD := usdpaa_of

ipsecfwd_config_sys_LDADD := rt

ipsecfwd_enc_20G.sh_install = $(INSTALL_BIN)
ipsecfwd_enc_20G.sh_install_flags = $(INSTALL_BIN_FLAGS)
ipsecfwd_enc_22G.sh_install = $(INSTALL_BIN)
ipsecfwd_enc_22G.sh_install_flags = $(INSTALL_BIN_FLAGS)
ipsecfwd_enc_40G.sh_install = $(INSTALL_BIN)
ipsecfwd_enc_40G.sh_install_flags = $(INSTALL_BIN_FLAGS)
ipsecfwd_enc_42G.sh_install = $(INSTALL_BIN)
ipsecfwd_enc_42G.sh_install_flags = $(INSTALL_BIN_FLAGS)
ipsecfwd_t1040qds_enc_7G.sh_install = $(INSTALL_BIN)
ipsecfwd_t1040qds_enc_7G.sh_install_flags = $(INSTALL_BIN_FLAGS)
ipsecfwd_t2080qds_enc_7G.sh_install = $(INSTALL_BIN)
ipsecfwd_t2080qds_enc_7G.sh_install_flags = $(INSTALL_BIN_FLAGS)
ipsecfwd_t2080_enc_42G.sh_install = $(INSTALL_BIN)
ipsecfwd_t2080_enc_42G.sh_install_flags = $(INSTALL_BIN_FLAGS)
ipsecfwd_t2080_enc_22G.sh_install = $(INSTALL_BIN)
ipsecfwd_t2080_enc_22G.sh_install_flags = $(INSTALL_BIN_FLAGS)
ipsecfwd_dec_20G.sh_install = $(INSTALL_BIN)
ipsecfwd_dec_20G.sh_install_flags = $(INSTALL_BIN_FLAGS)
ipsecfwd_dec_22G.sh_install = $(INSTALL_BIN)
ipsecfwd_dec_22G.sh_install_flags = $(INSTALL_BIN_FLAGS)
ipsecfwd_dec_40G.sh_install = $(INSTALL_BIN)
ipsecfwd_dec_40G.sh_install_flags = $(INSTALL_BIN_FLAGS)
ipsecfwd_dec_42G.sh_install = $(INSTALL_BIN)
ipsecfwd_dec_42G.sh_install_flags = $(INSTALL_BIN_FLAGS)
ipsecfwd_t1040qds_dec_7G.sh_install = $(INSTALL_BIN)
ipsecfwd_t1040qds_dec_7G.sh_install_flags = $(INSTALL_BIN_FLAGS)
ipsecfwd_t2080qds_dec_7G.sh_install = $(INSTALL_BIN)
ipsecfwd_t2080qds_dec_7G.sh_install_flags = $(INSTALL_BIN_FLAGS)
ipsecfwd_t2080_dec_42G.sh_install = $(INSTALL_BIN)
ipsecfwd_t2080_dec_42G.sh_install_flags = $(INSTALL_BIN_FLAGS)
ipsecfwd_mix_20G.sh_install = $(INSTALL_BIN)
ipsecfwd_mix_20G.sh_install_flags = $(INSTALL_BIN_FLAGS)
ipsecfwd_mix_15G.sh_install = $(INSTALL_BIN)
ipsecfwd_mix_15G.sh_install_flags = $(INSTALL_BIN_FLAGS)
ipsecfwd_mix_40G.sh_install = $(INSTALL_BIN)
ipsecfwd_mix_40G.sh_install_flags = $(INSTALL_BIN_FLAGS)
ipsecfwd_t2080_mix_40G.sh_install = $(INSTALL_BIN)
ipsecfwd_t2080_mix_40G.sh_install_flags = $(INSTALL_BIN_FLAGS)
ipsecfwd_t1040qds_mix_7G.sh_install = $(INSTALL_BIN)
ipsecfwd_t1040qds_mix_7G.sh_install_flags = $(INSTALL_BIN_FLAGS)
ipsecfwd_t4240_enc_48G.sh.sh_install = $(INSTALL_BIN)
ipsecfwd_t4240_enc_48G.sh.sh_install_flags = $(INSTALL_BIN_FLAGS)
ipsecfwd_t4240_dec_48G.sh.sh_install = $(INSTALL_BIN)
ipsecfwd_t4240_mix_48G.sh.sh_install_flags = $(INSTALL_BIN_FLAGS)
ipsecfwd_t4240_mix_48G.sh.sh_install = $(INSTALL_BIN)
ipsecfwd_t4240_dec_48G.sh.sh_install_flags = $(INSTALL_BIN_FLAGS)
ipsecfwd_b4860qds_mix_20G.sh.sh_install_flags = $(INSTALL_BIN_FLAGS)
ipsecfwd_b4860qds_mix_20G.sh.sh_install = $(INSTALL_BIN)
ipsecfwd_t2080_mix_22G.sh_install = $(INSTALL_BIN)
ipsecfwd_t2080_mix_22G.sh_install_flags = $(INSTALL_BIN_FLAGS)
