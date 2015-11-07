#!/usr/bin/perl -W
# $Id$

use strict;
use warnings;

my %m;

while (<>) {
	if (/alloc\([0-9]+\)=([0-9a-fx]+)/i) {
		$m{$1} = $.;
	} elsif (/free\(([0-9a-fx]+)\)/i) {
		my $r = $1;
		warn "line $.: invalid free $r" unless exists $m{$r};
		delete $m{$r};
	}
}

my ($k, $v);
while (($k, $v) = each %m) {
	print "unfreed mem $k at line $v\n";
}
