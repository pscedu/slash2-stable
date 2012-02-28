#!/usr/bin/env perl
# $Id$
# %PSC_COPYRIGHT%

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

