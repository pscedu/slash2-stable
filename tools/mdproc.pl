#!/usr/bin/perl -W -i
# $Id$

# Better algorithm:
# use file mtime as long as mtime != ctime

use strict;
use warnings;
use Getopt::Std;

sub usage {
	warn "usage: $0 file ...\n";
	exit 1;
}

my %opts;
getopts("", \%opts) or usage;

my @m = qw(0 January February March April May June July
    August September October November December);

local $/;
while (<>) {
	if (my ($yr, $mon, $day) = /^\.\\" \$Id: \Q$ARGV\E \d+ (\d+)-(\d+)-0*(\d+) /m) {
		s/^\.Dd .*/.Dd $m[$mon] $day, $yr/m;
	}
	print;

	system("groff -z $ARGV");
}
