%{!?ibmadlib: %define ibmadlib libibmad-devel}
%{!?name: %define name mstflint}
%{!?version: %define version 3.7.1}
%{!?release: %define release 1}
%{!?ppcbuild: %define ppcbuild 0}
%{!?ppc64build: %define ppc64build 0}

Summary: Mellanox firmware burning application
Name: %{name}
Version: %{version}
Release: 1
License: GPL/BSD
Url: http://openfabrics.org
Group: System Environment/Base
BuildRoot: %{_tmppath}/%{name}-%{version}-%{release}
Source: %{name}-%{version}.tar.gz
ExclusiveArch: i386 i486 i586 i686 x86_64 ia64 ppc ppc64
BuildRequires: zlib-devel %{ibmadlib}

%description
This package contains firmware update tool, vpd dump and register dump tools
for network adapters based on Mellanox Technologies chips.

%prep
%setup -q

%build

%if %{ppcbuild}
    config_flags="$config_flags -host=ppc-linux MST_CPU=ppc MST_EMBEDDED=1 --without-pythontools"
%endif

%if %{ppc64build}
    config_flags="$config_flags -host=ppc64-linux MST_CPU=ppc64 --without-pythontools"
%endif

%configure ${config_flags}

make

%install
rm -rf $RPM_BUILD_ROOT
make DESTDIR=${RPM_BUILD_ROOT} install
# remove unpackaged files from the buildroot
rm -f $RPM_BUILD_ROOT%{_libdir}/*.la

%clean
rm -rf $RPM_BUILD_ROOT

%files
%defattr(-,root,root)
%{_bindir}/mstmread
%{_bindir}/mstmwrite
%{_bindir}/mstflint
%{_bindir}/mstregdump
%{_bindir}/mstmtserver
%{_bindir}/mstvpd
%{_bindir}/mstmcra
%{_bindir}/mstconfig
%{_bindir}/hca_self_test.ofed
%{_includedir}/mtcr_ul/mtcr.h
%{_libdir}/libmtcr_ul.a
%{_datadir}/mstflint
%{_mandir}/man1/*

%changelog
* Mon Oct 12 2014 Oren Kladnitsky <orenk@dev.mellanox.co.il>
   MFT 3.7.1

* Mon Jul 31 2014 Oren Kladnitsky <orenk@dev.mellanox.co.il>
   MFT 3.7.0 Updates

* Mon Mar 31 2014 Oren Kladnitsky <orenk@dev.mellanox.co.il>
   MFT 3.6.0 Updates

* Tue Dec 24 2013 Oren Kladnitsky <orenk@dev.mellanox.co.il>
   MFT 3.5.0 Updates

* Wed Mar 20 2013 Oren Kladnitsky <orenk@dev.mellanox.co.il>
   MFT 3.0.0

* Thu Dec  4 2008 Oren Kladnitsky <orenk@dev.mellanox.co.il>
   Added hca_self_test.ofed installation
   
* Fri Dec 23 2007 Oren Kladnitsky <orenk@dev.mellanox.co.il>
   Added mtcr.h installation
   
* Fri Dec 07 2007 Ira Weiny <weiny2@llnl.gov> 1.0.0
   initial creation

