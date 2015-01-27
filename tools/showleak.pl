#!/usr/bin/env perl
# $Id$
# %PSC_START_COPYRIGHT%
# -----------------------------------------------------------------------------
# Copyright (c) 2012-2014, Pittsburgh Supercomputing Center (PSC).
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

# use warnings;
# use strict;

#[1330381035:740627 sliricthr19:0x801c15400:mem dynarray.c _psc_dynarray_resize 67] alloc()=0x80a952060 sz=8 fl=0
#[1330381035:740843 sliricthr19:0x801c15400:mem dynarray.c _psc_dynarray_resize 67] realloc(0x80a952060)=0x822125d60 sz=16 fl=0
#[1330381035:741191 sliricthr19:0x801c15400:mem dynarray.c psc_dynarray_free 180] free(0x822125d60)

my %h;

while (<>) {
	if ($. % 3000 == 0) {
		print STDERR "\rline $.";
	}

	if (/alloc\(\)=(0x[a-f0-9]+)/) {
		#print "alloc $1\n";
		$h{$1} = $_;

	} elsif (/realloc\((0x[a-f0-9]+)\)=(0x[a-f0-9]+)/) {
		#print "realloc $1 -> $2\n";
		delete $h{$1};

		$h{$2} = $_;

	} elsif (/free\((0x[a-f0-9]+)\)/) {
		#print "free $1\n";
		delete $h{$1};
	}
}

foreach $k (keys %h) {
	print $h{$k};
}
