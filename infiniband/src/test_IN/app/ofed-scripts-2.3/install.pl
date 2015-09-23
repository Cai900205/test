#!/usr/bin/perl
#
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


use strict;
use File::Basename;
use File::Path;
use File::Find;
use File::Copy;
use Cwd;
use Term::ANSIColor qw(:constants);
use sigtrap 'handler', \&sig_handler, 'normal-signals';
my $PREREQUISIT = "172";
my $MST_START_FAIL = "173";
my $NO_HARDWARE = "171";
my $SUCCESS = "0";
my $DEVICE_INI_MISSING = "2";
my $ERROR = "1";
my $EINVAL = "22";
my $ENOSPC = "28";
my $NONOFEDRPMS = "174";
my $enable_mlnx_tune = 0;

$ENV{"LANG"} = "en_US.UTF-8";

if ($<) {
    print RED "Only root can run $0", RESET "\n";
    exit $PREREQUISIT;
}

$| = 1;
my $LOCK_EXCLUSIVE = 2;
my $UNLOCK         = 8;
#Setup some defaults
my $KEY_ESC=27;
my $KEY_CNTL_C=3;
my $KEY_ENTER=13;

my $BASIC = 1;
my $HPC = 2;
my $ALL = 3;
my $CUSTOM = 4;
my $VMA = 5;
my $VMAVPI = 6;
my $VMAETH = 7;
my $GUESTOS = 8;
my $HYPERVISOROS = 9;

my $interactive = 1;
my $quiet = 0;
my $verbose = 1;
my $verbose2 = 0;
my $verbose3 = 0;

my $print_available = 0;

my $clear_string = `clear`;
my $bonding_force_all_os = 0;

my $vendor_pre_install = "";
my $vendor_post_install = "";
my $vendor_pre_uninstall = "";
my $vendor_post_uninstall = "";

my $DISTRO = "";
my $rpmbuild_flags = "";
my $rpminstall_flags = "";
my $rpminstall_parameter = "-i";

my $WDIR    = dirname($0);
chdir $WDIR;
my $CWD     = getcwd;
my $TMPDIR  = '/tmp';
my $netdir;

my $conf_dir = $CWD;
my $config = $conf_dir . '/ofed.conf';
chomp $config;
my $config_net;

my $builddir = "/var/tmp/";
chomp $builddir;

my $PACKAGE     = 'OFED';
my $ofedlogs = "/tmp/$PACKAGE.$$.logs";
mkpath([$ofedlogs]);

my $default_prefix = '/usr';
chomp $default_prefix;
my $prefix = $default_prefix;

my $build32 = 0;
my $arch = `uname -m`;
chomp $arch;
my $kernel = `uname -r`;
chomp $kernel;
my $kernel_sources = "/lib/modules/$kernel/build";
chomp $kernel_sources;
my $ib_udev_rules = "/etc/udev/rules.d/90-ib.rules";

# Define RPMs environment
my $dist_rpm;
my $dist_rpm_ver = 0;
my $dist_rpm_rel = 0;

my $umad_dev_rw = 0;
my $umad_dev_na = 0;
my $config_given = 0;
my $config_net_given = 0;
my $kernel_given = 0;
my $kernel_source_given = 0;
my $install_option;
my $check_linux_deps = 1;
my $force = 0;
my $update = 0;
my $build_only = 0;
my $uninstall = 1;
my $kmp = 1;
my %disabled_packages;
my %force_enable_packages;
my %packages_deps = ();
my %modules_deps = ();
my $knem_prefix = '';
my $mxm_prefix = '/opt/mellanox/mxm';
my $fca_prefix = '/opt/mellanox/fca';
my $hcoll_prefix = '/opt/mellanox/hcoll';
my $openshmem_prefix = '/opt/mellanox/openshmem';
my $with_memtrack = 0;
my $with_vma = 0;
my $with_fabric_collector = 0;
my $package_manager = "";
my $with_valgrind = 0;
my $disable_valgrind = 0;

while ( $#ARGV >= 0 ) {

   my $cmd_flag = shift(@ARGV);

    if ( $cmd_flag eq "-c" or $cmd_flag eq "--config" ) {
        $config = shift(@ARGV);
        $interactive = 0;
        $config_given = 1;
    } elsif ( $cmd_flag eq "-n" or $cmd_flag eq "--net" ) {
        $config_net = shift(@ARGV);
        $config_net_given = 1;
    } elsif ( $cmd_flag eq "-l" or $cmd_flag eq "--prefix" ) {
        $prefix = shift(@ARGV);
        $prefix =~ s/\/$//;
    } elsif ( $cmd_flag eq "-k" or $cmd_flag eq "--kernel" ) {
        $kernel = shift(@ARGV);
        $kernel_given = 1;
    } elsif ( $cmd_flag eq "-s" or $cmd_flag eq "--kernel-sources" ) {
        $kernel_sources = shift(@ARGV);
        $kernel_source_given = 1;
    } elsif ( $cmd_flag eq "-U" or $cmd_flag eq "--update" ) {
        $update = 1;
    } elsif ( $cmd_flag eq "-b" or $cmd_flag eq "--build-only" ) {
        $build_only = 1;
    } elsif ( $cmd_flag eq "-p" or $cmd_flag eq "--print-available" ) {
        $print_available = 1;
    } elsif ( $cmd_flag eq "--force" ) {
        $force = 1;
    } elsif ( $cmd_flag eq "--disable-kmp" ) {
        $kmp = 0;
    } elsif ( $cmd_flag eq "--all" ) {
        $interactive = 0;
        $install_option = 'all';
    } elsif ( $cmd_flag eq "--hpc" ) {
        $interactive = 0;
        $install_option = 'hpc';
    } elsif ( $cmd_flag eq "--with-vma" ) {
        $with_vma = 1;
    } elsif ( $cmd_flag eq "--vma" ) {
        $interactive = 0;
        $install_option = 'vma';
        $with_vma = 1;
    } elsif ( $cmd_flag eq "--vma-eth" ) {
        $interactive = 0;
        $install_option = 'vmaeth';
        $with_vma = 1;
    } elsif ( $cmd_flag eq "--vma-vpi" ) {
        $interactive = 0;
        $install_option = 'vmavpi';
        $with_vma = 1;
    } elsif ( $cmd_flag eq "--basic" ) {
        $interactive = 0;
        $install_option = 'basic';
    } elsif ( $cmd_flag eq "--guest" ) {
        $interactive = 0;
        $install_option = 'guest-os';
    } elsif ( $cmd_flag eq "--hypervisor" ) {
        $interactive = 0;
        $install_option = 'hypervisor-os';
    } elsif ( $cmd_flag eq "--umad-dev-rw" ) {
        $umad_dev_rw = 1;
    } elsif ( $cmd_flag eq "--umad-dev-na" ) {
        $umad_dev_na = 1;
    } elsif ( $cmd_flag eq "--build32" ) {
        if (supported32bit()) {
            $build32 = 1;
        }
    } elsif ( $cmd_flag eq "--without-depcheck" ) {
        $check_linux_deps = 0;
    } elsif ( $cmd_flag eq "--with-memtrack" ) {
        $with_memtrack = 1;
    } elsif ( $cmd_flag eq "--builddir" ) {
        $builddir = shift(@ARGV);
    } elsif ( $cmd_flag eq "-q" ) {
        $quiet = 1;
    } elsif ( $cmd_flag eq "-v" ) {
        $verbose = 1;
    } elsif ( $cmd_flag eq "-vv" ) {
        $verbose = 1;
        $verbose2 = 1;
    } elsif ( $cmd_flag eq "-vvv" ) {
        $verbose = 1;
        $verbose2 = 1;
        $verbose3 = 1;
    } elsif ($cmd_flag eq "--conf-dir") {
	$conf_dir = shift(@ARGV);
	mkpath([$conf_dir]) unless -d "$conf_dir";
	if (not $config_given) {
		$config = $conf_dir . '/ofed.conf';
	}
    } elsif ($cmd_flag eq "--with-fabric-collector") {
        $with_fabric_collector = 1;
    } elsif ( $cmd_flag eq "--with-valgrind" ) {
        $with_valgrind = 1;
        $disable_valgrind = 0;
    } elsif ( $cmd_flag eq "--without-valgrind" ) {
        $with_valgrind = 0;
        $disable_valgrind = 1;
    } elsif ( $cmd_flag eq "--enable-mlnx_tune" ) {
        $enable_mlnx_tune = 1;
    } elsif ( $cmd_flag =~ /--without|--disable/ ) {
        my $pckg = $cmd_flag;
        $pckg =~ s/--without-|--disable-//;
        $disabled_packages{$pckg} = 1;
    } elsif ( $cmd_flag =~ /--with-|--enable-/ ) {
        my $pckg = $cmd_flag;
        $pckg =~ s/--with-|--enable-//;
        $force_enable_packages{$pckg} = 1;
    } elsif ( $cmd_flag eq "--distro" ) {
        $DISTRO = shift(@ARGV);
    } else {
        &usage();
        exit 1;
    }
}

if ($update and $build32) {
    print RED "Update is not supported for 32-bit libraries.", RESET "\n";
    exit $ENOSPC if (not $force);
    print YELLOW ON_BLUE "Disabling update. The previous version will be removed...", RESET "\n";
    $update = 0;
}

$ENV{"OFA_DIR"} = "$prefix/src/ofa_kernel";

$uninstall = 0 if ($update or $build_only);
$rpminstall_parameter = "-UF" if ($update);

if (-f "/etc/issue") {
    $dist_rpm = `rpm -qf /etc/issue 2> /dev/null | grep -v "is not owned by any package" | head -1`;
    chomp $dist_rpm;
    if ($dist_rpm) {
        $dist_rpm = `rpm -q --queryformat "[%{NAME}]-[%{VERSION}]-[%{RELEASE}]" $dist_rpm`;
        chomp $dist_rpm;
        $dist_rpm_ver = get_rpm_ver_inst($dist_rpm);
        $dist_rpm_rel = get_rpm_rel_inst($dist_rpm);
    } else {
        $dist_rpm = "unsupported";
    }
}
else {
    $dist_rpm = "unsupported";
}
chomp $dist_rpm;

my $rpm_distro = '';

# don't auto-detect distro if it's provided by the user.
if ($DISTRO eq "") {
    print "Distro was not provided, trying to auto-detect the current distro...\n" if ($verbose2);

    if ($dist_rpm =~ /openSUSE-release-11.2/) {
        $DISTRO = "openSUSE11.2";
        $rpm_distro = "opensuse11sp2";
    } elsif ($dist_rpm =~ /openSUSE-release-12.1/) {
        $DISTRO = "openSUSE12.1";
        $rpm_distro = "opensuse12sp1";
    } elsif ($dist_rpm =~ /openSUSE/) {
        $DISTRO = "openSUSE";
        $rpm_distro = "opensuse11sp0";
    } elsif ($dist_rpm =~ /sles-release-12|SLES.*release-12/) {
        $DISTRO = "SLES12";
        $rpm_distro = "sles12sp0";
    } elsif ($dist_rpm =~ /sles-release-11.3|SLES.*release-11.3/) {
        $DISTRO = "SLES11";
        $rpm_distro = "sles11sp3";
    } elsif ($dist_rpm =~ /sles-release-11.2|SLES.*release-11.2/) {
        $DISTRO = "SLES11";
        $rpm_distro = "sles11sp2";
    } elsif ($dist_rpm =~ /sles-release-11.1|SLES.*release-11.1/) {
        $DISTRO = "SLES11";
        $rpm_distro = "sles11sp1";
    } elsif ($dist_rpm =~ /sles-release-11/) {
        $DISTRO = "SLES11";
        $rpm_distro = "sles11sp0";
    } elsif ($dist_rpm =~ /sles-release-10-15.45.8/) {
        $DISTRO = "SLES10";
        $rpm_distro = "sles10sp3";
    } elsif ($dist_rpm =~ /sles-release-10-15.57.1/) {
        $DISTRO = "SLES10";
        $rpm_distro = "sles10sp4";
    } elsif ($dist_rpm =~ /redhat-release-.*-6.0|centos-release-6-0/) {
        $DISTRO = "RHEL6.0";
        $rpm_distro = "rhel6u0";
    } elsif ($dist_rpm =~ /redhat-release-.*-6.1|sl-release-6.1|centos-release-6-1/) {
        $DISTRO = "RHEL6.1";
        $rpm_distro = "rhel6u1";
    } elsif ($dist_rpm =~ /redhat-release-.*-6.2|sl-release-6.2|centos-release-6-2/) {
        $DISTRO = "RHEL6.2";
        $rpm_distro = "rhel6u2";
    } elsif ($dist_rpm =~ /redhat-release-.*-6.3|sl-release-6.3|centos-release-6-3/) {
        $DISTRO = "RHEL6.3";
        $rpm_distro = "rhel6u3";
    } elsif ($dist_rpm =~ /redhat-release-.*-6.4|sl-release-6.4|centos-release-6-4/) {
        $DISTRO = "RHEL6.4";
        $rpm_distro = "rhel6u4";
    } elsif ($dist_rpm =~ /redhat-release-.*-6.5|sl-release-6.5|centos-release-6-5/) {
        $DISTRO = "RHEL6.5";
        $rpm_distro = "rhel6u5";
    } elsif ($dist_rpm =~ /redhat-release-.*-6.6|sl-release-6.6|centos-release-6-6/) {
        $DISTRO = "RHEL6.6";
        $rpm_distro = "rhel6u6";
    } elsif ($dist_rpm =~ /redhat-release-.*-7.0|sl-release-7.0|centos-release-7-0/) {
        $DISTRO = "RHEL7.0";
        $rpm_distro = "rhel7u0";
    } elsif ($dist_rpm =~ /oraclelinux-release-6.*-1/) {
        $DISTRO = "OEL6.1";
        $rpm_distro = "oel6u1";
    } elsif ($dist_rpm =~ /oraclelinux-release-6.*-2/) {
        $DISTRO = "OEL6.2";
        $rpm_distro = "oel6u2";
    } elsif ($dist_rpm =~ /oraclelinux-release-6.*-3/) {
        $DISTRO = "OEL6.3";
        $rpm_distro = "oel6u3";
    } elsif ($dist_rpm =~ /oraclelinux-release-6.*-4/) {
        $DISTRO = "OEL6.4";
        $rpm_distro = "oel6u4";
    } elsif ($dist_rpm =~ /oraclelinux-release-6.*-5/) {
        $DISTRO = "OEL6.5";
        $rpm_distro = "oel6u5";
    } elsif ($dist_rpm =~ /oraclelinux-release-6.*-6/) {
        $DISTRO = "OEL6.6";
        $rpm_distro = "oel6u6";
    } elsif ($dist_rpm =~ /oraclelinux-release-7.0/) {
        $DISTRO = "OEL7.0";
        $rpm_distro = "oel7u0";
    } elsif ($dist_rpm =~ /redhat-release-.*-5.8|centos-release-5-8|enterprise-release-5-8/) {
        $DISTRO = "RHEL5.8";
        $rpm_distro = "rhel5u8";
    } elsif ($dist_rpm =~ /redhat-release-.*-5.7|centos-release-5-7|enterprise-release-5-7/) {
        if ($kernel =~ /xs/) {
            $DISTRO = "XenServer6.x";
            $rpm_distro = "xenserver6ux";
        } else {
            $DISTRO = "RHEL5.7";
            $rpm_distro = "rhel5u7";
        }
    } elsif ($dist_rpm =~ /redhat-release-.*-5.6|centos-release-5-6|enterprise-release-5-6/) {
        $DISTRO = "RHEL5.6";
        $rpm_distro = "rhel5u6";
    } elsif ($dist_rpm =~ /redhat-release-.*-5.5|centos-release-5-5|enterprise-release-5-5/) {
        system("grep -wq XenServer /etc/issue > /dev/null 2>&1");
        my $res = $? >> 8;
        my $sig = $? & 127;
        if ($sig or $res) {
            $DISTRO = "RHEL5.5";
            $rpm_distro = "rhel5u5";
        } else {
            $DISTRO = "XenServer5.6";
            $rpm_distro = "xenserver5u6";
        }
    } elsif ($dist_rpm =~ /redhat-release-.*-5.4|centos-release-5-4/) {
        $DISTRO = "RHEL5.4";
        $rpm_distro = "rhel5u4";
    } elsif ($dist_rpm =~ /redhat-release-.*-5.3|centos-release-5-3/) {
        $DISTRO = "RHEL5.3";
        $rpm_distro = "rhel5u3";
    } elsif ($dist_rpm =~ /redhat-release-.*-5.2|centos-release-5-2/) {
        $DISTRO = "RHEL5.2";
        $rpm_distro = "rhel5u2";
    } elsif ($dist_rpm =~ /redhat-release-4AS-9/) {
        $DISTRO = "RHEL4.8";
        $rpm_distro = "rhel4u8";
    } elsif ($dist_rpm =~ /redhat-release-4AS-8/) {
        $DISTRO = "RHEL4.7";
        $rpm_distro = "rhel4u7";
    } elsif ($dist_rpm =~ /fedora-release-12/) {
        $DISTRO = "FC12";
        $rpm_distro = "fc12";
    } elsif ($dist_rpm =~ /fedora-release-13/) {
        $DISTRO = "FC13";
        $rpm_distro = "fc13";
    } elsif ($dist_rpm =~ /fedora-release-14/) {
        $DISTRO = "FC14";
        $rpm_distro = "fc14";
    } elsif ($dist_rpm =~ /fedora-release-15/) {
        $DISTRO = "FC15";
        $rpm_distro = "fc15";
    } elsif ($dist_rpm =~ /fedora-release-16/) {
        $DISTRO = "FC16";
        $rpm_distro = "fc16";
    } elsif ($dist_rpm =~ /fedora-release-17/) {
        $DISTRO = "FC17";
        $rpm_distro = "fc17";
    } elsif ($dist_rpm =~ /fedora-release-18/) {
        $DISTRO = "FC18";
        $rpm_distro = "fc18";
    } elsif ($dist_rpm =~ /fedora-release-19/) {
        $DISTRO = "FC19";
        $rpm_distro = "fc19";
    } elsif ($dist_rpm =~ /fedora-release-20/) {
        $DISTRO = "FC20";
        $rpm_distro = "fc20";
    } elsif ($dist_rpm =~ /fedora-release-21/) {
        $DISTRO = "FC21";
        $rpm_distro = "fc21";
    } else {
        $DISTRO = "unsupported";
        $rpm_distro = "unsupported";
        print RED "Current operation system is not supported!", RESET "\n";
        exit 1;
    }

    print "Auto-detected $DISTRO distro.\n" if ($verbose2);
} else {
    print "Using provided distro: $DISTRO\n" if ($verbose2);

    $rpm_distro = $DISTRO;
    $rpm_distro = lc($rpm_distro);
    $rpm_distro =~ s/\./u/g;

    $DISTRO = uc ($DISTRO);
    if ($DISTRO =~ /SLES/) {
        $DISTRO =~ s/SP.*//g;
    } elsif ($DISTRO =~ /OPEN/) {
        $DISTRO =~ s/OPEN/open/g;
    }
}

my $SRPMS = $CWD . '/' . 'SRPMS/';
chomp $SRPMS;
my $RPMS  = $CWD . '/' . 'RPMS' . '/' . $dist_rpm . '/' . $arch;
chomp $RPMS;
if (not -d $RPMS) {
    mkpath([$RPMS]);
}

my $target_cpu  = `rpm --eval '%{_target_cpu}'`;
chomp $target_cpu;

my $target_cpu32;
if ($arch eq "x86_64") {
    if (-f "/etc/SuSE-release") {
        $target_cpu32 = 'i586';
    }
    else {
        $target_cpu32 = 'i686';
    }
}
elsif ($arch eq "ppc64") {
    $target_cpu32 = 'ppc';
}
chomp $target_cpu32;

if ($kernel_given and not $kernel_source_given) {
    if (-d "/lib/modules/$kernel/build") {
        $kernel_sources = "/lib/modules/$kernel/build";
    }
    else {
        print RED "Provide path to the kernel sources for $kernel kernel.", RESET "\n";
        exit 1;
    }
}

my $kernel_rel = $kernel;
$kernel_rel =~ s/-/_/g;

if ($DISTRO =~ /UBUNTU.*/) {
    $rpminstall_flags .= ' --force-debian --nodeps ';
    $rpmbuild_flags .= ' --nodeps ';
}

if (not $check_linux_deps) {
    $rpmbuild_flags .= ' --nodeps';
    $rpminstall_flags .= ' --nodeps';
}
my $optflags  = `rpm --eval '%{optflags}'`;
chomp $optflags;

my $mandir      = `rpm --eval '%{_mandir}'`;
chomp $mandir;
my $sysconfdir  = `rpm --eval '%{_sysconfdir}'`;
chomp $sysconfdir;
my %main_packages = ();
my @selected_packages = ();
my @selected_by_user = ();
my @selected_modules_by_user = ();
my @packages_to_uninstall = ();
my @dependant_packages_to_uninstall = ();
my %selected_for_uninstall = ();
my %non_ofed_for_uninstall = ();
my @selected_kernel_modules = ();

my $ibutils_prefix = '/opt/ibutils';
my $ibutils2_prefix = '/usr';
my $compiler = "gcc";

my $infiniband_diags = '';
if ($install_option eq 'guest-os') {
    $infiniband_diags = 'infiniband-diags-guest';
} else {
    $infiniband_diags = 'infiniband-diags-compat';
}

if ($install_option eq 'guest-os') {
    $rpminstall_flags .= ' --nodeps ';
}

my $fca = '';
if ($arch =~ m/x86_64/ and $install_option ne 'guest-os') {
    $fca = "fca";
}

if ($DISTRO =~ /SLES|SUSE/) {
    $package_manager = "zypper";
} else {
    $package_manager = "yum";
}

my $pmi_opt = '';
#if (is_installed("slurm")) {
#    $pmi_opt = '--with-pmi';
#}

my $libstdc = '';
my $libgcc = 'libgcc';
my $libgfortran = '';
my $fortran = 'gcc-gfortran';
my $curl_devel = 'curl-devel';
my $libnl_devel = 'libnl-devel';
my $libnl = 'libnl';
my $glib2 = 'glib2';
my $openssl_devel = 'openssl-devel';
my $libcurl = 'libcurl';
my $python_libxml2 = "libxml2-python";
my $cairo = "cairo";
my $atk = "atk";
my $gtk2 = "gtk2";
if ($DISTRO eq "openSUSE11.2") {
    $libstdc = 'libstdc++44';
    $libgcc = 'libgcc44';
    $libgfortran = 'libgfortran44';
    $libcurl = "libcurl4";
} elsif ($DISTRO eq "openSUSE12.1") {
    $libstdc = 'libstdc++46';
    $libgcc = 'libgcc46';
    $libgfortran = 'libgfortran46';
    $fortran = 'libgfortran46';
    $curl_devel = 'libcurl-devel';
    $libnl_devel = 'libnl-1_1-devel';
    $libnl = 'libnl-1_1';
    $glib2 = "glib2-tools";
    $libcurl = "libcurl4";
} elsif ($DISTRO eq "openSUSE") {
    $libstdc = 'libstdc++42';
    $libgcc = 'libgcc42';
    $libcurl = "libcurl4";
} elsif ($DISTRO =~ /UBUNTU/) {
    $libstdc = 'libstdc++6';
    $libgfortran = 'libgfortran3';
} elsif ($DISTRO =~ m/SLES11/) {
    $libstdc = 'libstdc++43';
    $libgcc = 'libgcc43';
    $libgfortran = 'libgfortran43';
    $curl_devel = 'libcurl-devel';
    if ($rpm_distro eq "sles11sp2") {
        $libstdc = 'libstdc++46';
        $libgcc = 'libgcc46';
        $libgfortran = 'libgfortran46';
    }
    $fortran = "gcc-fortran";
    $openssl_devel = 'libopenssl-devel';
    $libcurl = "libcurl4";
} elsif ($DISTRO =~ m/SLES12/) {
    $libstdc = 'libstdc++6';
    $libgcc = 'libgcc_s1';
    $libgfortran = 'libgfortran3';
    $curl_devel = 'libcurl-devel';
    $fortran = "gcc-fortran";
    $openssl_devel = 'libopenssl-devel';
    $libcurl = "libcurl4";
    $python_libxml2 = "python-libxml2";
    $libnl_devel = 'libnl-1_1-devel';
    $libnl = 'libnl1';
    $glib2 = 'libglib-2_0-0';
    $cairo = "libcairo2";
    $atk = "libatk-1_0-0";
    $gtk2 = "libgtk-2_0-0";
} elsif ($DISTRO =~ m/RHEL|OEL|FC/) {
    $libstdc = 'libstdc++';
    $libgcc = 'libgcc';
    $libgfortran = 'gcc-gfortran';
    if ($DISTRO =~ m/RHEL6|RHEL7|OEL6|OEL7|FC/) {
        $curl_devel = 'libcurl-devel';
    }
}else {
    $libstdc = 'libstdc++';
}

my $libstdc_devel = ($DISTRO =~ m/UBUNTU/)?"libstdc++6-4.4-dev":"$libstdc-devel";
my $libexpat_devel = "";

if ($DISTRO =~ m/SLES11/) {
    $libexpat_devel = "libexpat-devel";
} elsif ($DISTRO =~ m/SLES12/) {
    $libstdc_devel = 'libstdc++-devel';
} elsif ($DISTRO =~ m/RHEL5.[4-6]|RHEL6|RHEL7|OEL6|OEL7/) {
    $libexpat_devel = "expat-devel";
} elsif ($DISTRO =~ m/RHEL5.3/) {
    $libexpat_devel = "expat-devel";
} elsif ($DISTRO =~ m/SLES10/) {
    $libexpat_devel = "expat";
} else {
    $libexpat_devel = "expat-devel";
}

my $valgrind_devel = ($DISTRO =~ m/RHEL6.[45]/ or $with_valgrind) ? "valgrind-devel" : "";

# Suffix for 32 and 64 bit packages
my $is_suse_suff64 = $arch eq "ppc64" && $DISTRO !~ /SLES11|SLES12/;
my $suffix_32bit = ($DISTRO =~ m/SLES|openSUSE/ && !$is_suse_suff64) ? "-32bit" : ".$target_cpu32";
my $suffix_64bit = ($DISTRO =~ m/SLES|openSUSE/ &&  $is_suse_suff64) ? "-64bit" : "";

sub usage
{
   print GREEN;
   print "\n Usage: $0 [-c <packages config_file>|--all|--hpc|--vma|--basic] [-n|--net <network config_file>]\n";

   print "\n           -c|--config <packages config_file>. Example of the config file can be found under docs (ofed.conf-example).";
   print "\n           -n|--net <network config_file>      Example of the config file can be found under docs (ofed_net.conf-example).";
   print "\n           --conf-dir           Destination directory to save the configuration file. Default: $CWD";
   print "\n           -l|--prefix          Set installation prefix.";
   print "\n           -p|--print-available Print available packages for current platform.";
   print "\n                                And create corresponding ofed.conf file.";
   print "\n           -k|--kernel <kernel version>. Default on this system: $kernel";
   print "\n           -s|--kernel-sources  <path to the kernel sources>. Default on this system: $kernel_sources";
   print "\n           -U|--update          Update installed version.";
   print "\n           -b|--build-only      Build binary RPMs without installing them.";
   print "\n           --build32            Build 32-bit libraries. Relevant for x86_64 and ppc64 platforms";
   print "\n           --without-depcheck   Skip Distro's libraries check";
   print "\n           --distro             Set Distro name for the running OS (e.g: rhel6.5, sles11sp3). Default: Use auto-detection.";
   print "\n           --disable-kmp        Build kernel-ib RPM instead of ofa_kernel KMP RPMs";
   print "\n           -v|-vv|-vvv          Set verbosity level";
   print "\n           -q                   Set quiet - no messages will be printed";
   print "\n           --force              Force installation";
   print "\n           --builddir           Change build directory. Default: $builddir";
   print "\n           --umad-dev-rw        Grant non root users read/write permission for umad devices instead of default";
   print "\n           --umad-dev-na        Prevent from non root users read/write access for umad devices. Overrides '--umad-dev-rw'";
   print "\n           --enable-mlnx_tune         Enable Running the mlnx_tune utility";
   print "\n           --without-<package>  Do not install package";
   print "\n           --with-<package>     Force installing package";
   print "\n           --with-memtrack      Build ofa_kernel RPM with memory tracking enabled for debugging";
   print "\n\n           --all|--hpc|--basic    Install all,hpc or basic packages correspondingly";
   print "\n\n           --vma|--vma-vpi    Install packages required by VMA to support VPI";
   print "\n\n           --vma-eth          Install packages required by VMA to work over Ethernet";
   print "\n\n           --with-vma         Set configuration for VMA use (to be used with any installation parameter).";
   print "\n\n           --guest            Install packages required by guest os";
   print "\n\n           --hypervisor       Install packages required by hypervisor os";
   print "\n\n           --with-fabric-collector    Install fabric-collector package.";
   print RESET "\n\n";
}

my $sysfsutils;
my $sysfsutils_devel;

if ($DISTRO =~ m/SLES|openSUSE/) {
    $sysfsutils = "sysfsutils";
    $sysfsutils_devel = "sysfsutils";
} elsif ($DISTRO =~ m/RHEL5/) {
    $sysfsutils = "libsysfs";
    $sysfsutils_devel = "libsysfs";
} elsif ($DISTRO =~ m/RHEL6|RHEL7|OEL6|OEL7/) {
    $sysfsutils = "libsysfs";
    $sysfsutils_devel = "libsysfs";
}

my $kernel_req = "";
if ($DISTRO =~ /RHEL|OEL/) {
    $kernel_req = "redhat-rpm-config";
} elsif ($DISTRO =~ /SLES10/) {
    $kernel_req = "kernel-syms";
} elsif ($DISTRO =~ /SLES11|SLES12/) {
    $kernel_req = "kernel-source";
}

my $network_dir;
if ($DISTRO =~ m/SLES/) {
    $network_dir = "/etc/sysconfig/network";
}
else {
    $network_dir = "/etc/sysconfig/network-scripts";
}

my $setpci = '/sbin/setpci';
my $lspci = '/sbin/lspci';

# List of packages that were included in the previous OFED releases
# for uninstall purpose

my @prev_ofed_packages = (
                        "kernel-ib", "kernel-ib-devel",
                        "mpich_mlx", "ibtsal", "openib", "opensm", "opensm-devel", "opensm-libs",
                        "mpi_ncsa", "mpi_osu", "thca", "ib-osm", "osm", "diags", "ibadm",
                        "ib-diags", "ibgdiag", "ibdiag", "ib-management",
                        "ib-verbs", "ib-ipoib", "ib-cm", "ib-sdp", "ib-dapl", "udapl",
                        "udapl-devel", "libdat", "libibat", "ib-kdapl", "ib-srp", "ib-srp_target",
                        "libipathverbs", "libipathverbs-devel",
                        "libehca", "libehca-devel", "dapl", "dapl-devel",
                        "libibcm", "libibcm-devel", "libibcommon", "libibcommon-devel",
                        "libibmad", "libibmad-devel", "libibumad", "libibumad-devel",
                        "ibsim", "ibsim-debuginfo",
                        "libibverbs1", "libibverbs", "libibverbs-devel", "libibverbs-utils",
                        "libipathverbs", "libipathverbs-devel", "libmthca",
                        "libmthca-devel", "libmlx4", "libmlx4-devel",
                        "librdmacm", "librdmacm-devel", "librdmacm-utils", "ibacm",
                        "openib-diags", "openib-mstflint", "openib-perftest", "openib-srptools", "openib-tvflash",
                        "openmpi", "openmpi-devel", "openmpi-libs",
                        "ibutils", "ibutils-devel", "ibutils-libs", "ibutils2", "ibutils2-devel",
                        "libnes", "libnes-devel",
                        "infinipath-psm", "infinipath-psm-devel",
                        "mellanox-firmware", "mellanox-ofed", "mlnxofed", "mft-int", "kernel-mft", "mft-compat", "mlx4_accl", "mlx4_accl_sys",
                        "compat-dapl", "compat-dapl-devel",
                        "mvapich", "mvapich2",
                        "mvapich_gcc", "openmpi_gcc", "mvapich2_gcc", "mpitests_mvapich2", "mpitests_openmpi",
                        "fabric-collector", "fabric-collector-debuginfo",
                        "libsdp", "libsdp-devel", "libsdp-debuginfo",
                        "sdpnetstat-debuginfo", "sdpnetstat",
                        );


