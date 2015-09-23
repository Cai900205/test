#!/bin/bash
# Copyright (c) 2012 Mellanox Technologies. All rights reserved.
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

VERSION="6.1"

LANG="en_US.UTF-8"

usage()
{
cat << EOF

	Usage: `basename $0` -m|--mlnx_ofed <path to MLNX_OFED directory> [--make-iso|--make-tgz]

		[--make-iso]                    			Create MLNX_OFED ISO image.
		[--make-tgz]                    			Create MLNX_OFED tarball. (Default)
		[-t|--tmpdir <local work dir>]
		[--kmp]							Enable KMP format if supported.
		[-k | --kernel]	<kernel version>			Kernel version to use.
		[-s | --kernel-sources] <path to the kernel sources>	Path to kernel headers.
		[-v|--verbose]
		[-n|--name]						Name of the package to be created.
		[-y|--yes]						Answer "yes" to all questions
		[--force]						Force removing packages that depends on MLNX_OFED
		[---without-<package>]			Do not install package-force
		[--distro]						Set Distro name for the running OS (e.g: rhel6.5, sles11sp3). Default: Use auto-detection.

EOF
}

# COLOR SETTINGS
TT_COLOR="yes"
SETCOLOR_SUCCESS="echo -en \\033[1;34m"
SETCOLOR_FAILURE="echo -en \\033[1;31m"
SETCOLOR_WARNING="echo -en \\033[1;35m"
SETCOLOR_NORMAL="echo -en \\033[0;39m"

LOG=/tmp/mlnx_ofed_iso.$$.log

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
		# cleanup
		exit 1
	fi
}

cleanup()
{
	/bin/rm -rf $tmpdir > /dev/null 2>&1
}

# Returns "0" if $1 is integer and "1" otherwise
is_integer() 
{
        printf "%s\n" $1 |grep -E "^[+-]?[0-9]+$" > /dev/null
        return $?
}

