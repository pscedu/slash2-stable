#!/usr/bin/perl -W
# $Id$

use strict;
use warnings;

my $s;
{
	local $/;
	$s = <>;
}

while ($s =~ /free\((0x[0-9a-f]+)\)/gc) {
	my $pos = $+[0];
	my $addr = $1;

	print $addr, "\n" if
	    $' =~ /free\($addr\)/ and
	    $` !~ /alloc\([0-9a-fx]*\)=$addr/;
}
