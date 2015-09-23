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

#AUTOMAKE_OPTIONS 	= gnits

# note that order matters: make the lib first then use it
if BUILD_IBMGTSIM
IBMGTSIMDIR             = ibmgtsim
endif

SUBDIRS                 = ibis ibdm ibdiag $(IBMGTSIMDIR)
DIST_SUBDIRS            = ibis ibdm ibdiag $(IBMGTSIMDIR)

# this will control the update of the files in order
MAINTAINERCLEANFILES = Makefile.in aclocal.m4 configure config-h.in

ACLOCAL			= aclocal -I $(ac_aux_dir)

EXTRA_DIST = ibutils.spec.in ibmgtsim debian/changelog \
             debian/compat debian/control \
             debian/copyright debian/dirs \
             debian/docs debian/ibutils.docs \
             debian/ibutils.install debian/ibutils.manpages \
             debian/libibdm1.install \
             debian/rules debian/source/format

dist-hook: ibutils.spec
	cp ibutils.spec $(distdir)

#dist-hook:
#	cd $(distdir)
#	cp -rf $(top_srcdir)/Clusters Clusters
