#! /usr/bin/perl -w

# usage:  cat file.lst opensm-path-records.dump | dump2psl.pl

my $h = "([a-fA-F0-9]+)";
my $H = "([xXa-fA-F0-9]+)";
my $D = "([0-9]+)";

my $n;
my $p;
my %port2node;

my $src_port = 0x0;

while (<>) {

    # parse .lst file, look for node GUID <-> port GUID relationship, hash it

    if (/[^C]*\{\s*(?:CA|SW) Ports[^N]*NodeGUID:$h\s*PortGUID:$h/) {


	$n = "0x" . $1;
	$p = "0x" . $2;

	if (defined $port2node{$p}) {
	    if ($port2node{$p} ne $n) {
		printf STDERR "Cowardly refusing to reassign port GUID $p\n";
		printf STDERR "from node GUID $port2node{$p} ";
		printf STDERR "to node GUID $n\n";
		next;
	    }
	    next;
	}
	$port2node{$p} = $n;
	# printf STDERR "Assigned port GUID $p to node GUID $n\n";
    }

    # parse opensm-path-records.dump, reformat for ibdmchk .psl format

    if (/(?:^Channel\s+Adapter|Switch)\s+$H/) {
	$src_port = $1;
    }

    if (/^\s*$H\s+:\s+$D\s+:\s+$D\s+:\s+$D\s+/) {

	if (!defined($port2node{$src_port})) {
	    next;
	}
	printf "%s %d %d\n", $port2node{$src_port}, oct($1), $2;
    }
}
