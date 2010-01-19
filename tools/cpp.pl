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

if ($c =~ m!psc_util/log\.h!) {
	# add missing return statements
	$c =~ s/^}$/return;}/g;

	# translate 'return;' into 'PSC_RETURNX();'
	$c =~ s/\breturn;/PSC_RETURNX();/g;

	# translate 'return ...;' into 'PSC_RETURN(...);'
	$c =~ s/\breturn\s*\(?(.*?)\)?\s*;/PSC_RETURN($1);/g;
}

print $c;
