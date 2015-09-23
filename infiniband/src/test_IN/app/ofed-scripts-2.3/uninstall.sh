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
#
# Description: MLNX_OFED package uninstall script

use strict;
use warnings;
use Term::ANSIColor qw(:constants);
use File::Path;

my $PREREQUISIT = "172";
my $ERROR = "1";
my $NONOFEDRPMS = "174";

$ENV{"LANG"} = "en_US.UTF-8";

if ($<) {
    print RED "Only root can run $0", RESET "\n";
    exit $PREREQUISIT;
}

my $ofed_info = `which ofed_info 2> /dev/null`;
if (not $ofed_info) {
    print "No OFED installation detected. Exiting ...\n";
    exit $ERROR;
}

my @packages_to_uninstall = ();
my @dependant_packages_to_uninstall = ();
my %selected_for_uninstall = ();
my %non_ofed_for_uninstall = ();
my $unload_modules = 0;
my $force = 0;
my $verbose = 0;
my $quiet = 0;
my $dry_run = 0;
my $PACKAGE = `ofed_info -s | sed -e 's/://'`;
chomp $PACKAGE;
my $ofedlogs = "/tmp/$PACKAGE.$$.logs";
my $prefix = '/usr';
my $info = '/etc/infiniband/info';
my $rpm_flags = '';

sub usage
{
   print GREEN;
   print "\n Usage: $0 [--unload-modules] [-v|--verbose] [-q|--quiet] [--dry-run]\n";

   print "\n           --unload-modules     Run /etc/init.d/openibd stop before uninstall";
   print "\n           --force              Force uninstallation and remove packages that depends on MLNX_OFED";
   print "\n           -v|--verbose         Increase verbosity level";
   print "\n           --dry-run            Print the list of packages to be uninstalled without actually uninstalling them";
   print "\n           -q                   Set quiet - no messages will be printed";
   print RESET "\n\n";
}

sub getch
{
    my $c;
    $c=getc(STDIN);

    return $c;
}

sub log_and_exit
{
    my $rc = shift @_;
    if ($rc) {
        print RED "See logs under $ofedlogs", RESET "\n";
    }

    exit $rc;
}

# Find in file $name line containing str1 and replace it with str2
# If str2 is empty the line with str1 will be removed
sub find_and_replace
{
    my $name = shift @_;
    my $str1 = shift @_;
    my $str2 = shift @_;

    my @lines;
    open(FD, "$name");
    while (<FD>) {
        push @lines, $_;
    }
    close (FD);

    open(FD, ">$name");
    foreach my $line (@lines) {
        chomp $line;
        if ($line =~ /$str1/) {
            print FD "$str2\n" if ($str2);
        } else {
            print FD "$line\n";
        }
    }
    close (FD);
}

sub is_installed_deb
{
    my $name = shift @_;
    my $res = 0;
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
    
    if (-f "/usr/bin/dpkg-query") {
        system("dpkg-query -W -f='\${Package} \${Version}\n' $name > /dev/null 2>&1");
    }
    else {
        system("rpm -q $name > /dev/null 2>&1");
    }
    $res = $? >> 8;

    return not $res;
}

sub mark_for_uninstall
{
    my $package = shift @_;
    if (not $selected_for_uninstall{$package}) {
        push (@dependant_packages_to_uninstall, "$package");
        my $pname = $package;
        $pname =~ s@-[0-9].*@@g;
        $pname =~ s@-dev.*@@g;
        $selected_for_uninstall{$package} = 1;
        if ( `ofed_info 2>/dev/null | grep -i $pname 2>/dev/null` eq "" and $pname !~ /ofed-scripts/) {
            $non_ofed_for_uninstall{$package} = 1;
        }
    }
}

sub get_requires
{
    my $package = shift @_;

    # Strip RPM version
    $package = `rpm -q --queryformat "[%{NAME}]" $package`;
    chomp $package;

    my @what_requires = `/bin/rpm -q --whatrequires $package 2>&1 | grep -v "no package requires" 2> /dev/null`;

    for my $pack_req (@what_requires) {
        chomp $pack_req;
        print "get_requires: $package is required by $pack_req\n" if ($verbose);
        get_requires($pack_req);
        mark_for_uninstall($pack_req);
    }
}

