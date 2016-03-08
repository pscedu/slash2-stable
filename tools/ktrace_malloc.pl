#!/usr/bin/env perl
# $Id$
# %ISC_START_LICENSE%
# ---------------------------------------------------------------------
# Copyright 2015-2016, Google, Inc.
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

#use warnings;
#use strict;

my %h;

while (<>) {
    if ($_ =~ /(0x[a-f0-9]+) = malloc\(([0-9]+)\)/) {
	$h{$1} = $1;
	$h{$1}{sz} = $2;
	$h{$1}{freed} = 0;

	#print "malloc $1 $2\n";

    } elsif ($_ =~ /free\((0x[a-f0-9]+)\)/) {
	$h{$1}{freed} = 1;

	#print "free $1\n";
    }
}

foreach $k (keys %h) {
    if ($h{$k}{freed} eq 0) {
	print "$h{$k} $h{$k}{sz}\n";
    } else {
	print "FREED $h{$k} $h{$k}{sz}\n";
    }
}

