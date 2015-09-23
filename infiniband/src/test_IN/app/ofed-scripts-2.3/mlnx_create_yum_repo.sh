#!/bin/bash
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

VERSION="1.0"

LANG="en_US.UTF-8"

repofile="/tmp/mlnx_ofed.repo"
tdir=
mlnx_iso=
kernel=
dflt_kernel=`uname -r`
arch=`uname -m`
build_arch=`rpm --eval '%{_target_cpu}'`
verbose=0
name=
comps=
yes=0

with_hpc=0
hpc_packages="mxm fca mpitests openmpi openshmem bupc mpi-selector mvapich2 libibprof ummunotify ummunotify-mlnx hcoll"

usage()
{
cat << EOF

	Usage: `basename $0` -m|--mlnx_ofed <path to MLNX_OFED directory> -t|--target <path to YUM repository> [options]

	Options:
		[-k|--kernel] <kernel version>       Kernel version to use
		                                        - Default is the current running kernel: $dflt_kernel
		[--comps] <path to comps.xml>        Custom XML file which defines YUM groups for MLNX_OFED YUM Repository
		                                        - Default is to build comps.xml based on available MLNX_OFED packages
		[--with-hpc]                         Include HPC packages in MLNX_OFED YUM Repository
		[-v|--verbose]                       Verbose
		[-y|--yes]                           Answer "yes" to all questions

EOF
}

# COLOR SETTINGS
TT_COLOR="yes"
SETCOLOR_SUCCESS="echo -en \\033[1;34m"
SETCOLOR_FAILURE="echo -en \\033[1;31m"
SETCOLOR_WARNING="echo -en \\033[1;35m"
SETCOLOR_NORMAL="echo -en \\033[0;39m"

LOG=/tmp/mlnx_yum.$$.log

# Print message with "ERROR:" prefix
err_echo()
{
	[ "$TT_COLOR" = "yes" ] && $SETCOLOR_FAILURE
	echo -n "ERROR: $@"
	[ "$TT_COLOR" = "yes" ] && $SETCOLOR_NORMAL
	echo
	return 0
}

# Print message with "WARNING:" prefix
warn_echo()
{
	[ "$TT_COLOR" = "yes" ] && $SETCOLOR_WARNING
	echo -n "WARNING: $@"
	[ "$TT_COLOR" = "yes" ] && $SETCOLOR_NORMAL
	echo
	return 0
}

# Print message (bold)
pass_echo()
{
	[ "$TT_COLOR" = "yes" ] && $SETCOLOR_SUCCESS
	echo -n "$@"
	[ "$TT_COLOR" = "yes" ] && $SETCOLOR_NORMAL
	echo
	return 0
}

trap_handler()
{
        err_echo "Killed by user."
        exit 1

}

# Get user's trap (Ctrl-C or kill signals)
trap 'trap_handler' 2 9 15

ex()
{
	if [ $verbose -eq 1 ]; then
		echo Running $@
	fi
	eval "$@" >> $LOG 2>&1
	if [ $? -ne 0 ]; then
		echo
		err_echo Failed executing \"$@\"
		err_echo See $LOG
		exit 1
	fi
}

############

if [ $UID -ne 0 ]; then
	echo You must be root to run `basename $0`
	exit 1
fi

if [ -z "$1" ]; then
	usage
	exit 1
fi

while [ ! -z "$1" ]
do
	case "$1" in
		-h | --help)
		usage
		shift
		exit 0
		;;
		-m | --mlnx_ofed)
		mlnx_ofed_dir=$2
		shift 2
		;;
		-k | --kernel)
		kernel=$2
		shift 2
		;;
		-t | --target)
		tdir=$2
		shift 2
		;;
		--comps)
		comps=$2
		shift 2
		;;
		-v | --verbose)
		verbose=1
		shift
		;;
		-y | --yes)
		yes=1
		shift
		;;
		--with-hpc)
		with_hpc=1
		shift
		;;
		--version)
		echo "Version: $VERSION"
		exit 0
		;;
		*)
		usage
		shift
		exit 1
		;;
	esac
done

if [ "$(rpm -q createrepo 2>&1 | grep -v "is not installed")" == "" ] ; then
	err_echo "createrepo is not installed!."
	pass_echo "Please install 'createrepo' package before running this tool."
	exit 1
fi

