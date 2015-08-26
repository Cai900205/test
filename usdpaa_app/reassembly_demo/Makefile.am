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

bin_PROGRAMS = reassembly_demo

dist_DATA = reassembly_demo_config-p4080.xml \
		reassembly_demo_config-b4860.xml \
		reassembly_demo_config-b4420.xml \
		reassembly_demo_config-p2041.xml \
		reassembly_demo_config-t2080.xml \
		reassembly_demo_config-t4240.xml \
		reassembly_demo_policy.xml \
		reassembly_demo_policy-v3.xml

reassembly_demo_CFLAGS := -I$(TOP_LEVEL)/apps/include \
			  $(FMC_CFLAGS) $(FMLIB_CFLAGS)

reassembly_demo_LDFLAGS := $(LIBXML2_LDFLAGS) $(LIBEDIT_LDFLAGS) \
			   $(FMC_LDFLAGS) $(FMLIB_LDFLAGS) \
			   -T $(TOP_LEVEL)/apps/ppac/ppac.lds
reassembly_demo_SOURCES := reassembly_demo.c vsp_api.c

reassembly_demo_LDADD := usdpaa_dpa_offload usdpaa_ppac usdpaa_syscfg \
		usdpaa_qbman usdpaa_fman usdpaa_dma_mem usdpaa_of \
		usdpaa_process fifo_queue