my @distro_ofed_packages = (
                        "libamso", "libamso-devel", "dapl2", "dapl2-devel", "mvapich", "mvapich2", "mvapich2-devel",
                        "mvapich-devel", "libboost_mpi1_36_0", "boost-devel", "boost-doc", "libmthca-rdmav2", "libcxgb3-rdmav2", "libcxgb4-rdmav2",
                        "libmlx4-rdmav2", "libibmad1", "libibumad1", "libibcommon1", "ofed", "ofa",
                        "rdma-ofa-agent", "libibumad3", "libibmad5"
                        );

my @mlnx_en_packages = (
                       "mlnx_en", "mlnx-en-devel", "mlnx_en-devel", "mlnx_en-doc", "mlnx-ofc", "mlnx-ofc-debuginfo"
                        );

# List of all available packages sorted following dependencies
if ($kmp) {
    system("/bin/rpm -qf $kernel_sources/scripts > /dev/null 2>&1");
    my $res = $? >> 8;
    my $sig = $? & 127;
    if ($sig or $res) {
        print "KMP is not supported for kernels which were not installed as RPM.\n" if ($verbose2);
        $kmp = 0;
    }
}

if ($kmp and ($DISTRO =~ m/XenServer|RHEL5.2|OEL|FC|SLES10/ or $kernel =~ /xs|fbk|fc|debug|uek/)) {
    print RED "KMP is not supported on $DISTRO. Switching to non-KMP mode", RESET "\n" if ($verbose2);;
    $kmp = 0;
}

# Disable KMP for kernels incompatible with the original Distro's kernel
if ($kmp) {
    # RHEL
    if (($DISTRO eq "RHEL7.0" and $kernel !~ /3.10.0-123.*el7\.$arch/) or
        ($DISTRO eq "RHEL6.6" and $kernel !~ /2.6.32-4[0-9][0-9].*el6\.$arch/) or
        ($DISTRO eq "RHEL6.5" and $kernel !~ /2.6.32-4[0-9][0-9].*el6\.$arch/) or
        ($DISTRO eq "RHEL6.4" and $kernel !~ /2.6.32-358.*el6\.$arch/) or
        ($DISTRO eq "RHEL6.3" and $kernel !~ /2.6.32-279.*el6\.$arch/) or
        ($DISTRO eq "RHEL6.2" and $kernel !~ /2.6.32-220.*el6\.$arch/) or
        ($DISTRO eq "RHEL6.1" and $kernel !~ /2.6.32-131.*el6\.$arch/)) {
        $kmp = 0;
    }
    # SLES
    if (($rpm_distro eq "sles12sp0" and $kernel !~ /3.12.[1-9][2-9]-[0-9].*/) or
        ($rpm_distro eq "sles11sp3" and $kernel !~ /3.0.7[6-9]-[0-9].[0-9]*|3.0.[8-9][0-9]-[0-9].[0-9]*|3.0.[1-9][0-9][0-9]-[0-9].[0-9]/) or
        ($rpm_distro eq "sles11sp2" and $kernel !~ /3\.0\.(1[3-9]|[2-8][0-9])-0\.([0-9]|27)/) or #3.0.[13-80]-0.[0-9].[1-9]
        ($rpm_distro eq "sles11sp1" and $kernel !~ /2\.6\.32\.(1[2-9]|[2-5][0-9])-0\.[0-9]/) ) { #2.6.32.[12-54]-0.[0-9].[1-2]
        $kmp = 0;
    }
    # OpenSUSE
    if ($DISTRO eq "openSUSE12.1" and ($kernel !~ /3\.1\.[0-9]/) ) {
        $kmp = 0;
    }
}

# set which rpm to use for those rpms that support KMP
my $kernel_rpm;
my $knem_rpm;
my $kernel_mft_rpm;
my $ummunotify_rpm;

if ($kmp) {
    $kernel_rpm = "mlnx-ofa_kernel";
    $knem_rpm = "knem-mlnx";
    $kernel_mft_rpm = "kernel-mft-mlnx";
    $ummunotify_rpm = "ummunotify-mlnx";
} else {
    $kernel_rpm = "kernel-ib";
    $knem_rpm = "knem";
    $kernel_mft_rpm = "kernel-mft";
    $ummunotify_rpm = "ummunotify";
}

my @kernel_packages = ($kernel_rpm, "$kernel_rpm-devel", $knem_rpm, "ib-bonding", "ib-bonding-debuginfo", $kernel_mft_rpm, "mlx4_accl_sys", "mlx4_accl", $ummunotify_rpm, 'iser', 'srp');
my @basic_kernel_modules = ("core", "mthca", "mlx4", "mlx4_en", "mlx4_vnic", "mlx4_fc", "mlx5", "cxgb3", "cxgb4", "nes", "ehca", "qib", "ipoib", "ipath", "amso1100");
my @ulp_modules = ("sdp", "srp", "srpt", "rds", "qlgc_vnic", "iser", "e_ipoib", "nfsrdma", "9pnet_rdma", "9p", "cxgb3i", "cxgb4i");

# kernel modules in "technology preview" status can be installed by
# adding "module=y" to the ofed.conf file in unattended installation mode
# or by selecting the module in custom installation mode during interactive installation
my @tech_preview;

my @kernel_modules = (@basic_kernel_modules, @ulp_modules);

my $kernel_configure_options = '';
my $user_configure_options = '';

my @misc_packages = ("ofed-docs", "ofed-scripts");

# The entries will be added later in add_new_mpitests_pkg function.
my @mpitests_packages = ();

my @mpi_packages = ( "mpi-selector",
                     "mvapich2",
                     "openmpi",
                     "openshmem",
                     @mpitests_packages
                    );

my @rds_packages = ( "rds-tools", "rds-devel", "rnfs-utils"
                    );

my @user_packages = ("libibverbs", "libibverbs-devel", "libibverbs-devel-static",
                     "libibverbs-utils", "libibverbs-debuginfo",
                     "libmthca", "libmthca-devel-static", "libmthca-debuginfo",
                     "libmlx4", "libmlx4-devel", "libmlx4-debuginfo",
                     "libmlx5", "libmlx5-devel", "libmlx5-debuginfo", "libmlx5-rdmav2",
                     "libehca", "libehca-devel-static", "libehca-debuginfo",
                     "libcxgb3", "libcxgb3-devel", "libcxgb3-debuginfo",
                     "libcxgb4", "libcxgb4-devel", "libcxgb4-debuginfo",
                     "libnes", "libnes-devel-static", "libnes-debuginfo",
                     "libipathverbs", "libipathverbs-devel", "libipathverbs-debuginfo",
                     "libibcm", "libibcm-devel", "libibcm-debuginfo",
                     "libibumad", "libibumad-devel", "libibumad-static", "libibumad-debuginfo",
                     "libibmad", "libibmad-devel", "libibmad-static", "libibmad-debuginfo",
                     "ibsim", "ibsim-debuginfo", "ibacm",
                     "librdmacm", "librdmacm-utils", "librdmacm-devel", "librdmacm-debuginfo",
                     "libsdp", "libsdp-devel", "libsdp-debuginfo",
                     "opensm", "opensm-libs", "opensm-devel", "opensm-debuginfo", "opensm-static",
                     "dapl", "dapl-devel", "dapl-devel-static", "dapl-utils", "dapl-debuginfo",
                     "perftest", "mstflint",
                     "qlvnictools", "sdpnetstat", "sdpnetstat-debuginfo", "srptools", @rds_packages,
                     "ibutils2", "ibutils", "cc_mgr", "dump_pr", "ar_mgr", "ibdump",
                     "infiniband-diags", "infiniband-diags-compat", "qperf", "qperf-debuginfo",
                     "ofed-docs", "ofed-scripts",
                     "fca", "mxm", "bupc",
                     "infinipath-psm", "infinipath-psm-devel", @mpi_packages, "libibprof", "hcoll", "libvma", "libvma-debuginfo",
                     "fabric-collector", "fabric-collector-debuginfo",
                     );

# List of packages that were included in the previous OFED releases
# for uninstall purpose
my @distro_packages = (
                      "fcoe-utils", "scsi-target-utils", "anaconda"
                      );

my @mft_packages = (
                   "mft-int", "kernel-mft", "mft-compat"
                   );

my @basic_kernel_packages = ($kernel_rpm, "$kernel_rpm-devel", $kernel_mft_rpm, 'iser', 'srp');
my @basic_user_packages = ("libibverbs", "libibverbs-utils", "libmthca", "libmlx4", "libmlx5",
                            "libehca", "libcxgb3", "libcxgb4", "libnes", "libipathverbs", "librdmacm", "librdmacm-utils",
                            "mstflint", @misc_packages);

my @hpc_kernel_packages = (@basic_kernel_packages, $knem_rpm, "ib-bonding", "mlx4_accl_sys", "mlx4_accl", $ummunotify_rpm);
my @hpc_kernel_modules = (@basic_kernel_modules);
my @hpc_user_packages = (@basic_user_packages, "fca", "mxm" ,"bupc" ,"ibacm", "librdmacm",
                        "librdmacm-utils", "dapl", "dapl-devel", "dapl-devel-static", "dapl-utils",
                        "infiniband-diags", "infiniband-diags-compat", "ibutils2", "ibutils", "cc_mgr", "dump_pr", "ar_mgr", "ibdump", "qperf", "mstflint", "perftest", @rds_packages, @mpi_packages, "libibprof", "hcoll");

my @vma_kernel_packages = (@basic_kernel_packages, "ib-bonding");
my @vma_kernel_modules = (@basic_kernel_modules);
my @vmavpi_user_packages = ("libibverbs", "libibverbs-devel", "libibverbs-devel-static", "libibverbs-utils", "libmlx4", "libmlx4-devel", "libmlx5", "libmlx5-devel",
                            "ibacm", "librdmacm", "librdmacm-devel", "librdmacm-utils", "perftest", "mstflint", "infiniband-diags", "infiniband-diags-compat",
                            "opensm", "opensm-libs", "opensm-devel", "opensm-static", "mft", "ibutils2", "ibutils", "ibdump", @misc_packages, "libvma");
my @vma_user_packages = (@vmavpi_user_packages);
my @vmaeth_user_packages = ("libibverbs", "libibverbs-devel", "libibverbs-devel-static", "libibverbs-utils", "libmlx4", "libmlx4-devel", "libmlx5", "libmlx5-devel",
                            "ibacm", "librdmacm", "librdmacm-devel",, "librdmacm-utils", "perftest", "mstflint", "mft", "ibutils2", "ibutils",
                            @misc_packages, "libvma");

my @hypervisor_kernel_packages = (@basic_kernel_packages);
my @guest_kernel_packages = (@basic_kernel_packages);
my @guest_kernel_modules = ("core","mlx4","mlx4_en","ipoib","srp","iser");
my @hypervisor_kernel_modules = ("core","mlx4","mlx4_en","mlx4_vnic","mlx5","ipoib","srp","iser");

my @sroiv_common_user_packages = ("udapl","ofed-scripts","libibcm","libibcm-devel",
					"ibacm","librdmacm","librdmacm-utils","librdmacm-devel",
					"libibverbs","libibverbs-devel","libibverbs-devel-static","libibverbs-utils",
					"libmlx4","libmlx4-devel","libmlx5","libmlx5-devel","libibumad","libibumad-devel","libibumad-static",
					"libibmad","libibmad-devel","libibmad-static","perftest",
					"srptools","qperf", @rds_packages);

my @hypervisor_user_packages = (@sroiv_common_user_packages,"infiniband-diags","infiniband-diags-compat","osm","mstflint","ofed-docs",
					"dapl","dapl-devel","dapl-devel-static","dapl-utils",
					"opensm","opensm-libs","opensm-devel","opensm-static","ibutils2","ibutils");

my @guest_user_packages = (@sroiv_common_user_packages,"infiniband-diags-guest", @mpi_packages);

# all_packages is required to save ordered (following dependencies) list of
# packages. Hash does not saves the order
my @all_packages = (@kernel_packages, @user_packages);

# which modules are required for the standalone module rpms
my %standalone_kernel_modules_info = (
        'iser' => ["core", "ipoib"],
        'srp' => ["core", "ipoib"],
);

my %kernel_modules_info = (
        'core' =>
            { name => "core", available => 1, selected => 0,
            included_in_rpm => 0, requires => [], },
        'mthca' =>
            { name => "mthca", available => 1, selected => 0,
            included_in_rpm => 0, requires => ["core"], },
        'mlx4' =>
            { name => "mlx4", available => 1, selected => 0,
            included_in_rpm => 0, requires => ["core"], },
        'mlx5' =>
            { name => "mlx5", available => 1, selected => 0,
            included_in_rpm => 0, requires => ["core"], },
        'mlx4_en' =>
            { name => "mlx4_en", available => 1, selected => 0,
            included_in_rpm => 0, requires => ["core","mlx4"], },
        'mlx4_vnic' =>
            { name => "mlx4_vnic", available => 1, selected => 0,
            included_in_rpm => 0, requires => ["core","mlx4"], },
        'mlx4_fc' =>
            { name => "mlx4_fc", available => 0, selected => 0,
            included_in_rpm => 0, requires => ["core","mlx4_en"], },
        'ehca' =>
            { name => "ehca", available => 0, selected => 0,
            included_in_rpm => 0, requires => ["core"], },
        'ipath' =>
            { name => "ipath", available => 1, selected => 0,
            included_in_rpm => 0, requires => ["core"], },
        'qib' =>
            { name => "qib", available => 0, selected => 0,
            included_in_rpm => 0, requires => ["core"], },
        'cxgb3' =>
            { name => "cxgb3", available => 1, selected => 0,
            included_in_rpm => 0, requires => ["core"], },
        'cxgb4' =>
            { name => "cxgb4", available => 1, selected => 0,
            included_in_rpm => 0, requires => ["core"], },
        'cxgb3i' =>
            { name => "cxgb3i", available => 1, selected => 0,
            included_in_rpm => 0, requires => ["core"], },
        'cxgb4i' =>
            { name => "cxgb4i", available => 1, selected => 0,
            included_in_rpm => 0, requires => ["core"], },
        'nes' =>
            { name => "nes", available => 1, selected => 0,
            included_in_rpm => 0, requires => ["core"], },
        'ipoib' =>
            { name => "ipoib", available => 1, selected => 0,
            included_in_rpm => 0, requires => ["core"], },
        'sdp' =>
            { name => "sdp", available => 0, selected => 0,
            included_in_rpm => 0, requires => ["core", "ipoib"], },
        'srp' =>
            { name => "srp", available => 1, selected => 0,
            included_in_rpm => 0, requires => ["core", "ipoib"], },
        'srpt' =>
            { name => "srpt", available => 0, selected => 0,
            included_in_rpm => 0, requires => ["core"], },
        'rds' =>
            { name => "rds", available => 1, selected => 0,
            included_in_rpm => 0, requires => ["core", "ipoib"], },
        'e_ipoib' =>
            { name => "e_ipoib", available => 1, selected => 0,
            included_in_rpm => 0, requires => ["core", "ipoib"], },
        'iser' =>
            { name => "iser", available => 1, selected => 0,
            included_in_rpm => 0, requires => ["core", "ipoib"], ofa_req_inst => [] },
        'qlgc_vnic' =>
            { name => "qlgc_vnic", available => 0, selected => 0,
            included_in_rpm => 0, requires => ["core"], },
        'nfsrdma' =>
            { name => "nfsrdma", available => 1, selected => 0,
            included_in_rpm => 0, requires => ["core", "ipoib"], },
        '9pnet_rdma' =>
            { name => "9pnet_rdma", available => 1, selected => 0,
            included_in_rpm => 0, requires => ["core"], },
        'amso1100' =>
            { name => "amso1100", available => 1, selected => 0,
            included_in_rpm => 0, requires => ["core"], },
        '9p' =>
            { name => "9p", available => 1, selected => 0,
            included_in_rpm => 0, requires => ["core"], },
        );

