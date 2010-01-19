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
	# add missing return statements
	$c =~ s/^}$/return;}/g;

	# catch 'return' without an arg
	$c =~ s/\breturn;/PFL_RETURNX();/g;

	# catch 'return' with a string literal arg
	$c =~ s/\breturn\s*\(?\s*(".*?")\s*\)?\s*;/PFL_RETURN_STRLIT($1);/g;

	# catch 'return' with an args
	$c =~ s/\breturn\s*(.*?)\s*;/PFL_RETURN($1);/g;
}

print $c;
