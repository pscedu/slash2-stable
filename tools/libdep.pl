#!/usr/bin/perl -W
# $Id$

use strict;
use warnings;

my @l_opts;
my @L_opts;

my $i;
for ($i = 0; $i < @ARGV; $i++) {
	if ($ARGV[$i] =~ /^-l/) {
		push @l_opts, $';
	} elsif ($ARGV[$i] =~ /^-L/) {
		push @L_opts, $';
	}
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
		print " ", $name if -f $name;
	}
}