my %packages_info = (
        # Kernel packages
        'ofa_kernel' =>
            { name => "ofa_kernel", parent => "ofa_kernel",
            selected => 0, installed => 0, rpm_exist => 0, rpm_exist32 => 0,
            available => 1, mode => "kernel", dist_req_build => ["rpmbuild", "make", "gcc"],
            dist_req_inst => ["pciutils","python"], ofa_req_build => [], ubuntu_dist_req_build => ["make","gcc"], ubuntu_dist_req_inst => ["pciutils","python"],
            ofa_req_inst => ["ofed-scripts"], configure_options => '' },
        'ofa_kernel-devel' =>
            { name => "ofa_kernel", parent => "ofa_kernel",
            selected => 0, installed => 0, rpm_exist => 0, rpm_exist32 => 0,
            available => 1, mode => "kernel", dist_req_build => [],
            dist_req_inst => [], ofa_req_build => [], ofa_req_inst => ["ofed-scripts"], configure_options => '' },
        'mlnx-ofa_kernel' =>
           { name => "mlnx-ofa_kernel", parent => "mlnx-ofa_kernel",
           selected => 0, installed => 0, rpm_exist => 0, rpm_exist32 => 0,
           available => 1, mode => "kernel", dist_req_build => ["make", "gcc", "$kernel_req"],
           dist_req_inst => ["pciutils","python",$python_libxml2],ubuntu_dist_req_build => ["make","gcc"],ubuntu_dist_req_inst => ["pciutils","python"],
           ofa_req_build => [], ofa_req_inst => ["ofed-scripts"], configure_options => '' },
        'mlnx-ofa_kernel-devel' =>
           { name => "mlnx-ofa_kernel-devel", parent => "mlnx-ofa_kernel",
           selected => 0, installed => 0, rpm_exist => 0, rpm_exist32 => 0,
           available => 1, mode => "kernel", dist_req_build => [],
           dist_req_inst => [], ofa_req_build => [], ofa_req_inst => ["ofed-scripts"], configure_options => '' },
        'kernel-ib' =>
            { name => "kernel-ib", parent => "mlnx-ofa_kernel",
            selected => 0, installed => 0, rpm_exist => 0, rpm_exist32 => 0,
            available => 1, mode => "kernel", dist_req_build => ["make", "gcc"],
            dist_req_inst => ["pciutils",$python_libxml2],ubuntu_dist_req_build => ["make","gcc"],ubuntu_dist_req_inst => ["pciutils"],
            ofa_req_build => [], ofa_req_inst => ["ofed-scripts"], },
        'kernel-ib-devel' =>
            { name => "kernel-ib-devel", parent => "mlnx-ofa_kernel",
            selected => 0, installed => 0, rpm_exist => 0, rpm_exist32 => 0,
            available => 1, mode => "kernel", dist_req_build => [],
            dist_req_inst => [], ofa_req_build => [],
            ofa_req_inst => ["kernel-ib"], },
        'ib-bonding' =>
            { name => "ib-bonding", parent => "ib-bonding",
            selected => 0, installed => 0, rpm_exist => 0, rpm_exist32 => 0,
            available => 0, mode => "kernel", dist_req_build => [],
            dist_req_inst => [], ofa_req_build => [], ofa_req_inst => [], configure_options => '' },
        'ib-bonding-debuginfo' =>
            { name => "ib-bonding-debuginfo", parent => "ib-bonding",
            selected => 0, installed => 0, rpm_exist => 0, rpm_exist32 => 0,
            available => 0, mode => "kernel", dist_req_build => [],
            dist_req_inst => [], ofa_req_build => [], ofa_req_inst => [], },
        'kernel-mft' =>
            { name => "kernel-mft", parent => "kernel-mft",
            selected => 0, installed => 0, rpm_exist => 0, rpm_exist32 => 0,
            available => 1, mode => "kernel", dist_req_build => [],
            dist_req_inst => [], ofa_req_build => [], ofa_req_inst => ["ofed-scripts"], configure_options => '' },
        'kernel-mft-mlnx' =>
            { name => "kernel-mft-mlnx", parent => "kernel-mft",
            selected => 0, installed => 0, rpm_exist => 0, rpm_exist32 => 0,
            available => 1, mode => "kernel", dist_req_build => [],
            dist_req_inst => [], ofa_req_build => [], ofa_req_inst => ["ofed-scripts"], configure_options => '' },
        'mlx4_accl_sys' =>
            { name => "mlx4_accl_sys", parent => "mlx4_accl_sys",
            selected => 0, installed => 0, rpm_exist => 0, rpm_exist32 => 0,
            available => 0, mode => "kernel", dist_req_build => [],
            dist_req_inst => [], ofa_req_build => [], ofa_req_inst => ["ofed-scripts"], configure_options => '' },
        'mlx4_accl' =>
            { name => "mlx4_accl", parent => "mlx4_accl",
            selected => 0, installed => 0, rpm_exist => 0, rpm_exist32 => 0,
            available => 0, mode => "kernel", dist_req_build => [],
            dist_req_inst => [], ofa_req_build => ["mlx4_accl_sys"], ofa_req_inst => ["ofed-scripts","mlx4_accl_sys"], configure_options => '' },
        'knem' =>
            { name => "knem", parent => "knem",
            selected => 0, installed => 0, rpm_exist => 0, rpm_exist32 => 0,
            available => 1, mode => "kernel", dist_req_build => [],
            dist_req_inst => [], ofa_req_build => [], ofa_req_inst => [], configure_options => '' },
        'knem-mlnx' =>
            { name => "knem-mlnx", parent => "knem",
            selected => 0, installed => 0, rpm_exist => 0, rpm_exist32 => 0,
            available => 1, mode => "kernel", dist_req_build => [],
            dist_req_inst => [], ofa_req_build => [], ofa_req_inst => [], configure_options => '' },
        'ummunotify' =>
            { name => "ummunotify", parent => "ummunotify",
            selected => 0, installed => 0, rpm_exist => 0, rpm_exist32 => 0,
            available => 1, mode => "kernel", dist_req_build => [],
            dist_req_inst => [], ofa_req_build => [], ofa_req_inst => [], configure_options => '' },
        'ummunotify-mlnx' =>
            { name => "ummunotify-mlnx", parent => "ummunotify",
            selected => 0, installed => 0, rpm_exist => 0, rpm_exist32 => 0,
            available => 1, mode => "kernel", dist_req_build => [],
            dist_req_inst => [], ofa_req_build => [], ofa_req_inst => [], configure_options => '' },

        # User space libraries
        'libibverbs' =>
            { name => "libibverbs", parent => "libibverbs",
            selected => 0, installed => 0, rpm_exist => 0, rpm_exist32 => 0,
            available => 1, mode => "user", dist_req_build =>
            ($build32 == 1 )?["$valgrind_devel", "gcc__3.3.3", "glibc-devel$suffix_64bit","glibc-devel$suffix_32bit","$libgcc","$libgcc" . (($DISTRO eq "SLES10")?".$target_cpu32":"$suffix_32bit"), "$libnl_devel"."$suffix_64bit", ($DISTRO =~ /SUSE/)?"$libnl_devel"."$suffix_32bit":(($arch =~ /ppc/)?"$libnl_devel":"$libnl_devel.$target_cpu32")]:["$valgrind_devel", "gcc__3.3.3", "glibc-devel$suffix_64bit","$libgcc", "$libnl_devel"."$suffix_64bit"],
            dist_req_inst => ( $build32 == 1 )?["$libnl"."$suffix_64bit", ($dist_rpm !~ /sles-release-11.1/)?"$libnl"."$suffix_32bit":"$libnl.$target_cpu32"]:["$libnl"."$suffix_64bit"] ,
            ofa_req_build => [], ofa_req_inst => ["ofed-scripts"],
            ubuntu_dist_req_build =>( $build32 == 1 )?["gcc", "libc6-dev","libc6-dev-i386",
            "libgcc1","lib32gcc1"]:["gcc", "libc6-dev","libgcc1"],
            ubuntu_dist_req_inst => [],install32 => 1, exception => 0, configure_options => '' },
        'libibverbs-devel' =>
            { name => "libibverbs-devel", parent => "libibverbs",
            selected => 0, installed => 0, rpm_exist => 0, rpm_exist32 => 0,
            available => 1, mode => "user", dist_req_build => [],
            dist_req_inst => [], ofa_req_build => [],
            ofa_req_inst => ["libibverbs"],
            install32 => 1, exception => 0 },
        'libibverbs-devel-static' =>
            { name => "libibverbs-devel-static", parent => "libibverbs",
            selected => 0, installed => 0, rpm_exist => 0, rpm_exist32 => 0,
            available => 1, mode => "user", dist_req_build => [],
            dist_req_inst => [], ofa_req_build => [],
            ofa_req_inst => ["libibverbs"],
            install32 => 1, exception => 0 },
        'libibverbs-utils' =>
            { name => "libibverbs-utils", parent => "libibverbs",
            selected => 0, installed => 0, rpm_exist => 0, rpm_exist32 => 0,
            available => 1, mode => "user", dist_req_build => [],
            dist_req_inst => [], ofa_req_build => [],
            ofa_req_inst => ["libibverbs"],
            install32 => 0, exception => 0 },
        'libibverbs-debuginfo' =>
            { name => "libibverbs-debuginfo", parent => "libibverbs",
            selected => 0, installed => 0, rpm_exist => 0, rpm_exist32 => 0,
            available => 1, mode => "user", dist_req_build => [],
            dist_req_inst => [], ofa_req_build => [],
            ofa_req_inst => [],
            install32 => 0, exception => 0 },

        'libmthca' =>
            { name => "libmthca", parent => "libmthca",
            selected => 0, installed => 0, rpm_exist => 0, rpm_exist32 => 0,
            available => 1, mode => "user", dist_req_build => [],
            dist_req_inst => [], ofa_req_build => ["libibverbs","libibverbs-devel"],
            ofa_req_inst => ["libibverbs"],
            install32 => 1, exception => 0, configure_options => '' },
        'libmthca-devel-static' =>
            { name => "libmthca-devel-static", parent => "libmthca",
            selected => 0, installed => 0, rpm_exist => 0, rpm_exist32 => 0,
            available => 1, mode => "user", dist_req_build => [],
            dist_req_inst => [], ofa_req_build => ["libibverbs","libibverbs-devel"],
            ofa_req_inst => ["libibverbs", "libmthca"],
            install32 => 1, exception => 0 },
        'libmthca-debuginfo' =>
            { name => "libmthca-debuginfo", parent => "libmthca",
            selected => 0, installed => 0, rpm_exist => 0, rpm_exist32 => 0,
            available => 1, mode => "user", dist_req_build => [],
            dist_req_inst => [], ofa_req_build => ["libibverbs","libibverbs-devel"],
            ofa_req_inst => [],
            install32 => 0, exception => 0 },

        'libmlx4' =>
            { name => "libmlx4", parent => "libmlx4",
            selected => 0, installed => 0, rpm_exist => 0, rpm_exist32 => 0,
            available => 1, mode => "user", dist_req_build => ["$valgrind_devel"],
            dist_req_inst => [], ofa_req_build => ["libibverbs-devel"],
            ofa_req_inst => ["libibverbs"],
            install32 => 1, exception => 0, configure_options => '' },
        'libmlx4-devel' =>
            { name => "libmlx4-devel", parent => "libmlx4",
            selected => 0, installed => 0, rpm_exist => 0, rpm_exist32 => 0,
            available => 1, mode => "user", dist_req_build => [],
            dist_req_inst => [], ofa_req_build => ["libibverbs","libibverbs-devel"],
            ofa_req_inst => ["libibverbs","libmlx4"],
            install32 => 1, exception => 0 },
        'libmlx4-debuginfo' =>
            { name => "libmlx4-debuginfo", parent => "libmlx4",
            selected => 0, installed => 0, rpm_exist => 0, rpm_exist32 => 0,
            available => 1, mode => "user", dist_req_build => [],
            dist_req_inst => [], ofa_req_build => ["libibverbs","libibverbs-devel"],
            ofa_req_inst => [],
            install32 => 0, exception => 0 },

        'libmlx5' =>
            { name => "libmlx5", parent => "libmlx5",
            selected => 0, installed => 0, rpm_exist => 0, rpm_exist32 => 0,
            available => 1, mode => "user", dist_req_build => ["$valgrind_devel"],
            dist_req_inst => [], ofa_req_build => ["libibverbs-devel"],
            ofa_req_inst => ["libibverbs"],
            install32 => 1, exception => 0, configure_options => '' },
        'libmlx5-devel' =>
            { name => "libmlx5-devel", parent => "libmlx5",
            selected => 0, installed => 0, rpm_exist => 0, rpm_exist32 => 0,
            available => 1, mode => "user", dist_req_build => [],
            dist_req_inst => [], ofa_req_build => ["libibverbs","libibverbs-devel"],
            ofa_req_inst => ["libibverbs","libmlx5"],
            install32 => 1, exception => 0 },
        'libmlx5-debuginfo' =>
            { name => "libmlx5-debuginfo", parent => "libmlx5",
            selected => 0, installed => 0, rpm_exist => 0, rpm_exist32 => 0,
            available => 1, mode => "user", dist_req_build => [],
            dist_req_inst => [], ofa_req_build => ["libibverbs","libibverbs-devel"],
            ofa_req_inst => [],
            install32 => 0, exception => 0 },

        'libehca' =>
            { name => "libehca", parent => "libehca",
            selected => 0, installed => 0, rpm_exist => 0, rpm_exist32 => 0,
            available => 0, mode => "user", dist_req_build => [],
            dist_req_inst => [], ofa_req_build => ["libibverbs-devel"],
            ofa_req_inst => ["libibverbs"],
            install32 => 1, exception => 0, configure_options => '' },
        'libehca-devel-static' =>
            { name => "libehca-devel-static", parent => "libehca",
            selected => 0, installed => 0, rpm_exist => 0, rpm_exist32 => 0,
            available => 0, mode => "user", dist_req_build => [],
            dist_req_inst => [], ofa_req_build => ["libibverbs-devel"],
            ofa_req_inst => ["libibverbs", "libehca"],
            install32 => 1, exception => 0 },
        'libehca-debuginfo' =>
            { name => "libehca-debuginfo", parent => "libehca",
            selected => 0, installed => 0, rpm_exist => 0, rpm_exist32 => 0,
            available => 0, mode => "user", dist_req_build => [],
            dist_req_inst => [], ofa_req_build => [],
            ofa_req_inst => [],
            install32 => 0, exception => 0 },

        'libcxgb3' =>
            { name => "libcxgb3", parent => "libcxgb3",
            selected => 0, installed => 0, rpm_exist => 0, rpm_exist32 => 0,
            available => 1, mode => "user", dist_req_build => [],
            dist_req_inst => [], ofa_req_build => ["libibverbs-devel"],
            ofa_req_inst => ["libibverbs"],
            install32 => 1, exception => 0, configure_options => '' },
        'libcxgb3-devel' =>
            { name => "libcxgb3-devel", parent => "libcxgb3",
            selected => 0, installed => 0, rpm_exist => 0, rpm_exist32 => 0,
            available => 1, mode => "user", dist_req_build => [],
            dist_req_inst => [], ofa_req_build => ["libibverbs","libibverbs-devel"],
            ofa_req_inst => ["libibverbs", "libcxgb3"],
            install32 => 1, exception => 0 },
        'libcxgb3-debuginfo' =>
            { name => "libcxgb3-debuginfo", parent => "libcxgb3",
            selected => 0, installed => 0, rpm_exist => 0, rpm_exist32 => 0,
            available => 1, mode => "user", dist_req_build => [],
            dist_req_inst => [], ofa_req_build => ["libibverbs","libibverbs-devel"],
            ofa_req_inst => [],
            install32 => 0, exception => 0 },

        'libcxgb4' =>
            { name => "libcxgb4", parent => "libcxgb4",
            selected => 0, installed => 0, rpm_exist => 0, rpm_exist32 => 0,
            available => 1, mode => "user", dist_req_build => [],
            dist_req_inst => [], ofa_req_build => ["libibverbs-devel"],
            ofa_req_inst => ["libibverbs"],
            install32 => 1, exception => 0, configure_options => '' },
        'libcxgb4-devel' =>
            { name => "libcxgb4-devel", parent => "libcxgb4",
            selected => 0, installed => 0, rpm_exist => 0, rpm_exist32 => 0,
            available => 1, mode => "user", dist_req_build => [],
            dist_req_inst => [], ofa_req_build => ["libibverbs","libibverbs-devel"],
            ofa_req_inst => ["libibverbs", "libcxgb4"],
            install32 => 1, exception => 0 },
        'libcxgb4-debuginfo' =>
            { name => "libcxgb4-debuginfo", parent => "libcxgb4",
            selected => 0, installed => 0, rpm_exist => 0, rpm_exist32 => 0,
            available => 1, mode => "user", dist_req_build => [],
            dist_req_inst => [], ofa_req_build => ["libibverbs","libibverbs-devel"],
            ofa_req_inst => [],
            install32 => 0, exception => 0 },

        'libnes' =>
            { name => "libnes", parent => "libnes",
            selected => 0, installed => 0, rpm_exist => 0, rpm_exist32 => 0,
            available => 1, mode => "user", dist_req_build => [],
            dist_req_inst => [], ofa_req_build => ["libibverbs-devel"],
            ofa_req_inst => ["libibverbs"],
            install32 => 1, exception => 0, configure_options => '' },
        'libnes-devel-static' =>
            { name => "libnes-devel-static", parent => "libnes",
            selected => 0, installed => 0, rpm_exist => 0, rpm_exist32 => 0,
            available => 1, mode => "user", dist_req_build => [],
            dist_req_inst => [], ofa_req_build => ["libibverbs","libibverbs-devel"],
            ofa_req_inst => ["libibverbs", "libnes"],
            install32 => 1, exception => 0 },
        'libnes-debuginfo' =>
            { name => "libnes-debuginfo", parent => "libnes",
            selected => 0, installed => 0, rpm_exist => 0, rpm_exist32 => 0,
            available => 1, mode => "user", dist_req_build => [],
            dist_req_inst => [], ofa_req_build => ["libibverbs","libibverbs-devel"],
            ofa_req_inst => [],
            install32 => 0, exception => 0 },

        'libipathverbs' =>
            { name => "libipathverbs", parent => "libipathverbs",
            selected => 0, installed => 0, rpm_exist => 0, rpm_exist32 => 0,
            available => 0, mode => "user", dist_req_build => [],
            dist_req_inst => [], ofa_req_build => ["libibverbs-devel"],
            ofa_req_inst => ["libibverbs"],
            install32 => 1, exception => 0, configure_options => '' },
        'libipathverbs-devel' =>
            { name => "libipathverbs-devel", parent => "libipathverbs",
            selected => 0, installed => 0, rpm_exist => 0, rpm_exist32 => 0,
            available => 0, mode => "user", dist_req_build => [],
            dist_req_inst => [], ofa_req_build => ["libibverbs","libibverbs-devel"],
            ofa_req_inst => ["libibverbs", "libipathverbs"],
            install32 => 1, exception => 0 },
        'libipathverbs-debuginfo' =>
            { name => "libipathverbs-debuginfo", parent => "libipathverbs",
            selected => 0, installed => 0, rpm_exist => 0, rpm_exist32 => 0,
            available => 0, mode => "user", dist_req_build => [],
            dist_req_inst => [], ofa_req_build => ["libibverbs","libibverbs-devel"],
            ofa_req_inst => [],
            install32 => 0, exception => 0 },

        'libibcm' =>
            { name => "libibcm", parent => "libibcm",
            selected => 0, installed => 0, rpm_exist => 0, rpm_exist32 => 0,
            available => 1, mode => "user", dist_req_build => [],
            dist_req_inst => [], ofa_req_build => ["libibverbs-devel"],
            ofa_req_inst => ["libibverbs"],
            install32 => 1, exception => 0, configure_options => '' },
        'libibcm-devel' =>
            { name => "libibcm-devel", parent => "libibcm",
            selected => 0, installed => 0, rpm_exist => 0, rpm_exist32 => 0,
            available => 1, mode => "user", dist_req_build => [],
            dist_req_inst => [], ofa_req_build => ["libibverbs-devel"],
            ofa_req_inst => ["libibverbs", "libibverbs-devel", "libibcm"],
            install32 => 1, exception => 0 },
        'libibcm-debuginfo' =>
            { name => "libibcm-debuginfo", parent => "libibcm",
            selected => 0, installed => 0, rpm_exist => 0, rpm_exist32 => 0,
            available => 1, mode => "user", dist_req_build => [],
            dist_req_inst => [], ofa_req_build => ["libibverbs","libibverbs-devel"],
            ofa_req_inst => [],
            install32 => 0, exception => 0 },
        # Management
        'libibumad' =>
            { name => "libibumad", parent => "libibumad",
            selected => 0, installed => 0, rpm_exist => 0, rpm_exist32 => 0,
            available => 1, mode => "user", dist_req_build => ["libtool"],
            dist_req_inst => [],ubuntu_dist_req_build => ["libtool"],ubuntu_dist_req_inst => [], ofa_req_build => [],
            ofa_req_inst => [],
            install32 => 1, exception => 0, configure_options => '' },
        'libibumad-devel' =>
            { name => "libibumad-devel", parent => "libibumad",
            selected => 0, installed => 0, rpm_exist => 0, rpm_exist32 => 0,
            available => 1, mode => "user", dist_req_build => [],
            dist_req_inst => [], ofa_req_build => [],
            ofa_req_inst => ["libibumad"],
            install32 => 1, exception => 0 },
        'libibumad-static' =>
            { name => "libibumad-static", parent => "libibumad",
            selected => 0, installed => 0, rpm_exist => 0, rpm_exist32 => 0,
            available => 1, mode => "user", dist_req_build => [],
            dist_req_inst => [], ofa_req_build => [],
            ofa_req_inst => ["libibumad"],
            install32 => 1, exception => 0 },
        'libibumad-debuginfo' =>
            { name => "libibumad-debuginfo", parent => "libibumad",
            selected => 0, installed => 0, rpm_exist => 0, rpm_exist32 => 0,
            available => 1, mode => "user", dist_req_build => [],
            dist_req_inst => [], ofa_req_build => [],
            ofa_req_inst => [],
            install32 => 0, exception => 0 },

        'libibmad' =>
            { name => "libibmad", parent => "libibmad",
            selected => 0, installed => 0, rpm_exist => 0, rpm_exist32 => 0,
            available => 1, mode => "user", dist_req_build => ["libtool"],
            dist_req_inst => [],ubuntu_dist_req_build => ["libtool"],ubuntu_dist_req_inst => [], ofa_req_build => ["libibumad-devel"],
            ofa_req_inst => ["libibumad"],
            install32 => 1, exception => 0, configure_options => '' },
        'libibmad-devel' =>
            { name => "libibmad-devel", parent => "libibmad",
            selected => 0, installed => 0, rpm_exist => 0, rpm_exist32 => 0,
            available => 1, mode => "user", dist_req_build => [],
            dist_req_inst => [], ofa_req_build => ["libibumad-devel"],
            ofa_req_inst => ["libibmad", "libibumad-devel"],
            install32 => 1, exception => 0 },
        'libibmad-static' =>
            { name => "libibmad-static", parent => "libibmad",
            selected => 0, installed => 0, rpm_exist => 0, rpm_exist32 => 0,
            available => 1, mode => "user", dist_req_build => [],
            dist_req_inst => [], ofa_req_build => ["libibumad-devel"],
            ofa_req_inst => ["libibmad", "libibumad-devel"],
            install32 => 1, exception => 0 },
        'libibmad-debuginfo' =>
            { name => "libibmad-debuginfo", parent => "libibmad",
            selected => 0, installed => 0, rpm_exist => 0, rpm_exist32 => 0,
            available => 1, mode => "user", dist_req_build => [],
            dist_req_inst => [], ofa_req_build => ["libibumad-devel"],
            ofa_req_inst => [],
            install32 => 0, exception => 0 },

        'opensm' =>
            { name => "opensm", parent => "opensm",
            selected => 0, installed => 0, rpm_exist => 0, rpm_exist32 => 0,
            available => 1, mode => "user", dist_req_build => ["bison", "flex"],
            dist_req_inst => [],ubuntu_dist_req_build => ["bison", "flex"],ubuntu_dist_req_inst => [], ofa_req_build => ["libibumad-devel"],
            ofa_req_inst => ["opensm-libs"],
            install32 => 0, exception => 0, configure_options => '' },
        'opensm-devel' =>
            { name => "opensm-devel", parent => "opensm",
            selected => 0, installed => 0, rpm_exist => 0, rpm_exist32 => 0,
            available => 1, mode => "user", dist_req_build => [],
            dist_req_inst => [], ofa_req_build => ["libibumad-devel"],
            ofa_req_inst => ["libibumad-devel", "opensm-libs"],
            install32 => 1, exception => 0 },
        'opensm-libs' =>
            { name => "opensm-libs", parent => "opensm",
            selected => 0, installed => 0, rpm_exist => 0, rpm_exist32 => 0,
            available => 1, mode => "user", dist_req_build => ["bison", "flex"],
            dist_req_inst => [],ubuntu_dist_req_build => ["bison", "flex"],ubuntu_dist_req_inst => [], ofa_req_build => ["libibumad-devel"],
            ofa_req_inst => ["libibumad"],
            install32 => 1, exception => 0 },
        'opensm-static' =>
            { name => "opensm-static", parent => "opensm",
            selected => 0, installed => 0, rpm_exist => 0, rpm_exist32 => 0,
            available => 1, mode => "user", dist_req_build => [],
            dist_req_inst => [], ofa_req_build => ["libibumad-devel"],
            ofa_req_inst => ["libibumad-devel", "opensm-libs"],
            install32 => 1, exception => 0 },
        'opensm-debuginfo' =>
            { name => "opensm-debuginfo", parent => "opensm",
            selected => 0, installed => 0, rpm_exist => 0, rpm_exist32 => 0,
            available => 1, mode => "user", dist_req_build => [],
            dist_req_inst => [], ofa_req_build => ["libibumad-devel"],
            ofa_req_inst => [],
            install32 => 0, exception => 0 },

        'ibsim' =>
            { name => "ibsim", parent => "ibsim",
            selected => 0, installed => 0, rpm_exist => 0, rpm_exist32 => 0,
            available => 1, mode => "user", dist_req_build => [],
            dist_req_inst => [], ofa_req_build => ["libibumad-devel", "libibmad-devel"],
            ofa_req_inst => ["libibumad", "libibmad"],
            install32 => 0, exception => 0, configure_options => '' },
        'ibsim-debuginfo' =>
            { name => "ibsim-debuginfo", parent => "ibsim",
            selected => 0, installed => 0, rpm_exist => 0, rpm_exist32 => 0,
            available => 1, mode => "user", dist_req_build => [],
            dist_req_inst => [], ofa_req_build => ["libibumad-devel", "libibmad-devel"],
            ofa_req_inst => [],
            install32 => 0, exception => 0, configure_options => '' },

        'ibacm' =>
            { name => "ibacm", parent => "ibacm",
            selected => 0, installed => 0, rpm_exist => 0, rpm_exist32 => 0,
            available => 1, mode => "user", dist_req_build => [],
            dist_req_inst => [], ofa_req_build => ["libibverbs-devel", "libibumad-devel"],
            ofa_req_inst => ["libibverbs", "libibumad"],
            install32 => 0, exception => 0, configure_options => '' },
        'librdmacm' =>
            { name => "librdmacm", parent => "librdmacm",
            selected => 0, installed => 0, rpm_exist => 0, rpm_exist32 => 0,
            available => 1, mode => "user", dist_req_build => ["$valgrind_devel"],
            dist_req_inst => [], ofa_req_build => ["libibverbs-devel"],
            ofa_req_inst => ["libibverbs", "libibverbs-devel"],
            install32 => 1, exception => 0, configure_options => '' },
        'librdmacm-devel' =>
            { name => "librdmacm-devel", parent => "librdmacm",
            selected => 0, installed => 0, rpm_exist => 0, rpm_exist32 => 0,
            available => 1, mode => "user", dist_req_build => [],
            dist_req_inst => [], ofa_req_build => ["libibverbs-devel"],
            ofa_req_inst => ["librdmacm", "libibverbs-devel"],
            install32 => 1, exception => 0 },
        'librdmacm-utils' =>
            { name => "librdmacm-utils", parent => "librdmacm",
            selected => 0, installed => 0, rpm_exist => 0, rpm_exist32 => 0,
            available => 1, mode => "user", dist_req_build => [],
            dist_req_inst => [], ofa_req_build => ["libibverbs-devel"],
            ofa_req_inst => ["librdmacm"],
            install32 => 0, exception => 0 },
        'librdmacm-debuginfo' =>
            { name => "librdmacm-debuginfo", parent => "librdmacm",
            selected => 0, installed => 0, rpm_exist => 0, rpm_exist32 => 0,
            available => 1, mode => "user", dist_req_build => [],
            dist_req_inst => [], ofa_req_build => ["libibverbs-devel"],
            ofa_req_inst => [],
            install32 => 0, exception => 0 },

        'libsdp' =>
            { name => "libsdp", parent => "libsdp",
            selected => 0, installed => 0, rpm_exist => 0, rpm_exist32 => 0,
            available => 1, mode => "user", dist_req_build => [],
            dist_req_inst => [], ofa_req_build => [],
            ofa_req_inst => [],
            install32 => 1, exception => 0, configure_options => '' },
        'libsdp-devel' =>
            { name => "libsdp-devel", parent => "libsdp",
            selected => 0, installed => 0, rpm_exist => 0, rpm_exist32 => 0,
            available => 1, mode => "user", dist_req_build => [],
            dist_req_inst => [], ofa_req_build => [],
            ofa_req_inst => ["libsdp"],
            install32 => 1, exception => 0 },
        'libsdp-debuginfo' =>
            { name => "libsdp-debuginfo", parent => "libsdp",
            selected => 0, installed => 0, rpm_exist => 0, rpm_exist32 => 0,
            available => 1, mode => "user", dist_req_build => [],
            dist_req_inst => [], ofa_req_build => [],
            ofa_req_inst => [],
            install32 => 0, exception => 0 },

        'perftest' =>
            { name => "perftest", parent => "perftest",
            selected => 0, installed => 0, rpm_exist => 0, rpm_exist32 => 0,
            available => 1, mode => "user", dist_req_build => [],
            dist_req_inst => [], ofa_req_build => ["libibverbs-devel", "librdmacm-devel", "libibumad-devel"],
            ofa_req_inst => ["libibverbs", "librdmacm", "libibumad"],
            install32 => 0, exception => 0, configure_options => '' },
        'perftest-debuginfo' =>
            { name => "perftest-debuginfo", parent => "perftest",
            selected => 0, installed => 0, rpm_exist => 0, rpm_exist32 => 0,
            available => 1, mode => "user", dist_req_build => [],
            dist_req_inst => [], ofa_req_build => [],
            ofa_req_inst => [],
            install32 => 0, exception => 0 },

        'mft' =>
            { name => "mft", parent => "mft",
            selected => 0, installed => 0, rpm_exist => 0, rpm_exist32 => 0,
            available => 1, mode => "user", dist_req_build => ["expat", "$libexpat_devel", "tcl__8.4", "tcl-devel__8.4", "tk", $libstdc_devel ],
            dist_req_inst => ["expat", "tcl__8.4", "tk", $libstdc],
            ubuntu_dist_req_build => ["libexpat1", "libexpat1-dev", "tcl8.5", "tcl8.5-dev", "tk", "$libstdc_devel" ],
            ubuntu_dist_req_inst => ["expat", "tcl8.5", "tk", "$libstdc"],
            ofa_req_build => ["libibmad-devel"], ofa_req_inst => ["libibmad"], configure_options => '' },
        'mft-debuginfo' =>
            { name => "mft-debuginfo", parent => "mft",
            selected => 0, installed => 0, rpm_exist => 0, rpm_exist32 => 0,
            available => 1, mode => "user", dist_req_build => [],
            dist_req_inst => [], ofa_req_build => [], ofa_req_inst => [], },

        'mstflint' =>
            { name => "mstflint", parent => "mstflint",
            selected => 0, installed => 0, rpm_exist => 0, rpm_exist32 => 0,
            available => 1, mode => "user",
            dist_req_build => ["zlib-devel$suffix_64bit", "$libstdc_devel$suffix_64bit", "gcc-c++"],
            dist_req_inst => [], ofa_req_build => ["libibmad-devel"],
            ubuntu_dist_req_build => ["zlib1g-dev", "$libstdc_devel", "gcc","g++","byacc"],ubuntu_dist_req_inst => [],
            ofa_req_inst => ["libibmad"],
            install32 => 0, exception => 0, configure_options => '' },
        'mstflint-debuginfo' =>
            { name => "mstflint-debuginfo", parent => "mstflint",
            selected => 0, installed => 0, rpm_exist => 0, rpm_exist32 => 0,
            available => 1, mode => "user", dist_req_build => [],
            dist_req_inst => [], ofa_req_build => [],
            ofa_req_inst => [],
            install32 => 0, exception => 0 },

        'ibvexdmtools' =>
            { name => "ibvexdmtools", parent => "qlvnictools",
            selected => 0, installed => 0, rpm_exist => 0, rpm_exist32 => 0,
            available => 0, mode => "user", dist_req_build => [],
            dist_req_inst => [], ofa_req_build => ["libibumad-devel"],
            ofa_req_inst => ["libibumad"],
            install32 => 0, exception => 0, configure_options => '' },
        'qlgc_vnic_daemon' =>
            { name => "qlgc_vnic_daemon", parent => "qlvnictools",
            selected => 0, installed => 0, rpm_exist => 0, rpm_exist32 => 0,
            available => 0, mode => "user", dist_req_build => [],
            dist_req_inst => [], ofa_req_build => [],
            ofa_req_inst => [],
            install32 => 0, exception => 0, configure_options => '' },
        'qlvnictools' =>
            { name => "qlvnictools", parent => "qlvnictools",
            selected => 0, installed => 0, rpm_exist => 0, rpm_exist32 => 0,
            available => 0, mode => "user", dist_req_build => [],
            dist_req_inst => [], ofa_req_build => ["libibumad-devel"],
            ofa_req_inst => ["ibvexdmtools", "qlgc_vnic_daemon", "libibumad"],
            install32 => 0, exception => 0, configure_options => '' },
        'qlvnictools-debuginfo' =>
            { name => "qlvnictools-debuginfo", parent => "qlvnictools",
            selected => 0, installed => 0, rpm_exist => 0, rpm_exist32 => 0,
            available => 0, mode => "user", dist_req_build => [],
            dist_req_inst => [], ofa_req_build => [],
            ofa_req_inst => [],
            install32 => 0, exception => 0 },

        'sdpnetstat' =>
            { name => "sdpnetstat", parent => "sdpnetstat",
            selected => 0, installed => 0, rpm_exist => 0, rpm_exist32 => 0,
            available => 1, mode => "user", dist_req_build => [],
            dist_req_inst => [], ofa_req_build => [],
            ofa_req_inst => [],
            install32 => 0, exception => 0, configure_options => '' },
        'sdpnetstat-debuginfo' =>
            { name => "sdpnetstat-debuginfo", parent => "sdpnetstat",
            selected => 0, installed => 0, rpm_exist => 0, rpm_exist32 => 0,
            available => 1, mode => "user", dist_req_build => [],
            dist_req_inst => [], ofa_req_build => [],
            ofa_req_inst => [],
            install32 => 0, exception => 0 },

        'srptools' =>
            { name => "srptools", parent => "srptools",
            selected => 0, installed => 0, rpm_exist => 0, rpm_exist32 => 0,
            available => 1, mode => "user", dist_req_build => [],
            dist_req_inst => [], ofa_req_build => ["libibumad-devel", "libibverbs-devel"],
            ofa_req_inst => ["libibumad", "libibverbs"],
            install32 => 0, exception => 0, configure_options => '' },
        'srptools-debuginfo' =>
            { name => "srptools-debuginfo", parent => "srptools",
            selected => 0, installed => 0, rpm_exist => 0, rpm_exist32 => 0,
            available => 1, mode => "user", dist_req_build => [],
            dist_req_inst => [], ofa_req_build => ["libibumad-devel"],
            ofa_req_inst => [],
            install32 => 0, exception => 0 },

        'dcbx' =>
            { name => "dcbx", parent => "dcbx",
            selected => 0, installed => 0, rpm_exist => 0, rpm_exist32 => 0,
            available => 0, mode => "user", dist_req_build => [],
            dist_req_inst => [], ofa_req_build => [],
            ofa_req_inst => [],
            install32 => 0, exception => 0, configure_options => '' },
        'dcbx-debuginfo' =>
            { name => "dcbx-debuginfo", parent => "dcbx",
            selected => 0, installed => 0, rpm_exist => 0, rpm_exist32 => 0,
            available => 0, mode => "user", dist_req_build => [],
            dist_req_inst => [], ofa_req_build => [],
            ofa_req_inst => [],
            install32 => 0, exception => 0 },

        'rnfs-utils' =>
            { name => "rnfs-utils", parent => "rnfs-utils",
            selected => 0, installed => 0, rpm_exist => 0, rpm_exist32 => 0,
            available => 0, mode => "user", dist_req_build => [],
            dist_req_inst => [], ofa_req_build => [],
            ofa_req_inst => [],
            install32 => 0, exception => 0, configure_options => '' },
        'rnfs-utils-debuginfo' =>
            { name => "rnfs-utils-debuginfo", parent => "rnfs-utils",
            selected => 0, installed => 0, rpm_exist => 0, rpm_exist32 => 0,
            available => 0, mode => "user", dist_req_build => [],
            dist_req_inst => [], ofa_req_build => [],
            ofa_req_inst => [],
            install32 => 0, exception => 0 },

        'rds-tools' =>
            { name => "rds-tools", parent => "rds-tools",
            selected => 0, installed => 0, rpm_exist => 0, rpm_exist32 => 0,
            available => 1, mode => "user", dist_req_build => [],
            dist_req_inst => [], ofa_req_build => [],
            ofa_req_inst => [],
            install32 => 0, exception => 0, configure_options => '' },
        'rds-devel' =>
            { name => "rds-devel", parent => "rds-tools",
            selected => 0, installed => 0, rpm_exist => 0, rpm_exist32 => 0,
            available => 1, mode => "user", dist_req_build => [],
            dist_req_inst => [], ofa_req_build => [],
            ofa_req_inst => ["rds-tools"],
            install32 => 0, exception => 0, configure_options => '' },
        'rds-tools-debuginfo' =>
            { name => "rds-tools-debuginfo", parent => "rds-tools",
            selected => 0, installed => 0, rpm_exist => 0, rpm_exist32 => 0,
            available => 1, mode => "user", dist_req_build => [],
            dist_req_inst => [], ofa_req_build => [],
            ofa_req_inst => [],
            install32 => 0, exception => 0 },

        'qperf' =>
            { name => "qperf", parent => "qperf",
            selected => 0, installed => 0, rpm_exist => 0, rpm_exist32 => 0,
            available => 1, mode => "user", dist_req_build => [],
            dist_req_inst => [], ofa_req_build => ["libibverbs-devel", "librdmacm-devel"],
            ofa_req_inst => ["libibverbs", "librdmacm"],
            install32 => 0, exception => 0, configure_options => '' },
        'qperf-debuginfo' =>
            { name => "qperf-debuginfo", parent => "qperf",
            selected => 0, installed => 0, rpm_exist => 0, rpm_exist32 => 0,
            available => 1, mode => "user", dist_req_build => [],
            dist_req_inst => [], ofa_req_build => [],
            ofa_req_inst => [],
            install32 => 0, exception => 0 },

        'ibutils' =>
            { name => "ibutils", parent => "ibutils",
            selected => 0, installed => 0, rpm_exist => 0, rpm_exist32 => 0,
            available => 1, mode => "user", dist_req_build => ["tcl__8.4", "tcl-devel__8.4", "tk", "$libstdc_devel"],
            dist_req_inst => ["tcl__8.4", "tk", "$libstdc"], ofa_req_build => ["libibverbs-devel", "opensm-libs", "opensm-devel"],
            ofa_req_inst => ["libibumad", "opensm-libs"],
            ubuntu_dist_req_build => ["tcl8.5", "tcl8.5-dev", "tk", "$libstdc_devel"],
            ubuntu_dist_req_inst => ["tcl8.5", "tk", "$libstdc"],
            install32 => 0, exception => 0, configure_options => '' },
        'ibutils-debuginfo' =>
            { name => "ibutils-debuginfo", parent => "ibutils",
            selected => 0, installed => 0, rpm_exist => 0, rpm_exist32 => 0,
            available => 0, mode => "user", dist_req_build => [],
            dist_req_inst => [], ofa_req_build => [],
            ofa_req_inst => [],
            install32 => 0, exception => 0 },

        'ibutils2' =>
            { name => "ibutils2", parent => "ibutils2",
            selected => 0, installed => 0, rpm_exist => 0, rpm_exist32 => 0, internal => 1,
            available => 1, mode => "user", dist_req_build => ["tcl__8.4", "tcl-devel__8.4", "tk", "$libstdc_devel"],
            dist_req_inst => ["tcl__8.4", "tk", "$libstdc"], ofa_req_build => ["libibumad-devel"],
            ofa_req_inst => ["libibumad"],
            ubuntu_dist_req_build => ["tcl8.5", "tcl8.5-dev", "tk", "$libstdc_devel"],
            ubuntu_dist_req_inst => ["tcl8.5", "tk", "$libstdc"],
            install32 => 0, exception => 0, configure_options => '' },
        'ibutils2-debuginfo' =>
            { name => "ibutils2-debuginfo", parent => "ibutils2",
            selected => 0, installed => 0, rpm_exist => 0, rpm_exist32 => 0, internal => 1,
            available => 1, mode => "user", dist_req_build => [],
            dist_req_inst => [], ofa_req_build => [],
            ofa_req_inst => [],
            install32 => 0, exception => 0 },

        'ar_mgr' =>
            { name => "ar_mgr", parent => "ar_mgr",
            selected => 0, installed => 0, rpm_exist => 0, rpm_exist32 => 0, internal => 1,
            available => 1, mode => "user", dist_req_build => ["$libstdc_devel"],
            dist_req_inst => ["$libstdc"], ofa_req_build => ["opensm-libs", "opensm-devel", "ibutils2"],
            ofa_req_inst => ["opensm", "ibutils2"],
            ubuntu_dist_req_build => ["$libstdc_devel"],
            ubuntu_dist_req_inst => ["$libstdc"],
            install32 => 0, exception => 0, configure_options => '' },

        'cc_mgr' =>
            { name => "cc_mgr", parent => "cc_mgr",
            selected => 0, installed => 0, rpm_exist => 0, rpm_exist32 => 0, internal => 1,
            available => 1, mode => "user", dist_req_build => ["$libstdc_devel"],
            dist_req_inst => ["$libstdc"], ofa_req_build => ["opensm-libs", "opensm-devel", "ibutils2"],
            ofa_req_inst => ["opensm", "ibutils2"],
            ubuntu_dist_req_build => ["$libstdc_devel"],
            ubuntu_dist_req_inst => ["$libstdc"],
            install32 => 0, exception => 0, configure_options => '' },

        'dump_pr' =>
            { name => "dump_pr", parent => "dump_pr",
            selected => 0, installed => 0, rpm_exist => 0, rpm_exist32 => 0, internal => 1,
            available => 1, mode => "user", dist_req_build => ["$libstdc_devel"],
            dist_req_inst => ["$libstdc"], ofa_req_build => ["opensm-libs", "opensm-devel"],
            ofa_req_inst => ["opensm"],
            ubuntu_dist_req_build => ["$libstdc_devel"],
            ubuntu_dist_req_inst => ["$libstdc"],
            install32 => 0, exception => 0, configure_options => '' },

        'ibdump' =>
            { name => "ibdump", parent => "ibdump",
            selected => 0, installed => 0, rpm_exist => 0, rpm_exist32 => 0, internal => 1,
            available => 1, mode => "user", dist_req_build => ["$libstdc_devel"],
            dist_req_inst => ["$libstdc"], ofa_req_build => ["libibverbs-devel", "mstflint"],
            ofa_req_inst => ["libibverbs", "mstflint"],
            ubuntu_dist_req_build => ["$libstdc_devel"],
            ubuntu_dist_req_inst => ["$libstdc"],
            install32 => 0, exception => 0, configure_options => '' },
        'ibdump-debuginfo' =>
            { name => "ibdump-debuginfo", parent => "ibdump",
            selected => 0, installed => 0, rpm_exist => 0, rpm_exist32 => 0, internal => 1,
            available => 1, mode => "user", dist_req_build => [],
            dist_req_inst => [], ofa_req_build => [],
            ofa_req_inst => [],
            install32 => 0, exception => 0 },

        'infiniband-diags' =>
            { name => "infiniband-diags", parent => "infiniband-diags",
            selected => 0, installed => 0, rpm_exist => 0, rpm_exist32 => 0,
            available => 1, mode => "user", dist_req_build => ["glib2-devel"],
            dist_req_inst => ["$glib2"], ofa_req_build => ["opensm-devel", "libibmad-devel", "libibumad-devel"],
            ofa_req_inst => ["libibumad", "libibmad", "opensm-libs"],
            install32 => 0, exception => 0, configure_options => '' },
        'infiniband-diags-guest' =>
            { name => "infiniband-diags-guest", parent => "infiniband-diags",
            selected => 0, installed => 0, rpm_exist => 0, rpm_exist32 => 0,
            available => 1, mode => "user", dist_req_build => ["glib2-devel"],
            dist_req_inst => ["$glib2"], ofa_req_build => ["opensm-devel"],
            ofa_req_inst => ["infiniband-diags","opensm-libs"],
            install32 => 0, exception => 0 },
        'infiniband-diags-compat' =>
            { name => "infiniband-diags-compat", parent => "infiniband-diags",
            selected => 0, installed => 0, rpm_exist => 0, rpm_exist32 => 0,
            available => 1, mode => "user", dist_req_build => ["glib2-devel"],
            dist_req_inst => ["$glib2"], ofa_req_build => ["opensm-devel"],
            ofa_req_inst => ["infiniband-diags","opensm-libs"],
            install32 => 0, exception => 0 },
        'infiniband-diags-debuginfo' =>
            { name => "infiniband-diags-debuginfo", parent => "infiniband-diags",
            selected => 0, installed => 0, rpm_exist => 0, rpm_exist32 => 0,
            available => 1, mode => "user", dist_req_build => ["glib2-devel"],
            dist_req_inst => ["$glib2"], ofa_req_build => [],
            ofa_req_inst => [],
            install32 => 0, exception => 0 },

        'dapl' =>
            { name => "dapl", parent => "dapl",
            selected => 0, installed => 0, rpm_exist => 0, rpm_exist32 => 0,
            available => 1, mode => "user", dist_req_build => [],
            dist_req_inst => [], ofa_req_build => ["libibverbs", "libibverbs-devel", "librdmacm", "librdmacm-devel", "libibmad-devel"],
            ofa_req_inst => ["libibverbs", "librdmacm"],
            install32 => 0, exception => 0, configure_options => '' },
        'dapl-devel' =>
            { name => "dapl-devel", parent => "dapl",
            selected => 0, installed => 0, rpm_exist => 0, rpm_exist32 => 0,
            available => 1, mode => "user", dist_req_build => [],
            dist_req_inst => [], ofa_req_build => ["libibverbs","libibverbs-devel", "librdmacm", "librdmacm-devel"],
            ofa_req_inst => ["dapl"],
            install32 => 0, exception => 0, configure_options => '' },
        'dapl-devel-static' =>
            { name => "dapl-devel-static", parent => "dapl",
            selected => 0, installed => 0, rpm_exist => 0, rpm_exist32 => 0,
            available => 1, mode => "user", dist_req_build => [],
            dist_req_inst => [], ofa_req_build => ["libibverbs","libibverbs-devel", "librdmacm", "librdmacm-devel"],
            ofa_req_inst => ["dapl"],
            install32 => 0, exception => 0, configure_options => '' },
        'dapl-utils' =>
            { name => "dapl-utils", parent => "dapl",
            selected => 0, installed => 0, rpm_exist => 0, rpm_exist32 => 0,
            available => 1, mode => "user", dist_req_build => [],
            dist_req_inst => [], ofa_req_build => ["libibverbs","libibverbs-devel", "librdmacm", "librdmacm-devel"],
            ofa_req_inst => ["dapl"],
            install32 => 0, exception => 0, configure_options => '' },
        'dapl-debuginfo' =>
            { name => "dapl-debuginfo", parent => "dapl",
            selected => 0, installed => 0, rpm_exist => 0, rpm_exist32 => 0,
            available => 1, mode => "user", dist_req_build => [],
            dist_req_inst => [], ofa_req_build => ["libibverbs","libibverbs-devel", "librdmacm", "librdmacm-devel"],
            ofa_req_inst => [],
            install32 => 0, exception => 0 },

        'mxm' =>
            { name => "mxm", parent => "mxm",
            selected => 0, installed => 0, rpm_exist => 0, rpm_exist32 => 0,
            available => 1, mode => "user", dist_req_build => ["$libstdc_devel","gcc-c++","binutils-devel","$libstdc","$valgrind_devel"],
            dist_req_inst => ["$libstdc", "$cairo", "$atk", "$gtk2"], ofa_req_build => ["libibverbs-devel","librdmacm-devel","libibmad-devel","libibumad-devel","$knem_rpm"],
            ofa_req_inst => ["libibumad", "libibverbs"],
            ubuntu_dist_req_build => ["$libstdc_devel"],
            ubuntu_dist_req_inst => ["$libstdc"],
            install32 => 0, exception => 0, configure_options => '' },

        'mpi-selector' =>
            { name => "mpi-selector", parent => "mpi-selector",
            selected => 0, installed => 0, rpm_exist => 0, rpm_exist32 => 0,
            available => 1, mode => "user", dist_req_build => ["tcsh"],
            dist_req_inst => ["tcsh"], ofa_req_build => [],
            ofa_req_inst => [],
            ubuntu_dist_req_build => ["tcsh"],ubuntu_dist_req_inst => ["tcsh"],
            install32 => 0, exception => 0, configure_options => '' },

        'mvapich' =>
            { name => "mvapich", parent => "mvapich",
            selected => 0, installed => 0, rpm_exist => 0, rpm_exist32 => 0,
            available => 0, mode => "user", dist_req_build => ["$libgfortran","$libstdc_devel"],
            dist_req_inst => ["$libstdc"], ofa_req_build => ["libibumad-devel", "libibverbs-devel"],
            ofa_req_inst => ["mpi-selector", "libibverbs", "libibumad"],
            ubuntu_dist_req_build => ["$libstdc_devel"],
            ubuntu_dist_req_inst => [""],
            install32 => 0, exception => 0, configure_options => '' },

        'mvapich2' =>
            { name => "mvapich2", parent => "mvapich2",
            selected => 0, installed => 0, rpm_exist => 0, rpm_exist32 => 0,
            available => 1, mode => "user", dist_req_build => ["$libgfortran", $sysfsutils_devel, "$libstdc_devel"],
            dist_req_inst => [$sysfsutils, "$libstdc"], ofa_req_build => ["libibumad-devel", "libibverbs-devel"],
            ofa_req_inst => ["mpi-selector", "librdmacm", "libibumad", "libibumad-devel"],
            install32 => 0, exception => 0, configure_options => '' },

        'openmpi' =>
            { name => "openmpi", parent => "openmpi",
            selected => 0, installed => 0, rpm_exist => 0, rpm_exist32 => 0,
            available => 1, mode => "user", dist_req_build => ["$libgfortran","$libstdc_devel"],
            dist_req_inst => ["$libstdc"], ofa_req_build => ($arch eq "x86_64")?["libibverbs-devel", "librdmacm-devel", "$fca", "hcoll","mxm", "$knem_rpm", "libibmad-devel"]:["libibverbs-devel", "librdmacm-devel", "libibmad-devel"],
            ofa_req_inst => ($arch eq "x86_64")?["libibverbs", "librdmacm-devel", "mpi-selector", "$fca", "hcoll","mxm", "$knem_rpm"]:["libibverbs", "librdmacm-devel", "mpi-selector"],
            ubuntu_dist_req_build => ["$libgfortran","$libstdc_devel"],
            ubuntu_dist_req_inst => ["$libstdc"],
            install32 => 0, exception => 0, configure_options => '' },

# Note: this node of 'mpitests' is a generic node, it should stay available=0.
# the real mpitests packages that will be installed are defined in function "add_new_mpitests_pkg".
# mpitests will be compiled with all available openmpi/mvapich versions.
        'mpitests' =>
            { name => "mpitests", parent => "mpitests",
            selected => 0, installed => 0, rpm_exist => 0, rpm_exist32 => 0,
            available => 0, mode => "user", dist_req_build => ["$fortran"],
            dist_req_inst => [], ofa_req_build => ["libibumad-devel", "librdmacm-devel", "libibmad-devel"],
            ofa_req_inst => [],
            install32 => 0, exception => 0, configure_options => '' },

        'ofed-docs' =>
            { name => "ofed-docs", parent => "ofed-docs",
            selected => 0, installed => 0, rpm_exist => 0, rpm_exist32 => 0,
            available => 1, mode => "user", dist_req_build => [],
            dist_req_inst => [], ofa_req_build => [],
            ofa_req_inst => [],
            install32 => 0, exception => 0 },

        'ofed-scripts' =>
            { name => "ofed-scripts", parent => "ofed-scripts",
            selected => 0, installed => 0, rpm_exist => 0, rpm_exist32 => 0,
            available => 1, mode => "user", dist_req_build => ["python-devel"],
            dist_req_inst => [], ofa_req_build => [],
            ofa_req_inst => [],
            install32 => 0, exception => 0 },
        'infinipath-psm' =>
            { name => "infinipath-psm", parent=> "infinipath-psm",
             selected => 0, installed => 0, rpm_exits => 0, rpm_exists32 => 0,
             available => 0, mode => "user", dist_req_build => [],
             dist_req_inst => [], ofa_req_build => [],
             ofa_req_inst => [], install32 => 0, exception => 0 },
        'infinipath-psm-devel' =>
            { name => "infinipath-psm-devel", parent=> "infinipath-psm",
             selected => 0, installed => 0, rpm_exits => 0, rpm_exists32 => 0,
             available => 0, mode => "user", dist_req_build => [],
             dist_req_inst => [], ofa_req_build => [],
             ofa_req_inst => ["infinipath-psm"], install32 => 0, exception => 0 },
        'openshmem' =>
            { name => "openshmem", parent=> "openshmem",
             selected => 0, installed => 0, rpm_exits => 0, rpm_exists32 => 0,
             available => 1, mode => "user", dist_req_build => [],
             dist_req_inst => [], ofa_req_build => ["$fca","opensm-devel","$knem_rpm","mxm", "libibmad-devel", "librdmacm-devel"],
             ofa_req_inst => ["$fca","mxm","opensm-libs","$knem_rpm"], install32 => 0, exception => 0 },
        'fca' =>
            { name => "fca", parent=> "fca",
             selected => 0, installed => 0, rpm_exits => 0, rpm_exists32 => 0,
             available => 1, mode => "user", dist_req_build => ["gcc-c++","$libstdc_devel","$curl_devel"],
             dist_req_inst => [],
             dist_req_build => ["gcc-c++","$libstdc_devel","$curl_devel"],
             dist_req_inst => [],
             ofa_req_build => ["librdmacm-devel","libibverbs-devel","libibmad-devel","libibumad-devel","opensm-devel",$infiniband_diags],
             ofa_req_inst => [$infiniband_diags,"librdmacm"], install32 => 0, exception => 0 },
        'bupc' =>
             { name => "bupc", parent=> "bupc",
              selected => 0, installed => 0, rpm_exits => 0, rpm_exists32 => 0,
              available => 1, mode => "user", dist_req_build => ["gcc-c++","$libstdc_devel"],
              dist_req_inst => ["$libstdc",], ofa_req_build => ["libibverbs-devel","$fca","mxm","openmpi"],
              ofa_req_inst => ["libibverbs","$fca","mxm","openmpi"], install32 => 0, exception => 0 },
        'libibprof' =>
            { name => "libibprof", parent=> "libibprof",
             selected => 0, installed => 0, rpm_exits => 0, rpm_exists32 => 0,
             available => 1, mode => "user",
             dist_req_inst => [],
             dist_req_build => ["gcc-c++","$libstdc_devel","$libstdc"],
             dist_req_inst => [],
             ofa_req_build => ["libibverbs-devel", "hcoll", "mxm"],
             ofa_req_inst => ["libibverbs", "hcoll", "mxm"], install32 => 0, exception => 0 },
        'hcoll' =>
            { name => "hcoll", parent=> "hcoll",
             selected => 0, installed => 0, rpm_exits => 0, rpm_exists32 => 0,
             available => 1, mode => "user",
             dist_req_inst => [],
             dist_req_build => ["gcc-c++","$libstdc_devel","$libstdc"],
             dist_req_inst => [],
             ofa_req_build => ["librdmacm-devel","libibverbs-devel","libibmad-devel","libibumad-devel"],
             ofa_req_inst => ["libibverbs", "librdmacm", "libibmad", "libibumad"], install32 => 0, exception => 0 },

        'libvma' =>
            { name => "libvma", parent=> "libvma",
             selected => 0, installed => 0, rpm_exits => 0, rpm_exists32 => 0,
             available => 1, mode => "user",
             dist_req_inst => [],
             dist_req_build => ["$libnl_devel"],
             dist_req_inst => ["$libnl"],
             ofa_req_build => ["librdmacm-devel","libibverbs-devel","libmlx4-devel"],
             ofa_req_inst => ["libibverbs", "librdmacm", "libmlx4"], install32 => 0, exception => 0 },
        'libvma-debuginfo' =>
            { name => "libvma-debuginfo", parent=> "libvma",
             selected => 0, installed => 0, rpm_exits => 0, rpm_exists32 => 0,
             available => 1, mode => "user",
             dist_req_inst => [],
             dist_req_build => ["$libnl_devel"],
             dist_req_inst => ["$libnl"],
             ofa_req_build => ["librdmacm-devel","libibverbs-devel","libmlx4-devel"],
             ofa_req_inst => ["libibverbs", "librdmacm", "libmlx4"], install32 => 0, exception => 0 },

        'fabric-collector' =>
            { name => "fabric-collector", parent=> "fabric-collector",
             selected => 0, installed => 0, rpm_exits => 0, rpm_exists32 => 0,
             available => 1, mode => "user",
             dist_req_inst => ["$libcurl"],
             dist_req_build => ( $build32 == 1 )?["glibc-devel$suffix_64bit","glibc-devel$suffix_32bit", "$curl_devel", "$openssl_devel"]:["glibc-devel$suffix_64bit", "$curl_devel", "$openssl_devel"],
             ofa_req_build => ["libibmad-devel", "libibumad-devel", "opensm-devel"],
             ofa_req_inst => ["libibmad", "libibumad", "opensm-libs"], install32 => 0, exception => 0 },
        'fabric-collector-debuginfo' =>
            { name => "fabric-collector-debuginfo", parent=> "fabric-collector",
             selected => 0, installed => 0, rpm_exits => 0, rpm_exists32 => 0,
             available => 1, mode => "user",
             dist_req_inst => ["$libcurl"],
             dist_req_build => ( $build32 == 1 )?["glibc-devel$suffix_64bit","glibc-devel$suffix_32bit", "$curl_devel", "$openssl_devel"]:["glibc-devel$suffix_64bit", "$curl_devel", "$openssl_devel"],
             ofa_req_build => ["libibmad-devel", "libibumad-devel", "opensm-devel"],
             ofa_req_inst => ["libibmad", "libibumad", "opensm-libs"], install32 => 0, exception => 0 },

        'iser' =>
            { name => "iser", parent => "iser",
            selected => 0, installed => 0, rpm_exist => 0, rpm_exist32 => 0,
            available => 1, mode => "kernel", dist_req_build => ["make", "gcc"],
            dist_req_inst => ["pciutils","python"],
            ofa_req_build => ["$kernel_rpm-devel"],
            ofa_req_inst => ["ofed-scripts","$kernel_rpm"], configure_options => '' },

        'srp' =>
            { name => "srp", parent => "srp",
            available => 1, mode => "kernel", dist_req_build => ["make", "gcc"],
            dist_req_inst => ["pciutils","python"],
            ofa_req_build => ["$kernel_rpm-devel"],
            ofa_req_inst => ["ofed-scripts","$kernel_rpm"], configure_options => '' },
);


