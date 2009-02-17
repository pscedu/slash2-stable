#!/usr/bin/perl -W
# $Id$

use strict;
use warnings;

my @l_opts;
my @L_opts;
my @libs;

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
		print " ", $name if -f $name; # XXX call `last'
	}
}

foreach my $lib (@libs) {
	print " ", $lib;
}
