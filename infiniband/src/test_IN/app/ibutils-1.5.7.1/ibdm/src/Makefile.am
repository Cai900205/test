#--
# Copyright (c) 2004-2010 Mellanox Technologies LTD. All rights reserved.
#
# This software is available to you under a choice of one of two
# licenses.  You may choose to be licensed under the terms of the GNU
# General Public License (GPL) Version 2, available from the file
# COPYING in the main directory of this source tree, or the
# OpenIB.org BSD license below:
#
#     Redistribution and use in source and binary forms, with or
#     without modification, are permitted provided that the following
#     conditions are met:
#
#      - Redistributions of source code must retain the above
#        copyright notice, this list of conditions and the following
#        disclaimer.
#
#      - Redistributions in binary form must reproduce the above
#        copyright notice, this list of conditions and the following
#        disclaimer in the documentation and/or other materials
#        provided with the distribution.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
# EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
# MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
# NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
# BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
# ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
# CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
# SOFTWARE.
#--

## Makefile.am -- Process this file with automake to produce Makefile.in

# MAINTAINERCLEANFILES	= Makefile.in common.h stamp-common

# INCLUDES		= -I$(top_builddir) -I$(top_srcdir) $(TCL_CPPFLAGS)
pkginclude_HEADERS = ibsysapi.h

AM_CPPFLAGS = -I$(top_srcdir)/ibdm

bin_PROGRAMS = ibdmchk ibdmtr ibtopodiff ibnlparse
LDADD = -L../ibdm -libdmcom 

# Why do we need to provide extra dep? the LDADD should have worked isnt it?
# Deprecated : ibdmsim_DEPENDENCIES=../ibdm/libibdmcom.la
ibdmtr_DEPENDENCIES=../ibdm/libibdmcom.la
ibdmchk_DEPENDENCIES=../ibdm/libibdmcom.la
ibtopodiff_DEPENDENCIES=../ibdm/libibdmcom.la
ibnlparse_DEPENDENCIES=../ibdm/libibdmcom.la

# Deprecated : ibdmsim_SOURCES = fabric_sim.cpp
ibdmtr_SOURCES = fabric_tr.cpp
ibdmchk_SOURCES = osm_check.cpp
ibtopodiff_SOURCES = test_topo_match.cpp
ibnlparse_SOURCES = test_ibnl_parser.cpp

lib_LTLIBRARIES = libibsysapi.la
libibsysapi_la_SOURCES = ibsysapi.cpp
libibsysapi_la_LDFLAGS = -version-info 1:0:0
libibsysapi_la_LIBADD = -L../ibdm -libdmcom 

#regexp_test_SOURCES = regexp_test.cpp

# Don't distribute common.h, since it is build host dependent!
dist-hook:
	rm -f $(distdir)/common.h
