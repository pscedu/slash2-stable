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

