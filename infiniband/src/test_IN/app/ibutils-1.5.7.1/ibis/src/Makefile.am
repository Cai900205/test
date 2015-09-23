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

# Support debug mode through config variable
if DEBUG
DBG = -O0 -g -Wall
else
DBG = -O2 -Wall
endif

AM_CFLAGS = $(TCL_CPPFLAGS) $(OSM_CFLAGS) $(DBG) -fno-strict-aliasing -fPIC -DIBIS_VERSION=\"$(VERSION)\"
AM_CXXFLAGS = $(TCL_CPPFLAGS) $(OSM_CFLAGS) $(DBG) -fno-strict-aliasing -fPIC  -DIBIS_VERSION=\"$(VERSION)\"


# ibis shared library version triplet is:
# API_ID:API_VER:NUM_PREV_API_SUP = x:y:z
# * change of API_ID means new API
# * change of AGE means how many API backward compt
# * change of API_VER is required every version
# Results with SO version: x-z:z:y
LIB_VER_TRIPLET="1:0:0"
LIB_FILE_TRIPLET=1.0.0

lib_LTLIBRARIES = libibis.la
noinst_LIBRARIES = libibiscom.a

libibiscom_a_SOURCES = ibbbm.c ibcr.c	ibis.c ibis_gsi_mad_ctrl.c \
	ibpm.c ibsac.c ibsm.c ibvs.c ibcc.c
# Below op causes renaming of the obj files - used when same source creates 2 targets (TODO: is this duplication needed ?)
libibiscom_a_CFLAGS = $(AM_CFLAGS)

# client library to be used by IBIS TCL package:
libibis_la_SOURCES = ibis_wrap.c ibbbm.c ibcr.c	ibis.c ibis_gsi_mad_ctrl.c \
	ibpm.c ibsac.c ibsm.c ibvs.c ibcc.c

libibis_la_LDFLAGS = -version-info $(LIB_VER_TRIPLET) -no-undefined  \
	 $(OSM_LDFLAGS) $(TCL_LIBS)

bin_PROGRAMS = ibis

# this is used for the libraries link
LDADD = $(OSM_LDFLAGS)

ibis_SOURCES = ibissh_wrap.cpp

# note the order of the libraries does matter as we static link
ibis_LDADD = -L. -libiscom $(OSM_LDFLAGS) $(TCL_LIBS)


# SWIG FILES:
SWIG_IFC_FILES= $(srcdir)/ibbbm.i \
	$(srcdir)/ibcr.i \
	$(srcdir)/ibis.i \
	$(srcdir)/ibis_typemaps.i \
	$(srcdir)/ibpm.i \
	$(srcdir)/ibsac.i \
	$(srcdir)/ibsm.i \
	$(srcdir)/ibvs.i \
	$(srcdir)/ibcc.i

$(srcdir)/ibis.c: $(srcdir)/git_version.h

# track latest GIT version for this tree:
GIT=$(shell which git)

$(srcdir)/git_version.h:  @MAINTAINER_MODE_TRUE@ FORCE
	if test x$(GIT) != x ; then \
	   gitver=`cd $(srcdir) ; git rev-parse --verify HEAD`; \
	   changes=`cd $(srcdir) ; git diff . | grep ^diff | wc -l`; \
	else \
	   gitver=undefined; changes=0; \
	fi ; \
	if test $$changes != 0; then gitver="$$gitver with-local-mods"; fi; \
	echo "#define IBIS_CODE_VERSION \"$$gitver\"" > .git_version.h ;\
	if test -f $(srcdir)/git_version.h ; then \
		if test `diff .git_version.h $(srcdir)/git_version.h | wc -l` != 0; then \
			mv -f .git_version.h $(srcdir)/git_version.h; \
			echo "Updated code version to: $$gitver"; \
		fi; \
	else \
		mv -f .git_version.h $(srcdir)/git_version.h; \
		echo "Created code version file with version: $$gitver"; \
	fi;

FORCE:

# only generate the swig wrappers if they do not exist...
# so we avoid un-needed swig dependency
if HAS_SWIG
$(srcdir)/ibis_wrap.c: @MAINTAINER_MODE_TRUE@ $(SWIG_IFC_FILES)
	swig -I$(srcdir) -dhtml -tcl8 -o swig_wrap.c $(srcdir)/ibis.i
	$(srcdir)/fixSwigWrapper -g -s -p -o $@
	rm -f swig_wrap.c

$(srcdir)/ibissh_wrap.cpp: @MAINTAINER_MODE_TRUE@ $(SWIG_IFC_FILES)
	swig -I$(srcdir) -dhtml -tcl8  -ltclsh.i -o swig_wrap.c $(srcdir)/ibis.i
	$(srcdir)/fixSwigWrapper -g -s -p -r ibis -o $@
	rm -f swig_wrap.c
endif

swigclean:
	rm -f $(srcdir)/ibis_wrap.c $(srcdir)/ibissh_wrap.cpp *wrap.o

# we need there extra in the distribution as their dependency is not defined
EXTRA_DIST = swig_extended_obj.c fixSwigWrapper pkgIndex.tcl \
	 ibbbm_base.h \
	 ibbbm.h \
	 ibcr_base.h \
	 ibcr.h \
	 ibis_api.h \
	 ibis_base.h \
	 ibis_gsi_mad_ctrl.h \
	 ibis.h \
	 ibpm_base.h \
	 ibpm.h \
	 ibsac.h \
	 ibsm.h \
	 ibvs_base.h \
	 ibvs.h \
	 ibcc.h \
	 git_version.h

# we do not want the temporary and the archive libs installed:
install-libLTLIBRARIES:

# this actually over write the lib install
install-exec-am: install-binPROGRAMS
	mkdir -p $(DESTDIR)/$(libdir)/ibis$(VERSION)
	cp .libs/libibis.so.$(LIB_FILE_TRIPLET) $(DESTDIR)/$(libdir)/ibis$(VERSION)/libibis.so.$(VERSION)
	sed 's/%VERSION%/'$(VERSION)'/g' $(srcdir)/pkgIndex.tcl > $(DESTDIR)/$(libdir)/ibis$(VERSION)/pkgIndex.tcl