my @hidden_packages = ();

my %MPI_SUPPORTED_COMPILERS = (gcc => 0, pgi => 0, intel => 0, pathscale => 0);

my %gcc = ('gcc' => 0, 'gfortran' => 0, 'g77' => 0, 'g++' => 0);
my %pathscale = ('pathcc' => 0, 'pathCC' => 0, 'pathf90' => 0);
my %pgi = ('pgf77' => 0, 'pgf90' => 0, 'pgCC' => 0);
my %intel = ('icc' => 0, 'icpc' => 0, 'ifort' => 0);

# mvapich2 environment
my $mvapich2_conf_impl = "ofa";
my $mvapich2_conf_romio = 1;
my $mvapich2_conf_shared_libs = 1;
my $mvapich2_conf_ckpt = 0;
my $mvapich2_conf_blcr_home;
my $mvapich2_conf_vcluster = "small";
my $mvapich2_conf_io_bus;
my $mvapich2_conf_link_speed;
my $mvapich2_conf_dapl_provider = "";
my $mvapich2_comp_env;
my $mvapich2_dat_lib;
my $mvapich2_dat_include;
my $mvapich2_conf_done = 0;

my $TOPDIR = $builddir . '/' . $PACKAGE . "_topdir";
chomp $TOPDIR;

rmtree ("$TOPDIR");
mkpath([$TOPDIR . '/BUILD' ,$TOPDIR . '/RPMS',$TOPDIR . '/SOURCES',$TOPDIR . '/SPECS',$TOPDIR . '/SRPMS']);

if ($config_given and $install_option) {
    print RED "\nError: '-c' option can't be used with '--all|--hpc|--vma|--basic'", RESET "\n";
    exit 1;
}

if ($config_given and not -e $config) {
    print RED "$config does not exist", RESET "\n";
    exit 1;
}

if (not $config_given and -e $config) {
    move($config, "$config.save");
}

if ($quiet) {
    $verbose = 0;
    $verbose2 = 0;
    $verbose3 = 0;
}

my %ifcfg = ();
if ($config_net_given and not -e $config_net) {
    print RED "$config_net does not exist", RESET "\n";
    exit 1;
}

my $eth_dev;
if ($config_net_given) {
    open(NET, "$config_net") or die "Can't open $config_net: $!";
    while (<NET>) {
        my ($param, $value) = split('=');
        chomp $param;
        chomp $value;
        my $dev = $param;
        $dev =~ s/(.*)_(ib[0-9]+)/$2/;
        chomp $dev;

        if ($param =~ m/IPADDR/) {
            $ifcfg{$dev}{'IPADDR'} = $value;
        }
        elsif ($param =~ m/NETMASK/) {
            $ifcfg{$dev}{'NETMASK'} = $value;
        }
        elsif ($param =~ m/NETWORK/) {
            $ifcfg{$dev}{'NETWORK'} = $value;
        }
        elsif ($param =~ m/BROADCAST/) {
            $ifcfg{$dev}{'BROADCAST'} = $value;
        }
        elsif ($param =~ m/ONBOOT/) {
            $ifcfg{$dev}{'ONBOOT'} = $value;
        }
        elsif ($param =~ m/LAN_INTERFACE/) {
            $ifcfg{$dev}{'LAN_INTERFACE'} = $value;
        }
        else {
            print RED "Unsupported parameter '$param' in $config_net\n" if ($verbose2);
        }
    }
    close(NET);
}

sub sig_handler
{
    exit $ERROR;
}

sub getch
{
        my $c;
        system("stty -echo raw");
        $c=getc(STDIN);
        system("stty echo -raw");
        # Exit on Ctrl+c or Esc
        if ($c eq "\cC" or $c eq "\e") {
            print "\n";
            exit $ERROR;
        }
        print "$c\n";
        return $c;
}

sub get_rpm_name_arch
{
    my $ret = `rpm --queryformat "[%{NAME}] [%{ARCH}]" -qp @_ | grep -v Freeing`;
    chomp $ret;
    return $ret;
}

sub get_rpm_ver
{
    my $ret = `rpm --queryformat "[%{VERSION}]\n" -qp @_ | uniq`;
    chomp $ret;
    return $ret;
}

sub get_rpm_rel
{
    my $ret = `rpm --queryformat "[%{RELEASE}]\n" -qp @_ | uniq`;
    chomp $ret;
    return $ret;
}

# Get RPM name and version of the INSTALLED package
sub get_rpm_ver_inst
{
    my $ret;
    if ($DISTRO =~ /DEBIAN|UBUNTU/) {
        $ret = `dpkg-query -W -f='\${Version}\n' @_ | cut -d ':' -f 2 | uniq`;
    }
    else {
        $ret = `rpm --queryformat '[%{VERSION}]\n' -q @_ | uniq`;
    }
    chomp $ret;
    return $ret;
}

sub get_rpm_rel_inst
{
    my $ret = `rpm --queryformat "[%{RELEASE}]\n" -q @_ | uniq`;
    chomp $ret;
    return $ret;
}

sub get_rpm_info
{
    my $ret = `rpm --queryformat "[%{NAME}] [%{VERSION}] [%{RELEASE}] [%{DESCRIPTION}]" -qp @_`;
    chomp $ret;
    return $ret;
}

sub supported32bit
{
    if ($arch =~ /i[0-9]86|ia64|ppc/) {
        return 0;
    }
    return 1
}

# Check whether compiler $1 exist
sub set_compilers
{
    if (`which gcc 2> /dev/null`) {
        $gcc{'gcc'} = 1;
    }
    if (`which g77 2> /dev/null`) {
        $gcc{'g77'} = 1;
    }
    if (`which g++ 2> /dev/null`) {
        $gcc{'g++'} = 1;
    }
    if (`which gfortran 2> /dev/null`) {
        $gcc{'gfortran'} = 1;
    }

    if (`which pathcc 2> /dev/null`) {
        $pathscale{'pathcc'} = 1;
    }
    if (`which pathCC 2> /dev/null`) {
        $pathscale{'pathCC'} = 1;
    }
    if (`which pathf90 2> /dev/null`) {
        $pathscale{'pathf90'} = 1;
    }

    if (`which pgcc 2> /dev/null`) {
        $pgi{'pgcc'} = 1;
    }
    if (`which pgCC 2> /dev/null`) {
        $pgi{'pgCC'} = 1;
    }
    if (`which pgf77 2> /dev/null`) {
        $pgi{'pgf77'} = 1;
    }
    if (`which pgf90 2> /dev/null`) {
        $pgi{'pgf90'} = 1;
    }

    if (`which icc 2> /dev/null`) {
        $intel{'icc'} = 1;
    }
    if (`which icpc 2> /dev/null`) {
        $intel{'icpc'} = 1;
    }
    if (`which ifort 2> /dev/null`) {
        $intel{'ifort'} = 1;
    }
}

sub set_cfg
{
    my $srpm_full_path = shift @_;

    my $info = get_rpm_info($srpm_full_path);
    my $name = (split(/ /,$info,4))[0];
    my $version = (split(/ /,$info,4))[1];

    ( $main_packages{$name}{$version}{'name'},
      $main_packages{$name}{$version}{'version'},
      $main_packages{$name}{$version}{'release'},
      $main_packages{$name}{$version}{'description'} ) = split(/ /,$info,4);
      $main_packages{$name}{$version}{'srpmpath'}   = $srpm_full_path;

    print "set_cfg: " .
             "name: $name, " .
             "original name: $main_packages{$name}{$version}{'name'}, " .
             "version: $main_packages{$name}{$version}{'version'}, " .
             "release: $main_packages{$name}{$version}{'release'}, " .
             "srpmpath: $main_packages{$name}{$version}{'srpmpath'}\n" if ($verbose3);

    # mpitests needs to be compiled with all available openmpi/mvapich versions.
    if ($name =~ /openmpi|mvapich/) {
        add_new_mpitests_pkg($name, $version);
    }
}

sub add_new_mpitests_pkg
{
    my $name = shift @_;
    my $version = shift @_;

    # clone mpitests node
    my $dlver = $version;
    $dlver =~ s/\./_/g;
    my $newName = "mpitests_$name\_\_$dlver";
    clone_pkg_info_node("mpitests", $newName);
    # enable it
    $packages_info{$newName}{'available'} = 1;
    # add requirements
    push(@{$packages_info{$newName}{'ofa_req_build'}}, $name);
    push(@{$packages_info{$newName}{'ofa_req_inst'}}, $name);
    # add it to relevant packages groups
    push(@user_packages, $newName);
    push(@all_packages, $newName);
    push(@hpc_user_packages, $newName);
    push(@guest_user_packages, $newName);
    push(@mpi_packages, $newName);
}

sub clone_pkg_info_node
{
    my $name = shift @_;
    my $new_name = shift @_;

    for my $key (keys %{$packages_info{$name}}) {
        $packages_info{$new_name}{$key} = $packages_info{$name}{$key};
    }
}

# set a given property to a given value for all packages with name matching the given name.
sub set_property_for_packages_like
{
    my $name = shift @_;
    my $property = shift @_;
    my $value = shift @_;

    for my $key (keys %packages_info) {
        if ($key =~ /$name/) {
            $packages_info{$key}{$property} = $value;
        }
    }
}

sub disable_package
{
    my $key = shift;

    if (exists $packages_info{$key}) {
        $packages_info{$key}{'available'} = 0;
        for my $requester (@{$packages_deps{$key}{'required_by'}}) {
            disable_package($requester);
        }
    }
    # modules
    if (exists $kernel_modules_info{$key}) {
        $kernel_modules_info{$key}{'available'} = 0;
        for my $requester (@{$modules_deps{$key}{'required_by'}}) {
            disable_package($requester);
        }
    }
}

sub enable_package
{
    my $key = shift;

    if (exists $packages_info{$key}) {
        $packages_info{$key}{'available'} = 1;
        for my $req ( @{ $packages_info{$key}{'ofa_req_inst'} } ) {
            enable_package($req);
        }
    }
}

sub enable_module
{
    my $key = shift;

    if (exists $kernel_modules_info{$key}) {
        $kernel_modules_info{$key}{'available'} = 1;
        for my $req ( @{ $kernel_modules_info{$key}{'requires'} } ) {
            enable_module($req);
        }
    }
}

sub add_enabled_pkgs_by_user
{
    ##############
    # handle with/enable flags
    for my $key ( keys %force_enable_packages ) {
        ### fix kernel package name
        # if kmp not supported
        if ($key =~ m/mlnx-ofa_kernel/ and not $kmp) {
            $key =~ s/mlnx-ofa_kernel/kernel-ib/;
        }
        if ($key =~ m/knem-mlnx|kernel-mft-mlnx|ummunotify-mlnx/ and not $kmp) {
            $key =~ s/-mlnx//;
        }
        # if kmp supported
        if ($key =~ m/kernel-ib/ and $kmp) {
            $key =~ s/kernel-ib/mlnx-ofa_kernel/;
        }
        if ($key =~ m/knem|kernel-mft|ummunotify/ and $key !~ m/mlnx/ and $kmp) {
            $key .= "-mlnx";
        }

        if (exists $packages_info{$key}) {
            enable_package($key);
            push (@selected_by_user, $key);
        }
        if (exists $kernel_modules_info{$key}) {
            enable_module($key);
            push (@selected_modules_by_user , $key);
        }
    }
}

# Set packages availability depending OS/Kernel/arch
sub set_availability
{
    set_compilers();

    if ($DISTRO =~ /RHEL|OEL|FC/) {
        push(@{$packages_info{'openmpi'}{'dist_req_build'}}, 'numactl-devel');
        push(@{$packages_info{'hcoll'}{'dist_req_build'}}, 'numactl-devel');
        push(@{$packages_info{'openmpi'}{'dist_req_inst'}}, 'numactl');
        push(@{$packages_info{'hcoll'}{'dist_req_inst'}}, 'numactl');
    }

    # mvapich and openmpi
    if ($gcc{'gcc'}) {
        if ($gcc{'g77'} or $gcc{'gfortran'}) {
            $packages_info{'mvapich2'}{'available'} = 1;
            set_property_for_packages_like('mpitests_mvapich', 'available', 1);
        }
        $packages_info{'openmpi'}{'available'} = 1;
        set_property_for_packages_like('mpitests_openmpi', 'available', 1);
    }

    if ($DISTRO =~ /XenServer|DEBIAN|UBUNTU/) {
        for my $package (@mpi_packages) {
            $packages_info{$package}{'available'} = 0;
        }
        $packages_info{'mxm'}{'available'} = 0;
        $packages_info{'fca'}{'available'} = 0;
        $packages_info{"$knem_rpm"}{'available'} = 0;
        $packages_info{'openshmem'}{'available'} = 0;
        $packages_info{'bupc'}{'available'} = 0;
        $packages_info{"$ummunotify_rpm"}{'available'} = 0;
        $packages_info{'libibprof'}{'available'} = 0;
        $packages_info{'hcoll'}{'available'} = 0;
    }

    if ($arch !~ m/x86_64|ppc64/ or $DISTRO =~ /RHEL5|XenServer|UBUNTU/) {
        for my $package (@mpi_packages) {
            $packages_info{$package}{'available'} = 0;
        }
        $packages_info{'mxm'}{'available'} = 0;
        $packages_info{'fca'}{'available'} = 0;
        $packages_info{"$knem_rpm"}{'available'} = 0;
        $packages_info{'openshmem'}{'available'} = 0;
        $packages_info{'bupc'}{'available'} = 0;
        $packages_info{"$ummunotify_rpm"}{'available'} = 0;
        $packages_info{'libibprof'}{'available'} = 0;
        $packages_info{'hcoll'}{'available'} = 0;
    }

    if ($arch eq "ppc64") {
        $packages_info{'fca'}{'available'} = 1;
    }

    # QIB
    if ($arch =~ m/x86_64/ and $DISTRO !~ /FC/) {
        if ($rpm_distro ne "sles11sp3" and $DISTRO !~ /SUSE|SLES12|RHEL7.0/) {
            $packages_info{'infinipath-psm'}{'available'} = 1;
            $packages_info{'infinipath-psm-devel'}{'available'} = 1;
        }
        $packages_info{'libipathverbs'}{'available'} = 1;
        $packages_info{'libipathverbs-devel'}{'available'} = 1;
        $packages_info{'libipathverbs-debuginfo'}{'available'} = 1;
        $kernel_modules_info{'qib'}{'available'} = 1;
    }

    # SRP Target
    if ($DISTRO =~ m/SLES10|SLES11|RHEL5.[34]/ or $kernel =~ m/2.6.2[7-9]|2.6.3[0-2]/) {
            $kernel_modules_info{'srpt'}{'available'} = 0;
    }

    # debuginfo RPM currently are not supported on SuSE and Ubuntu
    if ($DISTRO =~ m/SLES|DEBIAN|UBUNTU|SUSE/) {
        for my $package (@all_packages) {
            if ($package =~ m/-debuginfo/) {
                $packages_info{$package}{'available'} = 0;
            }
        }
    }

    if ($DISTRO =~ m/SLES10/) {
        $kernel_modules_info{'mthca'}{'available'} = 0;
        $kernel_modules_info{'mlx4_en'}{'available'} = 0;
        $kernel_modules_info{'mlx4_vnic'}{'available'} = 0;
        $kernel_modules_info{'e_ipoib'}{'available'} = 0;
        $kernel_modules_info{'nfsrdma'}{'available'} = 0;
        $kernel_modules_info{'9pnet_rdma'}{'available'} = 0;
        $kernel_modules_info{'9p'}{'available'} = 0;
        $kernel_modules_info{'cxgb3'}{'available'} = 0;
        $kernel_modules_info{'cxgb3i'}{'available'} = 0;
        $kernel_modules_info{'cxgb4'}{'available'} = 0;
        $kernel_modules_info{'cxgb4i'}{'available'} = 0;
        $kernel_modules_info{'nes'}{'available'} = 0;
        $kernel_modules_info{'qib'}{'available'} = 0;
        $kernel_modules_info{"ipath"}{"available"} = 0;
        $kernel_modules_info{'srp'}{'available'} = 0;
        $kernel_modules_info{'rds'}{'available'} = 0;
        $kernel_modules_info{'sdp'}{'available'} = 0;
        $kernel_modules_info{'srpt'}{'available'} = 0;
        $kernel_modules_info{'amso1100'}{'available'} = 0;
        $packages_info{'knem'}{'available'} = 0;
        $packages_info{'knem-mlnx'}{'available'} = 0;
        $packages_info{'ummunotify'}{'available'} = 0;
        $packages_info{'ummunotify-mlnx'}{'available'} = 0;
        $packages_info{'libvma'}{'available'} = 0;
        $packages_info{'libcxgb3'}{'available'} = 0;
        $packages_info{'libcxgb3-devel'}{'available'} = 0;
        $packages_info{'libcxgb4'}{'available'} = 0;
        $packages_info{'libcxgb4-devel'}{'available'} = 0;
        $packages_info{'libnes'}{'available'} = 0;
        $packages_info{'libnes-devel-static'}{'available'} = 0;
        $packages_info{'libipathverbs'}{'available'} = 0;
        $packages_info{'libipathverbs-devel'}{'available'} = 0;
        $packages_info{'srptools'}{'available'} = 0;
        $packages_info{'rds-tools'}{'available'} = 0;
        $packages_info{'rds-devel'}{'available'} = 0;
        $packages_info{'infinipath-psm'}{'available'} = 0;
        $packages_info{'infinipath-psm-devel'}{'available'} = 0;
        for my $package (@mpi_packages) {
            $packages_info{$package}{'available'} = 0;
        }
        $packages_info{'mxm'}{'available'} = 0;
        $packages_info{'fca'}{'available'} = 0;
        $packages_info{"$knem_rpm"}{'available'} = 0;
        $packages_info{'openshmem'}{'available'} = 0;
        $packages_info{'bupc'}{'available'} = 0;
        $packages_info{"$ummunotify_rpm"}{'available'} = 0;
        $packages_info{'libibprof'}{'available'} = 0;
        $packages_info{'hcoll'}{'available'} = 0;
    }

    # UBUNTU
    if ($DISTRO =~ m/UBUNTU/) {
        $packages_info{'ibutils2'}{'available'} = 0; #compilation failed
        $packages_info{'ibutils'}{'available'} = 0; #compilation failed
        $packages_info{'ar_mgr'}{'available'} = 0; #compilation failed
        $packages_info{'cc_mgr'}{'available'} = 0; #compilation failed
        $packages_info{'dump_pr'}{'available'} = 0; #compilation failed
    }

    # ipath and qib are supported only on 64bit OS.
    if ($arch !~ m/x86_64/) {
       $kernel_modules_info{"ipath"}{"available"} = 0;
       $kernel_modules_info{"qib"}{"available"} = 0;
    }

    if ( not $with_vma or $arch !~ m/x86_64|ppc64/) {
       $packages_info{'libvma'}{'available'} = 0;
       $packages_info{'libvma-debuginfo'}{'available'} = 0;
    }

    # enable fabric-collector only if --with-fabric-collector was given and the OS is supported.
    if ( not ($with_fabric_collector and ($DISTRO =~ /RHEL6/ or $rpm_distro eq "sles11sp1" or $rpm_distro eq "sles11sp2" or $rpm_distro eq "sles11sp3")) ) {
       $packages_info{'fabric-collector'}{'available'} = 0;
       $packages_info{'fabric-collector-debuginfo'}{'available'} = 0;
    }

    if ($arch =~ /ppc/) {
       $build32 = 0;
       print("Detected PPC arch.. Disabling buidling 32bit rpms...\n") if ($verbose);
    }

    if ($kernel !~ /^3.*uek/) {
       $packages_info{'libsdp'}{'available'} = 0;
       $packages_info{'libsdp-devel'}{'available'} = 0;
       $packages_info{'libsdp-debuginfo'}{'available'} = 0;
       $packages_info{'sdpnetstat'}{'available'} = 0;
       $packages_info{'sdpnetstat-debuginfo'}{'available'} = 0;
    }

    if ($kernel =~ /fbk36/) {
       $kernel_modules_info{'rds'}{'available'} = 0;
    }

    if ($kernel =~ /^3\.1[6-9]/) {
        $kernel_modules_info{'nfsrdma'}{'available'} = 0;
    }

    ##############
    # handle without/disable flags
    if (keys %disabled_packages) {
        # build deps list
        for my $pkg (keys %packages_info) {
            for my $req ( @{ $packages_info{$pkg}{'ofa_req_inst'}} , @{ $packages_info{$pkg}{'ofa_req_build'}} ) {
                next if not $req;
                push (@{$packages_deps{$req}{'required_by'}}, $pkg);
            }
        }
        for my $mod (keys %kernel_modules_info) {
            for my $req ( @{ $kernel_modules_info{$mod}{'requires'} } ) {
                next if not $req;
                push (@{$modules_deps{$req}{'required_by'}}, $mod);
            }
        }
        # disable packages
        for my $key ( keys %disabled_packages ) {
            disable_package($key);
        }
    }
    # end of handle without/disable flags

    if ($DISTRO eq "SLES10") {
            $packages_info{'iser'}{'available'} = 0;
            $packages_info{'srp'}{'available'} = 0;
    }

    # keep this at the end of the function.
    add_enabled_pkgs_by_user();
}

