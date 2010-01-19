#!/usr/bin/perl -W
# $Id$

use strict;
use warnings;

open F, "<", $ARGV[0];
local $/;
my $c = <F>;
close F;

# debugging file ID
print qq{# 1 "$ARGV[0]"\n};

if ($c =~ m!psc_util/log\.h! && $ARGV[0] !~ m!/log\.c$!) {
	# strip comments
	$c =~ s!/\*(.*?)\*/! join '', $1 =~ /\n/g !ges;

	# add enter markers
	$c =~ s/^{\s*$/{ PFL_ENTER();/g;

	# make return explicit
	$c =~ s/^}\s*$/return; }/g;

	# catch 'return' without an arg
	$c =~ s/\breturn;/PFL_RETURNX();/g;

	# catch 'return' with a string literal arg
	$c =~ s/\breturn\s*\(?\s*(".*?")\s*\)?\s*;/PFL_RETURN_STRLIT($1);/gs;

	# catch 'return' with an arg
	$c =~ s/\breturn\b\s*(.*?)\s*;/PFL_RETURN($1);/gs;
}

print $c;
