
%define RELEASE 0.1
%define rel %{?CUSTOM_RELEASE}%{!?CUSTOM_RELEASE:%RELEASE}

Summary: OpenFabrics Alliance InfiniBand Diagnostic Tools
Name: infiniband-diags
Version: 1.6.4.MLNX20140817.10ee11a
Release: %rel%{?dist}
License: GPLv2 or BSD
Group: System Environment/Libraries
BuildRoot: %{_tmppath}/%{name}-%{version}-%{release}-root-%(%{__id_u} -n)
Source: http://www.openfabrics.org/downloads/management/infiniband-diags-1.6.4.MLNX20140817.10ee11a.tar.gz
Url: http://openfabrics.org/
BuildRequires: libibmad-devel, opensm-devel, libibumad-devel, glib2-devel
Requires: libibmad, opensm-libs, libibumad, glib2
Provides: perl(IBswcountlimits)
Obsoletes: openib-diags

%description
This package provides IB diagnostic programs and scripts needed to
diagnose an IB subnet.

%package compat
Summary: OpenFabrics Alliance InfiniBand Diagnostic Tools
Group: System Environment/Libraries
BuildRequires: libibmad-devel, opensm-devel, libibumad-devel
Requires: libibmad, opensm-libs, libibumad

%description compat
Deprecated scripts and utilities which provide duplicated functionality, most
often at a reduced performance.  These are maintained for the time being for
compatibility reasons.

%package guest
Summary: OpenFabrics Alliance InfiniBand Diagnostic Tools for Guest OS
Group: System Environment/Libraries

%description guest
This package provides IB diagnostic programs and scripts for Virtual Machines,
needed to diagnose an IB subnet.

%prep
%setup -q

%build
%configure --enable-compat-utils
make

