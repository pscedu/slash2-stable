#!/usr/bin/perl -W
# $Id$
# %ISC_START_LICENSE%
# ---------------------------------------------------------------------
# Copyright 2015-2016, Google, Inc.
# Copyright 2009-2015, Pittsburgh Supercomputing Center
# All rights reserved.
#
# Permission to use, copy, modify, and distribute this software for any
# purpose with or without fee is hereby granted, provided that the
# above copyright notice and this permission notice appear in all
# copies.
#
# THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL
# WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS.  IN NO EVENT SHALL THE
# AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
# DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
# PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
# TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
# PERFORMANCE OF THIS SOFTWARE.
# --------------------------------------------------------------------
# %END_LICENSE%

# TODO
# - exclude typedump

use Getopt::Std;
use strict;
use warnings;

sub usage {
	die "usage: $0 file ...\n";
}

sub errx {
	die "$0: ", @_, "\n";
}

sub err {
	die "$0: ", @_, ": $!\n";
}

sub uniq {
	local $_;
	my @uniq;
	my $last = undef;

	foreach (@_) {
		unless (defined $last and $last eq $_) {
			push @uniq, $_;
			$last = $_;
		}
	}
	return @uniq;
}

sub excl {
	my ($master, $mask) = @_;
	my @excl;

	my $j = 0;
	for (my $i = 0; $i < @$master; $i++) {
		for (; $j < @$mask && $master->[$i] gt $mask->[$j]; $j++) {
		}
		push @excl, $master->[$i] unless
		    $j < @$mask && $master->[$i] eq $mask->[$j];
	}
	return (@excl);
}

sub maxlen {
	my ($ra) = @_;
	my $maxlen = 0;
	foreach my $t (@$ra) {
		my $tlen = length $t;
		$maxlen = $tlen if $tlen > $maxlen;
	}
	return ($maxlen);
};

my %opts;
getopts("", \%opts) or usage();
usage() unless @ARGV;

my %syms = (
	defs		=> [],
	enum_vals	=> [],
	enum_types	=> [],
	funcs		=> [],
	structs		=> [],
	typedefs	=> [],
	undefs		=> [],
	unions		=> [],
	vars		=> [],
);
my %msyms = (
	defs		=> [],
	enum_vals	=> [],
	enum_types	=> [],
	funcs		=> [],
	structs		=> [],
	typedefs	=> [],
	undefs		=> [],
	unions		=> [],
	vars		=> [],
);
my (@srcs, @hdrs);

foreach my $file (@ARGV) {
	if ($file =~ /\.[cly]$/) {
		push @srcs, $file;
	} elsif ($file =~ /\.h$/) {
		push @hdrs, $file;
	} else {
		errx "$file: unknown file type";
	}
}

foreach my $hdr (@hdrs) {
	local $/;
	open HDR, "<", $hdr or err $hdr;
	my $c = <HDR>;
	close HDR;

	$c =~ s/\\\n//gs;
	$c =~ s!/\*.*?\*/!!gs;
	$c =~ s!//.*!!gm;

	while ($c =~ s/^\s*enum(\s+\w+|)\s*{\s*(.*?)\s*}\s*;//ms) {
		push @{ $syms{enum_types} }, $1 if $1;
		push @{ $syms{enum_vals} }, map { s/\s*=.*//; $_ } split /\s*,\s*/, $2;
	}

	1 while $c =~ s/{[^{}]*?}//gs;
	$c =~ s/^\s*#\s*(?:if|ifdef|ifndef|include|endif|else)\b.*//gm;
	$c =~ s/\n(?=\w+\s*\()/ /gs;
	$c =~ s/\n(?=\s+\*?\w+)/ /gs;

	while ($c =~ s/^\s*#\s*define\s+(\w+).*//m) {
		my $def = $1;
		push @{ $syms{defs} }, $def unless $def =~ /^_/;
	}

	while ($c =~ s/^\s*#\s*undef\s+(\w+)\s*$//m) {
		my $undef = $1;
		push @{ $syms{undefs} }, $undef unless $undef =~ /^_/;
	}

	while ($c =~ s/^\s*struct\s+(\w+)\s*;//m) {
		push @{ $syms{structs} }, $1;
	}

	while ($c =~ s/^\s*typedef\s+(?:(?:struct|union)\s+)?(\w+)\s+(.*)//m) {
		my $name = $2;
		push @{ $syms{typedefs} }, $name =~ /(\w+)/;;
	}

	while ($c =~ s/^.*?(\w+)\s*\(\s*[^*].*//m) {
		push @{ $syms{funcs} }, $1;
	}

	while ($c =~ s/^.*?([a-zA-Z0-9_\[\]]+)\s*;\s*$//m) {
		my $var = $1;
		$var =~ s/\[.*//;
		push @{ $syms{vars} }, $var;
	}

#	$c =~ s/\n\n+/\n/gs;
#	print $c;
}

foreach my $key (keys %syms) {
	$syms{$key} = [ uniq sort @{ $syms{$key} } ];
}

$syms{defs} = [ excl $syms{defs}, $syms{undefs} ];
delete $syms{undefs};

foreach my $src (@srcs) {
	local $/;
	open SRC, "<", $src or err $src;
	my $c = <SRC>;
	close SRC;

	$c =~ s!/\*.*?\*/!!gs;
	$c =~ s!//.*!!gm;

	foreach my $key (keys %syms) {
		foreach my $tag (@{ $syms{$key} }) {
			$tag = "struct\\s+$tag" if $key eq "struct";
			$tag = "union\\s+$tag" if $key eq "union";
			push @{ $msyms{$key} }, $tag if $c =~ /$tag/;
		}
	}
}

foreach my $key (keys %msyms) {
	$msyms{$key} = [ uniq sort @{ $msyms{$key} } ];
}

foreach my $key (keys %syms) {
	$syms{$key} = [ excl $syms{$key}, $msyms{$key} ];
}

my $width = `stty size | awk '{print \$2}'`;
$width = 80 unless $width && $width =~ /^\d+$/ &&
    $width > 34 && $width < 300;
$width -= 4;

my $first = 1;
foreach my $key (keys %syms) {
	next unless @{ $syms{$key} };
	print "\n" unless $first;
	print "unused $key:\n";
	my $maxlen = maxlen $syms{$key};
	my $ncols = $width / ($maxlen + 3);
	my $colwidth = $width / $ncols;
	my $n = 0;
	foreach my $sym (@{ $syms{$key} }) {
		print "    " if ($n % $ncols) == 0;
		printf "%-*s", $colwidth, $sym;
		$n++;
		print "\n" if ($n % $ncols) == 0 || $n == @{ $syms{$key} };
	}
	$first = 0;
}