if [ -z "$kernel" ]; then
	kernel=$dflt_kernel
fi

if [ -z "$mlnx_ofed_dir" ]; then
	err_echo "Path to MLNX_OFED directory is not defined."
	usage
	exit 1
fi

if [ ! -z "$comps" -a ! -f "$comps" ]; then
	err_echo "Path to comps file doesn't exist: $comps"
	usage
	exit 1
fi

if [ -z "$tdir" ]; then
	err_echo "Path to YUM Repository target directory is not defined."
	usage
	exit 1
fi

# Set distro
distro_rpm=`rpm -qf /etc/issue 2> /dev/null | head -1`
case $distro_rpm in
        redhat-release-4AS-8)
        distro=rhel4.7
        ;;
        redhat-release-4AS-9)
        distro=rhel4.8
        ;;
        redhat-release*-5.2*|centos-release-5-2*)
        distro=rhel5.2
        ;;
        redhat-release*-5.3*|centos-release-5-3*)
        distro=rhel5.3
        ;;
        redhat-release*-5.4*|centos-release-5-4*)
        distro=rhel5.4
        ;;
        redhat-release*-5.5*|centos-release-5-5*|enterprise-release-5-5*)
        if (grep -q XenServer /etc/issue 2> /dev/null); then
            distro=xenserver
        else
            distro=rhel5.5
        fi
        ;;
        redhat-release*-5.6*|centos-release-5-6*|enterprise-release-5-6*)
        distro=rhel5.6
        ;;
        redhat-release*-5.7*|centos-release-5-7*|enterprise-release-5-7*)
        distro=rhel5.7
        ;;
        redhat-release*-5.8*|centos-release-5-8*|enterprise-release-5-8*)
        distro=rhel5.8
        ;;
        redhat-release*-5.9*|centos-release-5-9*|enterprise-release-5-9*)
        distro=rhel5.9
        ;;
        redhat-release*-6.0*|centos-release-6-0*|sl-release-6.0*)
        distro=rhel6
        ;;
        redhat-release*-6.1*|centos-release-6-1*|sl-release-6.1*)
        distro=rhel6.1
        ;;
        redhat-release*-6.2*|centos-release-6-2*|sl-release-6.2*)
        distro=rhel6.2
        ;;
        redhat-release*-6.3*|centos-release-6-3*|sl-release-6.3*)
        distro=rhel6.3
        ;;
        redhat-release*-6.4*|centos-release-6-4*|sl-release-6.4*)
        distro=rhel6.4
        ;;
        redhat-release*-7.0*|centos-release-7-0*|sl-release-7.0*)
        distro=rhel7.0
        ;;
        redhat-release*-6.5*|centos-release-6-5*|sl-release-6.5*)
        distro=rhel6.5
        ;;
        redhat-release*-6.6*|centos-release-6-6*|sl-release-6.6*)
        distro=rhel6.6
        ;;
        oraclelinux-release-6Server-1*)
        distro=oel6.1
        ;;
        oraclelinux-release-6Server-2*)
        distro=oel6.2
        ;;
        oraclelinux-release-6Server-3*)
        distro=oel6.3
        ;;
        oraclelinux-release-6Server-4*)
        distro=oel6.4
        ;;
        oraclelinux-release-6Server-5*)
        distro=oel6.5
        ;;
        oraclelinux-release-6Server-6*)
        distro=oel6.6
        ;;
        oraclelinux-release-7.0*)
        distro=oel7.0
        ;;
        sles-release-10-15.*)
        distro=sles10sp2
        ;;
        sles-release-10-15.45.*)
        distro=sles10sp3
        ;;
        sles-release-10-15.57.*)
        distro=sles10sp4
        ;;
        sles-release-11-72.*)
        distro=sles11
        ;;
        sles-release-11.1*|*SLES*release-11.1*)
        distro=sles11sp1
        ;;
        sles-release-11.2*|*SLES*release-11.2*)
        distro=sles11sp2
        ;;
        sles-release-11.3*|*SLES*release-11.3*)
        distro=sles11sp3
        ;;
        sles-release-12-1*|*SLES*release-12-1*)
        distro=sles12sp0
        ;;
        fedora-release-14*)
        distro=fc14
        ;;
        fedora-release-16*)
        distro=fc16
        ;;
        fedora-release-17*)
        distro=fc17
        ;;
        fedora-release-18*)
        distro=fc18
        ;;
        fedora-release-19*)
        distro=fc19
        ;;
        fedora-release-20*)
        distro=fc20
        ;;
        fedora-release-21*)
        distro=fc21
        ;;
        *)
        err_echo "Linux Distribution ($distro_rpm) is not supported"
        exit 1
        ;;
