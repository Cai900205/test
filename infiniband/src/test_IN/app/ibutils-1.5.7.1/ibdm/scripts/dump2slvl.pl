#! /usr/bin/perl -w

my $switch = 0x0;

my $H = "([xXa-fA-F0-9]+)";
my $D = "([0-9]+)";
my $DD = "\\s+([0-9]+)\\s+([0-9]+)";

while (<>) {

    if (/^Switch\s+$H/) {
	$switch = $1;
    }

    if (/^\s*$D\s+$D\s+:$DD$DD$DD$DD$DD$DD$DD$DD/) {

	printf "%s %d %d", $switch, $1, $2;
	printf " 0x%x%x 0x%x%x", $3, $4, $5, $6;
	printf " 0x%x%x 0x%x%x", $7, $8, $9, $10;
	printf " 0x%x%x 0x%x%x", $11, $12, $13, $14;
	printf " 0x%x%x 0x%x%x", $15, $16, $17, $18;
	printf "\n";
    }
}