# Set rpm_exist parameter for existing RPMs
sub set_existing_rpms
{
    # Check if the ofed-scripts RPM exist and its prefix is the same as required one
    my $scr_rpm = '';
    my $arch = $target_cpu;

    $scr_rpm = <$RPMS/ofed-scripts-[0-9]*.$arch.rpm>;
    if ( -f $scr_rpm ) {
        my $current_prefix = `rpm -qlp $scr_rpm | grep ofed_info | sed -e "s@/bin/ofed_info@@"`;
        chomp $current_prefix;
        print "Found $scr_rpm. Its installation prefix: $current_prefix\n" if ($verbose2);
        if (not $current_prefix eq $prefix) {
            print "Required prefix is: $prefix\n" if ($verbose2);
            print "Going to rebuild RPMs from scratch\n" if ($verbose2);
            return;
        }
    }

    for my $binrpm ( <$RPMS/*.rpm> ) {
        my ($rpm_name, $rpm_arch) = (split ' ', get_rpm_name_arch($binrpm));
        my $ver = get_rpm_ver($binrpm);
        my $ver_no_rel = $ver; #version without kernel release
        $ver_no_rel =~ s/_.*//g;
        if ($DISTRO =~ /RHEL5|XenServer/ and $arch eq "i386" and $rpm_name =~ /ofa_kernel|kernel-ib/) {
            $arch = 'i686';
        }
        if ($rpm_name =~ /ib-bonding|knem|kernel-mft|mlx4_accl|ummunotify|iser|srp$/ and not $kmp) {
            if (($rpm_arch eq $arch) and (get_rpm_rel($binrpm) eq $kernel_rel)) {
                $main_packages{$rpm_name}{$ver}{'rpmpath'} = $binrpm;
                $packages_info{$rpm_name}{$ver}{'rpm_exist'} = 1;
                print "$rpm_name $ver RPM exist\n" if ($verbose2);
            }
        } elsif ($rpm_name =~ /kernel-ib/) {
            my $kernel_from_release = get_rpm_rel($binrpm);
            chomp $kernel_from_release;
            $kernel_from_release =~ s/_OFED(.*)//;
            if (($rpm_arch eq $arch) and ($kernel_from_release eq "$kernel_rel")) {
                $main_packages{$rpm_name}{$ver}{'rpmpath'} = $binrpm;
                $packages_info{$rpm_name}{$ver}{'rpm_exist'} = 1;
                print "$rpm_name $ver RPM exist\n" if ($verbose2);
            }

        # W/A for kmp packages that has only kmod and kmp rpms
        } elsif ($kmp and $rpm_name =~ /kmp|kmod/ and $rpm_name =~ /kernel-mft-mlnx|iser|srp/) {
                my $pkname = $rpm_name;
                $pkname =~ s/kmod-//g;
                $pkname =~ s/-kmp.*//g;
                $main_packages{$rpm_name}{$ver}{'rpmpath'} = $binrpm;
                $packages_info{"$rpm_name"}{$ver}{'rpm_exist'} = 1;
                $packages_info{"$pkname"}{$ver}{'rpm_exist'} = 1;
                $packages_info{"$pkname"}{$ver_no_rel}{'rpm_exist'} = 1;
                print "$rpm_name $ver ($pkname) RPM exist\n" if ($verbose2);
        } else {
            if ($rpm_arch eq $arch and
                        ($rpm_name !~ /iser|srp$/)) {
                $main_packages{$rpm_name}{$ver}{'rpmpath'} = $binrpm;
                $packages_info{$rpm_name}{$ver}{'rpm_exist'} = 1;
                print "$rpm_name $ver RPM exist\n" if ($verbose2);
            }
            elsif ($rpm_arch eq $target_cpu32) {
                $main_packages{$rpm_name}{$ver}{'rpmpath'} = $binrpm;
                $packages_info{$rpm_name}{$ver}{'rpm_exist32'} = 1;
                print "$rpm_name $ver 32-bit RPM exist\n" if ($verbose2);
            }
        }
        $arch = $target_cpu;
    }
}

sub mvapich2_config
{
    my $ans;
    my $done;

    if ($mvapich2_conf_done) {
        return;
    }

    if (not $interactive) {
        $mvapich2_conf_done = 1;
        return;
    }

    print "\nPlease choose an implementation of MVAPICH2:\n\n";
    print "1) OFA (IB and iWARP)\n";
    print "2) uDAPL\n";
    $done = 0;

    while (not $done) {
        print "Implementation [1]: ";
        $ans = getch();

        if (ord($ans) == $KEY_ENTER or $ans eq "1") {
            $mvapich2_conf_impl = "ofa";
            $done = 1;
        }

        elsif ($ans eq "2") {
            $mvapich2_conf_impl = "udapl";
            $done = 1;
        }

        else {
            $done = 0;
        }
    }

    print "\nEnable ROMIO support [Y/n]: ";
    $ans = getch();

    if ($ans =~ m/Nn/) {
        $mvapich2_conf_romio = 0;
    }

    else {
        $mvapich2_conf_romio = 1;
    }

    print "\nEnable shared library support [Y/n]: ";
    $ans = getch();

    if ($ans =~ m/Nn/) {
        $mvapich2_conf_shared_libs = 0;
    }

    else {
        $mvapich2_conf_shared_libs = 1;
    }

    # OFA specific options.
    if ($mvapich2_conf_impl eq "ofa") {
        $done = 0;

        while (not $done) {
            print "\nEnable Checkpoint-Restart support [y/N]: ";
            $ans = getch();

            if ($ans =~ m/[Yy]/) {
                $mvapich2_conf_ckpt = 1;
                print "\nBLCR installation directory [or nothing if not installed]: ";
                my $tmp = <STDIN>;
                chomp $tmp;

                if (-d "$tmp") {
                    $mvapich2_conf_blcr_home = $tmp;
                    $done = 1;
                }

                else {
                    print RED "\nBLCR installation directory not found.", RESET "\n";
                }
            }

            else {
                $mvapich2_conf_ckpt = 0;
                $done = 1;
            }
        }
    }

    else {
        $mvapich2_conf_ckpt = 0;
    }

    # uDAPL specific options.
    if ($mvapich2_conf_impl eq "udapl") {
        print "\nCluster size:\n\n1) Small\n2) Medium\n3) Large\n";
        $done = 0;

        while (not $done) {
            print "Cluster size [1]: ";
            $ans = getch();

            if (ord($ans) == $KEY_ENTER or $ans eq "1") {
                $mvapich2_conf_vcluster = "small";
                $done = 1;
            }

            elsif ($ans eq "2") {
                $mvapich2_conf_vcluster = "medium";
                $done = 1;
            }

            elsif ($ans eq "3") {
                $mvapich2_conf_vcluster = "large";
                $done = 1;
            }
        }

        print "\nI/O Bus:\n\n1) PCI-Express\n2) PCI-X\n";
        $done = 0;

        while (not $done) {
            print "I/O Bus [1]: ";
            $ans = getch();

            if (ord($ans) == $KEY_ENTER or $ans eq "1") {
                $mvapich2_conf_io_bus = "PCI_EX";
                $done = 1;
            }

            elsif ($ans eq "2") {
                $mvapich2_conf_io_bus = "PCI_X";
                $done = 1;
            }
        }

        if ($mvapich2_conf_io_bus eq "PCI_EX") {
            print "\nLink Speed:\n\n1) SDR\n2) DDR\n";
            $done = 0;

            while (not $done) {
                print "Link Speed [1]: ";
                $ans = getch();

                if (ord($ans) == $KEY_ENTER or $ans eq "1") {
                    $mvapich2_conf_link_speed = "SDR";
                    $done = 1;
                }

                elsif ($ans eq "2") {
                    $mvapich2_conf_link_speed = "DDR";
                    $done = 1;
                }
            }
        }

        else {
            $mvapich2_conf_link_speed = "SDR";
        }

        print "\nDefault DAPL provider []: ";
        $ans = <STDIN>;
        chomp $ans;

        if ($ans) {
            $mvapich2_conf_dapl_provider = $ans;
        }
    }

    $mvapich2_conf_done = 1;

    open(CONFIG, ">>$config") || die "Can't open $config: $!";;
    flock CONFIG, $LOCK_EXCLUSIVE;

    print CONFIG "mvapich2_conf_impl=$mvapich2_conf_impl\n";
    print CONFIG "mvapich2_conf_romio=$mvapich2_conf_romio\n";
    print CONFIG "mvapich2_conf_shared_libs=$mvapich2_conf_shared_libs\n";
    print CONFIG "mvapich2_conf_ckpt=$mvapich2_conf_ckpt\n";
    print CONFIG "mvapich2_conf_blcr_home=$mvapich2_conf_blcr_home\n" if ($mvapich2_conf_blcr_home);
    print CONFIG "mvapich2_conf_vcluster=$mvapich2_conf_vcluster\n";
    print CONFIG "mvapich2_conf_io_bus=$mvapich2_conf_io_bus\n" if ($mvapich2_conf_io_bus);
    print CONFIG "mvapich2_conf_link_speed=$mvapich2_conf_link_speed\n" if ($mvapich2_conf_link_speed);
    print CONFIG "mvapich2_conf_dapl_provider=$mvapich2_conf_dapl_provider\n" if ($mvapich2_conf_dapl_provider);

    flock CONFIG, $UNLOCK;
    close(CONFIG);
}

sub show_menu
{
    my $menu = shift @_;
    my $max_inp;

    print $clear_string;
    if ($menu eq "main") {
        print "$PACKAGE Distribution Software Installation Menu\n\n";
        print "   1) View $PACKAGE Installation Guide\n";
        print "   2) Install $PACKAGE Software\n";
        print "   3) Show Installed Software\n";
        print "   4) Configure IPoIB\n";
        print "   5) Uninstall $PACKAGE Software\n";
#        print "   6) Generate Supporting Information for Problem Report\n";
        print "\n   Q) Exit\n";
        $max_inp=5;
        print "\nSelect Option [1-$max_inp]:"
    }
    elsif ($menu eq "select") {
        print "$PACKAGE Distribution Software Installation Menu\n\n";
        print "   1) Basic ($PACKAGE modules and basic user level libraries)\n";
        print "   2) HPC ($PACKAGE modules and libraries, MPI and diagnostic tools)\n";
        print "   3) All packages (all of Basic, HPC)\n";
        print "   4) Customize\n";
        print "   5) Packages required by VMA (IB and Eth)\n";
        print "   6) Packages required by VMA IB\n";
        print "   7) Packages required by VMA Eth\n";
        print "   8) Packages required by Guest OS\n";
        print "   9) Packages required by Hypervisor OS\n";
        print "\n   Q) Exit\n";
        $max_inp=9;
        print "\nSelect Option [1-$max_inp]:"
    }

    return $max_inp;
}

# Select package for installation
sub select_packages
{
    my $cnt = 0;
    if ($interactive) {
        open(CONFIG, ">$config") || die "Can't open $config: $!";;
        flock CONFIG, $LOCK_EXCLUSIVE;
        my $ok = 0;
        my $inp;
        my $max_inp;
        while (! $ok) {
            $max_inp = show_menu("select");
            $inp = getch();
            if ($inp =~ m/[qQ]/ || $inp =~ m/[Xx]/ ) {
                die "Exiting\n";
            }
            if (ord($inp) == $KEY_ENTER) {
                next;
            }
            if ($inp =~ m/[0123456789abcdefABCDEF]/)
            {
                $inp = hex($inp);
            }
            if ($inp < 1 || $inp > $max_inp)
            {
                print "Invalid choice...Try again\n";
                next;
            }
            $ok = 1;
        }
        if ($inp == $BASIC) {
            for my $package (@basic_user_packages, @basic_kernel_packages) {
                next if (not $packages_info{$package}{'available'});
                my $parent = $packages_info{$package}{'parent'};
                next if (not is_srpm_available($parent));
                push (@selected_by_user, $package);
                print CONFIG "$package=y\n";
                $cnt ++;
            }
            for my $module ( @basic_kernel_modules ) {
                next if (not $kernel_modules_info{$module}{'available'});
                push (@selected_modules_by_user, $module);
                print CONFIG "$module=y\n";
            }
        }
        elsif ($inp == $HPC) {
            for my $package ( @hpc_user_packages, @hpc_kernel_packages ) {
                next if (not $packages_info{$package}{'available'});
                my $parent = $packages_info{$package}{'parent'};
                next if (not is_srpm_available($parent));
                push (@selected_by_user, $package);
                print CONFIG "$package=y\n";
                $cnt ++;
            }
            for my $module ( @hpc_kernel_modules ) {
                next if (not $kernel_modules_info{$module}{'available'});
                push (@selected_modules_by_user, $module);
                print CONFIG "$module=y\n";
            }
        }
        elsif ($inp == $VMA) {
            for my $package ( @vma_user_packages, @vma_kernel_packages ) {
                next if (not $packages_info{$package}{'available'});
                my $parent = $packages_info{$package}{'parent'};
                next if (not is_srpm_available($parent));
                push (@selected_by_user, $package);
                print CONFIG "$package=y\n";
                $cnt ++;
            }
            for my $module ( @vma_kernel_modules ) {
                next if (not $kernel_modules_info{$module}{'available'});
                push (@selected_modules_by_user, $module);
                print CONFIG "$module=y\n";
            }
        }
        elsif ($inp == $VMAVPI) {
            for my $package ( @vmavpi_user_packages, @vma_kernel_packages ) {
                next if (not $packages_info{$package}{'available'});
                my $parent = $packages_info{$package}{'parent'};
                next if (not is_srpm_available($parent));
                push (@selected_by_user, $package);
                print CONFIG "$package=y\n";
                $cnt ++;
            }
            for my $module ( @vma_kernel_modules ) {
                next if (not $kernel_modules_info{$module}{'available'});
                push (@selected_modules_by_user, $module);
                print CONFIG "$module=y\n";
            }
        }
        elsif ($inp == $VMAETH) {
            for my $package ( @vmaeth_user_packages, @vma_kernel_packages ) {
                next if (not $packages_info{$package}{'available'});
                my $parent = $packages_info{$package}{'parent'};
                next if (not is_srpm_available($parent));
                push (@selected_by_user, $package);
                print CONFIG "$package=y\n";
                $cnt ++;
            }
            for my $module ( @vma_kernel_modules ) {
                next if (not $kernel_modules_info{$module}{'available'});
                push (@selected_modules_by_user, $module);
                print CONFIG "$module=y\n";
            }
        }
        elsif ($inp == $GUESTOS) {
            for my $package ( @guest_kernel_packages,@guest_user_packages ) {
                next if (not $packages_info{$package}{'available'});
                my $parent = $packages_info{$package}{'parent'};
                next if (not is_srpm_available($parent));
                push (@selected_by_user, $package);
                print CONFIG "$package=y\n";
                $cnt ++;
            }
        }
        elsif ($inp == $HYPERVISOROS) {
            for my $package ( @hypervisor_kernel_packages,@hypervisor_user_packages) {
                next if (not $packages_info{$package}{'available'});
                my $parent = $packages_info{$package}{'parent'};
                next if (not is_srpm_available('srpmpath'));
                push (@selected_by_user, $package);
                print CONFIG "$package=y\n";
                $cnt ++;
            }
        }
        elsif ($inp == $ALL) {
            for my $package ( @all_packages, @hidden_packages ) {
                next if (not $packages_info{$package}{'available'});
                my $parent = $packages_info{$package}{'parent'};
                next if (not is_srpm_available($parent));
                push (@selected_by_user, $package);
                print CONFIG "$package=y\n";
                $cnt ++;
            }
            for my $module ( @kernel_modules ) {
                next if (not $kernel_modules_info{$module}{'available'});
                push (@selected_modules_by_user, $module);
                print CONFIG "$module=y\n";
            }
        }
        elsif ($inp == $CUSTOM) {
            my $ans;
            for my $package ( @all_packages ) {
                next if (not $packages_info{$package}{'available'});
                my $parent = $packages_info{$package}{'parent'};
                next if (not is_srpm_available($parent));
                print "Install $package? [y/N]:";
                $ans = getch();
                if ( $ans eq 'Y' or $ans eq 'y' ) {
                    print CONFIG "$package=y\n";
                    push (@selected_by_user, $package);
                    $cnt ++;

                    if ($package =~ /kernel-ib|ofa_kernel/) {
                        # Select kernel modules to be installed
                        for my $module ( @kernel_modules, @tech_preview ) {
                            next if (not $kernel_modules_info{$module}{'available'});
                            print "Install $module module? [y/N]:";
                            $ans = getch();
                            if ( $ans eq 'Y' or $ans eq 'y' ) {
                                push (@selected_modules_by_user, $module);
                                print CONFIG "$module=y\n";
                            }
                        }
                    }
                }
                else {
                    print CONFIG "$package=n\n";
                }
            }
            if ($arch eq "x86_64" or $arch eq "ppc64") {
                if (supported32bit()) {
                    print "Install 32-bit packages? [y/N]:";
                    $ans = getch();
                    if ( $ans eq 'Y' or $ans eq 'y' ) {
                        $build32 = 1;
                        print CONFIG "build32=1\n";
                    }
                    else {
                        $build32 = 0;
                        print CONFIG "build32=0\n";
                    }
                }
                else {
                    $build32 = 0;
                    print CONFIG "build32=0\n";
                }
            }
            print "Please enter the $PACKAGE installation directory: [$prefix]:";
            $ans = <STDIN>;
            chomp $ans;
            if ($ans) {
                $prefix = $ans;
                $prefix =~ s/\/$//;
            }
            print CONFIG "prefix=$prefix\n";
        }
        flock CONFIG, $UNLOCK;
    }
    else {
        if ($config_given) {
            open(CONFIG, "$config") || die "Can't open $config: $!";;
            while(<CONFIG>) {
                next if (m@^\s+$|^#.*@);
                my ($package,$selected) = (split '=', $_);
                chomp $package;
                chomp $selected;

                ### fix kernel package name
                # if kmp not supported
                if ($package =~ m/mlnx-ofa_kernel/ and not $kmp) {
                    $package =~ s/mlnx-ofa_kernel/kernel-ib/;
                }
                if ($package =~ m/knem-mlnx|kernel-mft-mlnx|ummunotify-mlnx/ and not $kmp) {
                    $package =~ s/-mlnx//;
                }
                # if kmp supported
                if ($package =~ m/kernel-ib/ and $kmp) {
                    $package =~ s/kernel-ib/mlnx-ofa_kernel/;
                }
                if ($package =~ m/knem|kernel-mft|ummunotify/ and $package !~ m/mlnx/ and $kmp) {
                    $package .= "-mlnx";
                }

                print "$package=$selected\n" if ($verbose3);

                if ($package eq "build32") {
                    if (supported32bit()) {
                        $build32 = 1 if ($selected);
                    }
                    next;
                }

                if ($package eq "prefix") {
                    $prefix = $selected;
                    $prefix =~ s/\/$//;
                    next;
                }

                if ($package eq "bonding_force_all_os") {
                    if ($selected =~ m/[Yy]|[Yy][Ee][Ss]/) {
                        $bonding_force_all_os = 1;
                    }
                    next;
                }

		if (substr($package,0,length("vendor_config")) eq "vendor_config") {
		       next;
		}

                if ($package eq "vendor_pre_install") {
		    if ( -f $selected ) {
			$vendor_pre_install = dirname($selected) . '/' . basename($selected);
		    }
		    else {
			print RED "\nVendor script $selected is not found", RESET "\n" if (not $quiet);
			exit 1
		    }
                    next;
                }

                if ($package eq "vendor_post_install") {
		    if ( -f $selected ) {
			$vendor_post_install = dirname($selected) . '/' . basename($selected);
		    }
		    else {
			print RED "\nVendor script $selected is not found", RESET "\n" if (not $quiet);
			exit 1
		    }
                    next;
                }

                if ($package eq "vendor_pre_uninstall") {
		    if ( -f $selected ) {
			$vendor_pre_uninstall = dirname($selected) . '/' . basename($selected);
		    }
		    else {
			print RED "\nVendor script $selected is not found", RESET "\n" if (not $quiet);
			exit 1
		    }
                    next;
                }

                if ($package eq "vendor_post_uninstall") {
		    if ( -f $selected ) {
			$vendor_post_uninstall = dirname($selected) . '/' . basename($selected);
		    }
		    else {
			print RED "\nVendor script $selected is not found", RESET "\n" if (not $quiet);
			exit 1
		    }
                    next;
                }

                if ($package eq "kernel_configure_options" or $package eq "OFA_KERNEL_PARAMS") {
                    $kernel_configure_options = $selected;
                    next;
                }

                if ($package eq "user_configure_options") {
                    $user_configure_options = $selected;
                    next;
                }

                if ($package =~ m/configure_options/) {
                    my $pack_name = (split '_', $_)[0];
                    $packages_info{$pack_name}{'configure_options'} = $selected;
                    next;
                }

                # mvapich2 configuration environment
                if ($package eq "mvapich2_conf_impl") {
                    $mvapich2_conf_impl = $selected;
                    next;
                }

                elsif ($package eq "mvapich2_conf_romio") {
                    $mvapich2_conf_romio = $selected;
                    next;
                }

                elsif ($package eq "mvapich2_conf_shared_libs") {
                    $mvapich2_conf_shared_libs = $selected;
                    next;
                }

                elsif ($package eq "mvapich2_conf_ckpt") {
                    $mvapich2_conf_ckpt = $selected;
                    next;
                }

                elsif ($package eq "mvapich2_conf_blcr_home") {
                    $mvapich2_conf_blcr_home = $selected;
                    next;
                }

                elsif ($package eq "mvapich2_conf_vcluster") {
                    $mvapich2_conf_vcluster = $selected;
                    next;
                }

                elsif ($package eq "mvapich2_conf_io_bus") {
                    $mvapich2_conf_io_bus = $selected;
                    next;
                }

                elsif ($package eq "mvapich2_conf_link_speed") {
                    $mvapich2_conf_link_speed = $selected;
                    next;
                }

                elsif ($package eq "mvapich2_conf_dapl_provider") {
                    $mvapich2_conf_dapl_provider = $selected;
                    next;
                }

                if (not $packages_info{$package}{'parent'} or $package =~ /iser|srp$/) {
                    my $modules = "@kernel_modules @tech_preview";
                    chomp $modules;
                    $modules =~ s/ /|/g;
                    if ($package =~ m/$modules/) {
                        if ( $selected eq 'y' ) {
                            if (not $kernel_modules_info{$package}{'available'}) {
                                print "$package is not available on this platform\n" if (not $quiet);
                            }
                            else {
                                push (@selected_modules_by_user, $package);
                            }
                            next if ($package !~ /iser|srp/);
                        }
                    }
                    else {
                       print "Unsupported package: $package\n" if (not $quiet);
                       next;
                    }
                }

                if (not $packages_info{$package}{'available'} and $selected eq 'y') {
                    print "$package is not available on this platform\n" if (not $quiet);
                    next;
                }

                if ( $selected eq 'y' ) {
                    my $parent = $packages_info{$package}{'parent'};
                    if (not is_srpm_available($parent)) {
                        print "Unsupported package: $package\n" if (not $quiet);
                        next;
                    }
                    push (@selected_by_user, $package);
                    print "select_package: selected $package\n" if ($verbose2);
                    $cnt ++;
                }
            }
        }
        else {
            open(CONFIG, ">$config") || die "Can't open $config: $!";
            flock CONFIG, $LOCK_EXCLUSIVE;
            if ($install_option eq 'all') {
                for my $package ( @all_packages ) {
                    next if (not $packages_info{$package}{'available'});
                    my $parent = $packages_info{$package}{'parent'};
                    next if (not is_srpm_available($parent));
                    push (@selected_by_user, $package);
                    print CONFIG "$package=y\n";
                    $cnt ++;
                }
                for my $module ( @kernel_modules ) {
                    next if (not $kernel_modules_info{$module}{'available'});
                    push (@selected_modules_by_user, $module);
                    print CONFIG "$module=y\n";
                }
            }
            elsif ($install_option eq 'guest-os') {
                for my $package ( @guest_kernel_packages,@guest_user_packages ) {
                    next if (not $packages_info{$package}{'available'});
                    my $parent = $packages_info{$package}{'parent'};
                    next if (not is_srpm_available($parent));
                    push (@selected_by_user, $package);
                    print CONFIG "$package=y\n";
                    $cnt ++;
                }
                for my $module ( @guest_kernel_modules ) {
                    next if (not $kernel_modules_info{$module}{'available'});
                    push (@selected_modules_by_user, $module);
                    print CONFIG "$module=y\n";
                }
            }
            elsif ($install_option eq 'hypervisor-os') {
                for my $package ( @hypervisor_kernel_packages,@hypervisor_user_packages ) {
                    next if (not $packages_info{$package}{'available'});
                    my $parent = $packages_info{$package}{'parent'};
                    next if (not is_srpm_available($parent));
                    push (@selected_by_user, $package);
                    print CONFIG "$package=y\n";
                    $cnt ++;
                }
                for my $module ( @hypervisor_kernel_modules ) {
                    next if (not $kernel_modules_info{$module}{'available'});
                    push (@selected_modules_by_user, $module);
                    print CONFIG "$module=y\n";
                }
            }
            elsif ($install_option eq 'hpc') {
                for my $package ( @hpc_user_packages, @hpc_kernel_packages ) {
                    next if (not $packages_info{$package}{'available'});
                    my $parent = $packages_info{$package}{'parent'};
                    next if (not is_srpm_available($parent));
                    push (@selected_by_user, $package);
                    print CONFIG "$package=y\n";
                    $cnt ++;
                }
                for my $module ( @hpc_kernel_modules ) {
                    next if (not $kernel_modules_info{$module}{'available'});
                    push (@selected_modules_by_user, $module);
                    print CONFIG "$module=y\n";
                }
            }
            elsif ($install_option =~ m/vma/) {
                my @list = ();
                if ($install_option eq 'vma') {
                    @list = (@vma_user_packages);
                } elsif ($install_option eq 'vmavpi') {
                    @list = (@vmavpi_user_packages);
                } elsif ($install_option eq 'vmaeth') {
                    @list = (@vmaeth_user_packages);
                }
                for my $package ( @list, @vma_kernel_packages ) {
                    next if (not $packages_info{$package}{'available'});
                    my $parent = $packages_info{$package}{'parent'};
                    next if (not is_srpm_available($parent));
                    push (@selected_by_user, $package);
                    print CONFIG "$package=y\n";
                    $cnt ++;
                }
                for my $module ( @vma_kernel_modules ) {
                    next if (not $kernel_modules_info{$module}{'available'});
                    push (@selected_modules_by_user, $module);
                    print CONFIG "$module=y\n";
                }
            }
            elsif ($install_option eq 'basic') {
                for my $package (@basic_user_packages, @basic_kernel_packages) {
                    next if (not $packages_info{$package}{'available'});
                    my $parent = $packages_info{$package}{'parent'};
                    next if (not is_srpm_available($parent));
                    push (@selected_by_user, $package);
                    print CONFIG "$package=y\n";
                    $cnt ++;
                }
                for my $module ( @basic_kernel_modules ) {
                    next if (not $kernel_modules_info{$module}{'available'});
                    push (@selected_modules_by_user, $module);
                    print CONFIG "$module=y\n";
                }
            }
            else {
                print RED "\nUnsupported installation option: $install_option", RESET "\n" if (not $quiet);
                exit 1;
            }
        }

        flock CONFIG, $UNLOCK;
    }
    close(CONFIG);


    return $cnt;
}

sub module_in_rpm
{
    my $name = shift @_;
    my $module = shift @_;
    my $ver = shift @_;
    my $ret = 1;
    my $package;

    my $version = $main_packages{$packages_info{$name}{'parent'}}{$ver}{'version'};
    my $release = $kernel_rel;

    if ($name =~ /kernel-ib/) {
        $release .= '_' . $main_packages{$packages_info{$name}{'parent'}}{$ver}{'release'};
    }

    my $arch = $target_cpu;
    if ($DISTRO =~ /RHEL5|XenServer/ and $target_cpu eq "i386" and $name =~ /ofa_kernel|kernel-ib/) {
        $arch = 'i686';
    }

    if ($name =~ /mlnx-ofa_kernel/) {
        $package = $main_packages{$name}{$ver}{'rpmpath'};
    } else {
        $package = "$RPMS/$name-$version-$release.$arch.rpm";
    }
    chomp $package;

    if (not -f $package) {
        print "is_module_in_rpm: $package not found\n";
        return 1;
    }

    if ($module eq "nfsrdma") {
        $module = "xprtrdma";
    } elsif ($module eq "e_ipoib") {
        $module = "eth_ipoib";
    } elsif ($module eq "amso1100") {
        $module = "iw_c2";
    }

    open(LIST, "rpm -qlp $package |") or die "Can't run 'rpm -qlp $package': $!\n";
    while (<LIST>) {
        if (/$module[a-z_]*.ko/) {
            print "is_module_in_rpm: $module $_\n" if ($verbose3);
            $ret = 0;
            last;
        }
    }
    close LIST;

    if ($ret) {
        print "$module not in $package\n" if ($verbose2);
    }

    return $ret;
}

sub mark_for_uninstall
{
    my $package = shift @_;
    if (not $selected_for_uninstall{$package}) {
        push (@dependant_packages_to_uninstall, "$package");
        $selected_for_uninstall{$package} = 1;
        my $pname = $package;
        $pname =~ s@-[0-9].*@@g;
        if (not exists $packages_info{$pname}) {
            $non_ofed_for_uninstall{$pname} = 1;
        }
    }
}

sub get_requires
{
    my $package = shift @_;

    # Strip RPM version
    my $pname = `rpm -q --queryformat "[%{NAME}]" $package`;
    chomp $pname;

    my @what_requires = `/bin/rpm -q --whatrequires $pname 2> /dev/null | grep -v "no package requires" 2> /dev/null`;

    for my $pack_req (@what_requires) {
        chomp $pack_req;
        print "get_requires: $pname is required by $pack_req\n" if ($verbose2);
        get_requires($pack_req);
        mark_for_uninstall($pack_req);
    }
}

sub select_dependent
{
    my $package = shift @_;
    my $pname = $packages_info{$package}{'parent'};
    for my $ver (keys %{$main_packages{$pname}}) {
        if ( (not $packages_info{$package}{$ver}{'rpm_exist'}) or
             ($build32 and not $packages_info{$package}{$ver}{'rpm_exist32'}) ) {
            for my $req ( @{ $packages_info{$package}{'ofa_req_build'} } ) {
                next if not $req;
                if ($packages_info{$req}{'available'} and not $packages_info{$req}{'selected'}) {
                    print "resolve_dependencies: $package requires $req for rpmbuild\n" if ($verbose2);
                    select_dependent($req);
                }
            }
        }

        for my $req ( @{ $packages_info{$package}{'ofa_req_inst'} } ) {
            next if not $req;
            if ($packages_info{$req}{'available'} and not $packages_info{$req}{'selected'}) {
                print "resolve_dependencies: $package requires $req for rpm install\n" if ($verbose2);
                select_dependent($req);
            }
        }

        if (not $packages_info{$package}{'selected'}) {
            return if (not $packages_info{$package}{'available'});
            # Assume that the requirement is not strict. E.g. openmpi dependency on fca
            my $parent = $packages_info{$package}{'parent'};
            return if (not $main_packages{$parent}{$ver}{'srpmpath'});
            $packages_info{$package}{'selected'} = 1;
            push (@selected_packages, $package);
            print "select_dependent: Selected package $package\n" if ($verbose2);
        }
    }
}

sub select_dependent_module
{
    my $module = shift @_;

    for my $req ( @{ $kernel_modules_info{$module}{'requires'} } ) {
        print "select_dependent_module: $module requires $req for rpmbuild\n" if ($verbose2);
        if (not $kernel_modules_info{$req}{'selected'}) {
            select_dependent_module($req);
        }
    }
    if (not $kernel_modules_info{$module}{'selected'}) {
        $kernel_modules_info{$module}{'selected'} = 1;
        push (@selected_kernel_modules, $module);
        print "select_dependent_module: Selected module $module\n" if ($verbose2);
    }
}

sub resolve_dependencies
{
    for my $package ( @selected_by_user ) {
            # Get the list of dependencies
            select_dependent($package);
            if (exists $standalone_kernel_modules_info{$package}) {
                for my $mod (@{$standalone_kernel_modules_info{$package}}) {
                    if ($kernel_modules_info{$mod}{'available'}) {
                        push (@selected_modules_by_user, $mod);
                    }
                }
            }

            if ($package =~ /mvapich2_*/ and not $print_available) {
                    mvapich2_config();
            }
        }

    for my $module ( @selected_modules_by_user ) {
        select_dependent_module($module);
    }

    my @kernel_rpms = qw(kernel-ib kmod-mlnx-ofa_kernel mlnx-ofa_kernel-kmp-default);
    for my $kernel_rpm ( @kernel_rpms ) {
        my $pname = $packages_info{$kernel_rpm}{'parent'};
        if ( not $pname and $kernel_rpm =~ m/mlnx-ofa_kernel/ ) {
           $pname = "mlnx-ofa_kernel";
        }
        for my $ver (keys %{$main_packages{$pname}}) {
            if ($packages_info{$kernel_rpm}{$ver}{'rpm_exist'}) {
                for my $module (@selected_kernel_modules) {
                    if (module_in_rpm($kernel_rpm, $module, $ver)) {
                        $packages_info{$kernel_rpm}{$ver}{'rpm_exist'} = 0;
                        $packages_info{'mlnx-ofa_kernel'}{$ver}{'rpm_exist'} = 0;
                        last;
                    }
                }
                if ($with_memtrack) {
                    if (module_in_rpm($kernel_rpm, "memtrack", $ver)) {
                        $packages_info{$kernel_rpm}{$ver}{'rpm_exist'} = 0;
                        $packages_info{'mlnx-ofa_kernel'}{$ver}{'rpm_exist'} = 0;
                        last;
                    }
                }
            }
        }
    }
}

sub check_linux_dependencies
{
    my $err = 0;
    my $p1 = 0;
    my $gcc_32bit_printed = 0;
	my %missing_packages = ();
    if (! $check_linux_deps) {
        return 0;
    }
    my $dist_req_build = ($DISTRO =~ m/UBUNTU/)?'ubuntu_dist_req_build':'dist_req_build';
    for my $package ( @selected_packages ) {
        my $pname = $packages_info{$package}{'parent'};
        for my $ver (keys %{$main_packages{$pname}}) {
                # Check rpmbuild requirements
                if ($package =~ /ofa_kernel|kernel-ib|ib-bonding|kernel-mft|mlx4_accl|knem|ummunotify/) {
                    if (not $packages_info{$package}{$ver}{'rpm_exist'}) {
                        if ($kernel !~ /2.6.16.60|2.6.3[2-9]|3.[0-9]|3.1[0-5]/) {
                            print RED "Kernel $kernel is not supported.", RESET "\n";
                            print BLUE "For the list of Supported Platforms and Operating Systems see", RESET "\n";
                            print BLUE "$CWD/docs/OFED_release_notes.txt", RESET "\n";
                            exit 1;
                        }
                        # kernel sources required
                        if ( not -d "$kernel_sources/scripts" ) {
                            print RED "$kernel_sources/scripts is required to build $package RPM.", RESET "\n";
                            print RED "Please install the corresponding kernel-source or kernel-devel RPM.", RESET "\n";
                            $err++;
                        }
                    }
                }

                if($DISTRO =~/UBUNTU/){
                    if(not is_installed_deb("rpm")){
                        print RED "rpm is required to build OFED", RESET "\n" if ($verbose2);
                        $missing_packages{"rpm"} = 1;
                    }
                }

                if ($DISTRO =~ m/RHEL|FC/) {
                    if (not is_installed("rpm-build")) {
                        print RED "rpm-build is required to build OFED", RESET "\n" if ($verbose2);
                        $missing_packages{"rpm-build"} = 1;
                        $err++;
                    }
                }

                if ($DISTRO =~ m/RHEL6.[45]/) {
                        if (not is_installed("redhat-rpm-config")) {
                            print RED "redhat-rpm-config rpm is required to build $package", RESET "\n" if ($verbose2);
                            $missing_packages{"redhat-rpm-config"} = 1;
                            $err++;
                        }
                }

                if (not $packages_info{$package}{$ver}{'rpm_exist'}) {
                    for my $req ( @{ $packages_info{$package}{$dist_req_build} } ) {
                        my ($req_name, $req_version) = (split ('__',$req));
                        next if not $req_name;
                        print BLUE "check_linux_dependencies: $req_name  is required to build $package $ver", RESET "\n" if ($verbose3);
                        my $is_installed_flag = ($DISTRO =~ m/UBUNTU/)?(is_installed_deb($req_name)):(is_installed($req_name));
                        if (not $is_installed_flag) {
                            print RED "$req_name rpm is required to build $package $ver", RESET "\n" if ($verbose2);
                            $missing_packages{"$req_name"} = 1;
                            $err++;
                        }
                        if ($req_version) {
                            my $inst_version = get_rpm_ver_inst($req_name);
                            print "check_linux_dependencies: $req_name installed version $inst_version, required at least $req_version\n" if ($verbose3);
                            if ($inst_version lt $req_version) {
                                print RED "$req_name-$req_version rpm is required to build $package $ver", RESET "\n" if ($verbose2);
                                $missing_packages{"$req_name-$req_version"} = 1;
                                $err++;
                            }
                        }
                    }
                    if ($build32) {
                        if (not -f "/usr/lib/crt1.o") {
                            if (! $p1) {
                                print RED "glibc-devel 32bit is required to build 32-bit libraries.", RESET "\n" if ($verbose2);
                                $missing_packages{"glibc-devel$suffix_32bit"} = 1;
                                $p1 = 1;
                                $err++;
                            }
                        }
                        if ($DISTRO eq "SLES11") {
                            if (not is_installed("gcc-32bit")) {
                                if (not $gcc_32bit_printed) {
                                    print RED "gcc-32bit is required to build 32-bit libraries.", RESET "\n" if ($verbose2);
                                    $missing_packages{"gcc-32bit"} = 1;
                                    $gcc_32bit_printed++;
                                    $err++;
                                }
                            }
                        }
                        if ($arch eq "ppc64") {
                            my @libstdc32 = </usr/lib/libstdc++.so.*>;
                            if ($package eq "mstflint") {
                                if (not scalar(@libstdc32)) {
                                    print RED "$libstdc 32bit is required to build mstflint.", RESET "\n" if ($verbose2);
                                    $missing_packages{"$libstdc$suffix_32bit"} = 1;
                                    $err++;
                                }
                            }
                            elsif ($package =~ /openmpi/) {
                                my @libsysfs = </usr/lib/libsysfs.so>;
                                if (not scalar(@libstdc32)) {
                                    print RED "$libstdc_devel 32bit is required to build openmpi.", RESET "\n" if ($verbose2);
                                    $missing_packages{"$libstdc_devel$suffix_32bit"} = 1;
                                    $err++;
                                }
                                if (not scalar(@libsysfs)) {
                                    print RED "$sysfsutils_devel 32bit is required to build openmpi.", RESET "\n" if ($verbose2);
                                    $missing_packages{"$sysfsutils_devel$suffix_32bit"} = 1;
                                    $err++;
                                }
                            }
                        }
                    }
                    if ($package eq "rnfs-utils") {
                        if (not is_installed("krb5-devel")) {
                            print RED "krb5-devel is required to build rnfs-utils.", RESET "\n" if ($verbose2);
                            $missing_packages{"krb5-devel"} = 1;
                            $err++;
                        }
                        if ($DISTRO =~ m/RHEL|FC/) {
                            if (not is_installed("krb5-libs")) {
                                print RED "krb5-libs is required to build rnfs-utils.", RESET "\n" if ($verbose2);
                                $missing_packages{"krb5-libs"} = 1;
                                $err++;
                            }
                            if (not is_installed("libevent-devel")) {
                                print RED "libevent-devel is required to build rnfs-utils.", RESET "\n" if ($verbose2);
                                $missing_packages{"libevent-devel"} = 1;
                                $err++;
                            }
                            if (not is_installed("nfs-utils-lib-devel")) {
                                print RED "nfs-utils-lib-devel is required to build rnfs-utils.", RESET "\n" if ($verbose2);
                                $missing_packages{"nfs-utils-lib-devel"} = 1;
                                $err++;
                            }
                            if (not is_installed("openldap-devel")) {
                                print RED "openldap-devel is required to build rnfs-utils.", RESET "\n" if ($verbose2);
                                $missing_packages{"openldap-devel"} = 1;
                                $err++;
                            }
                        } else {
                            if ($DISTRO eq "SLES11") {
                                if (not is_installed("libevent-devel")) {
                                    print RED "libevent-devel is required to build rnfs-utils.", RESET "\n" if ($verbose2);
                                    $missing_packages{"libevent-devel"} = 1;
                                    $err++;
                                }
                                if (not is_installed("nfsidmap-devel")) {
                                    print RED "nfsidmap-devel is required to build rnfs-utils.", RESET "\n" if ($verbose2);
                                    $missing_packages{"nfsidmap-devel"} = 1;
                                    $err++;
                                }
                                if (not is_installed("libopenssl-devel")) {
                                    print RED "libopenssl-devel is required to build rnfs-utils.", RESET "\n" if ($verbose2);
                                    $missing_packages{"libopenssl-devel"} = 1;
                                    $err++;
                                }
                            } elsif ($DISTRO eq "SLES10") {
                                if (not is_installed("libevent")) {
                                    print RED "libevent is required to build rnfs-utils.", RESET "\n" if ($verbose2);
                                    $missing_packages{"libevent"} = 1;
                                    $err++;
                                }
                                if (not is_installed("nfsidmap")) {
                                    print RED "nfsidmap is required to build rnfs-utils.", RESET "\n" if ($verbose2);
                                    $missing_packages{"nfsidmap"} = 1;
                                    $err++;
                                }
                            }
                            if (not is_installed("krb5")) {
                                print RED "krb5 is required to build rnfs-utils.", RESET "\n" if ($verbose2);
                                $missing_packages{"krb5"} = 1;
                                $err++;
                            }
                            if (not is_installed("openldap2-devel")) {
                                print RED "openldap2-devel is required to build rnfs-utils.", RESET "\n" if ($verbose2);
                                $missing_packages{"openldap2-devel"} = 1;
                                $err++;
                            }
                            if (not is_installed("cyrus-sasl-devel")) {
                                print RED "cyrus-sasl-devel is required to build rnfs-utils.", RESET "\n" if ($verbose2);
                                $missing_packages{"cyrus-sasl-devel"} = 1;
                                $err++;
                            }
                        }

                        my $blkid_so = ($arch =~ m/x86_64/) ? "/usr/lib64/libblkid.so" : "/usr/lib/libblkid.so";
                        my $blkid_pkg = ($kernel =~ m/2.6.2[6-7]/ and $DISTRO =~ m/SLES/) ? "libblkid-devel" :
                                        "e2fsprogs-devel";
                        $blkid_pkg .= ($arch =~ m/powerpc|ppc64/) ? "-32bit" : "";

                        if (not -e $blkid_so) {
                            print RED "$blkid_pkg is required to build rnfs-utils.", RESET "\n" if ($verbose2);
                            $missing_packages{"$blkid_pkg"} = 1;
                            $err++;
                        }
                    }
                }
                my $dist_req_inst = ($DISTRO =~ m/UBUNTU/)?'ubuntu_dist_req_inst':'dist_req_inst';
                # Check installation requirements
                for my $req ( @{ $packages_info{$package}{$dist_req_inst} } ) {
                    my ($req_name, $req_version) = (split ('__',$req));
                    next if not $req_name;
                    my $is_installed_flag = ($DISTRO =~ m/UBUNTU/)?(is_installed_deb($req_name)):(is_installed($req_name));
                    if (not $is_installed_flag) {
                        print RED "$req_name rpm is required to install $package $ver", RESET "\n" if ($verbose2);
                        $missing_packages{"$req_name"} = 1;
                        $err++;
                    }
                    if ($req_version) {
                        my $inst_version = get_rpm_ver_inst($req_name);
                        print "check_linux_dependencies: $req_name installed version $inst_version, required $req_version\n" if ($verbose3);
                        if ($inst_version lt $req_version) {
                            print RED "$req_name-$req_version rpm is required to install $package $ver", RESET "\n" if ($verbose2);
                            $missing_packages{"$req_name-$req_version"} = 1;
                            $err++;
                        }
                    }
                }
                if ($build32) {
                    if (not -f "/usr/lib/crt1.o") {
                        if (! $p1) {
                            print RED "glibc-devel 32bit is required to install 32-bit libraries.", RESET "\n" if ($verbose2);
                            $missing_packages{"glibc-devel$suffix_32bit"} = 1;
                            $p1 = 1;
                            $err++;
                        }
                    }
                    if ($arch eq "ppc64") {
                        my @libstdc32 = </usr/lib/libstdc++.so.*>;
                        if ($package eq "mstflint") {
                            if (not scalar(@libstdc32)) {
                                print RED "$libstdc 32bit is required to install mstflint.", RESET "\n" if ($verbose2);
                                $missing_packages{"$libstdc$suffix_32bit"} = 1;
                                $err++;
                            }
                        }
                        elsif ($package =~ /openmpi/) {
                            my @libsysfs = </usr/lib/libsysfs.so.*>;
                            if (not scalar(@libstdc32)) {
                                print RED "$libstdc 32bit is required to install openmpi.", RESET "\n" if ($verbose2);
                                $missing_packages{"$libstdc$suffix_32bit"} = 1;
                                $err++;
                            }
                            if (not scalar(@libsysfs)) {
                                print RED "$sysfsutils 32bit is required to install openmpi.", RESET "\n" if ($verbose2);
                                $missing_packages{"$sysfsutils$suffix_32bit"} = 1;
                                $err++;
                            }
                        }
                    }
                }
        }
    }
    if ($err) {
        # display a summary of missing packages
        if (keys %missing_packages) {
            print RED "Error: One or more required packages for installing OFED-internal are missing.", RESET "\n";
            print RED "Please install the missing packages using your Linux distribution Package Management tool.", RESET "\n";
            print "Run:\n$package_manager install " . join(' ', (keys %missing_packages)) . "\n";
        }
        exit $PREREQUISIT;
    }
}

# Print the list of selected packages
sub print_selected
{
    print GREEN "\nBelow is the list of ${PACKAGE} packages that you have chosen
    \r(some may have been added by the installer due to package dependencies):\n", RESET "\n";
    for my $package ( @selected_packages ) {
        print "$package\n";
    }
    if ($build32) {
        print "32-bit binaries/libraries will be created\n";
    }
    print "\n";
}

sub build_kernel_rpm
{
    my $name = shift @_;
    my $ver = shift @_;
    my $cmd;
    my $res = 0;
    my $sig = 0;
    my $TMPRPMS;

    $cmd = "rpmbuild --rebuild $rpmbuild_flags --define '_topdir $TOPDIR'";

    if ($name =~ /ofa_kernel/) {
        $kernel_configure_options .= " $packages_info{$name}{'configure_options'}";

        for my $module ( @selected_kernel_modules ) {
            if ($module eq "core") {
                $kernel_configure_options .= " --with-core-mod --with-user_mad-mod --with-user_access-mod --with-addr_trans-mod";
            }
            elsif ($module eq "ipath") {
                $kernel_configure_options .= " --with-ipath_inf-mod";
            }
            elsif ($module eq "qib") {
                $kernel_configure_options .= " --with-qib-mod";
            }
            elsif ($module eq "srpt") {
                $kernel_configure_options .= " --with-srp-target-mod";
            }
            else {
                $kernel_configure_options .= " --with-$module-mod";
            }
        }

        if ($DISTRO eq "DEBIAN") {
                $kernel_configure_options .= " --without-modprobe";
        }

        if ($with_memtrack) {
                $kernel_configure_options .= " --with-memtrack";
        }

        if ($DISTRO =~ /RHEL5|XenServer/ and $target_cpu eq "i386") {
            $cmd .= " --define '_target_cpu i686'";
        }
        $cmd .= " --nodeps";
        $cmd .= " --define '_dist .$rpm_distro'" if $kmp;
        $cmd .= " --define 'configure_options $kernel_configure_options'";
        $cmd .= " --define 'KVERSION $kernel'";
        $cmd .= " --define 'K_SRC $kernel_sources'";
        $cmd .= " --define 'KMP 1'" if $kmp;
    }
    elsif ($name =~ /kernel-mft/) {
        $cmd .= " --define 'KVERSION $kernel'";
        $cmd .= " --define 'K_SRC $kernel_sources'";
        $cmd .= " --define 'source 1'" if $kmp;
        $cmd .= " --define 'debug_package %{nil}'";
        $cmd .= " --define '_dist .$rpm_distro'" if $kmp;
        $cmd .= " --define 'KMP 1'" if $kmp;
    }
    elsif ($name =~ /mlx4_accl/) {
        $cmd .= " --define 'KVERSION $kernel'";
        $cmd .= " --define 'K_SRC $kernel_sources'";
        $cmd .= " --define 'FLAVOR .$kernel_rel'";
        $cmd .= " --define 'debug_package %{nil}'";
    }
    elsif ($name eq 'ib-bonding') {
        $cmd .= " --define 'KVERSION $kernel'";
        $cmd .= " --define '_release $kernel_rel'";
        $cmd .= " --define 'force_all_os $bonding_force_all_os'";
    }
    elsif ($name =~ /knem/) {
        $cmd .= " --define '_release $kernel_rel'" if not $kmp;
        $cmd .= " --define 'KVERSION $kernel'";
        $cmd .= " --define 'K_SRC $kernel_sources'";
        $cmd .= " --define '_dist .$rpm_distro'" if $kmp;
        $cmd .= " --define 'KMP 1'" if $kmp;
    }
    elsif ($name =~ /ummunotify/) {
        $cmd .= " --define '_release $kernel_rel'" if not $kmp;
        $cmd .= " --define 'KVERSION $kernel'";
        $cmd .= " --define 'K_SRC $kernel_sources'";
        $cmd .= " --define '_dist .$rpm_distro'" if $kmp;
        $cmd .= " --define 'KMP 1'" if $kmp;
    }
    elsif ($name =~ /iser/) {
        $cmd .= " --define 'src_release $kernel_rel'" if not $kmp;
        $cmd .= " --define 'KVERSION $kernel'";
        $cmd .= " --define 'K_SRC $kernel_sources'";
        $cmd .= " --define '_dist .$rpm_distro'" if $kmp;
        $cmd .= " --define 'KMP 1'" if $kmp;
    }
    elsif ($name =~ /srp/) {
        $cmd .= " --define 'src_release $kernel_rel'" if not $kmp;
        $cmd .= " --define 'KVERSION $kernel'";
        $cmd .= " --define 'K_SRC $kernel_sources'";
        $cmd .= " --define '_dist .$rpm_distro'" if $kmp;
        $cmd .= " --define 'KMP 1'" if $kmp;
    }

    $cmd .= " --define '_prefix $prefix'";

    $cmd .= " $main_packages{$name}{$ver}{'srpmpath'}";


    print "Running $cmd\n" if ($verbose);
    system("echo $cmd > $ofedlogs/$name-$ver.rpmbuild.log 2>&1");
    system("$cmd >> $ofedlogs/$name-$ver.rpmbuild.log 2>&1");
    $res = $? >> 8;
    $sig = $? & 127;
    if ($sig or $res) {
        print RED "Failed to build $name $ver RPM", RESET "\n";
        print RED "See $ofedlogs/$name-$ver.rpmbuild.log", RESET "\n";
        exit 1;
    }

    my $arch = $target_cpu;
    if ($DISTRO =~ /RHEL5|XenServer/ and $target_cpu eq "i386" and $name =~ /ofa_kernel|kernel-ib/) {
        $arch = 'i686';
    }
    $TMPRPMS = "$TOPDIR/RPMS/$arch";
    chomp $TMPRPMS;

    print "TMPRPMS $TMPRPMS\n" if ($verbose2);

    for my $myrpm ( <$TMPRPMS/*.rpm> ) {
        print "Created $myrpm\n" if ($verbose2);
        if ($name =~ /ofa_kernel|kernel-ib/) {
            system("/bin/rpm -qlp $myrpm | grep lib.modules | awk -F '/' '{print\$4}' | sort -u >> $RPMS/.supported_kernels");
        }
        if ($name =~ /knem/) {
            $knem_prefix = `/bin/rpm -qlp $myrpm | grep -w "sbin\$" 2>/dev/null | sed -e "s@/sbin@@"`;
            chomp $knem_prefix;
        }
        my ($myrpm_name, $myrpm_arch) = (split ' ', get_rpm_name_arch($myrpm));
        # W/A for kmp packages that has only kmod and kmp rpms
        if($kmp and $myrpm_name =~ /kmp|kmod/ and $myrpm_name =~ /kernel-mft-mlnx|iser|srp/) {
            $myrpm_name =~ s/kmod-//g;
            $myrpm_name =~ s/-kmp.*//g;
        }
        if ($build_only and $myrpm_name =~ /kernel.*devel/) {
            system("echo /bin/rpm -ihv --root $builddir $myrpm");
            system("/bin/rpm -ihv --nodeps --root $builddir $myrpm");
            $ENV{"OFA_DIR"} = $builddir . "$prefix/src/ofa_kernel";
        }
        move($myrpm, $RPMS);
        $packages_info{$myrpm_name}{$ver}{'rpm_exist'} = 1;
    }
}