esac

# set KMP
kmp=1

case $distro in
	rhel5.2 | oel* | fc* | xenserver*)
	kmp=0
	;;
esac

case $kernel in
	*xs*|*fbk*|*fc*|*debug*|*uek*)
	kmp=0
;;
esac

if [[ "$distro" == "rhel7.0" && ! "$kernel" =~ "3.10.0-123" ]] ||
   [[ "$distro" == "rhel6.5" && ! "$kernel" =~ 2.6.32-4[0-9][0-9] ]] ||
   [[ "$distro" == "rhel6.4" && ! "$kernel" =~ "2.6.32-358" ]] ||
   [[ "$distro" == "rhel6.3" && ! "$kernel" =~ "2.6.32-279" ]] ||
   [[ "$distro" == "rhel6.2" && ! "$kernel" =~ "2.6.32-220" ]] ||
   [[ "$distro" == "rhel6.1" && ! "$kernel" =~ "2.6.32-131" ]]; then
	kmp=0
fi

if [ $verbose -eq 1 ]; then
	pass_echo "KMP=$kmp"
fi

### MAIN ###
pass_echo "Creating MLNX_OFED_LINUX YUM Repository under $tdir..."
echo See log file $LOG
echo

mnt_point=$mlnx_ofed_dir
if [[ ! -f $mnt_point/.mlnx && ! -f $mnt_point/mlnx ]]; then
	err_echo "$mlnx_ofed_dir is not a supported MLNX_OFED_LINUX directory"
	exit 1
fi

if [ -f $mnt_point/.mlnx ]; then
	mlnx_version=`cat $mnt_point/.mlnx`
else
	mlnx_version=`cat $mnt_point/mlnx`
fi

if [ $verbose -eq 1 ]; then
	pass_echo "Detected MLNX_OFED_LINUX-${mlnx_version}"
fi
name=MLNX_OFED_LINUX-${mlnx_version}-${distro}-${arch}

rpm_kernel=${kernel//-/_}
if [ $kmp -eq 0 ]; then
	# Check that required RPMs already exist in the MLNX_OFED
	if ( /bin/ls $mnt_point/RPMS/kernel-ib-*-${rpm_kernel}[_.]*${build_arch}.rpm > /dev/null 2>&1 ) &&
		( /bin/ls $mnt_point/RPMS/kernel-mft-*-${rpm_kernel}.${build_arch}.rpm > /dev/null 2>&1 ) && 
		( /bin/ls $mnt_point/RPMS/ummunotify-*-${rpm_kernel}.${build_arch}.rpm > /dev/null 2>&1 ) && 
		( /bin/ls $mnt_point/RPMS/knem-*-${rpm_kernel}.${build_arch}.rpm > /dev/null 2>&1 ) ; then
		if [ $verbose -eq 1 ]; then
			pass_echo "Required rpms for kernel ($kernel) are already supported by MLNX_OFED_LINUX"
		fi
	else
		err_echo "The $kernel kernel is installed, MLNX_OFED does not have drivers available for this kernel."
		err_echo "You can run mlnx_add_kernel_support.sh in order to to generate an MLNX_OFED package with drivers for this kernel.";
		exit 0
	fi
fi

tdir="$tdir/MLNX_OFED/MLNX_OFED_LINUX-${mlnx_version}-${distro}-${arch}_${kernel}"
if [ -d "$tdir" ]; then
	warn_echo "$tdir already exists!"
	if [ $yes -ne 1 ]; then
		read -p "Do you want to clear $tdir ?[y/N]:" ans
		case $ans in
			y | Y)
			;;
			*)
			exit 0
			;;
		esac
	fi
	warn_echo "Removing $tdir..."
	ex "rm -rf $tdir"
fi
ex "mkdir -p $tdir"
ex "mkdir -p $tdir/RPMS"

# build comps.xml if not given
if [ -z "$comps" ]; then
	pass_echo "comps file was not provided, going to build it..."
	comps=$tdir/comps.xml

	# add header
	/bin/cat << EOF >> $comps
