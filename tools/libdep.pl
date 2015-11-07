#!/usr/bin/env perl
# $Id$
# %PSC_START_COPYRIGHT%
# -----------------------------------------------------------------------------
# Copyright (c) 2008-2012, Pittsburgh Supercomputing Center (PSC).
#
# Permission to use, copy, modify, and distribute this software
# for any purpose with or without fee is hereby granted, provided
# that the above copyright notice and this permission notice
# appear in all copies.
#
# THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL
# WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS.  IN NO EVENT SHALL
# THE AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR
# CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
# LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT,
# NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
# CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
#
# Pittsburgh Supercomputing Center	phone: 412.268.4960  fax: 412.268.5832
# 300 S. Craig Street			e-mail: remarks@psc.edu
# Pittsburgh, PA 15213			web: http://www.psc.edu/
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