# Check disk space requirments $1 - required size $2 - directory to check
check_space_req()
{
        local space_req=$1
        local dir=$2
        
        shift 2
        
        while [ ! -d $dir ]
        do
                dir=${dir%*/*}
        done
        
        local avail_space=`/bin/df $dir | tail -1 | awk '{print$4}' | tr -d '[:space:]'`
        
        if ! is_integer $avail_space; then
                # Wrong avail_space found
                return 0
        fi
        
        if [ $avail_space -lt $space_req ]; then
                echo
                err_echo "Not enough disk space in the ${dir} directory. Required ${space_req}KB"
                echo
                exit 1
        fi

        return 0
}

if [ $UID -ne 0 ]; then
	echo You must be root to run `basename $0`
	exit 1
fi

if [ -z "$1" ]; then
	usage
	exit 1
fi

mlnx_iso=
kernel=
kernel_sources=
dflt_kernel=`uname -r`
dflt_kernel_sources="/lib/modules/${dflt_kernel}/build/"
arch=`uname -m`
build_arch=`rpm --eval '%{_target_cpu}'`
verbose=0
make_iso=0
make_tgz=1
mofed_type="TGZ"
name=
yes=0
force=
disabled_pkgs=
distro=
# pass distro to install.pl only if it was provided by the user
distro1=

TMP=${TMP:-"/tmp"}
mlnx_tmp=mlnx_iso.$$
tmpdir=${TMP}/$mlnx_tmp

KMP="--disable-kmp"

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
		-s | --kernel-sources)
		kernel_sources=$2
		shift 2
		;;
		-t | --tmpdir)
		tmpdir=$2/$mlnx_tmp
		shift 2
		;;
		--kmp)
		KMP=
		shift
		;;
		-v | --verbose)
		verbose=1
		shift
		;;
		--make-iso)
		make_iso=1
		make_tgz=0
		mofed_type="ISO"
		shift
		;;
		--make-tgz)
		make_tgz=1
		make_iso=0
		mofed_type="TGZ"
		shift
		;;
		-n | --name)
		name=$2
		shift 2
		;;
		-y | --yes)
		yes=1
		shift
		;;
		--force)
		force="--force"
		shift
		;;
		--version)
		echo "Version: $VERSION"
		exit 0
		;;
		--without-*)
		disabled_pkgs="$disabled_pkgs $1"
		shift
		;;
		--distro)
		distro=$2
		distro1="--distro $distro"
		shift 2
		;;
		*)
		usage
		shift
		exit 1
		;;
	esac
done

if [ -z "$kernel_sources" ]; then
	if [ -z "$kernel" ]; then
		kernel=$dflt_kernel
		kernel_sources=$dflt_kernel_sources
	else
		kernel_sources="/lib/modules/${kernel}/build"
	fi
fi

if [ $make_iso -eq 1 ]; then
    if ! ( which mkisofs > /dev/null 2>&1 ); then
        err_echo "mkisofs command not found"
        exit 1
    fi
fi

if [ -z "$kernel" ]; then
	kernel=$dflt_kernel
fi

if [ -z "$mlnx_ofed_dir" ]; then
	err_echo "Path to MLNX_OFED directory is not defined."
	usage
	exit 1
fi

if [ ! -d $kernel_sources ]; then
	err_echo "Kernel sources directory ($kernel_sources) not found"
	exit 1
fi

# About 600MB is required
check_space_req 614400 `dirname $tmpdir`

mkdir -p $tmpdir

# Set distro
distro_rpm=`rpm -qf /etc/issue 2> /dev/null | head -1`
if [ "X$distro" == "X" ]; then
	if [ $verbose -eq 1 ]; then
		echo "Distro was not provided, trying to auto-detect the current distro..."
	fi
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
		redhat-release*-6.5*|centos-release-6-5*|sl-release-6.5*)
		distro=rhel6.5
		;;
		redhat-release*-6.6*|centos-release-6-6*|sl-release-6.6*)
		distro=rhel6.6
		;;
		redhat-release*-7.0*|centos-release-7-0*|sl-release-7.0*)
		distro=rhel7.0
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
		oraclelinux-release-7-0*)
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
		openSUSE-release-12.1*)
		distro=opensuse12sp1
		;;
		openSUSE-release-12.2*)
		distro=opensuse12sp2
		;;
		openSUSE-release-12.3*)
		distro=opensuse12sp3
		;;
		openSUSE-release-13.1*)
		distro=opensuse13sp1
		;;
		*)
		err_echo "Linux Distribution ($distro_rpm) is not supported"
		exit 1
		;;
	esac
	if [ $verbose -eq 1 ]; then
		echo "Auto-detected $distro distro."
	fi
else
	if [ $verbose -eq 1 ]; then
		echo "Using provided distro: $distro"
	fi
fi

### MAIN ###
pass_echo "Note: This program will create MLNX_OFED_LINUX ${mofed_type} for ${distro} under $TMP directory."
pass_echo "      All Mellanox, OEM, OFED, or Distribution IB packages will be removed."

if [ $yes -ne 1 ]; then
read -p "Do you want to continue?[y/N]:" ans
case $ans in
	y | Y)
	;;
	*)
	exit 0
	;;
esac
fi

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

MLNX_OFED_DISTRO=`cat $mnt_point/distro 2>/dev/null`
if ! [ "X$MLNX_OFED_DISTRO" == "X$distro" ]; then
	echo "WARNING: The current MLNX_OFED_LINUX is intended for $MLNX_OFED_DISTRO !"
	echo "You may need to use the '--skip-distro-check' flag to install the resulting MLNX_OFED_LINUX on this system."
	echo
fi

rpm_kernel=${kernel//-/_}
# Check that required RPMs not already exist in the MLNX_OFED
if ( /bin/ls $mnt_point/RPMS/kernel-ib-*-${rpm_kernel}[_.]*${build_arch}.rpm > /dev/null 2>&1 ) &&
	( /bin/ls $mnt_point/RPMS/kernel-mft-*-${rpm_kernel}.${build_arch}.rpm > /dev/null 2>&1 ) &&
	( /bin/ls $mnt_point/RPMS/srp-*-${rpm_kernel}.${build_arch}.rpm > /dev/null 2>&1 ) &&
	( /bin/ls $mnt_point/RPMS/iser-*-${rpm_kernel}.${build_arch}.rpm > /dev/null 2>&1 ) &&
	( /bin/ls $mnt_point/RPMS/knem-*-${rpm_kernel}.${build_arch}.rpm > /dev/null 2>&1 ) ; then
	pass_echo "Required kernel ($kernel) is already supported by MLNX_OFED_LINUX"
	cleanup
	exit 0
fi

iso_dir=MLNX_OFED_LINUX-${mlnx_version}-${distro}-${arch}-ext
if [ -n "$name" ]; then
	iso_dir=$name
fi
iso_name=${iso_dir}.iso

ex cp -a $mnt_point $tmpdir/$iso_dir

/bin/rm -f $tmpdir/$iso_name

# Check presence of OFED tgz file
ofed_tgz=`ls $tmpdir/$iso_dir/src/*OFED*tgz 2> /dev/null`
if [ -z "$ofed_tgz" ]; then
	err_echo "OFED tgz package not found under ${iso_dir}/src directory"
	exit 1
fi
ofed=`basename $ofed_tgz`
ofed=${ofed/.tgz/}

nes="nes=y"
qib="qib=y"
iser="iser=y"
cxgb3="cxgb3=y"
cxgb4="cxgb4=y"
mlx5="mlx5=y"
nfsrdma="nfsrdma=y"

cd $tmpdir
# Build missing OFED kernel dependent RPMs
ex tar xzf $ofed_tgz
cat << EOF > ofed.conf
core=y
mthca=y
mlx4=y
$mlx5
mlx4_en=y
mlx4_fc=n
mlx4_vnic=y
$cxgb3
$cxgb4
$nes
$qib
$iser
ipoib=y
e_ipoib=y
sdp=n
srp=y
srpt=n
rds=y
$nfsrdma
kernel-ib=y
kernel-ib-devel=y
kernel-mft=y
knem=y
ofed-scripts=y
EOF

pass_echo "Building OFED RPMs. Please wait..."
ex ${ofed}/install.pl -c ${tmpdir}/ofed.conf --kernel $kernel --kernel-sources $kernel_sources --builddir $tmpdir $KMP $force $disabled_pkgs --build-only $distro1

rpms=`find ${ofed}/RPMS -name "*kernel*" -o -name "knem*" -o -name "srp*" -o -name "iser*"`

for p in $rpms
do
	if ( echo $p | grep debuginfo > /dev/null 2>&1 ); then
		continue
	fi
	ex install -m 0644 $p $tmpdir/$iso_dir/RPMS/
done

echo "$kernel" >> $tmpdir/$iso_dir/.added_kernels
echo "$kernel" >> $tmpdir/$iso_dir/.supported_kernels
echo "$kernel" >> $tmpdir/$iso_dir/added_kernels
echo "$kernel" >> $tmpdir/$iso_dir/supported_kernels

if [ "X$distro1" != "X" ]; then
	echo "skip-distro-check" > $tmpdir/$iso_dir/distro
fi

if [ $make_iso -eq 1 ]; then
    # Create new ISO image
    pass_echo "Running mkisofs..."
    mkisofs -A "MLNX_OFED_LINUX-${mlnx_version} Host Software $distro CD" \
                    -o ${TMP}/$iso_name \
                    -J -joliet-long -r $tmpdir/$iso_dir >> $LOG 2>&1

    if [ $? -ne 0 ] || [ ! -e ${TMP}/$iso_name ]; then
    	err_echo "Failed to create ${TMP}/$iso_name"
    	exit 1
    fi

    pass_echo "Created ${TMP}/$iso_name"
else
    cd $tmpdir
    ex tar czf ${TMP}/${iso_dir}.tgz $iso_dir
    pass_echo "Created ${TMP}/${iso_dir}.tgz"
fi

cleanup
/bin/rm -f $LOG
