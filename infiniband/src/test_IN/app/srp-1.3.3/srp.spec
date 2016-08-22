#
# Copyright (c) 2014 Mellanox Technologies. All rights reserved.
#
# This Software is licensed under one of the following licenses:
#
# 1) under the terms of the "Common Public License 1.0" a copy of which is
#    available from the Open Source Initiative, see
#    http://www.opensource.org/licenses/cpl.php.
#
# 2) under the terms of the "The BSD License" a copy of which is
#    available from the Open Source Initiative, see
#    http://www.opensource.org/licenses/bsd-license.php.
#
# 3) under the terms of the "GNU General Public License (GPL) Version 2" a
#    copy of which is available from the Open Source Initiative, see
#    http://www.opensource.org/licenses/gpl-license.php.
#
# Licensee has the right to choose one of the above licenses.
#
# Redistributions of source code must retain the above copyright
# notice and one of the license notices.
#
# Redistributions in binary form must reproduce both the above copyright
# notice, one of the license notices in the documentation
# and/or other materials provided with the distribution.
#
#

%{!?_name: %define _name srp}
%{!?_version: %define _version 1.3.3}
%{!?_release: %define _release OFED.2.3.135.g7e4238c}

# KMP is disabled by default
%{!?KMP: %global KMP 0}

# take kernel version or default to uname -r
%{!?KVERSION: %global KVERSION %(uname -r)}
%global kernel_version %{KVERSION}
%global krelver %(echo -n %{KVERSION} | sed -e 's/-/_/g')
# take path to kernel sources if provided, otherwise look in default location (for non KMP rpms).
%{!?K_SRC: %global K_SRC /lib/modules/%{KVERSION}/build}

# define release version
%{!?src_release: %global src_release %{_release}_%{krelver}}
%if "%{KMP}" != "1"
%global _release1 %{src_release}
%else
%global _release1 %{_release}
%endif

Summary: %{_name} Driver
Name: %{_name}
Version: %{_version}
Release: %{_release1}%{?_dist}
License: GPL/BSD
Url: http://www.mellanox.com
Group: System Environment/Base
Source: %{_name}-%{_version}.tgz
BuildRoot: %{?build_root:%{build_root}}%{!?build_root:/var/tmp/OFED}
Vendor: Mellanox Technologies
%description
%{name} kernel modules

# build KMP rpms?
%if "%{KMP}" == "1"
%global kernel_release() $(make -C %{1} kernelrelease | grep -v make)
BuildRequires: %kernel_module_package_buildreqs
%(mkdir -p %{buildroot})
%(echo '%defattr (-,root,root)' > %{buildroot}/file_list)
%(echo '/lib/modules/%2-%1' >> %{buildroot}/file_list)
%(echo '%{_sysconfdir}/depmod.d/%{name}-%1.conf' >> %{buildroot}/file_list)
%{kernel_module_package -f %{buildroot}/file_list}
%else
%global kernel_source() %{K_SRC}
%global kernel_release() %{KVERSION}
%global flavors_to_build default
%endif

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
    $RPM_BUILD_DIR/%{name}-%{version}/source/tools/sign-modules $RPM_BUILD_ROOT/lib/modules/ || exit 1 \
%{nil}

%global __debug_package 1
%global buildsubdir %{name}-%{version}
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
%debug_package
%endif

# set modules dir
%if "%{_vendor}" == "suse"
%define install_mod_dir updates/%{name}
%else
%if 0%{?fedora}
%define install_mod_dir updates/%{name}
%else
%define install_mod_dir extra/%{name}
%endif
%endif


%prep
%setup
set -- *
mkdir source
mv "$@" source/
mkdir obj

%build
export EXTRA_CFLAGS='-DVERSION=\"%version\"'
export INSTALL_MOD_DIR=%{install_mod_dir}
export CONF_OPTIONS="%{configure_options}"
for flavor in %{flavors_to_build}; do
	export K_SRC=%{kernel_source $flavor}
	export KVER=%{kernel_release $K_SRC}
	export LIB_MOD_DIR=/lib/modules/$KVER/$INSTALL_MOD_DIR
	rm -rf obj/$flavor
	cp -r source obj/$flavor
	cd $PWD/obj/$flavor
	make
	cd -
done

%install
export INSTALL_MOD_PATH=$RPM_BUILD_ROOT
export INSTALL_MOD_DIR=%install_mod_dir
export PREFIX=%{_prefix}
for flavor in %flavors_to_build; do
	export K_SRC=%{kernel_source $flavor}
	export KVER=%{kernel_release $K_SRC}
	cd $PWD/obj/$flavor
	make install
	# Cleanup unnecessary kernel-generated module dependency files.
	find $INSTALL_MOD_PATH/lib/modules -iname 'modules.*' -exec rm {} \;
	cd -
done

# Set the module(s) to be executable, so that they will be stripped when packaged.
find %{buildroot} -type f -name \*.ko -exec %{__chmod} u+x \{\} \;

%{__install} -d $RPM_BUILD_ROOT%{_sysconfdir}/depmod.d/
for module in `find $RPM_BUILD_ROOT/ -name '*.ko'`
do
ko_name=${module##*/}
mod_name=${ko_name/.ko/}
mod_path=${module/*\/%{name}}
mod_path=${mod_path/\/${ko_name}}
%if "%{_vendor}" == "suse"
    %if "%{KMP}" == "1"
        for flavor in %{flavors_to_build}; do
            if [[ $module =~ $flavor ]];then
                echo "override ${mod_name} * updates/%{name}${mod_path}" >> $RPM_BUILD_ROOT%{_sysconfdir}/depmod.d/%{name}-$flavor.conf
            fi
        done
    %else
        echo "override ${mod_name} * updates/%{name}${mod_path}" >> $RPM_BUILD_ROOT%{_sysconfdir}/depmod.d/%{name}.conf
    %endif
%else
    %if 0%{?fedora}
        echo "override ${mod_name} * updates/%{name}${mod_path}" >> $RPM_BUILD_ROOT%{_sysconfdir}/depmod.d/%{name}.conf
    %else
        echo "override ${mod_name} * extra/%{name}${mod_path}" >> $RPM_BUILD_ROOT%{_sysconfdir}/depmod.d/%{name}.conf
        %if "%{KMP}" == "1"
            echo "override ${mod_name} * weak-updates/%{name}${mod_path}" >> $RPM_BUILD_ROOT%{_sysconfdir}/depmod.d/%{name}.conf
        %endif
    %endif
%endif
done


%clean
rm -rf %{buildroot}

%post
if [ $1 -ge 1 ]; then # 1 : This package is being installed or reinstalled
/sbin/depmod %{KVERSION}
fi # 1 : closed
# add SRP_LOAD=no to  openib.conf
if [ -f "/etc/infiniband/openib.conf" ] && ! (grep -q SRP_LOAD /etc/infiniband/openib.conf > /dev/null 2>&1) ; then
    echo "# Load SRP module" >> /etc/infiniband/openib.conf
    echo "SRP_LOAD=no" >> /etc/infiniband/openib.conf
fi
# END of post

%postun
/sbin/depmod %{KVERSION}

%if "%{KMP}" != "1"
%files
%defattr(-,root,root,-)
/lib/modules/%{KVERSION}/
%{_sysconfdir}/depmod.d/%{name}.conf
%endif

%changelog
* Thu Feb 20 2014 Alaa Hleihel <alaa@mellanox.com>
- Initial packaging