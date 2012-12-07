#!/usr/bin/env perl
# $Id$

use Getopt::Std;
use warnings;
use strict;

sub usage {
	warn "usage: $0 has-version need-version\n";
	exit 2;
}

my %opts;
getopts("", \%opts) or usage();
usage() unless @ARGV == 2;

my @has = split m[\.], $ARGV[0];
my @need = split m[\.], $ARGV[1];

for (my $i = 0; $i < @has; $i++) {
	exit 1 if $i >= @need;
	exit 1 if $has[$i] < $need[$i];
	exit 0 if $has[$i] > $need[$i];
}
exit 0;
