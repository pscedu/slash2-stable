#!/usr/bin/env perl
# $Id$
# %PSC_START_COPYRIGHT%
# -----------------------------------------------------------------------------
# Copyright (c) 2012, Pittsburgh Supercomputing Center (PSC).
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

# use warnings;
# use strict;

#[1330381035:740627 sliricthr19:0x801c15400:mem dynarray.c _psc_dynarray_resize 67] alloc()=0x80a952060 sz=8 fl=0
#[1330381035:740843 sliricthr19:0x801c15400:mem dynarray.c _psc_dynarray_resize 67] realloc(0x80a952060)=0x822125d60 sz=16 fl=0
#[1330381035:741191 sliricthr19:0x801c15400:mem dynarray.c psc_dynarray_free 180] free(0x822125d60)

my %h;

while (<>) {
    chomp;
    (@a) = split ' ', $_;

    if ($a[5] =~ /alloc\(\)=(0x[a-f0-9]+)/) {
	#print "alloc $1\n";
	$h{$1} = $1;
	$h{$1}{sz} = $a[6];
	$h{$1}{func} = "$a[3]-" . $a[4];
	$h{$1}{freed} = 0;

    } elsif ($a[5] =~ /realloc\((0x[a-f0-9]+)\)=(0x[a-f0-9]+)/) {
	#print "realloc $1 -> $2\n";
	if (!defined $h{$1}) {
	    $h{$1} = $1;
	}

	$h{$1}{freed} = 1;

    } elsif ($a[5] =~ /free\((0x[a-f0-9]+)\)/) {
	#print "free $1\n";
	if (!defined $h{$1}) {
	    $h{$1} = $1;
	}

	$h{$1}{freed} = 1;
    }
}

foreach $k (keys %h) {
    if ($h{$k}{freed} eq 0) {
	print "$h{$k} $h{$k}{sz} func=$h{$k}{func}\n";
    } else {
	#print "FREED $h{$k} $h{$k}{sz} func=$h{$k}{func} \n";
    }
}