<?xml version="1.0" encoding="UTF-8"?>
<comps>
EOF

	# add groups info
	for group in all hpc basic vma vma-vpi vma-eth guest hypervisor
	do
		if [[ $group =~ hpc ]] && [ $with_hpc -eq 0 ]; then
			continue
		fi
		pkgs=$($mnt_point/mlnxofedinstall --${group} -p -k $kernel --skip-distro-check 2>/dev/null | grep "MLNX_OFED packages:" 2>/dev/null | sed -e 's@MLNX_OFED packages: @@' 2>/dev/null)
		group_id=$group
		group_name=$(echo $group | tr '[:lower:]' '[:upper:]')

		/bin/cat << EOF >> $comps
	<group>
		<id>$group_id</id>
		<name>MLNX_OFED $group_name</name>
		<default>true</default>
		<description>Mellanox OpenFabrics Enterprise Distribution for Linux: MLNX_OFED $group_name packages</description>
		<uservisible>true</uservisible>
		<packagelist>
EOF
		for pname in $pkgs
		do
			if [ $with_hpc -eq 0 ] && ( [ ! "$(echo $hpc_packages | grep $pname)" == "" ] || [[ $pname =~ mpitest* ]] ); then
				continue
			fi
			/bin/cat << EOF >> $comps
			<packagereq type="default">$pname</packagereq>
EOF
		# kmp
		if [ $kmp -eq 1 ]; then
			for kmprpm in $(/bin/ls -1 $mnt_point/RPMS/{kmod*$pname-[0-9].*,$pname-kmp*}.rpm 2>/dev/null)
			do
				kmpname=$(/bin/rpm -qp --queryformat "[%{NAME}]" $kmprpm 2>/dev/null)
				/bin/cat << EOF >> $comps
			<packagereq type="default">$kmpname</packagereq>
EOF
			done
		fi

		done
		/bin/cat << EOF >> $comps
		</packagelist>
	</group>
EOF
		pkgs=
	done

	# close XML
	/bin/cat << EOF >> $comps
</comps>
EOF

fi

pass_echo "Copying RPMS..."
# copy all user space rpms
ex "find $mnt_point/RPMS/ -type f \( -name '*rpm' ! -name '*ummunotify*' ! -name '*knem*' ! -name '*ofa_kernel*' ! -name '*kernel-ib*' ! -name '*kernel-mft*' ! -name '*iser*' ! -name '*srp-*' \) -exec /bin/cp '{}' $tdir/RPMS \;"

# copy kernel rpms
if [ $kmp -eq 0 ]; then
	# copy rpms without KMP support
	ex "/bin/cp $mnt_point/RPMS/-*-${rpm_kernel}[_.]*${build_arch}.rpm $tdir/RPMS"
else
	# copy rpms with KMP support
	ex "find $mnt_point/RPMS/ -type f \( -name \"*mlnx-ofa_kernel-*.rpm\" -o -name \"*kernel-mft-mlnx-*.rpm\" -o -name \"*ummunotify-mlnx-*.rpm\" -o -name \"*knem-mlnx-*.rpm\" -o -name \"kmod-srp-*.rpm\" -o -name \"kmod-iser-*.rpm\" \) -exec /bin/cp '{}' $tdir/RPMS \;"
fi

# Build YUM repo
pass_echo "Building YUM Repository..."
cd $tdir
ex "createrepo -q -g $comps $tdir"
if [ -f "$mnt_point/RPM-GPG-KEY-Mellanox" ]; then
	ex "/bin/cp $mnt_point/RPM-GPG-KEY-Mellanox ./"
else
	ex "wget http://www.mellanox.com/downloads/ofed/RPM-GPG-KEY-Mellanox"
fi

# Build YUM repo settings file
pass_echo "Creating YUM Repository settings file at: $repofile"
/bin/cat << EOF > $repofile
[mlnx_ofed]
name=$name - kernel: $kernel Repository
baseurl=file://$tdir
enabled=1
gpgkey=file://$tdir/RPM-GPG-KEY-Mellanox
gpgcheck=1
EOF

pass_echo "Done."
pass_echo "Copy $repofile to /etc/yum.repos.d/ to use MLNX_OFED YUM Repository."

/bin/rm -f $LOG
