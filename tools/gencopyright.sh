#!/bin/sh
# $Id$
# TODO:
# - run "svn status" and get the start year instead of hardcode 2006

usage()
{
	echo "usage: $0 file ..." >&2
	exit 1
}

parg=
# uncomment to force copyrights into all files
#parg=-F

shift $(($OPTIND - 1))

if [ $# -eq 0 ]; then
	usage;
fi

for i; do
	perl -W -i - $parg $i <<'EOF'
use warnings;
use strict;
use Getopt::Std;
use File::Basename;

my %opts;
getopts("F", \%opts);

local $/;

my $data = <>;

my $startyr = 2006;
my $yr;

my $fn = basename $ARGV;

if ($data =~ m{/\* \$Id: \Q$fn\E \d+ (\d+)-}) {
	$yr = $1;
} else {
	$yr = 1900 + (localtime((stat $ARGV)[9]))[5];
}

if ($yr < $startyr) {
	warn "$ARGV: $yr from Id tag before $startyr\n";
	$yr = $startyr;
}

my $cpyears = $startyr;
$cpyears .= "-$yr" if $yr > $startyr;

if ($opts{F}) {
	# Force insertion: if the file does not contain a copyright section,
	# insert at the top after any $Id tags.
	unless ($data =~ /%PSC_(?:START|NO)_COPYRIGHT%/) {
		$data =~ s{((/\*\s*\$Id.*?\*/\n)?)}{$1/\*
 \* %PSC_START_COPYRIGHT%
 \* -----------------------------------------------------------------------------
 \* -----------------------------------------------------------------------------
 \* %PSC_END_COPYRIGHT%
 \*/
};
	}
} else {
	$data =~ s
{/* %PSC_COPYRIGHT% */
}{/\*
 \* %PSC_START_COPYRIGHT%
 \* -----------------------------------------------------------------------------
 \* -----------------------------------------------------------------------------
 \* %PSC_END_COPYRIGHT%
 \*/
};
}

$data =~ s
{/\*
 \* %PSC_START_COPYRIGHT%
 \* -----------------------------------------------------------------------------.*?
 \* -----------------------------------------------------------------------------(.*?)
 \* %PSC_END_COPYRIGHT%
 \*/
}
{/*
 * %PSC_START_COPYRIGHT%
 * -----------------------------------------------------------------------------
 * Copyright (c) $cpyears, Pittsburgh Supercomputing Center (PSC).
 *
 * Permission to use, copy, and modify this software and its documentation
 * without fee for personal use or non-commercial use within your organization
 * is hereby granted, provided that the above copyright notice is preserved in
 * all copies and that the copyright and this permission notice appear in
 * supporting documentation.  Permission to redistribute this software to other
 * organizations or individuals is not permitted without the written permission
 * of the Pittsburgh Supercomputing Center.  PSC makes no representations about
 * the suitability of this software for any purpose.  It is provided "as is"
 * without express or implied warranty.
 * -----------------------------------------------------------------------------$1
 * %PSC_END_COPYRIGHT%
 */
}s;

print $data;
EOF
done
