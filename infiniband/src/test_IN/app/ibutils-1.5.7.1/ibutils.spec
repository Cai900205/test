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

%{!?build_ibmgtsim: %define build_ibmgtsim 0}
%{?build_ibmgtsim: %define ibmgtsim --enable-ibmgtsim}

Summary: OpenIB Mellanox InfiniBand Diagnostic Tools
Name: ibutils
Version: 1.5.7.1
Release: 0.10.g385f871
License: GPL/BSD
Url: http://openfabrics.org/downloads/%{name}-%{version}.tar.gz
Group: System Environment/Libraries
Source: http://www.openfabrics.org/downloads/ibutils-1.5.7.1-0.10.g385f871.tar.gz
BuildRoot: %{?build_root:%{build_root}}%{!?build_root:%{_tmppath}/%{name}-%{version}-%{release}-root-%(%{__id_u} -n)}
Requires: opensm-libs, libibumad, tk
BuildRequires: opensm-devel, libibumad-devel
Vendor: Mellanox Technologies Ltd.
%description
ibutils provides IB network and path diagnostics.


%prep
%setup -n %{name}-%{version}

%build
%configure %{?configure_options} %{?ibmgtsim}
%{__make} %{?mflags}

%install
rm -rf $RPM_BUILD_ROOT
%{__make} install DESTDIR=$RPM_BUILD_ROOT %{?mflags_install}
/bin/rm -f $RPM_BUILD_ROOT/%{_libdir}/*.la
/bin/rm -f $RPM_BUILD_ROOT/%{_prefix}/bin/git_version.tcl

install -d $RPM_BUILD_ROOT/etc/profile.d
cat > $RPM_BUILD_ROOT/etc/profile.d/ibutils.sh << EOF
if ! echo \${PATH} | grep -q %{_prefix}/bin ; then
        PATH=\${PATH}:%{_prefix}/bin
fi
EOF
cat > $RPM_BUILD_ROOT/etc/profile.d/ibutils.csh << EOF
if ( "\${path}" !~ *%{_prefix}/bin* ) then
        set path = ( \$path %{_prefix}/bin )
endif
EOF

touch ibutils-files
case %{_prefix} in
        /usr | /usr/)
        ;;
        *)
        install -d $RPM_BUILD_ROOT/etc/ld.so.conf.d
        echo "%{_libdir}" >> $RPM_BUILD_ROOT/etc/ld.so.conf.d/ibutils.conf
        echo "/etc/ld.so.conf.d/ibutils.conf" >> ibutils-files
        ;;
esac


%clean
#Remove installed driver after rpm build finished
rm -rf $RPM_BUILD_ROOT
rm -rf $RPM_BUILD_DIR/%{name}-%{version}

%post
/sbin/ldconfig

###
### Files
###
%files -f ibutils-files
%defattr(-,root,root)
%{_prefix}/bin/ibis
%{_prefix}/bin/ibdmsh
%{_prefix}/bin/ibtopodiff
%{_prefix}/bin/ibnlparse
%{_prefix}/bin/ibdmtr
%{_prefix}/bin/ibdmchk
%{_prefix}/bin/ibdiagui
%{_prefix}/bin/ibdiagnet
%{_prefix}/bin/ibdiagpath
%{_libdir}/libibdmcom.so*
%{_libdir}/libibdmcom.a
%{_libdir}/libibdm.so*
%{_libdir}/libibdm.a
%{_libdir}/ibis1.5.7.1
%{_libdir}/ibdm1.5.7.1
%{_libdir}/ibdiagnet1.5.7.1
%{_libdir}/ibdiagpath1.5.7.1
%{_libdir}/ibdiagui1.5.7.1
%{_prefix}/include/ibdm
%{_mandir}/man1/ibdiagnet.1*
%{_mandir}/man1/ibdiagpath.1*
%{_mandir}/man1/ibdiagui.1*
%{_mandir}/man1/ibis.1*
%{_mandir}/man1/ibtopodiff.1*
%{_mandir}/man1/ibdmtr.1*
%{_mandir}/man1/ibdmsh.1*
%{_mandir}/man1/ibdmchk.1*
%{_mandir}/man1/ibdm-topo-file.1*
%{_mandir}/man1/ibdm-ibnl-file.1*
/etc/profile.d/ibutils.sh
/etc/profile.d/ibutils.csh
%{_prefix}/bin/dump2slvl.pl
%{_prefix}/bin/dump2psl.pl
%{_libdir}/libibsysapi.so*
%{_libdir}/libibsysapi.a

# These are only installed when ibmgtsim is built
%if %{build_ibmgtsim}
%{_prefix}/bin/mkSimNodeDir
%{_prefix}/bin/ibmssh
%{_prefix}/bin/ibmsquit
%{_prefix}/bin/RunSimTest
%{_prefix}/bin/IBMgtSim
%{_libdir}/libibmscli.so*
%{_libdir}/libibmscli.a
%{_prefix}/include/ibmgtsim
%{_prefix}/share/ibmgtsim
%{_mandir}/man1/ibmssh.1*
%{_mandir}/man1/ibmsquit.1*
%{_mandir}/man1/mkSimNodeDir.1*
%{_mandir}/man1/RunSimTest.1*
%{_mandir}/man1/IBMgtSim.1*
%endif

# END Files

%changelog
* Tue Aug 21 2006 Vladimir Sokolovsky <vlad@mellanox.co.il>
- Added ibmgtsim to the rpm
* Sun Jul 30 2006 Vladimir Sokolovsky <vlad@mellanox.co.il>
- Added man pages and share/ibmgtsim
* Tue May 16 2006 Vladimir Sokolovsky <vlad@mellanox.co.il>
- Added ibutils sh, csh and conf to update environment
* Sun Apr  2 2006 Vladimir Sokolovsky <vlad@mellanox.co.il>
- Initial packaging for openib gen2 stack
