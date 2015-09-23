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

man_MANS = ibdmtr.1 \
	ibdmchk.1 \
	ibdmsh.1 \
	ibdm-topo-file.1 \
	ibdm-ibnl-file.1 \
	ibtopodiff.1

EXTRA_DIST = \
	ibdmtr.pod         ibdmtr.1 \
	ibdmchk.pod        ibdmchk.1 \
	ibdmsh.pod         ibdmsh.1 \
	ibdm-topo-file.pod ibdm-topo-file.1 \
	ibdm-ibnl-file.pod ibdm-ibnl-file.1 \
	ibtopodiff.pod     ibtopodiff.1 \
	ibdm_pod2man

%.1: @MAINTAINER_MODE_TRUE@ %.pod
	$(srcdir)/ibdm_pod2man $< $@
	if test ! $@ -ef $(srcdir)/$@ ; then cp -f $@ $(srcdir)/$@; fi

