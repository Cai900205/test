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

# we would like to export these headers during install
pkginclude_HEADERS = git_version.h Fabric.h RouteSys.h Bipartite.h \
		 SubnMgt.h TraceRoute.h CredLoops.h Regexp.h \
		 TopoMatch.h SysDef.h Congestion.h ibnl_parser.h ibdm.i

# ibdm shared library version triplet is:
# API_ID:API_VER:NUM_PREV_API_SUP = x:y:z
# * change of API_ID means new API
# * change of AGE means how many API backward compt
# * change of API_VER is required every version
# Results with SO version: x-z:z:y
LIB_VER_TRIPLET="2:1:1"
LIB_FILE_TRIPLET=1.1.1

AM_YFLAGS = -d

# we are going to have here 3 targets:
# libibdmcom - common convinient library of objects
# libibdm - the TCL shared lib to be used as a package
# ibdmsh - the TCL shell

common_SOURCES = Fabric.cpp RouteSys.cc Bipartite.cc \
	SubnMgt.cpp TraceRoute.cpp CredLoops.cpp TopoMatch.cpp SysDef.cpp \
	LinkCover.cpp Congestion.cpp ibnl_parser.cc ibnl_scanner.cc FatTree.cpp

# Support debug mode through config variable
if DEBUG
DBG = -O0 -g
else
DBG = -O2
endif

# We have a special mode where we know the package will be eventually moved
# to $with_final_prefix
if USE_FINAL_PREFIX
IBNL_PATH=-DIBDM_IBNL_DIR='"$(with_final_prefix)/lib/ibdm$(VERSION)"'
else
IBNL_PATH=-DIBDM_IBNL_DIR='"$(libdir)/ibdm$(VERSION)"'
endif

AM_CXXFLAGS = $(DBG) $(IBNL_PATH) $(TCL_CPPFLAGS)
AM_CFLAGS =   $(DBG) $(IBNL_PATH) $(TCL_CFLAGS)

lib_LTLIBRARIES = libibdmcom.la libibdm.la
libibdmcom_la_SOURCES = $(common_SOURCES)
libibdmcom_la_LIBADD = 
libibdmcom_la_LDFLAGS = -version-info $(LIB_VER_TRIPLET)

libibdm_la_SOURCES = ibdm_wrap.cpp ibdm.i $(common_SOURCES)
libibdm_la_LIBADD = $(TCL_LIBS) 
libibdm_la_LDFLAGS = -Wl,-rpath -Wl,$(TCL_PREFIX)/lib -version-info $(LIB_VER_TRIPLET) -no-undefined

bin_PROGRAMS  = ibdmsh
ibdmsh_SOURCES = ibdmsh_wrap.cpp
ibdmsh_LDADD =  -libdmcom $(TCL_LIBS)
ibdmsh_LDFLAGS = -static -Wl,-rpath -Wl,$(TCL_PREFIX)/lib
ibdmsh_DEPENDENCIES=$(lib_LTLIBRARIES)

$(srcdir)/Fabric.cpp: $(srcdir)/git_version.h

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
	echo "#define IBDM_CODE_VERSION \"$$gitver\"" > .git_version.h ;\
	if test -f $(srcdir)/git_version.h ; then \
		if test `diff .git_version.h $(srcdir)/git_version.h | wc -l` != 0; then \
			mv -f .git_version.h $(srcdir)/git_version.h; \
			echo "Updated code version to: $$gitver"; \
		fi; \
	else \
		mv -f .git_version.h $(srcdir)/git_version.h; \
		echo "Creates code version to: $$gitver"; \
	fi;

FORCE:

# Workaound for libtool to use g++ for linking (verified as un-needed but JIC)
# CXXLINK = $(LIBTOOL) --tag=CXX --mode=link \
#           $(CXXLD) $(AM_CXXFLAGS) $(CXXFLAGS) $(AM_LDFLAGS) $(LDFLAGS) -o $@

# only generate the swig wrappers if they do not exist...
# so we avoid un-needed swig dependency
if HAS_SWIG
$(srcdir)/ibdm_wrap.cpp: @MAINTAINER_MODE_TRUE@ $(srcdir)/ibdm.i
	swig -c++ -dhtml -tcl8 -o swig_wrap.c $(srcdir)/ibdm.i
	$(srcdir)/fixSwigWrapper -g -s -p -o $@
	rm -f swig_wrap.c

$(srcdir)/ibdmsh_wrap.cpp: @MAINTAINER_MODE_TRUE@ $(srcdir)/ibdm.i
	swig -c++ -dhtml -tcl8  -ltclsh.i -o swig_wrap.c $(srcdir)/ibdm.i
	$(srcdir)/fixSwigWrapper -g -s -p -r ibdm -o $@
	rm -f swig_wrap.c
endif

swigclean:
	rm -f $(srcdir)/ibdm_wrap.cpp $(srcdir)/ibdmsh_wrap.cpp *wrap.o

ibnl_parser.cc: @MAINTAINER_MODE_TRUE@ $(srcdir)/ibnl_parser.yy
	$(srcdir)/../config/ylwrap $(srcdir)/ibnl_parser.yy \
		y.tab.c ibnl_parser.cc \
	 	y.tab.h ibnl_parser.h \
		y.output ibnl_parser.output -- bison -y -d
	if test ! ibnl_parser.cc -ef $(srcdir)/ibnl_parser.cc ; then cp ibnl_parser.cc $(srcdir)/ibnl_parser.cc ; fi
	if test ! ibnl_parser.h -ef $(srcdir)/ibnl_parser.h ; then cp ibnl_parser.h $(srcdir)/ibnl_parser.h; fi

ibnl_scanner.cc: @MAINTAINER_MODE_TRUE@ $(srcdir)/ibnl_scanner.ll
	$(srcdir)/../config/ylwrap $(srcdir)/ibnl_scanner.ll \
		lex.yy.c ibnl_scanner.cc -- flex
	if test ! ibnl_scanner.cc -ef $(srcdir)/ibnl_scanner.cc ; then cp ibnl_scanner.cc $(srcdir)/ibnl_scanner.cc; fi

# Don't distribute common.h, since it is build host dependent!
dist-hook: git_version.h
	rm -f $(distdir)/common.h

# we need there extra in the distribution as their dependency is not defined
EXTRA_DIST = swig_alternate_mangling.cpp fixSwigWrapper pkgIndex.tcl ibdm.i

# this actually over write the lib install
install-exec-am: install-binPROGRAMS
	mkdir -p $(DESTDIR)$(libdir)/ibdm$(VERSION)
	cp .libs/$(shell grep dlname libibdm.la | sed 's/.*=\(.*\)/\1/') \
	   $(DESTDIR)$(libdir)/ibdm$(VERSION)/libibdm.so.$(VERSION)
	sed 's/%VERSION%/'$(VERSION)'/g' $(srcdir)/pkgIndex.tcl > $(DESTDIR)$(libdir)/ibdm$(VERSION)/pkgIndex.tcl

