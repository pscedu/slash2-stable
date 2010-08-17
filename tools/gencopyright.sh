#!/bin/sh
# $Id$
# %PSC_COPYRIGHT%

usage()
{
	echo "usage: $0 file ..." >&2
	exit 1
}

if getopts "" c; then
	usage
fi
shift $(($OPTIND - 1))

if [ $# -eq 0 ]; then
	usage;
fi

for i; do
	if [ -h "$i" ]; then
		continue
	fi
	perl -W -i - $i <<'EOF'
use File::Basename;
use warnings;
use strict;

local $/;

my $data = <>;
my $yr;
my $fn = $ARGV;
my $bn = basename $fn;

my @out = split /\n/, join '\n', `svn log '$fn'`;

my $startyr = 2006;

foreach my $ln (@out) {
	my (undef, $t_yr) =
	    ($ln =~ /^r(\d+) \s+ \| \s+ (?:\w+) \s+ \| \s+
	    (\d+)-(?:\d+)-0*(?:\d+) \s+ (?:\d+):(?:\d+):(?:\d+)/x) or next;
	$startyr = $t_yr;
}

$startyr = $1 if $data =~
    m{Copyright \(c\) (\d+)(?:-\d+)?, Pittsburgh Supercomputing Center \(PSC\)\.};

my $endyr = 1900 + (localtime((stat $ARGV)[9]))[5];
if ($data =~ m{/\A(?:.*\n)?.*\$Id: \Q$bn\E \d+ (\d+)-}m) {
	$endyr = $1;
}

if ($endyr < $startyr) {
	warn "$ARGV: $endyr from Id tag before $startyr\n";
	$endyr = $startyr;
}

my $cpyears = $startyr;
$cpyears .= "-$endyr" if $endyr > $startyr;

$data =~ s
{/^(.*) %PSC_COPYRIGHT%.*
}{
	my $delim = $1;
	my $cdeli = $delim;
	my $end = "";

	if ($delim eq "/*") {
		$cdeli = " *";
		$end = "\n */";
	}

	<<EOF2}e;
$delim %PSC_START_COPYRIGHT%
$cdeli -----------------------------------------------------------------------------
$cdeli -----------------------------------------------------------------------------
$cdeli %PSC_END_COPYRIGHT%$end
EOF2

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