%install
rm -rf $RPM_BUILD_ROOT
make DESTDIR=${RPM_BUILD_ROOT} install
# remove unpackaged files from the buildroot
rm -f $RPM_BUILD_ROOT%{_libdir}/*.la

%clean
rm -rf $RPM_BUILD_ROOT

%files compat
%defattr(-,root,root)
%{_sbindir}/ibcheckerrs
%{_mandir}/man8/ibcheckerrs.*
%{_sbindir}/ibchecknet
%{_mandir}/man8/ibchecknet.*
%{_sbindir}/ibchecknode
%{_mandir}/man8/ibchecknode.*
%{_sbindir}/ibcheckport
%{_mandir}/man8/ibcheckport.*
%{_sbindir}/ibcheckportwidth
%{_mandir}/man8/ibcheckportwidth.*
%{_sbindir}/ibcheckportstate
%{_mandir}/man8/ibcheckportstate.*
%{_sbindir}/ibcheckwidth
%{_mandir}/man8/ibcheckwidth.*
%{_sbindir}/ibcheckstate
%{_mandir}/man8/ibcheckstate.*
%{_sbindir}/ibcheckerrors
%{_mandir}/man8/ibcheckerrors.*
%{_sbindir}/ibdatacounts
%{_mandir}/man8/ibdatacounts.*
%{_sbindir}/ibdatacounters
%{_mandir}/man8/ibdatacounters.*
%{_sbindir}/ibdiscover.pl
%{_mandir}/man8/ibdiscover.*
%{_sbindir}/ibswportwatch.pl
%{_mandir}/man8/ibswportwatch.*
%{_sbindir}/ibqueryerrors.pl
%{_sbindir}/iblinkinfo.pl
%{_sbindir}/ibprintca.pl
%{_mandir}/man8/ibprintca.*
%{_sbindir}/ibprintswitch.pl
%{_mandir}/man8/ibprintswitch.*
%{_sbindir}/ibprintrt.pl
%{_mandir}/man8/ibprintrt.*
%{_sbindir}/set_nodedesc.sh


%files
%defattr(-,root,root)
# C programs here
%{_sbindir}/ibaddr
%{_mandir}/man8/ibaddr.*
%{_sbindir}/ibnetdiscover
%{_mandir}/man8/ibnetdiscover.*
%{_sbindir}/ibping
%{_mandir}/man8/ibping.*
%{_sbindir}/ibportstate
%{_mandir}/man8/ibportstate.*
%{_sbindir}/ibroute
%{_mandir}/man8/ibroute.*
%{_sbindir}/ibstat
%{_mandir}/man8/ibstat.*
%{_sbindir}/ibsysstat
%{_mandir}/man8/ibsysstat.*
%{_sbindir}/ibtracert
%{_mandir}/man8/ibtracert.*
%{_sbindir}/perfquery
%{_mandir}/man8/perfquery.*
%{_sbindir}/sminfo
%{_mandir}/man8/sminfo.*
%{_sbindir}/smpdump
%{_mandir}/man8/smpdump.*
%{_sbindir}/smpquery
%{_mandir}/man8/smpquery.*
%{_sbindir}/saquery
%{_mandir}/man8/saquery.*
%{_sbindir}/vendstat
%{_mandir}/man8/vendstat.*
%{_sbindir}/iblinkinfo
%{_mandir}/man8/iblinkinfo.*
%{_sbindir}/ibqueryerrors
%{_mandir}/man8/ibqueryerrors.*
%{_sbindir}/ibcacheedit
%{_mandir}/man8/ibcacheedit.*
%{_sbindir}/ibccquery
%{_mandir}/man8/ibccquery.*
%{_sbindir}/ibccconfig
%{_mandir}/man8/ibccconfig.*
%{_sbindir}/dump_fts
%{_mandir}/man8/dump_fts.*
%{_sbindir}/ibmirror

# scripts here
%{_sbindir}/ibhosts
%{_mandir}/man8/ibhosts.*
%{_sbindir}/ibswitches
%{_mandir}/man8/ibswitches.*
%{_sbindir}/ibnodes
%{_mandir}/man8/ibnodes.*
%{_sbindir}/ibrouters
%{_mandir}/man8/ibrouters.*
%{_sbindir}/ibfindnodesusing.pl
%{_mandir}/man8/ibfindnodesusing.*
%{_sbindir}/ibidsverify.pl
%{_mandir}/man8/ibidsverify.*
%{_sbindir}/check_lft_balance.pl
%{_mandir}/man8/check_lft_balance.*
%{_sbindir}/dump_lfts.sh
%{_mandir}/man8/dump_lfts.*
%{_sbindir}/dump_mfts.sh
%{_mandir}/man8/dump_mfts.*
%{_sbindir}/ibclearerrors
%{_mandir}/man8/ibclearerrors.*
%{_sbindir}/ibclearcounters
%{_mandir}/man8/ibclearcounters.*
%{_sbindir}/ibstatus
%{_mandir}/man8/ibstatus.*

# and the rest
%{_mandir}/man8/infiniband-diags.*
%{_libdir}/*.a
%{_libdir}/*.so*
%{_mandir}/man3/*
%{_includedir}/infiniband/*.h
%define _perldir %(perl -e 'use Config; $T=$Config{installvendorlib}; print $T;')
%{_perldir}/*
%{_sysconfdir}/*
%doc README COPYING ChangeLog

%files guest
%defattr(-,root,root)
%{_sbindir}/ibstat
%{_sbindir}/ibstatus
%{_mandir}/man8/ibstat.8*
%{_mandir}/man8/ibstatus.8*

%changelog
* Mon Mar 03 2008 Albert Chu <chu11@llnl.gov> - 1.3.5
- Add check_lft_balance script.

* Wed Oct 31 2007 Ira Weiny <weiny2@llnl.gov> - 1.3.2
- Change switch-map option to node-name-map

* Thu Aug 9 2007 Ira Weiny <weiny2@llnl.gov> - 1.3.1
- Change set_mthca_nodedesc.sh to set_nodedesc.sh

* Tue Jul 10 2007 Hal Rosenstock <halr@voltaire.com> - 1.3.1
- Add link width and speed to topology file output in ibnetdiscover
- Add support for -R(outer_list) in ibnetdiscover
- Add script and man page for ibidsverify
- Moved diags from bin to sbin
- Add scripts and man pages for display on IB routers
- Add GUID to output line for ports in ibqueryerrors.pl
- Add ibdatacounts and ibdatacounters scripts and man pages
- Add peer port link width and speed validation in iblinkinfo.pl
- Display remote LID with peer port info in IBswcountlimits.pm
- Handle peer ports at 1x that should be wider and 2.5 Gbps
  links that should be faster in ibportstate
- Add LinkSpeed/Width components to output of ibportstate
- Add support for IB routers
- Add grouping support for ISR2012 and ISR2004 in ibnetdiscover
- Remove all uses of "/tmp" from perl scripts
- Add switch map support for saquery -O and -U options
- Add support for saquery -s (isSMdisabled)
- Add name input checks to saquery (-O and -U)

* Thu Mar 29 2007 Hal Rosenstock <halr@voltaire.com> - 1.3.0
- Add some extra debug information to IBswcountlimits.pm
- Send normal output to stdout in ibtracert
- Don't truncate NodeDescriptions containing ctl characters in ibdiag_common
- Fix ibnetdiscover grouping for Cisco SFS7000
- Add support to query the GUIDInfo table in smpquery
- Allow user to specify a default switch map file

* Fri Mar 9 2007 Hal Rosenstock <halr@voltaire.com> - 1.2.5
- Find perl modules in perl sitearch directory
- Fix non standard prefix install for diag scripts
- Clean gcc-4.1 warnings in saquery and ibdiag_common

* Fri Mar 2 2007 Hal Rosenstock <halr@voltaire.com> - 1.2.4
- OpenFabrics 1.2.4 release
- Fix diag rpmbuild from make dist
- Include set_mthca_nodedesc.sh and dump_lfts.sh in the rpm

* Thu Mar 1 2007 Hal Rosenstock <halr@voltaire.com> - 1.2.3
- OpenFabrics 1.2.3 release
- Fixed saquery timeout handling

* Tue Feb 27 2007 Hal Rosenstock <halr@voltaire.com> - 1.2.2
- OpenFabrics 1.2.2 release
- Minor changes to ibswitches and ibhosts output

* Thu Feb 14 2007 Hal Rosenstock <halr@voltaire.com> - 1.2.1
- OpenFabrics 1.2.1 release
- Initial release of vendstat tool

* Fri Feb 2 2007 Hal Rosenstock <halr@voltaire.com> - 1.2.0
- OpenFabrics 1.2.0 release
- Added brief option to ibcheckerrors and ibcheckerrs
- Updated man pages
- Added build version to saquery and updated build version tags of other tools
- Added -N | nocolor to usage display of scripts
- Fixed -nocolor and -G options on scripts
- Fixed error return status in ibchecknet
- Added exit code to ibcheckerrors
- Added nodename to output of ibcheckerrs
- ibqueryerrors.pl fixes and improvements
- Removed use of tmpfile for ibroute data in ibfindnodeusing.pl
- Fixed undefined subroutine error in iblinkinfo.pl
- Added switch-map option to ibtracert and ibnetdiscover
- Cleaned up node descriptions before printing in saquery
- Clarified --src-to-dst option in saquery
- Added peer NodeDescription and LID to output of inbetdiscover
- For grouping, ordered Spine and Line Nodes (for Voltaire chassis) in ibnetdiscover
- Cleaned up node descriptions before printing in ibtracert and ibroute
- Added additional sematics to -m option of saquery
- Added dump_mfts.sh similar to dump_lfts.sh
- ibnetdiscover improvements (memory leaks, ports moving, etc.)
- Converted iblinkspeed.pl into iblinkinfo.pl and added additional capabilities
- Added 0x in front of GUID printing of ibtracert
- Fixed loopback handling in ibnetdiscover
- Added support for querying Service Records to saquery
- Added support for PerfMgt IsExtendedWidthSupported IBA 1.2 erratum in perfquery
- For query operations, added peer port checking of linkwidth and speed
  active in ibportstate
- Added support for DrSLID in smpquery
- Added IB router support to ibnetdiscover and ibtracert
- Added additional options to saquery
- Added support to change LinkSpeedEnabled in ibportstate

* Fri Sep 22 2006 Hal Rosenstock <halr@voltaire.com> - 1.1.0
- OpenFabrics 1.1 release

* Wed Sep 13 2006 Hal Rosenstock <halr@voltaire.com> - 1.1.0-rc5
- OpenFabrics 1.1-rc5 release

* Wed Sep 6 2006 Hal Rosenstock <halr@voltaire.com> - 1.1.0-rc4
- OpenFabrics 1.1-rc4 release

* Wed Aug 23 2006 Hal Rosenstock <halr@voltaire.com> - 1.1.0-rc3
- OpenFabrics 1.1-rc3 release

* Mon Aug 14 2006 Hal Rosenstock <halr@voltaire.com> - 1.1.0-rc2
- OpenFabrics 1.1-rc2 release
- Added ibsysstat man page

* Wed Jul 26 2006 Hal Rosenstock <halr@voltaire.com> - 1.1.0-rc1
- OpenFabrics 1.1-rc1 release
- Added man pages
- Made diag command/script options more consistent
- saquery tool added
- dump_lft.sh script added
- Renamed discover.pl to ibdiscover.pl

* Sun Jun 10 2006 Hal Rosenstock <halr@voltaire.com> - 1.0-1
- OpenFabrics 1.0 release

* Tue May 30 2006 Hal Rosenstock <halr@voltaire.com> - 1.0.0-rc6
- Maintenance release

* Fri May 12 2006 Hal Rosenstock <halr@voltaire.com> - 1.0.0-rc5
- Maintenance release

* Thu Apr 27 2006 Hal Rosenstock <halr@voltaire.com> - 1.0.0-rc4
- Maintenance release
- Note rc3 skipped to sync with OFED

* Mon Apr 10 2006 Hal Rosenstock <halr@voltaire.com> - 1.0.0-rc2
- Maintenance release

* Mon Feb 27 2006 Hal Rosenstock <halr@voltaire.com> - 1.0.0-rc1
- Initial spec file and release
