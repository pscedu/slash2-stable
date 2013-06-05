#!/bin/sh
# $Id$
# %PSC_START_COPYRIGHT%
# -----------------------------------------------------------------------------
# Copyright (c) 2009-2013, Pittsburgh Supercomputing Center (PSC).
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
    m{Copyright \(c\) (\d+)(?:-\d+)?,? Pittsburgh Supercomputing Center \(PSC\)\.};

my $endyr = 1900 + (localtime((stat $ARGV)[9]))[5];
$endyr = $1 if $data =~ m{\A(?:.*\n)?.*\$Id: \Q$bn\E \d+ (\d+)-}m;

if ($endyr < $startyr) {
	warn "$ARGV: $endyr from Id tag before $startyr\n";
	$endyr = $startyr;
}

my $cpyears = $startyr;
$cpyears .= "-$endyr" if $endyr > $startyr;

my $d_start = "/*\n";
my $d_cont = " *";
my $d_end = "\n */";

if ($data =~ m{^(.*) %PSC(?:GPL)?_(START_)?COPYRIGHT%}m) {
	$d_cont = $1;
	$d_cont = " *" if $d_cont eq "/*";

	unless ($d_cont eq " *") {
		$d_start = "";
		$d_end = "";
	}
}

$data =~ s{^(.*)\s*%(PSC|PSCGPL)_COPYRIGHT%.*\n}{<<EOF2}me;
$d_start$d_cont %$1_START_COPYRIGHT%
$d_cont -----------------------------------------------------------------------------
$d_cont -----------------------------------------------------------------------------
$d_cont %PSC_END_COPYRIGHT%$d_end
EOF2

$data =~ s
{\Q$d_start$d_cont\E %PSC_START_COPYRIGHT%
\Q$d_cont\E -----------------------------------------------------------------------------.*?
\Q$d_cont\E -----------------------------------------------------------------------------(.*)
\Q$d_cont\E %PSC_END_COPYRIGHT%\Q$d_end\E
}
{$d_start$d_cont %PSC_START_COPYRIGHT%
$d_cont -----------------------------------------------------------------------------
$d_cont Copyright (c) $cpyears, Pittsburgh Supercomputing Center (PSC).
$d_cont
$d_cont Permission to use, copy, and modify this software and its documentation
$d_cont without fee for personal use or non-commercial use within your organization
$d_cont is hereby granted, provided that the above copyright notice is preserved in
$d_cont all copies and that the copyright and this permission notice appear in
$d_cont supporting documentation.  Permission to redistribute this software to other
$d_cont organizations or individuals is not permitted without the written permission
$d_cont of the Pittsburgh Supercomputing Center.  PSC makes no representations about
$d_cont the suitability of this software for any purpose.  It is provided "as is"
$d_cont without express or implied warranty.
$d_cont -----------------------------------------------------------------------------$1
$d_cont %PSC_END_COPYRIGHT%$d_end
}s;

$data =~ s
{\Q$d_start$d_cont\E %PSCGPL_START_COPYRIGHT%
\Q$d_cont\E -----------------------------------------------------------------------------.*?
\Q$d_cont\E -----------------------------------------------------------------------------(.*)
\Q$d_cont\E %PSC_END_COPYRIGHT%\Q$d_end\E
}
{$d_start$d_cont %PSCGPL_START_COPYRIGHT%
$d_cont -----------------------------------------------------------------------------
$d_cont Copyright (c) $cpyears, Pittsburgh Supercomputing Center (PSC).
$d_cont
$d_cont This program is free software; you can redistribute it and/or modify
$d_cont it under the terms of the GNU General Public License as published by
$d_cont the Free Software Foundation; either version 2 of the License, or (at
$d_cont your option) any later version.
$d_cont
$d_cont This program is distributed WITHOUT ANY WARRANTY; without even the
$d_cont implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
$d_cont PURPOSE.  See the GNU General Public License contained in the file
$d_cont `COPYING-GPL' at the top of this distribution or at
$d_cont https://www.gnu.org/licenses/gpl-2.0.html for more details.
$d_cont
$d_cont Pittsburgh Supercomputing Center	phone: 412.268.4960  fax: 412.268.5832
$d_cont 300 S. Craig Street			e-mail: remarks\@psc.edu
$d_cont Pittsburgh, PA 15213			web: http://www.psc.edu/
$d_cont -----------------------------------------------------------------------------$1
$d_cont %PSC_END_COPYRIGHT%$d_end
}s;

print $data;
EOF
done
