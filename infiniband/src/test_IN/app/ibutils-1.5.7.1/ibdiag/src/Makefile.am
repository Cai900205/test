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

bin_SCRIPTS = ibdiagnet ibdiagpath ibdiagui git_version.tcl
EXTRA_DIST = ibdebug_if.tcl ibdebug.tcl \
	ibdiagnet.in ibdiagnet.pkg ibdiagnet.tcl \
	ibdiagpath.in ibdiagpath.pkg ibdiagpath.tcl \
	ibdiagui.in ibdiagui.pkg ibdiagui.tcl git_version.tcl

GIT=$(shell which git)

git_version.tcl : @MAINTAINER_MODE_TRUE@ FORCE
	if test x$(GIT) != x ; then \
	   gitver=`cd $(srcdir) ; git rev-parse --verify HEAD`; \
	   changes=`cd $(srcdir) ; git diff . | grep ^diff | wc -l`; \
	else \
	   gitver=undefined; changes=0; \
	fi ; \
	if test $$changes != 0; then gitver="$$gitver with-local-mods"; fi; \
	echo "set ibdiagSourceVersion \"$$gitver\"" > .git_version.tcl ;\
	if test -f $(srcdir)/git_version.tcl ; then \
		if test `diff .git_version.tcl $(srcdir)/git_version.tcl | wc -l` != 0; then \
			mv -f .git_version.tcl $(srcdir)/git_version.tcl; \
			echo "Updated code version to: $$gitver"; \
		fi; \
	else \
		mv -f .git_version.tcl $(srcdir)/git_version.tcl; \
		echo "Created code version file with version: $$gitver"; \
	fi;

FORCE:

ibdiagnet: ibdiagnet.in
	sed -e 's=%ibdiag_libdir%='$(DESTDIR)$(libdir)'=' \
	    -e 's=%with_ibdm_lib%='$(with_ibdm_lib)'=' \
	    -e 's=%with_ibis_bindir%='$(DESTDIR)$(bindir)'=' \
	    -e 's=%with_ibis%='$(with_ibis)'=' $(srcdir)/ibdiagnet.in > ibdiagnet

ibdiagpath: ibdiagpath.in
	sed -e 's=%ibdiag_libdir%='$(DESTDIR)$(libdir)'=' \
	    -e 's=%with_ibdm_lib%='$(with_ibdm_lib)'=' \
	    -e 's=%with_ibis_bindir%='$(DESTDIR)$(bindir)'=' \
	    -e 's=%with_ibis%='$(with_ibis)'=' $(srcdir)/ibdiagpath.in > ibdiagpath

ibdiagui: ibdiagui.in
	sed -e 's=%ibdiag_libdir%='$(DESTDIR)$(libdir)'=' \
	    -e 's=%with_ibdm_lib%='$(with_ibdm_lib)'=' \
	    -e 's=%with_tk_lib%='$(with_tk_lib)'=' \
	    -e 's=%with_graphviz_lib%='$(with_graphviz_lib)'=' \
	    -e 's=%with_ibis_bindir%='$(DESTDIR)$(bindir)'=' \
	    -e 's=%with_ibis%='$(with_ibis)'=' $(srcdir)/ibdiagui.in > ibdiagui

clean:
	rm -f ibdiagui ibdiagnet ibdiagpath

NET_LIB_DIR=$(DESTDIR)/$(libdir)/ibdiagnet$(VERSION)
PATH_LIB_DIR=$(DESTDIR)/$(libdir)/ibdiagpath$(VERSION)
UI_LIB_DIR=$(DESTDIR)/$(libdir)/ibdiagui$(VERSION)
install-exec-am: install-binSCRIPTS
	mkdir -p $(NET_LIB_DIR)
	cp $(srcdir)/ibdiagnet.pkg $(NET_LIB_DIR)/pkgIndex.tcl
	cp $(srcdir)/ibdebug.tcl $(NET_LIB_DIR)/ibdebug.tcl
	cp $(srcdir)/ibdebug_if.tcl $(NET_LIB_DIR)/ibdebug_if.tcl
	cp $(srcdir)/ibdiagnet.tcl $(NET_LIB_DIR)/ibdiagnet.tcl
	cp $(srcdir)/git_version.tcl $(NET_LIB_DIR)/git_version.tcl
	mkdir -p $(PATH_LIB_DIR)
	cp $(srcdir)/ibdiagpath.pkg $(PATH_LIB_DIR)/pkgIndex.tcl
	cp $(srcdir)/ibdebug.tcl $(PATH_LIB_DIR)/ibdebug.tcl
	cp $(srcdir)/ibdebug_if.tcl $(PATH_LIB_DIR)/ibdebug_if.tcl
	cp $(srcdir)/ibdiagpath.tcl $(PATH_LIB_DIR)/ibdiagpath.tcl
	cp $(srcdir)/git_version.tcl $(PATH_LIB_DIR)/git_version.tcl
	mkdir -p $(UI_LIB_DIR)
	cp $(srcdir)/ibdiagui.pkg $(UI_LIB_DIR)/pkgIndex.tcl
	cp $(srcdir)/ibdebug.tcl $(UI_LIB_DIR)/ibdebug.tcl
	cp $(srcdir)/ibdebug_if.tcl $(UI_LIB_DIR)/ibdebug_if.tcl
	cp $(srcdir)/ibdiagui.tcl $(UI_LIB_DIR)/ibdiagui.tcl
	cp $(srcdir)/git_version.tcl $(UI_LIB_DIR)/git_version.tcl
