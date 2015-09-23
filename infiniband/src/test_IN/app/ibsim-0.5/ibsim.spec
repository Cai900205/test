
%define RELEASE  0.6.gc67be6e
%define rel %{?CUSTOM_RELEASE}%{!?CUSTOM_RELEASE:%RELEASE}

Summary: InfiniBand fabric simulator for management
Name: ibsim
Version: 0.5
Release: 0.6.gc67be6e
License: GPLv2 or BSD
Group: System Environment/Libraries
BuildRoot: %{_tmppath}/%{name}-%{version}-%{release}-root-%(%{__id_u} -n)
Source: http://www.openfabrics.org/downloads/ibsim-0.5-0.6.gc67be6e.tar.gz
Url: http://openfabrics.org/
BuildRequires: libibmad-devel, libibumad-devel

%description
ibsim provides simulation of infiniband fabric for using with OFA OpenSM, diagnostic and management tools.

%prep
%setup -q

%build
export CFLAGS="${CFLAGS:-${RPM_OPT_FLAGS}}"
export LDFLAGS="${LDFLAGS:-${RPM_OPT_FLAGS}}"
make prefix=%_prefix libpath=%_libdir binpath=%_bindir %{?_smp_mflags}

%install
export CFLAGS="${CFLAGS:-${RPM_OPT_FLAGS}}"
export LDFLAGS="${LDFLAGS:-${RPM_OPT_FLAGS}}"
make DESTDIR=${RPM_BUILD_ROOT} prefix=%_prefix libpath=%_libdir binpath=%_bindir install

%clean
rm -rf $RPM_BUILD_ROOT

%files
%defattr(-,root,root)
%{_libdir}/umad2sim/libumad2sim*.so*
%{_bindir}/ibsim
%doc README COPYING TODO net-examples scripts

%changelog
