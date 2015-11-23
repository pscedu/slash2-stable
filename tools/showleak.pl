#!/usr/bin/env perl
# $Id$
# %ISC_START_LICENSE%
# ---------------------------------------------------------------------
# Copyright 2015, Google, Inc.
# Copyright 2012-2015, Pittsburgh Supercomputing Center
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

use Getopt::Std;
use warnings;
use strict;

sub usage {
}

my %opts;
getopts("A:F:R:", \%opts) or usage;

# alloc()=0x80a952060
# realloc(0x80a952060)=0x822125d60
# free(0x822125d60)

$opts{A} = qr/alloc\(\)=(0x[a-f0-9]+)/ unless exists $opts{A};
$opts{F} = qr/free\((0x[a-f0-9]+)\)/ unless exists $opts{F};
$opts{R} = qr/realloc\((0x[a-f0-9]+)\)=(0x[a-f0-9]+)/ unless exists $opts{R};

my %h;

my $tty = -t STDERR;

while (<>) {
	$_ = "$.\t$_";

	if ($tty) {
		print STDERR "\rline $." unless $. % 3000;
	}

	if (/$opts{A}/) {
		print "$h{$1}: clobber\n" if exists $h{$1};
		$h{$1} = $_;

	} elsif (/$opts{R}/) {
		delete $h{$1};
		$h{$2} = $_;

	} elsif (/$opts{F}/) {
		# XXX check for double frees or dangling frees
		delete $h{$1};
	}
}

foreach my $k (keys %h) {
	print $h{$k};
}