sub do_uninstall
{
    my $res = 0;
    my $sig = 0;
    my $cnt = 0;
    my @installed_rpms = `ofed_info | grep -A999 '^-' 2> /dev/null | grep -v '^-'`;
    my @other_ofed_rpms = `rpm -qa 2> /dev/null | grep -wE "rdma|ofed|openib|mft|kernel-ib|rds|ib-bonding|infiniband"`;
    my $cmd = "rpm -e --allmatches --nodeps $rpm_flags";

    for my $package (@other_ofed_rpms) {
        chomp $package;
        my $pname = $package;
        $pname =~ s@-[0-9].*@@g;
        $pname =~ s@-dev.*@@g;
        if ( `ofed_info 2>/dev/null | grep -i $pname 2>/dev/null` eq "" and $pname !~ /ofed-scripts/ ) {
            $non_ofed_for_uninstall{$package} = 1;
        }
    }

    if (is_installed("ofed")) {
        # W/A for SLES 10 SP4 in-box ofed RPM uninstall issue
        $cmd .= " --noscripts";
    }

    for my $package (@installed_rpms, @other_ofed_rpms) {
        chomp $package;
        next if ($package eq "mpi-selector");
        if (is_installed($package)) {
            push (@packages_to_uninstall, $package);
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

        if (not $dry_run) {
            system("$cmd >> $ofedlogs/ofed_uninstall.log 2>&1");
            $res = $? >> 8;
            $sig = $? & 127;
            if ($sig or $res) {
                print RED "Failed to uninstall the previous installation", RESET "\n";
                print RED "See $ofedlogs/ofed_uninstall.log", RESET "\n";
                log_and_exit $ERROR;
            }
        }
    }
}


sub uninstall
{
    my $res = 0;
    my $sig = 0;
    my $distro_rpms = '';
    my $cmd = '';

    do_uninstall();

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
                    $cmd = "rpm -e --allmatches --noscripts $rpm_flags mlnx-ofa_kernel";
                    print "\n$cmd\n" if (not $quiet);
                    open (LOG, "+>$ofedlogs/kmp_$package\_rpms_uninstall.log");
                    print LOG "$cmd\n";
                    close LOG;
    
                    if (not $dry_run) {
                        system("$cmd >> $ofedlogs/kmp_$package\_rpms_uninstall.log 2>&1");
                        $res = $? >> 8;
                        $sig = $? & 127;
                        if ($sig or $res) {
                            print RED "Failed to uninstall mlnx-ofa_kernel", RESET "\n";
                            log_and_exit $ERROR;
                        }
                    }
                }

                $cmd = "rpm -e --allmatches $rpm_flags $kmp_rpms";
                print "\n$cmd\n" if (not $quiet);
                open (LOG, "+>$ofedlogs/kmp_$package\_rpms_uninstall.log");
                print LOG "$cmd\n";
                close LOG;

                if (not $dry_run) {
                    system("$cmd >> $ofedlogs/kmp_$package\_rpms_uninstall.log 2>&1");
                    $res = $? >> 8;
                    $sig = $? & 127;
                    if ($sig or $res) {
                        print RED "Failed to uninstall $package KMP RPMs", RESET "\n";
                        log_and_exit $ERROR;
                    }
                }
            }
        }
    }
}


######### MAIN #########
while ( $#ARGV >= 0 ) {

   my $cmd_flag = shift(@ARGV);

    if ($cmd_flag eq "--unload-modules") {
        $unload_modules = 1;
    } elsif ($cmd_flag eq "--force") {
        $force = 1;
    } elsif ($cmd_flag eq "-v" or $cmd_flag eq "--verbose") {
        $verbose = 1;
    } elsif ($cmd_flag eq "-q" or $cmd_flag eq "--quiet") {
        $quiet = 1;
    } elsif ($cmd_flag eq "--dry-run") {
        $dry_run = 1;
    } elsif ($cmd_flag eq "-h" or $cmd_flag eq "--help") {
        usage();
        exit 0;
    }
}

if (not $force) {
    print "\nThis program will uninstall all $PACKAGE packages on your machine.\n\n";
    print "Do you want to continue?[y/N]:";
    my $ans = getch();
    print "\n";

    if ($ans !~ m/[yY]/) {
        exit $ERROR;
    }
}

mkpath([$ofedlogs]);

if (-e $info) {
    open(INFO, "$info|") or die "Failed to run: $info. Error $!\n";;
    while(<INFO>) {
        if (/^prefix=/) {
            $prefix = (split '=', $_)[1];
            last;
        }
    }
    close(INFO);
} else {
    $prefix = $0;
    $prefix =~ s/(\/sbin).*//g;
}

if ($unload_modules) {
    print "Unloading kernel modules...\n" if (not $quiet);

    if (not $dry_run) {
        system("/etc/init.d/openibd stop >> $ofedlogs/openibd_stop.log 2>&1");
        my $res = $? >> 8;
        my $sig = $? & 127;
        if ($sig or $res) {
            print RED "Failed to unload kernel modules", RESET "\n";
            log_and_exit $ERROR;
        }
    }
}

if (-e "/etc/debian_version") {
    $rpm_flags = "--force-debian";
}
uninstall();

exit 0 if ($dry_run);

if (-e "/etc/sysctl.conf") {
    system ("grep -q MLNX_nr_overcommit_hugepages /etc/sysctl.conf");
    my $res = $? >> 8;
    my $sig = $? & 127;
    if (not ($sig or $res)) {
        find_and_replace ("/etc/sysctl.conf", "nr_overcommit_hugepages", "");
    }
}

if (-e "/etc/modprobe.d/ipv6") {
   find_and_replace ("/etc/modprobe.d/ipv6", "# install ipv6 /bin/true", "install ipv6 /bin/true");
}

system("/sbin/modprobe -r knem > /dev/null 2>&1");
system("/bin/rm -f /etc/sysconfig/modules/knem.modules");

print "Uninstall finished successfully\n" if (not $quiet);
