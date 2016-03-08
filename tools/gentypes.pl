#!/usr/bin/env perl
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

use strict;
use warnings;
use PFL::Getoptv;

sub usage {
	die "usage: $0 [-g pat] [-h headers] [-x exclude]\n";
}

sub in_array {
	my ($ar, $elem) = @_;
	local $_;

	foreach (@$ar) {
		return 1 if $_ eq $elem;
	}
	return (0);
}

sub uniq {
	my @a;
	local $_;

	foreach (@_) {
		push @a, $_ unless @a && $_ eq $a[$#a];
	}
	return @a;
}

sub filter {
	my $xr = shift;
	my @a;
	local $_;

	foreach (@_) {
		push @a, $_ unless in_array $xr, $_;
	}
	return @a;
}

my %opts;
getoptv("g:h:x:", \%opts) or usage();
usage() if @ARGV;

my $outfn = $ARGV[0];

my @vals;
my @types;
my @hdrs;
my @enums;
@hdrs = uniq sort map { glob } @{ $opts{h} } if $opts{h};
if ($opts{x}) {
	@hdrs = filter [ map { glob } @{ $opts{x} } ], @hdrs;
}
my $lvl = 0;
my $in_enum = 0;
foreach my $hdr (@hdrs) {
	open HDR, "<", $hdr or die "$hdr: $!\n";
	while (<HDR>) {
		s{/\*.*?\*/}{}g;
		if ($lvl) {
			$lvl++ if /^\s*#\s*if/;
			$lvl-- if /^\s*#\s*endif/;
		} elsif ($in_enum) {
			$in_enum = 0 if /}/;
			push @enums, $1 if /^\s*(\w+)/;
		} else {
			$lvl = 1 if /^\s*#\s*if\s+0\s*$/;
			push @types, "struct $1" if /^(?:typedef\s+)?struct\s+(\w+)\s*{\s*/;
			push @types, $1 if /^typedef\s+(?:struct\s+)?(?:\w+)\s+(\w+)\s*;\s*/;
			$in_enum = 1 if /^enum\s*(\w*)\s*{\s*/;

			if ($opts{g}) {
				foreach my $pat (@{ $opts{g} }) {
					push @vals, /$pat/;
				}
			}
		}
	}
	close HDR;
}

print <<EOF;
@{[map { qq{#include "$_"\n} } @hdrs]}

void
typedump(void)
{
	@{[@types ? q{printf("structures:\n");} : ""]}
@{[map { "\tPRTYPE($_);\n" } uniq sort @types]}
	@{[@types ? q{printf("\n");} : ""]}

	@{[@vals ? q{printf("values:\n");} : ""]}
@{[map { s/^\s+|\s+$//g; "\tPRVAL($_);\n" } uniq sort @vals]}
	@{[@vals ? q{printf("\n");} : ""]}

	@{[@enums ? q{printf("enums:\n");} : ""]}
@{[map { "\tPRVAL($_);\n" } uniq sort @enums]}
	@{[@enums ? q{printf("\n");} : ""]}
}
EOF