sub build_rpm_32
{
    my $name = shift @_;
    my $ver = shift @_;
    my $parent = $packages_info{$name}{'parent'};
    my $cmd;
    my $res = 0;
    my $sig = 0;
    my $TMPRPMS;

    my $pref_env32;
    my $ldflags32;
    my $cflags32;
    my $cppflags32;
    my $cxxflags32;
    my $fflags32;
    my $ldlibs32;

    $ldflags32    .= " -m32 -g -O2 -L/usr/lib";
    $cflags32     .= " -m32 -g -O2";
    $cppflags32   .= " -m32 -g -O2";
    $cxxflags32   .= " -m32 -g -O2";
    $fflags32     .= " -m32 -g -O2";
    $ldlibs32     .= " -m32 -g -O2 -L/usr/lib";

    if ($prefix ne $default_prefix) {
        $ldflags32 .= " -L$prefix/lib";
        $cflags32 .= " -I$prefix/include";
        $cppflags32 .= " -I$prefix/include";
    }

    if ($parent =~ m/libibverbs/ and $arch !~ /ppc/ and $DISTRO =~ /SLES/) {
        $cppflags32 .= " -march=i586 ";
    }

    $pref_env32 .= " LDFLAGS='$ldflags32'";
    $pref_env32 .= " CFLAGS='$cflags32'";
    $pref_env32 .= " CPPFLAGS='$cppflags32'";
    $pref_env32 .= " CXXFLAGS='$cxxflags32'";
    $pref_env32 .= " FFLAGS='$fflags32'";
    $pref_env32 .= " LDLIBS='$ldlibs32'";

    $cmd = "$pref_env32 rpmbuild --rebuild $rpmbuild_flags --define '_topdir $TOPDIR'";
    $cmd .= " --target $target_cpu32";
    $cmd .= " --define '_prefix $prefix'";
    $cmd .= " --define 'dist %{nil}'";
    $cmd .= " --define '_exec_prefix $prefix'";
    $cmd .= " --define '_sysconfdir $sysconfdir'";
    $cmd .= " --define '_usr $prefix'";
    $cmd .= " --define '_lib lib'";
    $cmd .= " --define '__arch_install_post %{nil}'";

    if ($parent =~ m/dapl/) {
        my $def_doc_dir = `rpm --eval '%{_defaultdocdir}'`;
        chomp $def_doc_dir;
        $cmd .= " --define '_prefix $prefix'";
        $cmd .= " --define '_exec_prefix $prefix'";
        $cmd .= " --define '_sysconfdir $sysconfdir'";
        $cmd .= " --define '_defaultdocdir $def_doc_dir/$main_packages{$parent}{$ver}{'name'}-$main_packages{$parent}{$ver}{'version'}'";
        $cmd .= " --define '_usr $prefix'";
    }

    if ( $parent =~ m/libibverbs/ and ($rpm_distro =~ m/xenserver|sles10/) ) {
        $cmd .= " --define 'configure_options --without-resolve-neigh' ";
    }

    if ($parent =~ m/libibverbs|libmlx|librdmacm/) {
        if ($with_valgrind) {
            $cmd .= " --define '_with_valgrind 1'";
        } else {
            if (($DISTRO !~ m/RHEL6.[45]/) or $disable_valgrind) {
                $cmd .= " --define '_disable_valgrind 1'";
            }
        }
    }

    $cmd .= " $main_packages{$parent}{$ver}{'srpmpath'}";

    print "Running $cmd\n" if ($verbose);
    open(LOG, "+>$ofedlogs/$parent-$ver.rpmbuild32bit.log");
    print LOG "Running $cmd\n";
    close LOG;
    system("$cmd >> $ofedlogs/$parent-$ver.rpmbuild32bit.log 2>&1");
    $res = $? >> 8;
    $sig = $? & 127;
    if ($sig or $res) {
        print RED "Failed to build $parent $ver RPM", RESET "\n";
        print RED "See $ofedlogs/$parent-$ver.rpmbuild32bit.log", RESET "\n";
        exit $ERROR;
    }

    $TMPRPMS = "$TOPDIR/RPMS/$target_cpu32";
    chomp $TMPRPMS;
    for my $myrpm ( <$TMPRPMS/*.rpm> ) {
        print "Created $myrpm\n" if ($verbose2);
        my ($myrpm_name, $myrpm_arch) = (split ' ', get_rpm_name_arch($myrpm));
        move($myrpm, $RPMS);
        $packages_info{$myrpm_name}{$ver}{'rpm_exist32'} = 1;
    }
}

# Build RPM from source RPM
sub build_rpm
{
    my $name = shift @_;
    my $ver = shift @_;
    my $cmd;
    my $res = 0;
    my $sig = 0;
    my $TMPRPMS;

    my $ldflags;
    my $cflags;
    my $cppflags;
    my $cxxflags;
    my $fflags;
    my $ldlibs;
    my $openmpi_comp_env;
    my $parent = $packages_info{$name}{'parent'};
    my $srpmdir;
    my $srpmpath_for_distro;

    print "Build $name $ver RPM\n" if ($verbose);

    my $pref_env = '';
    if ($prefix ne $default_prefix) {
        if ($parent ne "mvapich" and $parent ne "mvapich2" and $parent !~ /openmpi/ and $parent ne "infinipath-psm") {
            $ldflags .= "$optflags -L$prefix/lib64 -L$prefix/lib";
            $cflags .= "$optflags -I$prefix/include";
            $cppflags .= "$optflags -I$prefix/include";
        }
    }

    if (not $packages_info{$name}{$ver}{'rpm_exist'}) {

        if ($parent eq "ibacm") {
            if ($DISTRO eq "FC14") {
                $ldflags    = " -g -O2 -lpthread";
            }
            $pref_env .= "rdmascript=openibd"
        }

        if ($arch eq "ppc64") {
            if ($DISTRO =~ m/SLES/ and $dist_rpm_rel gt 15.2) {
                # SLES 10 SP1
                if ($parent eq "ibutils") {
                    $packages_info{'ibutils'}{'configure_options'} .= " LDFLAGS=-L/usr/lib/gcc/powerpc64-suse-linux/4.1.2/64";
                }
                if ($parent =~ /ibutils2|cc_mgr|ar_mgr/ and $DISTRO =~ m/SLES10/) {
                    $packages_info{'ibutils2'}{'configure_options'} .= " LDFLAGS=-L/usr/lib/gcc/powerpc64-suse-linux/4.1.2/64";
                    $ldflags    .= " $optflags -m64 -g -O2 -L/usr/lib64 -L/usr/lib/gcc/powerpc64-suse-linux/4.1.2/64";
                    $cflags     .= " $optflags -m64 -g -O2";
                    $cppflags   .= " $optflags -m64 -g -O2";
                    $cxxflags   .= " $optflags -m64 -g -O2";
                    $fflags     .= " $optflags -m64 -g -O2";
                    $ldlibs     .= " $optflags -m64 -g -O2 -L/usr/lib64";
                }
                if ($parent =~ /openmpi/) {
                    $openmpi_comp_env .= ' LDFLAGS="-m64 -O2 -L/usr/lib/gcc/powerpc64-suse-linux/4.1.2/64"';
                }
                if ($parent eq "sdpnetstat" or $parent eq "rds-tools" or $parent eq "rnfs-utils") {
                    $ldflags    = " -g -O2";
                    $cflags     = " -g -O2";
                    $cppflags   = " -g -O2";
                    $cxxflags   = " -g -O2";
                    $fflags     = " -g -O2";
                    $ldlibs     = " -g -O2";
                }
            }
            else {
                if ($parent =~ /sdpnetstat|rds-tools|rnfs-utils/) {
                    # Override compilation flags on RHEL 4.0 and 5.0 PPC64
                    $ldflags    = " -g -O2";
                    $cflags     = " -g -O2";
                    $cppflags   = " -g -O2";
                    $cxxflags   = " -g -O2";
                    $fflags     = " -g -O2";
                    $ldlibs     = " -g -O2";
                }
                elsif ($parent !~ /ibutils/) {
                    $ldflags    .= " $optflags -m64 -g -O2 -L/usr/lib64";
                    $cflags     .= " $optflags -m64 -g -O2";
                    $cppflags   .= " $optflags -m64 -g -O2";
                    $cxxflags   .= " $optflags -m64 -g -O2";
                    $fflags     .= " $optflags -m64 -g -O2";
                    $ldlibs     .= " $optflags -m64 -g -O2 -L/usr/lib64";
                }
            }
        }

        if ($parent =~ /mxm/) {
            $ldflags .= " -lz ";
        }


        if ($parent =~ /^lib/ and $arch =~ /x86_64|ppc64/) {
            $ldflags    = " -g -O3";
            $cflags     = " -g -O3";
            $cppflags   = " -g -O3";
            $cxxflags   = " -g -O3";
            $fflags     = " -g -O3";
            $ldlibs     = " -g -O3";
        }

        if ($ldflags) {
            $pref_env   .= " LDFLAGS='$ldflags'";
        }
        if ($cflags) {
            $pref_env   .= " CFLAGS='$cflags'";
        }
        if ($cppflags) {
            $pref_env   .= " CPPFLAGS='$cppflags'";
        }
        if ($cxxflags) {
            $pref_env   .= " CXXFLAGS='$cxxflags'";
        }
        if ($fflags) {
            $pref_env   .= " FFLAGS='$fflags'";
        }
        if ($ldlibs) {
            $pref_env   .= " LDLIBS='$ldlibs'";
        }

        $cmd = "$pref_env rpmbuild --rebuild $rpmbuild_flags --define '_topdir $TOPDIR'";
        $cmd .= " --define 'dist %{nil}'";
        $cmd .= " --target $target_cpu";

        # Prefix should be defined per package
        if ($parent eq "ibutils") {
            $packages_info{'ibutils'}{'configure_options'} .= " --with-osm=$prefix";
            if ($DISTRO =~ m/SLES12/) {
                my $tklib = `/bin/ls -1d /usr/lib*/tcl/tk8.6 2> /dev/null | head -1`;
                chomp $tklib;
                $packages_info{'ibutils'}{'configure_options'} .= " --with-tk-lib=$tklib" if ($tklib);
            }
            $cmd .= " --define '_prefix $ibutils_prefix'";
            $cmd .= " --define '_exec_prefix $ibutils_prefix'";
            $cmd .= " --define '_sysconfdir $sysconfdir'";
            $cmd .= " --define '_usr $ibutils_prefix'";
            $cmd .= " --define '_mandir $ibutils_prefix/share/man'";
            $cmd .= " --define 'build_ibmgtsim 1'";
            $cmd .= " --define '__arch_install_post %{nil}'";
        }
        elsif ($parent eq "ibutils2") {
            my $global_cflags = `rpm --eval '%{__global_cflags}'`;
            chomp $global_cflags;
            $cmd .= " --define '__global_cflags $global_cflags'";
            $cmd =~ s/-Wp,-D_FORTIFY_SOURCE=2//g;
            $cmd .= " --define '_prefix $ibutils2_prefix'";
            $cmd .= " --define '_with_umad $prefix'";
            $cmd .= " --define '__arch_install_post %{nil}'";
            $cmd .= " --define '_exec_prefix $ibutils2_prefix'";
            $cmd .= " --define '_sysconfdir $sysconfdir'";
            $cmd .= " --define '_usr $ibutils2_prefix'";
            $cmd .= " --define '_mandir $ibutils2_prefix/share/man'";
            if ($arch eq "ppc64" and $DISTRO =~ m/SLES10/) {
                $cmd .= " --define '__make make libibis_la_LIBADD=\"-libumad -L../../misc/tool_trace/ -ltt\"'";
            }
        }
        elsif ($parent eq "cc_mgr") {
            $packages_info{'cc_mgr'}{'configure_options'} .= " --with-ibutils2=$ibutils2_prefix";
            $packages_info{'cc_mgr'}{'configure_options'} .= " --with-osm=$prefix";
            $cmd .= " --define '_prefix $prefix'";
            $cmd .= " --define '_exec_prefix $prefix'";
            $cmd .= " --define '_sysconfdir $sysconfdir'";
            $cmd .= " --define '_usr $prefix'";
            $cmd .= " --define '__arch_install_post %{nil}'";
        }
        elsif ($parent eq "ar_mgr") {
            $packages_info{'ar_mgr'}{'configure_options'} .= " --with-ibutils2=$ibutils2_prefix";
            $packages_info{'ar_mgr'}{'configure_options'} .= " --with-osm=$prefix";
            $cmd .= " --define '_prefix $prefix'";
            $cmd .= " --define '_exec_prefix $prefix'";
            $cmd .= " --define '_sysconfdir $sysconfdir'";
            $cmd .= " --define '_usr $prefix'";
            $cmd .= " --define '__arch_install_post %{nil}'";
        }
        elsif ($parent eq "dump_pr") {
            $packages_info{'dump_pr'}{'configure_options'} .= " --with-osm=$prefix";
            $cmd .= " --define '_prefix $prefix'";
            $cmd .= " --define '_exec_prefix $prefix'";
            $cmd .= " --define '_sysconfdir $sysconfdir'";
            $cmd .= " --define '_usr $prefix'";
            $cmd .= " --define '__arch_install_post %{nil}'";
        }
        elsif ($parent =~ /mxm|fca|hcoll/) {
            $cmd =~ s/-Wp,-D_FORTIFY_SOURCE=2//g;
            $cmd =~ s/-D_FORTIFY_SOURCE=2//g;
#            $cmd .= " --define 'configure_opts --with-ofed=$prefix'"; # issue: 206604
            $cmd .= " --define '__arch_install_post %{nil}'";
            $cmd .= " --nodeps ";
        }
        elsif ($parent =~ /openshmem/) {
            if (not $knem_prefix) {
                $knem_prefix = `/bin/rpm -ql $knem_rpm | grep -w "sbin\$" 2>/dev/null | sed -e "s@/sbin@@" | head -1`;
                chomp $knem_prefix;
            }

            my $fca_opt = '';
            if (is_installed("fca")) {
                $fca_opt = "--with-fca=$fca_prefix";
            } else {
                $fca_opt = "--without-fca";
            }

            my $mxm_opt = '';
            if (is_installed("mxm")) {
                $mxm_opt = "--with-mxm=$mxm_prefix";
            } else {
                $mxm_opt = "--without-mxm";
            }

            my $oshmem_opt = "--with-platform=contrib/platform/mellanox/optimized --with-oshmem --with-oshmem-test ";

            $cmd .= " --define 'configure_opts $fca_opt  $mxm_opt --with-knem=$knem_prefix $pmi_opt $oshmem_opt'";
            $cmd .= " --define '__arch_install_post %{nil}'";
            $cmd .= " --nodeps ";
        }
        elsif ($parent =~ /bupc/) {

            # Do not add _prefix for bupc as it is hardcoded in the bupc.spec
            # it's preferred to build bupc with openmpi 1.6
            my $MPI_CC = "";
            my $mpi_1_6 = "";
            for my $mpiver (keys %{$main_packages{'openmpi'}}){
                $MPI_CC = "MPI_CC=$prefix/mpi/gcc/openmpi-$mpiver/bin/mpicc";
                if ($mpiver lt "1.7") {
                    $mpi_1_6 = $mpiver;
                }
            }
            if ($mpi_1_6 ne "") {
                $MPI_CC = "MPI_CC=$prefix/mpi/gcc/openmpi-$mpi_1_6/bin/mpicc";
            }

            my $enable_mxm = '';
            if (is_installed("mxm")) {
                $enable_mxm = "--enable-mxm --with-mxmhome=$mxm_prefix";
            } else {
                $enable_mxm = "--disable-mxm";
            }
            my $enable_fca = '';
            if (is_installed("fca")) {
                $enable_fca = "--with-fca-enabled-by-default=0 --with-fca=$fca_prefix";
            } else {
                $enable_fca = "--disable-fca";
            }

            $cmd .= " --define 'configure_opts --disable-aligned-segments --enable-sptr-struct $enable_mxm $enable_fca --enable-mpi-compat $MPI_CC $pmi_opt'";
            $cmd .= " --define '__arch_install_post %{nil}'";
            $cmd .= " --nodeps ";
        }
        elsif ( $parent eq "mvapich") {
            $cmd .= " --define '_name $name'";
            $cmd .= " --define 'compiler $compiler'";
            $cmd .= " --define 'openib_prefix $prefix'";
            $cmd .= " --define '_usr $prefix'";
            $cmd .= " --define 'use_mpi_selector 1'";
            $cmd .= " --define '__arch_install_post %{nil}'";
            if ($packages_info{'mvapich'}{'configure_options'}) {
                $cmd .= " --define 'configure_options $packages_info{'mvapich'}{'configure_options'}'";
            }
            $cmd .= " --define 'mpi_selector $prefix/bin/mpi-selector'";
            $cmd .= " --define '_prefix $prefix/mpi/$compiler/$parent-$main_packages{$parent}{$ver}{'version'}'";
        }
        elsif ($parent eq "mvapich2") {
            $cmd .= " --define '_name $name'";
            $cmd .= " --define 'impl $mvapich2_conf_impl'";

            if ($compiler eq "gcc") {
                if ($gcc{'gfortran'}) {
                    if ($arch eq "ppc64") {
                        $mvapich2_comp_env = 'CC="gcc -m64" CXX="g++ -m64" F77="gfortran -m64" FC="gfortran -m64"';
                    }

                    else {
                        $mvapich2_comp_env = "CC=gcc CXX=g++ F77=gfortran FC=gfortran";
                    }
                }

                elsif ($gcc{'g77'}) {
                    if ($arch eq "ppc64") {
                        $mvapich2_comp_env = 'CC="gcc -m64" CXX="g++ -m64" F77="g77 -m64" FC=/bin/false';
                    }

                    else {
                        $mvapich2_comp_env = "CC=gcc CXX=g++ F77=g77 FC=/bin/false";
                    }
                }

                else {
                    $mvapich2_comp_env .= " --disable-f77 --disable-fc";
                }
            }

            if ($mvapich2_conf_impl eq "ofa") {
                if ($verbose) {
                    print BLUE;
                    print "Building the MVAPICH2 RPM [OFA]...\n";
                    print RESET;
                }

                $cmd .= " --define 'rdma --with-rdma=gen2'";
                $cmd .= " --define 'ib_include --with-ib-include=$prefix/include'";
                $cmd .= " --define 'ib_libpath --with-ib-libpath=$prefix/lib";
                $cmd .= "64" if $arch =~ m/x86_64|ppc64/;
                $cmd .= "'";

                if ($mvapich2_conf_ckpt) {
                    $cmd .= " --define 'blcr 1'";
                    $cmd .= " --define 'blcr_include --with-blcr-include=$mvapich2_conf_blcr_home/include'";
                    $cmd .= " --define 'blcr_libpath --with-blcr-libpath=$mvapich2_conf_blcr_home/lib'";
                }
            }

            elsif ($mvapich2_conf_impl eq "udapl") {
                if ($verbose) {
                    print BLUE;
                    print "Building the MVAPICH2 RPM [uDAPL]...\n";
                    print RESET;
                }

                $cmd .= " --define 'rdma --with-rdma=udapl'";
                $cmd .= " --define 'dapl_include --with-dapl-include=$prefix/include'";
                $cmd .= " --define 'dapl_libpath --with-dapl-libpath=$prefix/lib";
                $cmd .= "64" if $arch =~ m/x86_64|ppc64/;
                $cmd .= "'";

                $cmd .= " --define 'cluster_size --with-cluster-size=$mvapich2_conf_vcluster'";
                $cmd .= " --define 'io_bus --with-io-bus=$mvapich2_conf_io_bus'";
                $cmd .= " --define 'link_speed --with-link=$mvapich2_conf_link_speed'";
                $cmd .= " --define 'dapl_provider --with-dapl-provider=$mvapich2_conf_dapl_provider'" if ($mvapich2_conf_dapl_provider);
            }

            if ($packages_info{'mvapich2'}{'configure_options'}) {
                $cmd .= " --define 'configure_options $packages_info{'mvapich2'}{'configure_options'}'";
            }

            $cmd .= " --define 'shared_libs 1'" if $mvapich2_conf_shared_libs;
            $cmd .= " --define 'romio 1'" if $mvapich2_conf_romio;
            $cmd .= " --define 'comp_env $mvapich2_comp_env'";
            $cmd .= " --define 'auto_req 0'";
            $cmd .= " --define 'mpi_selector $prefix/bin/mpi-selector'";
            $cmd .= " --define '_prefix $prefix/mpi/$compiler/$parent-$main_packages{$parent}{$ver}{'version'}'";
        }
        elsif ($parent =~ /openmpi/) {
            my $use_default_rpm_opt_flags = 1;
            my $openmpi_ldflags = '';
            my $openmpi_wrapper_cxx_flags;
            my $openmpi_lib;

            if ($arch =~ m/x86_64|ppc64/) {
                $openmpi_lib = 'lib64';
            }
            else {
                $openmpi_lib = 'lib';
            }

            if ($compiler eq "gcc") {
                if ($gcc{'g++'}) {
                }
                else {
                    $openmpi_comp_env .= " --disable-mpi-cxx";
                }
                if ($gcc{'gfortran'}) {
                }
                elsif ($gcc{'g77'}) {
                    $openmpi_comp_env .= " F77=g77 --disable-mpi-f90";
                }
                else {
                    $openmpi_comp_env .= " --disable-mpi-f77 --disable-mpi-f90";
                }
            }

            if ($arch eq "ppc64") {
                # In the ppc64 case, add -m64 to all the relevant
                # flags because it's not the default.  Also
                # unconditionally add $OMPI_RPATH because even if
                # it's blank, it's ok because there are other
                # options added into the ldflags so the overall
                # string won't be blank.
                $openmpi_comp_env .= ' CFLAGS="-m64 -O2" CXXFLAGS="-m64 -O2" FCFLAGS="-m64 -O2" FFLAGS="-m64 -O2"';
                $openmpi_comp_env .= ' --with-wrapper-ldflags="-g -O2 -m64 -L/usr/lib64" --with-wrapper-cflags=-m64';
                $openmpi_comp_env .= ' --with-wrapper-cxxflags=-m64 --with-wrapper-fflags=-m64 --with-wrapper-fcflags=-m64';
                $openmpi_wrapper_cxx_flags .= " -m64";
            }

            if ($arch =~ /x86_64/) {
                if (is_installed("fca")) {
                    $openmpi_comp_env .= " --with-fca=$fca_prefix";
                }
                if (is_installed("hcoll")) {
                    $openmpi_comp_env .= " --with-hcoll=$hcoll_prefix";
                }
            }

            if ($openmpi_wrapper_cxx_flags) {
                $openmpi_comp_env .= " --with-wrapper-cxxflags=\"$openmpi_wrapper_cxx_flags\"";
            }

            if (not $knem_prefix) {
                $knem_prefix = `/bin/rpm -ql $knem_rpm | grep -w "sbin\$" 2>/dev/null | sed -e "s@/sbin@@" | head -1`;
                chomp $knem_prefix;
            }
            if ($arch =~ /x86_64/) {
                if (is_installed("mxm")) {
                    $openmpi_comp_env .= "  --with-mxm=$mxm_prefix";
                }
                if (is_installed("$knem_rpm")) {
                    $openmpi_comp_env .= "  --with-knem=$knem_prefix";
                }
            }
            $openmpi_comp_env .= " --with-platform=contrib/platform/mellanox/optimized $pmi_opt";

            $cmd .= " --define '_name $name'";
            $cmd .= " --define 'mpi_selector $prefix/bin/mpi-selector'";
            $cmd .= " --define 'use_mpi_selector 1'";
            $cmd .= " --define 'install_shell_scripts 1'";
            $cmd .= " --define 'shell_scripts_basename mpivars'";
            $cmd .= " --define '_usr $prefix'";
            $cmd .= " --define 'ofed 0'";
            $cmd .= " --define '_prefix $prefix/mpi/$compiler/$parent-$main_packages{$parent}{$ver}{'version'}'";
            $cmd .= " --define '_defaultdocdir $prefix/mpi/$compiler/$parent-$main_packages{$parent}{$ver}{'version'}'";
            $cmd .= " --define '_mandir %{_prefix}/share/man'";
            $cmd .= " --define '_datadir %{_prefix}/share'";
            $cmd .= " --define 'mflags -j 4'";
            $cmd .= " --define 'configure_options $packages_info{'openmpi'}{'configure_options'} $openmpi_ldflags $openmpi_comp_env '";
            $cmd .= " --define 'use_default_rpm_opt_flags $use_default_rpm_opt_flags'";
        }
        elsif ($parent eq "mpitests") {
            # name formation is:  mpitests_name__<version with _ instead of . >
            my $mpi = $name;
            $mpi =~ s/mpitests_//g; # mpi = name_ver
            my $base = (split('__', $mpi))[0];
            my $baseVer = (split('__', $mpi))[1];
            $baseVer =~ s/_/./g;

            $cmd .= " --define '_name $name'";
            $cmd .= " --define 'root_path /'";
            $cmd .= " --define '_usr $prefix'";
            $cmd .= " --define 'path_to_mpihome $prefix/mpi/$compiler/$base-$main_packages{$base}{$baseVer}{'version'}'";
        }
        elsif ($parent eq "mpi-selector") {
            $cmd .= " --define '_prefix $prefix'";
            $cmd .= " --define '_exec_prefix $prefix'";
            $cmd .= " --define '_sysconfdir $sysconfdir'";
            $cmd .= " --define '_usr $prefix'";
            $cmd .= " --define 'shell_startup_dir /etc/profile.d'";
        }
        elsif ($parent =~ m/dapl/) {
            my $def_doc_dir = `rpm --eval '%{_defaultdocdir}'`;
            chomp $def_doc_dir;
            $cmd .= " --define '_prefix $prefix'";
            $cmd .= " --define '_exec_prefix $prefix'";
            $cmd .= " --define '_sysconfdir $sysconfdir'";
            $cmd .= " --define '_defaultdocdir $def_doc_dir/$main_packages{$parent}{$ver}{'name'}-$main_packages{$parent}{$ver}{'version'}'";
            $cmd .= " --define '_usr $prefix'";
        }
        elsif ($parent =~ /libibprof/) {
            $cmd .= " --define 'configure_opts --with-ofed=$prefix'";
            $cmd .= " --nodeps ";
        }
        elsif ($parent =~ /fabric-collector/) {
            $cmd .= " --define '_prefix /opt/mellanox/fabric_collector'";
        }
        else {
            $cmd .= " --define '_prefix $prefix'";
            $cmd .= " --define '_exec_prefix $prefix'";
            $cmd .= " --define '_sysconfdir $sysconfdir'";
            $cmd .= " --define '_usr $prefix'";
        }

        if ($parent eq "mft") {
            $cmd .= " --define 'nodevmon 1'";
        }

        if ($parent eq "librdmacm") {
            if ( $packages_info{'ibacm'}{'selected'}) {
                $packages_info{'librdmacm'}{'configure_options'} .= " --with-ib_acm";
            }
        }

        if ($packages_info{$parent}{'configure_options'} or $user_configure_options) {
            $cmd .= " --define 'configure_options $packages_info{$parent}{'configure_options'} $user_configure_options'";
        }

        if ( $parent =~ m/libibverbs/ and ($rpm_distro =~ m/xenserver|sles10/) ) {
            $cmd .= " --define 'configure_options --without-resolve-neigh' ";
        }

        if ($parent =~ m/libibverbs|libmlx|librdmacm/) {
            if ($with_valgrind) {
                $cmd .= " --define '_with_valgrind 1'";
            } else {
                if (($DISTRO !~ m/RHEL6.[45]/) or $disable_valgrind) {
                    $cmd .= " --define '_disable_valgrind 1'";
                }
            }
        }
        if ($parent =~ /mxm/) {
            if ($with_valgrind or (($DISTRO =~ m/RHEL6.[45]/) and (not $disable_valgrind))) {
                $cmd .= " --with valgrind ";
            }
        }

        $cmd .= " $main_packages{$parent}{$ver}{'srpmpath'}";

        print "Running $cmd\n" if ($verbose);
        open(LOG, "+>$ofedlogs/$parent-$ver.rpmbuild.log");
        print LOG "Running $cmd\n";
        close LOG;
        system("$cmd >> $ofedlogs/$parent-$ver.rpmbuild.log 2>&1");
        $res = $? >> 8;
        $sig = $? & 127;
        if ($sig or $res) {
            print RED "Failed to build $parent $ver RPM", RESET "\n";
            print RED "See $ofedlogs/$parent-$ver.rpmbuild.log", RESET "\n";
            exit 1;
        }

        $TMPRPMS = "$TOPDIR/RPMS/$target_cpu";
        chomp $TMPRPMS;

        print "TMPRPMS $TMPRPMS\n" if ($verbose2);

        for my $myrpm ( <$TMPRPMS/*.rpm> ) {
            print "Created $myrpm\n" if ($verbose2);
            my ($myrpm_name, $myrpm_arch) = (split ' ', get_rpm_name_arch($myrpm));
            move($myrpm, $RPMS);
            $packages_info{$myrpm_name}{$ver}{'rpm_exist'} = 1;
        }
    }

    if ($build32 and $packages_info{$name}{'install32'} and
        not $packages_info{$name}{$ver}{'rpm_exist32'}) {
        build_rpm_32($name, $ver);
    }
}

sub install_kernel_rpm
{
    my $name = shift @_;
    my $ver = shift @_;
    my $cmd;
    my $res = 0;
    my $sig = 0;

    my $version = $main_packages{$packages_info{$name}{'parent'}}{$ver}{'version'};
    my $release = $kernel_rel;

    if ($name =~ /kernel-ib/) {
        $release .= '_' . $main_packages{$packages_info{$name}{'parent'}}{$ver}{'release'};
    }

    my $arch = $target_cpu;

    # will enter this block if  KMP is enabled and this is a package that supports kmp
    if ($name =~ /ofa_kernel|kernel-ib|knem-mlnx|kernel-mft-mlnx|ummunotify-mlnx/ or ( $name =~ /iser|srp/ and $kmp )) {
        # WA required to get rpmpath for KMP RPMs
        set_existing_rpms();
        if ($DISTRO =~ /RHEL5|XenServer/ and $target_cpu eq "i386") {
            $arch = 'i686';
        }

        if ($DISTRO =~ /SLES/) {
            my $kver =`rpm -q --queryformat "[%{VERSION}]\n" kernel-source | head -1`;
            chomp $kver;
            if ($name =~ /kmp/ and  $kver !~ /$kernel_rel/) {
                return;
            }

        }

        # KMP packages that have only kmod/kmp rpms
        if ($name !~ /kernel-mft-mlnx|iser|srp/) {
            $cmd = "rpm $rpminstall_parameter -vh $rpminstall_flags";
            $cmd .= " --nodeps";
            $cmd .= " $main_packages{$name}{$ver}{'rpmpath'}";

            print BLUE "Installing $name $ver RPM", RESET "\n" if ($verbose);
            print BLUE "cmd: $cmd", RESET "\n" if ($verbose);
            print "Running $cmd\n" if ($verbose2);
            system("$cmd > $ofedlogs/$name-$ver.rpminstall.log 2>&1");
            $res = $? >> 8;
            $sig = $? & 127;
            if ($sig or $res) {
                print RED "Failed to install $name $ver RPM", RESET "\n";
                print RED "See $ofedlogs/$name-$ver.rpminstall.log", RESET "\n";
                exit $ERROR;
            }
            system("cat $ofedlogs/$name-$ver.rpminstall.log") if (not $quiet);
            system("/sbin/depmod > /dev/null 2>&1");

            return if ($name eq 'mlnx-ofa_kernel-devel');
        }

        my @kmp_packages;
        if ($DISTRO =~ /SLES|SUSE/) {
            my $kver = `rpm -q --queryformat "[%{VERSION}]\n" kernel-source | head -1`;
            chomp $kver;
            @kmp_packages = <$RPMS/$name-kmp*.$arch.rpm>;
        } else {
            @kmp_packages = <$RPMS/kmod*$name*.$arch.rpm>;
        }

        for my $kmp_package (@kmp_packages) {
            $cmd = "rpm $rpminstall_parameter -vh $rpminstall_flags";
            $cmd .= " --nodeps";
            $cmd .= " $kmp_package";

            my $kmpname =`rpm -qp --queryformat "[%{NAME}]\n" $kmp_package`;
            chomp $kmpname;
            print BLUE "Installing $kmpname $ver RPM", RESET "\n" if ($verbose);
            print "Running $cmd\n" if ($verbose2);
            system("$cmd > $ofedlogs/$kmpname-$ver.rpminstall.log 2>&1");
            $res = $? >> 8;
            $sig = $? & 127;
            if ($sig or $res) {
                print RED "Failed to install $name $ver RPM", RESET "\n";
                print RED "See $ofedlogs/$kmpname-$ver.rpminstall.log", RESET "\n";
                exit $ERROR;
            }
            system("cat $ofedlogs/$kmpname-$ver.rpminstall.log") if (not $quiet);
            system("/sbin/depmod > /dev/null 2>&1");
        }
        return;
    }

    if ($name =~ /mlx4_accl/) {
        $release = $main_packages{$packages_info{$name}{'parent'}}{$ver}{'release'} . '.' . $release;
    }

    my $package = "$RPMS/$name-$version-$release.$target_cpu.rpm";

    if (not -f $package) {
        print RED "$package does not exist", RESET "\n";
        exit $ERROR;
    }

    $cmd = "rpm -iv $rpminstall_flags";
    $cmd .= " --nodeps";
    $cmd .= " $package";

    print "Running $cmd\n" if ($verbose);
    system("$cmd > $ofedlogs/$name-$ver.rpminstall.log 2>&1");
    $res = $? >> 8;
    $sig = $? & 127;
    if ($sig or $res) {
        print RED "Failed to install $name $ver RPM", RESET "\n";
        print RED "See $ofedlogs/$name-$ver.rpminstall.log", RESET "\n";
        exit $ERROR;
    }
}

sub install_rpm_32
{
    my $name = shift @_;
    my $ver = shift @_;
    my $cmd;
    my $res = 0;
    my $sig = 0;
    my $package;

    my $version = $main_packages{$packages_info{$name}{'parent'}}{$ver}{'version'};
    my $release = $main_packages{$packages_info{$name}{'parent'}}{$ver}{'release'};

    $package = "$RPMS/$name-$version-$release.$target_cpu32.rpm";
    if (not -f $package) {
        print RED "$package does not exist", RESET "\n";
        # exit 1;
    }

    $cmd = "rpm $rpminstall_parameter -v $rpminstall_flags";
    if ($DISTRO =~ m/SLES|SUSE/) {
        $cmd .= " --force";
    }
    $cmd .= " $package";

    print "Running $cmd\n" if ($verbose);
    system("$cmd > $ofedlogs/$name-$ver.rpminstall.log 2>&1");
    $res = $? >> 8;
    $sig = $? & 127;
    if ($sig or $res) {
        print RED "Failed to install $name $ver RPM", RESET "\n";
        print RED "See $ofedlogs/$name-$ver.rpminstall.log", RESET "\n";
        exit 1;
    }
}

# Install required RPM
sub install_rpm
{
    my $name = shift @_;
    my $ver = shift @_;
    my $tmp_name;
    my $cmd;
    my $res = 0;
    my $sig = 0;
    my $package;

    my $version = $main_packages{$packages_info{$name}{'parent'}}{$ver}{'version'};
    my $release = $main_packages{$packages_info{$name}{'parent'}}{$ver}{'release'};

    $package = "$RPMS/$name-$version-$release.$target_cpu.rpm";

    if (not -f $package) {
        print RED "$package does not exist", RESET "\n";
        exit $ERROR;
    }

    if ($name eq "mpi-selector") {
        $cmd = "rpm $rpminstall_parameter -v $rpminstall_flags --force";
    } elsif ($name =~ /openmpi|mvapich2/) {
        if (is_installed("$name-$version")) {
            system("rpm -e --nodeps $name-$version");
        }
        $cmd = "rpm -iv $rpminstall_flags --force";
    } else {
        if ($name eq "opensm" and $DISTRO eq "DEBIAN") {
            $rpminstall_flags .= " --nopost";
        }
        $cmd = "rpm $rpminstall_parameter -v $rpminstall_flags";
    }

    $cmd .= " --nodeps";

    if ($name =~ /ibacm/) {
        $cmd .= " --noscripts";
    }

    $cmd .= " $package";

    print "Running $cmd\n" if ($verbose);
    system("$cmd > $ofedlogs/$name-$ver.rpminstall.log 2>&1");
    $res = $? >> 8;
    $sig = $? & 127;
    if ($sig or $res) {
        print RED "Failed to install $name $ver RPM", RESET "\n";
        print RED "See $ofedlogs/$name-$ver.rpminstall.log", RESET "\n";
        exit $ERROR;
    }

    if ($build32 and $packages_info{$name}{'install32'}) {
        install_rpm_32($name, $ver);
    }
}

sub print_package_info
{
    print "\n\nDate:" . localtime(time) . "\n";
    for my $key ( keys %main_packages ) {
        print "$key:\n";
        print "======================================\n";
        for my $ver ( keys %{$main_packages{$key}} ) {
            print "-------------------------------\n";
            my %pack = %{$main_packages{$key}{$ver}};
            for my $subkey ( keys %pack ) {
                print $subkey . ' = ' . $pack{$subkey} . "\n";
            }
        }
        print "\n";
    }
}

sub is_installed_deb
{
    my $res = 0;
    my $name = shift @_;
    my $result = `dpkg-query -W -f='\${version}' $name`;
    if (($result eq "") && ($? == 0) ){
        $res = 1;
    }
    return not $res;
}

sub is_installed
{
    my $res = 0;
    my $name = shift @_;

    if ($DISTRO eq "DEBIAN") {
        system("dpkg-query -W -f='\${Package} \${Version}\n' $name > /dev/null 2>&1");
    }
    else {
        system("rpm -q $name > /dev/null 2>&1");
    }
    $res = $? >> 8;

    return not $res;
}

sub count_ports
{
    my $cnt = 0;
    open(LSPCI, "/sbin/lspci -n|");

    while (<LSPCI>) {
        if (/15b3:6282/) {
            $cnt += 2;  # infinihost iii ex mode
        }
        elsif (/15b3:5e8c|15b3:6274/) {
            $cnt ++;    # infinihost iii lx mode
        }
        elsif (/15b3:5a44|15b3:6278/) {
            $cnt += 2;  # infinihost mode
        }
        elsif (/15b3:6340|15b3:634a|15b3:6354|15b3:6732|15b3:673c|15b3:6746|15b3:6750|15b3:1003/) {
            $cnt += 2;  # connectx
        }
    }
    close (LSPCI);

    return $cnt;
}

sub is_valid_ipv4
{
    my $ipaddr = shift @_;

    if( $ipaddr =~ m/^(\d\d?\d?)\.(\d\d?\d?)\.(\d\d?\d?)\.(\d\d?\d?)/ ) {
        if($1 <= 255 && $2 <= 255 && $3 <= 255 && $4 <= 255) {
            return 0;
        }
    }
    return 1;
}

sub get_net_config
{
    my $interface = shift @_;

    open(IFCONFIG, "/sbin/ifconfig $interface |") or die "Failed to run /sbin/ifconfig $interface: $!";
    while (<IFCONFIG>) {
        next if (not m/inet addr:/);
        my $line = $_;
        chomp $line;
        $ifcfg{$interface}{'IPADDR'} = (split (' ', $line))[1];
        $ifcfg{$interface}{'IPADDR'} =~ s/addr://g;
        $ifcfg{$interface}{'BROADCAST'} = (split (' ', $line))[2];
        $ifcfg{$interface}{'BROADCAST'} =~ s/Bcast://g;
        $ifcfg{$interface}{'NETMASK'} = (split (' ', $line))[3];
        $ifcfg{$interface}{'NETMASK'} =~ s/Mask://g;
        if ($DISTRO =~ /RHEL6|RHEL7/) {
            $ifcfg{$interface}{'NM_CONTROLLED'} = "yes";
            $ifcfg{$interface}{'TYPE'} = "InfiniBand";
        }
    }
    close(IFCONFIG);
}

sub is_carrier
{
    my $ifcheck = shift @_;
    open(IFSTATUS, "ip link show dev $ifcheck |");
    while ( <IFSTATUS> ) {
        next unless m@(\s$ifcheck).*@;
        if( m/NO-CARRIER/ or not m/UP/ ) {
            close(IFSTATUS);
            return 0;
        }
    }
    close(IFSTATUS);
    return 1;
}

sub config_interface
{
    my $interface = shift @_;
    my $ans;
    my $dev = "ib$interface";
    my $target = "$network_dir/ifcfg-$dev";
    my $ret;
    my $ip;
    my $nm;
    my $nw;
    my $bc;
    my $onboot = 1;
    my $found_eth_up = 0;

    if ($interactive) {
        print "\nDo you want to configure $dev? [Y/n]:";
        $ans = getch();
        if ($ans =~ m/[nN]/) {
            return;
        }
        if (-e $target) {
            print BLUE "\nThe current IPoIB configuration for $dev is:\n";
            open(IF,$target);
            while (<IF>) {
                print $_;
            }
            close(IF);
            print "\nDo you want to change this configuration? [y/N]:", RESET;
            $ans = getch();
            if ($ans !~ m/[yY]/) {
                return;
            }
        }
        print "\nEnter an IP Adress: ";
        $ip = <STDIN>;
        chomp $ip;
        $ret = is_valid_ipv4($ip);
        while ($ret) {
            print "\nEnter a valid IPv4 Adress: ";
            $ip = <STDIN>;
            chomp $ip;
            $ret = is_valid_ipv4($ip);
        }
        print "\nEnter the Netmask: ";
        $nm = <STDIN>;
        chomp $nm;
        $ret = is_valid_ipv4($nm);
        while ($ret) {
            print "\nEnter a valid Netmask: ";
            $nm = <STDIN>;
            chomp $nm;
            $ret = is_valid_ipv4($nm);
        }
        print "\nEnter the Network: ";
        $nw = <STDIN>;
        chomp $nw;
        $ret = is_valid_ipv4($nw);
        while ($ret) {
            print "\nEnter a valid Network: ";
            $nw = <STDIN>;
            chomp $nw;
            $ret = is_valid_ipv4($nw);
        }
        print "\nEnter the Broadcast Adress: ";
        $bc = <STDIN>;
        chomp $bc;
        $ret = is_valid_ipv4($bc);
        while ($ret) {
            print "\nEnter a valid Broadcast Adress: ";
            $bc = <STDIN>;
            chomp $bc;
            $ret = is_valid_ipv4($bc);
        }
        print "\nStart Device On Boot? [Y/n]:";
        $ans = getch();
        if ($ans =~ m/[nN]/) {
            $onboot = 0;
        }

        print GREEN "\nSelected configuration:\n";
        print "DEVICE=$dev\n";
        print "IPADDR=$ip\n";
        print "NETMASK=$nm\n";
        print "NETWORK=$nw\n";
        print "BROADCAST=$bc\n";
        if ($DISTRO =~ /RHEL6|RHEL7/) {
            print "NM_CONTROLLED=yes\n";
            print "TYPE=InfiniBand\n";
        }
        if ($onboot) {
            print "ONBOOT=yes\n";
        }
        else {
            print "ONBOOT=no\n";
        }
        print "\nDo you want to save the selected configuration? [Y/n]:";
        $ans = getch();
        if ($ans =~ m/[nN]/) {
            return;
        }
    }
    else {
        if (not $config_net_given) {
            return;
        }
        print "Going to update $target\n" if ($verbose2);
        if ($ifcfg{$dev}{'LAN_INTERFACE'}) {
            $eth_dev = $ifcfg{$dev}{'LAN_INTERFACE'};
            if (not -e "/sys/class/net/$eth_dev") {
                print "Device $eth_dev is not present\n" if (not $quiet);
                return;
            }
            if ( is_carrier ($eth_dev) ) {
                $found_eth_up = 1;
            }
        }
        else {
            # Take the first existing Eth interface
            my @eth_devs = </sys/class/net/eth*>;
            for my $tmp_dev ( @eth_devs ) {
                $eth_dev = $tmp_dev;
                $eth_dev =~ s@/sys/class/net/@@g;
                if ( is_carrier ($eth_dev) ) {
                    $found_eth_up = 1;
                    last;
                }
            }
        }

        if ($found_eth_up) {
            get_net_config("$eth_dev");
        }

        if (not $ifcfg{$dev}{'IPADDR'}) {
            print "IP address is not defined for $dev\n" if ($verbose2);
            print "Skipping $dev configuration...\n" if ($verbose2);
            return;
        }
        if (not $ifcfg{$dev}{'NETMASK'}) {
            print "Netmask is not defined for $dev\n" if ($verbose2);
            print "Skipping $dev configuration...\n" if ($verbose2);
            return;
        }
        if (not $ifcfg{$dev}{'NETWORK'}) {
            print "Network is not defined for $dev\n" if ($verbose2);
            print "Skipping $dev configuration...\n" if ($verbose2);
            return;
        }
        if (not $ifcfg{$dev}{'BROADCAST'}) {
            print "Broadcast address is not defined for $dev\n" if ($verbose2);
            print "Skipping $dev configuration...\n" if ($verbose2);
            return;
        }

        my @ipib = (split('\.', $ifcfg{$dev}{'IPADDR'}));
        my @nmib = (split('\.', $ifcfg{$dev}{'NETMASK'}));
        my @nwib = (split('\.', $ifcfg{$dev}{'NETWORK'}));
        my @bcib = (split('\.', $ifcfg{$dev}{'BROADCAST'}));

        my @ipeth = (split('\.', $ifcfg{$eth_dev}{'IPADDR'}));
        my @nmeth = (split('\.', $ifcfg{$eth_dev}{'NETMASK'}));
        my @nweth = (split('\.', $ifcfg{$eth_dev}{'NETWORK'}));
        my @bceth = (split('\.', $ifcfg{$eth_dev}{'BROADCAST'}));

        for (my $i = 0; $i < 4 ; $i ++) {
            if ($ipib[$i] =~ m/\*/) {
                if ($ipeth[$i] =~ m/(\d\d?\d?)/) {
                    $ipib[$i] = $ipeth[$i];
                }
                else {
                    print "Cannot determine the IP address of the $dev interface\n" if (not $quiet);
                    return;
                }
            }
            if ($nmib[$i] =~ m/\*/) {
                if ($nmeth[$i] =~ m/(\d\d?\d?)/) {
                    $nmib[$i] = $nmeth[$i];
                }
                else {
                    print "Cannot determine the netmask of the $dev interface\n" if (not $quiet);
                    return;
                }
            }
            if ($bcib[$i] =~ m/\*/) {
                if ($bceth[$i] =~ m/(\d\d?\d?)/) {
                    $bcib[$i] = $bceth[$i];
                }
                else {
                    print "Cannot determine the broadcast address of the $dev interface\n" if (not $quiet);
                    return;
                }
            }
            if ($nwib[$i] !~ m/(\d\d?\d?)/) {
                $nwib[$i] = $nweth[$i];
            }
        }

        $ip = "$ipib[0].$ipib[1].$ipib[2].$ipib[3]";
        $nm = "$nmib[0].$nmib[1].$nmib[2].$nmib[3]";
        $nw = "$nwib[0].$nwib[1].$nwib[2].$nwib[3]";
        $bc = "$bcib[0].$bcib[1].$bcib[2].$bcib[3]";

        print GREEN "IPoIB configuration for $dev\n";
        print "DEVICE=$dev\n";
        print "IPADDR=$ip\n";
        print "NETMASK=$nm\n";
        print "NETWORK=$nw\n";
        print "BROADCAST=$bc\n";
        if ($onboot) {
            print "ONBOOT=yes\n";
        }
        else {
            print "ONBOOT=no\n";
        }
        print RESET "\n";
    }

    open(IF, ">$target") or die "Can't open $target: $!";
    if ($DISTRO =~ m/SLES|SUSE/) {
        print IF "BOOTPROTO='static'\n";
        print IF "IPADDR='$ip'\n";
        print IF "NETMASK='$nm'\n";
        print IF "NETWORK='$nw'\n";
        print IF "BROADCAST='$bc'\n";
        print IF "REMOTE_IPADDR=''\n";
        if ($onboot) {
            print IF "STARTMODE='onboot'\n";
        }
        else {
            print IF "STARTMODE='manual'\n";
        }
        print IF "WIRELESS=''\n";
    }
    else {
        print IF "DEVICE=$dev\n";
        print IF "BOOTPROTO=static\n";
        print IF "IPADDR=$ip\n";
        print IF "NETMASK=$nm\n";
        print IF "NETWORK=$nw\n";
        print IF "BROADCAST=$bc\n";
        if ($DISTRO =~ /RHEL6|RHEL7/) {
            print IF "NM_CONTROLLED=yes\n";
            print IF "TYPE=InfiniBand\n";
        }
        if ($onboot) {
            print IF "ONBOOT=yes\n";
        }
        else {
            print IF "ONBOOT=no\n";
        }
    }
    close(IF);
}

sub ipoib_config
{
    if ($interactive) {
        print BLUE;
        print "\nThe default IPoIB interface configuration is based on DHCP.";
        print "\nNote that a special patch for DHCP is required for supporting IPoIB.";
        print "\nThe patch is available under docs/dhcp";
        print "\nIf you do not have DHCP, you must change this configuration in the following steps.";
        print RESET "\n";
    }

    my $ports_num = count_ports();
    for (my $i = 0; $i < $ports_num; $i++ ) {
        config_interface($i);
    }

    if ($interactive) {
        print GREEN "IPoIB interfaces configured successfully",RESET "\n";
        print "Press any key to continue ...";
        getch();
    }

    if (-f "/etc/sysconfig/network/config") {
        my $nm = `grep ^NETWORKMANAGER=yes /etc/sysconfig/network/config`;
        chomp $nm;
        if ($nm) {
            print RED "Please set NETWORKMANAGER=no in the /etc/sysconfig/network/config", RESET "\n";
        }
    }

}

sub uninstall_mlnx_en
{
    my $res = 0;
    my $sig = 0;
    my $cnt = 0;

    if ( -f "/sbin/mlnx_en_uninstall.sh" ) {
        print BLUE "Uninstalling MLNX_EN driver", RESET "\n" if (not $quiet);
        system("yes | /sbin/mlnx_en_uninstall.sh > $ofedlogs/mlnx_en_uninstall.log 2>&1");
        $res = $? >> 8;
        $sig = $? & 127;
        if ($sig or $res) {
            print RED "Failed to uninstall MLNX_EN driver", RESET "\n";
            print RED "See $ofedlogs/mlnx_en_uninstall.log", RESET "\n";
            exit $ERROR;
        }
    }

    my $mlnx_en_cnt = 0;
    my $mlnx_en_rpms;
    my @other_mlnx_en_rpms = `rpm -qa *mlnx-en* 2> /dev/null`;
    for my $package (@mlnx_en_packages, @other_mlnx_en_rpms) {
        chomp $package;
        if (is_installed($package)) {
            $mlnx_en_rpms .= " $package";
            $mlnx_en_cnt ++;
        }
    }

    if ($mlnx_en_cnt) {
            my $cmd = "rpm -e --allmatches --nodeps";
            $cmd .= " $mlnx_en_rpms";
            print BLUE "Uninstalling MLNX_EN driver", RESET "\n" if (not $quiet);
            system("$cmd >> $ofedlogs/mlnx_en_uninstall.log 2>&1");
            $res = $? >> 8;
            $sig = $? & 127;
            if ($sig or $res) {
                print RED "Failed to uninstall MLNX_EN driver", RESET "\n";
                print RED "See $ofedlogs/mlnx_en_uninstall.log", RESET "\n";
                exit $ERROR;
            }
    }

}

sub uninstall_mft
{
    my $res = 0;
    my $sig = 0;
    my $cnt = 0;

    my $mft_rpms;
    for my $package (@mft_packages) {
        chomp $package;
        if (is_installed($package)) {
            $mft_rpms .= " $package";
            if (not $selected_for_uninstall{$package}) {
                push (@packages_to_uninstall, $package);
                $selected_for_uninstall{$package} = 1;
            }
        }
    }

    if (open (KMP_RPMS, 'rpm -qa --queryformat "[%{NAME}]\n" *kernel-mft* |')) {
        my $kmp_cnt = 0;
        my $kmp_rpms;
        while(<KMP_RPMS>) {
            chomp $_;
            $kmp_rpms .= " $_";
            $kmp_cnt ++;
        }
        close KMP_RPMS;

	    if ($kmp_cnt) {
            system("rpm -e --allmatches $kmp_rpms >> $ofedlogs/kmp_rpms_uninstall.log 2>&1");
            $res = $? >> 8;
            $sig = $? & 127;
            if ($sig or $res) {
                print RED "Failed to uninstall kernel-mft-mlnx KMP RPMs", RESET "\n";
                exit $ERROR;
            }
        }
    }
}

sub force_uninstall
{
    my $res = 0;
    my $sig = 0;
    my $cnt = 0;
    my @other_ofed_rpms = `rpm -qa 2> /dev/null | grep -wE "rdma|ofed|openib"`;
    my $cmd = "rpm -e --allmatches --nodeps";

    if (is_installed("ofed")) {
        # W/A for SLES 10 SP4 in-box ofed RPM uninstall issue
        $cmd .= " --noscripts";
    }

    for my $package (@all_packages, @hidden_packages, @prev_ofed_packages, @other_ofed_rpms, @distro_ofed_packages) {
        chomp $package;
        next if ($package eq "mpi-selector");
        if (is_installed($package)) {
            push (@packages_to_uninstall, $package);
            $selected_for_uninstall{$package} = 1;
        }
        if (is_installed("$package-static")) {
            push (@packages_to_uninstall, "$package-static");
            $selected_for_uninstall{$package} = 1;
        }
        if ($suffix_32bit and is_installed("$package$suffix_32bit")) {
            push (@packages_to_uninstall,"$package$suffix_32bit");
            $selected_for_uninstall{$package} = 1;
        }
        if ($suffix_64bit and is_installed("$package$suffix_64bit")) {
            push (@packages_to_uninstall,"$package$suffix_64bit");
            $selected_for_uninstall{$package} = 1;
        }
    }

    for my $package (@packages_to_uninstall) {
        get_requires($package);
    }

    if (not $force and keys %non_ofed_for_uninstall) {
        print "\nError: One or more packages depends on MLNX_OFED.\nThose packages should be removed before uninstalling MLNX_OFED:\n\n";
        print join(" ", (keys %non_ofed_for_uninstall)) . "\n\n";
        print "To force uninstallation use '--force' flag.\n";
        exit $NONOFEDRPMS;
    }

    for my $package (@packages_to_uninstall, @dependant_packages_to_uninstall) {
        if (is_installed("$package")) {
            $cmd .= " $package";
            $cnt ++;
        }
    }

    if ($cnt) {
        print "\n$cmd\n" if (not $quiet);
        open (LOG, "+>$ofedlogs/ofed_uninstall.log");
        print LOG "$cmd\n";
        close LOG;
        system("$cmd >> $ofedlogs/ofed_uninstall.log 2>&1");
        $res = $? >> 8;
        $sig = $? & 127;
        if ($sig or $res) {
            print RED "Failed to uninstall the previous installation", RESET "\n";
            print RED "See $ofedlogs/ofed_uninstall.log", RESET "\n";
            exit $ERROR;
        }
    }
}

sub uninstall
{
    my $res = 0;
    my $sig = 0;
    my $distro_rpms = '';

    return 0 if (not $uninstall);

    uninstall_mlnx_en();

    uninstall_mft();

    my $ofed_uninstall = `which ofed_uninstall.sh 2> /dev/null`;
    chomp $ofed_uninstall;
    if (-f "$ofed_uninstall") {
        print BLUE "Uninstalling the previous version of $PACKAGE", RESET "\n" if (not $quiet);
        if ($force) {
                system("yes | ofed_uninstall.sh --force >> $ofedlogs/ofed_uninstall.log 2>&1");
        } else {
                system("yes | ofed_uninstall.sh >> $ofedlogs/ofed_uninstall.log 2>&1");
        }
        $res = $? >> 8;
        $sig = $? & 127;
        if ($sig or $res) {
            if ($res == 174) {
                print "Error: One or more packages depends on MLNX_OFED.\nThose packages should be removed before uninstalling MLNX_OFED.\n";
                print "To force uninstallation use '--force' flag.\n";
                print RED "See $ofedlogs/ofed_uninstall.log", RESET "\n";
                exit $NONOFEDRPMS;
            }
            print RED "Failed to uninstall the previous installation", RESET "\n";
            print RED "See $ofedlogs/ofed_uninstall.log", RESET "\n";
            exit $ERROR;
        }
    }

    # Uninstall leftovers and previous OFED packages
    force_uninstall();

    # uninstall KMP packages
    for my $package (qw(srp iser ofa_kernel knem ummunotify)) {
        my $regEx;
        if ($package !~ /iser|srp$/) {
            $regEx = "*$package*";
        } else {
            $regEx = "\"kmod*$package*|$package*kmp*\"";
        }
        if (open (KMP_RPMS, "rpm -qa --queryformat \"[%{NAME}]\n\" $regEx |")) {
            my $kmp_cnt = 0;
            my $kmp_rpms;
            while(<KMP_RPMS>) {
                chomp $_;
                next if ($_ eq "mlnx-ofa_kernel");
                $kmp_rpms .= " $_";
                $kmp_cnt ++;
            }
            close KMP_RPMS;

            if ($kmp_cnt) {
                if ($package eq "ofa_kernel") {
                    print "rpm -e --allmatches --noscripts mlnx-ofa_kernel\n" if ($verbose);
                    system("rpm -e --allmatches --noscripts mlnx-ofa_kernel >> $ofedlogs/kmp_$package\_rpms_uninstall.log 2>&1");
                }
                print "rpm -e --allmatches $kmp_rpms\n" if ($verbose);
                system("rpm -e --allmatches $kmp_rpms >> $ofedlogs/kmp_$package\_rpms_uninstall.log 2>&1");
                $res = $? >> 8;
                $sig = $? & 127;
                if ($sig or $res) {
                    print RED "Failed to uninstall $package KMP RPMs", RESET "\n";
                    print RED "See $ofedlogs/kmp_$package\_rpms_uninstall.log", RESET "\n";
                    exit $ERROR;
                }
            }
        }
    }

    if ( -d "/lib/modules/$kernel/kernel/drivers/net/mtnic" ) {
        print "Uninstalling mtnic driver...\n" if (not $quiet);
        system("/sbin/rmmod mtnic > /dev/null 2>&1");
        system("/bin/rm -rf /lib/modules/$kernel/kernel/drivers/net/mtnic");
        system("/sbin/depmod > /dev/null 2>&1");
    }

}

sub install
{
    # Build and install selected RPMs
    for my $package ( @selected_packages ) {
        if ($packages_info{$package}{'internal'}) {
            my $parent = $packages_info{$package}{'parent'};
            if (not is_srpm_available($parent)) {
                print RED "$parent source RPM is not available", RESET "\n";
                next;
            }
        }
        my $pname = $packages_info{$package}{'parent'};
        for my $ver (keys %{$main_packages{$pname}}) {
            if ($packages_info{$package}{'mode'} eq "user") {
                if (not $packages_info{$package}{'exception'}) {
                    if ( (not $packages_info{$package}{$ver}{'rpm_exist'}) or
                         ($build32 and $packages_info{$package}{'install32'} and
                          not $packages_info{$package}{$ver}{'rpm_exist32'}) ) {
                        build_rpm($package, $ver);
                    }

                    if ( (not $packages_info{$package}{$ver}{'rpm_exist'}) or
                         ($build32 and $packages_info{$package}{'install32'} and
                          not $packages_info{$package}{$ver}{'rpm_exist32'}) ) {
                        print RED "$package $ver was not created", RESET "\n";
                        exit $ERROR;
                    }
                    print "Install $package $ver RPM:\n" if ($verbose and not $build_only);
                    install_rpm($package, $ver) if (not $build_only);
                }
            }
            else {
                # kernel modules
                if ($package =~ m/mlnx-ofa_kernel/ and not $kmp) {
                    $package =~ s/mlnx-ofa_kernel/kernel-ib/;
                }
                if ($package =~ m/knem-mlnx|kernel-mft-mlnx|ummunotify-mlnx/ and not $kmp) {
                    $package =~ s/-mlnx//;
                }
                if (not $packages_info{$package}{$ver}{'rpm_exist'}) {
                    my $parent = $packages_info{$package}{'parent'};
                    print "Build $parent $ver RPM\n" if ($verbose);
                    build_kernel_rpm($parent, $ver);
                }
                if (not $packages_info{$package}{$ver}{'rpm_exist'}) {
                    next if ($package =~ /devel/);
                    print RED "$package $ver was not created", RESET "\n";
                    exit $ERROR;
                }
                print "Install $package $ver RPM:\n" if ($verbose and not $build_only);
                install_kernel_rpm($package, $ver) if (not $build_only);
            }
        }
    }
}

sub check_pcie_link
{
    if (open (PCI, "$lspci -d 15b3: -n|")) {
        while(<PCI>) {
            my $devinfo = $_;
            $devinfo =~ /(15b3:[0-9a-fA-F][0-9a-fA-F][0-9a-fA-F][0-9a-fA-F])/;
            my $devid = $&;
            my $link_width = `$setpci -d $devid 72.B | cut -b1`;
            chomp $link_width;

            print BLUE "Device ($devid):\n";
            print "\t" . `$lspci -d $devid`;

            if ( $link_width eq "8" ) {
                print "\tLink Width: 8x\n";
            }
            else {
                print "\tLink Width is not 8x\n";
            }
            my $link_speed = `$setpci -d $devid 72.B | cut -b2`;
            chomp $link_speed;
            if ( $link_speed eq "1" ) {
                print "\tPCI Link Speed: 2.5Gb/s\n";
            }
            elsif ( $link_speed eq "2" ) {
                print "\tPCI Link Speed: 5Gb/s\n";
            }
            else {
                print "\tPCI Link Speed: Unknown\n";
            }
            print "", RESET "\n";
        }
        close (PCI);
    }
}

sub is_srpm_available
{
    my $name = shift;

    for my $ver (keys %{$main_packages{$name}}) {
        if ($main_packages{$name}{$ver}{'srpmpath'}) {
            return 1;
        }
    }

    return 0;
}


### MAIN AREA ###
sub main
{
    if ($print_available) {
        my @list = ();

        for my $srcrpm ( <$SRPMS*> ) {
            set_cfg ($srcrpm);
        }
        set_availability();

        if (!$install_option) {
            $install_option = 'all';
        }

        $config = $conf_dir . "/ofed-$install_option.conf";
        chomp $config;
        if ($install_option eq 'all') {
            @list = (@all_packages, @hidden_packages);
        }
        elsif ($install_option eq 'hpc') {
            @list = (@hpc_user_packages, @hpc_kernel_packages);
            @kernel_modules = (@hpc_kernel_modules);
        }
        elsif ($install_option eq 'hypervisor-os') {
            @list = (@hypervisor_user_packages, @hypervisor_kernel_packages);
            @kernel_modules = (@hypervisor_kernel_modules);
        }
        elsif ($install_option eq 'guest-os') {
            @list = (@guest_user_packages, @guest_kernel_packages);
            @kernel_modules = (@guest_kernel_modules);
        }
        elsif ($install_option =~ m/vma/) {
            if ($install_option eq 'vma') {
                @list = (@vma_user_packages, @vma_kernel_packages);
            } elsif ($install_option eq 'vmavpi') {
                @list = (@vmavpi_user_packages, @vma_kernel_packages);
            } elsif ($install_option eq 'vmaeth') {
                @list = (@vmaeth_user_packages, @vma_kernel_packages);
            }
            @kernel_modules = (@vma_kernel_modules);
        }
        elsif ($install_option eq 'basic') {
            @list = (@basic_user_packages, @basic_kernel_packages);
            @kernel_modules = (@basic_kernel_modules);
        }

        @selected_by_user = (@list);
        @selected_modules_by_user = (@kernel_modules);
        add_enabled_pkgs_by_user();
        resolve_dependencies();
        open(CONFIG, ">$config") || die "Can't open $config: $!";;
        flock CONFIG, $LOCK_EXCLUSIVE;
        print "\nOFED packages: ";
        for my $package ( @selected_packages ) {
            my $parent = $packages_info{$package}{'parent'};
            next if (not $packages_info{$package}{'available'} or not is_srpm_available($parent));
            print("$package available: $packages_info{$package}{'available'}\n") if ($verbose2);
            if ($package =~ /kernel-ib|ofa_kernel/ and $package !~ /devel/) {
                print "\nKernel modules: ";
                for my $module ( @selected_kernel_modules ) {
                    next if (not $kernel_modules_info{$module}{'available'});
                    print $module . ' ';
                    print CONFIG "$module=y\n";
                }
                print "\nRPMs: ";
            }
            print $package . ' ';
            print CONFIG "$package=y\n";
        }
        flock CONFIG, $UNLOCK;
        close(CONFIG);
        print "\n";
        print GREEN "Created $config", RESET "\n";
        exit $SUCCESS;
    }

    warn("Logs dir: $ofedlogs\n");
    my $num_selected = 0;

    if ($interactive) {
        my $inp;
        my $ok = 0;
        my $max_inp;

        while (! $ok) {
            $max_inp = show_menu("main");
            $inp = getch();

            if ($inp =~ m/[qQ]/ || $inp =~ m/[Xx]/ ) {
                die "Exiting\n";
            }
            if (ord($inp) == $KEY_ENTER) {
                next;
            }
            if ($inp =~ m/[0123456789abcdefABCDEF]/)
            {
                $inp = hex($inp);
            }
            if ($inp < 1 || $inp > $max_inp)
            {
                print "Invalid choice...Try again\n";
                next;
            }
            $ok = 1;
        }

        if ($inp == 1) {
            if (-e "$CWD/docs/${PACKAGE}_Installation_Guide.txt") {
                system("less $CWD/docs/${PACKAGE}_Installation_Guide.txt");
            }
            elsif (-e "$CWD/README.txt") {
                system("less $CWD/README.txt");
            }
            else {
                print RED "$CWD/docs/${PACKAGE}_Installation_Guide.txt does not exist...", RESET;
            }

            return 0;
        }
        elsif ($inp == 2) {
            for my $srcrpm ( <$SRPMS*> ) {
                set_cfg ($srcrpm);
            }

            # Set RPMs info for available source RPMs
            set_availability();
            $num_selected = select_packages();
            set_existing_rpms();
            resolve_dependencies();
            check_linux_dependencies();
            if (not $quiet) {
                print_selected();
            }
        }
        elsif ($inp == 3) {
            my $cnt = 0;
            for my $package ( @all_packages, @hidden_packages) {
                if (is_installed($package)) {
                    print "$package\n";
                    $cnt ++;
                }
            }
            if (not $cnt) {
                print "\nThere is no $PACKAGE software installed\n";
            }
            print GREEN "\nPress any key to continue...", RESET;
            getch();
            return 0;
        }
        elsif ($inp == 4) {
            ipoib_config();
            return 0;
        }
        elsif ($inp == 5) {
            uninstall();
            exit 0;
        }

    }
    else {
        for my $srcrpm ( <$SRPMS*> ) {
            next if ($srcrpm =~ /KMP/);
            set_cfg ($srcrpm);
        }

        for my $srcrpm ( <$SRPMS/KMP/*> ) {
            set_cfg ($srcrpm);
        }

        # Set RPMs info for available source RPMs
        set_availability();
        $num_selected = select_packages();
        set_existing_rpms();
        resolve_dependencies();
        check_linux_dependencies();
        if (not $quiet) {
            print_selected();
        }
    }

    if (not $num_selected) {
        print RED "$num_selected packages selected. Exiting...", RESET "\n";
        exit 1;
    }
    print BLUE "Detected Linux Distribution: $DISTRO", RESET "\n" if ($verbose3);

    # Uninstall the previous installations
    uninstall();
    my $vendor_ret;
    if (length($vendor_pre_install) > 0) {
	    print BLUE "\nRunning vendor pre install script: $vendor_pre_install", RESET "\n" if (not $quiet);
	    $vendor_ret = system ( "$vendor_pre_install", "CONFIG=$config",
		"RPMS=$RPMS", "SRPMS=$SRPMS", "PREFIX=$prefix", "TOPDIR=$TOPDIR", "QUIET=$quiet" );
	    if ($vendor_ret != 0) {
		    print RED "\nExecution of vendor pre install script failed.", RESET "\n" if (not $quiet);
		    exit 1;
	    }
    }
    install();

    system("/sbin/ldconfig > /dev/null 2>&1");

    if (-f "/etc/modprobe.conf.dist") {
        open(MDIST, "/etc/modprobe.conf.dist") or die "Can't open /etc/modprobe.conf.dist: $!";
        my @mdist_lines;
        while (<MDIST>) {
            push @mdist_lines, $_;
        }
        close(MDIST);

        open(MDIST, ">/etc/modprobe.conf.dist") or die "Can't open /etc/modprobe.conf.dist: $!";
        foreach my $line (@mdist_lines) {
            chomp $line;
            if ($line =~ /^\s*install ib_core|^\s*alias ib|^\s*alias net-pf-26 ib_sdp/) {
                print MDIST "# $line\n";
            } else {
                print MDIST "$line\n";
            }
        }
        close(MDIST);
    }

    if (length($vendor_pre_uninstall) > 0) {
	    system "cp $vendor_pre_uninstall $prefix/sbin/vendor_pre_uninstall.sh";
    }
    if (length($vendor_post_uninstall) > 0) {
	    system "cp $vendor_post_uninstall $prefix/sbin/vendor_post_uninstall.sh";
    }
    if (length($vendor_post_install) > 0) {
	    print BLUE "\nRunning vendor post install script: $vendor_post_install", RESET "\n" if (not $quiet);
	    $vendor_ret = system ( "$vendor_post_install", "CONFIG=$config",
		"RPMS=$RPMS", "SRPMS=$SRPMS", "PREFIX=$prefix", "TOPDIR=$TOPDIR", "QUIET=$quiet");
	    if ($vendor_ret != 0) {
		    print RED "\nExecution of vendor post install script failed.", RESET "\n" if (not $quiet);
		    exit 1;
	    }
    }

    if ($kernel_modules_info{'ipoib'}{'selected'}) {
        ipoib_config();

        # Decrease send/receive queue sizes on 32-bit arcitecture
        # BUG: https://bugs.openfabrics.org/show_bug.cgi?id=1420
        if ($arch =~ /i[3-6]86/) {
            if (-f "/etc/modprobe.d/ib_ipoib.conf") {
                open(MODPROBE_CONF, ">>/etc/modprobe.d/ib_ipoib.conf");
                print MODPROBE_CONF "options ib_ipoib send_queue_size=64 recv_queue_size=128\n";
                close MODPROBE_CONF;
            }
        }

        # BUG: https://bugs.openfabrics.org/show_bug.cgi?id=1449
        if (-f "/etc/modprobe.d/ipv6") {
            open(IPV6, "/etc/modprobe.d/ipv6") or die "Can't open /etc/modprobe.d/ipv6: $!";
            my @ipv6_lines;
            while (<IPV6>) {
                push @ipv6_lines, $_;
            }
            close(IPV6);

            open(IPV6, ">/etc/modprobe.d/ipv6") or die "Can't open /etc/modprobe.d/ipv6: $!";
            foreach my $line (@ipv6_lines) {
                chomp $line;
                if ($line =~ /^\s*install ipv6/) {
                    print IPV6 "# $line\n";
                } else {
                    print IPV6 "$line\n";
                }
            }
            close(IPV6);
        }
    }

    if ( not $quiet ) {
        check_pcie_link();
    }

    if ($umad_dev_rw or $umad_dev_na) {
        if (-f $ib_udev_rules) {
            open(IB_UDEV_RULES, $ib_udev_rules) or die "Can't open $ib_udev_rules: $!";
            my @ib_udev_rules_lines;
            while (<IB_UDEV_RULES>) {
                push @ib_udev_rules_lines, $_;
            }
            close(IB_UDEV_RULES);

            open(IB_UDEV_RULES, ">$ib_udev_rules") or die "Can't open $ib_udev_rules: $!";
            foreach my $line (@ib_udev_rules_lines) {
                chomp $line;
                if ($line =~ /umad/) {
                    if ($umad_dev_na) {
                        print IB_UDEV_RULES "KERNEL==\"umad*\", NAME=\"infiniband/%k\", MODE=\"0660\"\n";
                    } else {
                        print IB_UDEV_RULES "KERNEL==\"umad*\", NAME=\"infiniband/%k\", MODE=\"0666\"\n";
                    }
                } else {
                    print IB_UDEV_RULES "$line\n";
                }
            }
            close(IB_UDEV_RULES);
        }
    }

    my @openmpi_mca_params;
    # Openmpi post-install action
    if (-d "$prefix/mpi") {
        @openmpi_mca_params = `find $prefix/mpi -name openmpi-mca-params.conf 2> /dev/null`;
        for my $openmpi_conf (@openmpi_mca_params) {
            chomp $openmpi_conf;
            system("echo coll_fca_enable = 0 >> $openmpi_conf 2>&1");
            system("echo coll = ^ml >> $openmpi_conf");
        }
    }

    # Openshmem post-install action
    if (-d $openshmem_prefix) {
        @openmpi_mca_params = `find $openshmem_prefix -name openmpi-mca-params.conf 2> /dev/null`;
        for my $openmpi_conf (@openmpi_mca_params) {
            chomp $openmpi_conf;
            system("echo coll_fca_enable = 0 >> $openmpi_conf 2>&1");
            system("echo scoll_fca_enable = 0 >> $openmpi_conf 2>&1");
        }
    }

    if (-f "/etc/infiniband/openib.conf") {
        my @lines;
        open(FD, "/etc/infiniband/openib.conf");
        while (<FD>) {
            push @lines, $_;
        }
        close (FD);
        open(FD, ">/etc/infiniband/openib.conf");
        foreach my $line (@lines) {
            chomp $line;
            if ($line =~ m/(^SDP_LOAD=|^QIB_LOAD=).*/) {
                    print FD "${1}no\n";
            } elsif ($line =~ m/(^SET_IPOIB_CM=).*/ and $with_vma) {
                # Set IPoIB Datagram mode in case of VMA installation
                print FD "SET_IPOIB_CM=no\n";
            } else {
                    print FD "$line\n";
            }
        }
        close (FD);
    }

    if ($DISTRO =~ m/OEL/ and $kernel =~ m/2.6.32-279.19.1.el6/) {
        my @lines;
        open(FD, "/etc/infiniband/openib.conf");
        while (<FD>) {
            push @lines, $_;
        }
        close (FD);

        open(FD, ">/etc/infiniband/openib.conf");
        foreach my $line (@lines) {
            chomp $line;
            if ($line =~ m/(^RUN_SYSCTL=).*/) {
                    print FD "${1}no\n";
            } else {
                    print FD "$line\n";
            }
        }
        close (FD);
    }

    # Enable/disable mlnx_tune
    if ( -e "/etc/infiniband/openib.conf") {
        my @lines;
        open(FD, "/etc/infiniband/openib.conf");
        while (<FD>) {
            push @lines, $_;
        }
        close (FD);

        open(FD, ">/etc/infiniband/openib.conf");
        foreach my $line (@lines) {
            chomp $line;
            if ($line =~ m/(^RUN_MLNX_TUNE=).*/) {
                if ($enable_mlnx_tune) {
                    print FD "${1}yes\n";
                } else {
                    print FD "${1}no\n";
                }
            } else {
                    print FD "$line\n";
            }
        }
        close (FD);
    }

    my $mlnx_conf = "/etc/modprobe.d/mlnx.conf";
    if ($with_vma and -e "$mlnx_conf") {
        my @lines;
        open(FD, "$mlnx_conf");
        while (<FD>) {
            push @lines, $_;
        }
        close (FD);
        open(FD, ">$mlnx_conf");
        foreach my $line (@lines) {
            chomp $line;
            print FD "$line\n" unless ($line =~ /disable_raw_qp_enforcement|fast_drop|log_num_mgm_entry_size/);
        }
        print FD "options ib_uverbs disable_raw_qp_enforcement=1\n";
        print FD "options mlx4_core fast_drop=1\n";
        print FD "options mlx4_core log_num_mgm_entry_size=-1\n";
        close (FD);
    }

    if (is_installed("ibacm")) {
        # Disable ibacm daemon by default
        system("chkconfig --del ibacm > /dev/null 2>&1");
    }

    print GREEN "\nInstallation finished successfully.", RESET if (not $build_only);
    if ($interactive) {
        print GREEN "\nPress any key to continue...", RESET;
        getch();
    }
    else {
        print "\n";
    }
}

while (1) {
    main();
    exit 0 if (not $interactive);
}
