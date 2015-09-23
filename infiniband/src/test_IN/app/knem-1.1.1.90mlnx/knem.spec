# Copyright © INRIA 2009-2010
# Brice Goglin <Brice.Goglin@inria.fr>
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
# 3. The name of the author may not be used to endorse or promote products
#    derived from this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
# IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
# OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
# IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
# INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
# NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
# THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

# KMP is disabled by default
%{!?KMP: %global KMP 0}

%{!?_release: %global _release OFED.2.3.119.g1cdd893}
%{!?KVERSION: %global KVERSION %(uname -r)}
%global kernel_version %{KVERSION}
%{!?K_SRC: %global K_SRC /lib/modules/%{KVERSION}/build}
# set package name
%if "%{KMP}" == "1"
%{!?_name: %global _name knem-mlnx}
%else
%{!?_name: %global _name knem}
%endif

Summary: KNEM: High-Performance Intra-Node MPI Communication
Name: %{_name}
Version: 1.1.1.90mlnx
Release: %{_release}%{?_dist}
License: BSD
Group: System Environment/Libraries
Packager: Brice Goglin
Source0: knem-%{version}.tar.gz
BuildRoot: /var/tmp/%{name}-%{version}-build

%description
KNEM is a Linux kernel module enabling high-performance intra-node MPI communication for large messages. KNEM offers support for asynchronous and vectorial data transfers as well as loading memory copies on to Intel I/OAT hardware.
See http://runtime.bordeaux.inria.fr/knem/ for details.

%global debug_package %{nil}

# build KMP rpms?
%if "%{KMP}" == "1"
%global kernel_release() $(make -C %{1} kernelrelease | grep -v make)
BuildRequires: %kernel_module_package_buildreqs
%{kernel_module_package}
%else # not KMP
%global kernel_source() %{K_SRC}
%global kernel_release() %{KVERSION}
%global flavors_to_build default
%endif #end if "%{KMP}" == "1"

%description
KNEM is a Linux kernel module enabling high-performance intra-node MPI communication for large messages. KNEM offers support for asynchronous and vectorial data transfers as well as loading memory copies on to Intel I/OAT hardware.
See http://runtime.bordeaux.inria.fr/knem/ for details.

#
# setup module sign scripts if paths to the keys are given
#
%global WITH_MOD_SIGN %(if ( test -f "$MODULE_SIGN_PRIV_KEY" && test -f "$MODULE_SIGN_PUB_KEY" ); \
	then \
		echo -n '1'; \
	else \
		echo -n '0'; fi)

%if "%{WITH_MOD_SIGN}" == "1"
# call module sign script
%global __modsign_install_post \
    $RPM_BUILD_DIR/knem-%{version}/source/tools/sign-modules $RPM_BUILD_ROOT/lib/modules/ || exit 1 \
%{nil}

# Disgusting hack alert! We need to ensure we sign modules *after* all
# invocations of strip occur, which is in __debug_install_post if
# find-debuginfo.sh runs, and __os_install_post if not.
#
%global __spec_install_post \
  %{?__debug_package:%{__debug_install_post}} \
  %{__arch_install_post} \
  %{__os_install_post} \
  %{__modsign_install_post} \
%{nil}

%endif # end of setup module sign scripts
#

%if "%{_vendor}" == "suse"
%global install_mod_dir updates
%endif

%if "%{_vendor}" == "redhat"
%global install_mod_dir extra/%{name}
%global __find_requires %{nil}
%endif

%prep
%setup -n knem-%{version}
set -- *
mkdir source
mv "$@" source/
mkdir obj

%build
rm -rf $RPM_BUILD_ROOT
export INSTALL_MOD_DIR=%install_mod_dir
for flavor in %flavors_to_build; do
	export KSRC=%{kernel_source $flavor}
	export KVERSION=%{kernel_release $KSRC}
	export LIB_MOD_DIR=/lib/modules/$KVERSION/$INSTALL_MOD_DIR
	export MODULE_DESTDIR=/lib/modules/$KVERSION/$INSTALL_MOD_DIR
	rm -rf obj/$flavor
	cp -a source obj/$flavor
	cd $PWD/obj/$flavor
	./configure --prefix=/opt/knem-%{version} --with-linux-release=$KVERSION --with-linux=/lib/modules/$KVERSION/source --with-linux-build=$KSRC --libdir=/opt/knem-%{version}/lib
	make
	cd -
done

%install
export INSTALL_MOD_PATH=$RPM_BUILD_ROOT
export INSTALL_MOD_DIR=%install_mod_dir
export KPNAME=%{name}
mkdir -p $RPM_BUILD_ROOT/etc/udev/rules.d
install -d $RPM_BUILD_ROOT/usr/lib64/pkgconfig
for flavor in %flavors_to_build; do
	cd $PWD/obj/$flavor
	export KSRC=%{kernel_source $flavor}
	export KVERSION=%{kernel_release $KSRC}
	make DESTDIR=$RPM_BUILD_ROOT install KERNELRELEASE=$KVERSION
	export MODULE_DESTDIR=/lib/modules/$KVERSION/$INSTALL_MOD_DIR
	mkdir -p $RPM_BUILD_ROOT/lib/modules/$KVERSION/$INSTALL_MOD_DIR
	MODULE_DESTDIR=/lib/modules/$KVERSION/$INSTALL_MOD_DIR DESTDIR=$RPM_BUILD_ROOT KVERSION=$KVERSION $RPM_BUILD_ROOT/opt/knem-%{version}/sbin/knem_local_install
	cp knem.pc  $RPM_BUILD_ROOT/usr/lib64/pkgconfig
	cd -
done

%if "%{_vendor}" == "redhat"
# Set the module(s) to be executable, so that they will be stripped when packaged.
find %{buildroot} -type f -name \*.ko -exec %{__chmod} u+x \{\} \;
%if "%{KMP}" == "1"
%{__install} -d $RPM_BUILD_ROOT%{_sysconfdir}/depmod.d/
echo "override knem * weak-updates/%{name}" > $RPM_BUILD_ROOT%{_sysconfdir}/depmod.d/knem.conf
%endif
%else
find %{buildroot} -type f -name \*.ko -exec %{__strip} -p --strip-debug --discard-locals -R .comment -R .note \{\} \;
%endif

%clean
rm -rf $RPM_BUILD_ROOT

%post
getent group rdma >/dev/null 2>&1 || groupadd -r rdma
touch /etc/udev/rules.d/10-knem.rules
depmod -a

%files
%defattr(-, root, root)
%if "%{KMP}" != "1"
/lib/modules/%{KVERSION}/
%else
%if "%{_vendor}" == "redhat"
/etc/depmod.d/knem.conf
%endif
%endif
/opt/knem-%{version}
/usr/lib64/pkgconfig/knem.pc

%config(noreplace)
/etc/udev/rules.d/10-knem.rules

%changelog
* Mon Mar 17 2014 Alaa Hleihel <alaa@mellanox.com>
- Use one spec for KMP and non-KMP OS's.
* Thu Apr 18 2013 Alaa Hleihel <alaa@mellanox.com>
- Added KMP support
