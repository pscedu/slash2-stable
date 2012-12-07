#!/usr/bin/env perl
# $Id$
# %PSC_START_COPYRIGHT%
# -----------------------------------------------------------------------------
# Copyright (c) 2008-2010, Pittsburgh Supercomputing Center (PSC).
#
# Permission to use, copy, and modify this software and its documentation
# without fee for personal use or non-commercial use within your organization
# is hereby granted, provided that the above copyright notice is preserved in
# all copies and that the copyright and this permission notice appear in
# supporting documentation.  Permission to redistribute this software to other
# organizations or individuals is not permitted without the written permission
# of the Pittsburgh Supercomputing Center.  PSC makes no representations about
# the suitability of this software for any purpose.  It is provided "as is"
# without express or implied warranty.
# -----------------------------------------------------------------------------
# %PSC_END_COPYRIGHT%

use strict;
use warnings;
use constant WIDTH => 78;

my @l_opts;
my @L_opts;
my @libs;

my $llen = 0;
sub prdep {
	my $dep = shift;
	my $len = length $dep;
	if ($llen + $len + 1 > WIDTH) {
		print " \\\n  ";
		$llen = 0;
	} else {
		print " ";
		$llen++;
	}
	print $dep;
	$llen += $len;
}

foreach my $arg (@ARGV) {
	push(@l_opts, $'), next if $arg =~ /^-l/;
	push(@L_opts, $'), next if $arg =~ /^-L/;
	push(@libs, $arg), next if $arg =~ /\.a$/;
}

my @dirs = grep { -d $_ } @L_opts;

my $cc = $ENV{CC} || "cc";
my $out = `$cc -print-search-dirs`;
my @lines = split /\n/, $out;
foreach my $line (@lines) {
	if ($line =~ /^libraries: =/) {
		push @dirs, split /:/, $';
		last;
	}
}

foreach my $lib (@l_opts) {
	foreach my $path (@dirs) {
		my $name = "$path/lib$lib.a";
		prdep $name if -f $name; # XXX call `last'
	}
}

foreach my $lib (@libs) {
	prdep $lib;
}

print "\n";
