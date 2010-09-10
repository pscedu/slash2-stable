#!/bin/sh
# $Id$
# %PSC_START_COPYRIGHT%
# -----------------------------------------------------------------------------
# Copyright (c) 2007-2010, Pittsburgh Supercomputing Center (PSC).
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

getopts "f" opt

if [ x"$opt" != x"f" ]; then
	echo "This script should only be run on nothing-to-lose repositories,"
	echo "because it *WILL* destroy all changes present."
	echo
	echo "If you are sure, then invoke with the -f flag."
	exit
fi

reldir=$(dirname $0)
root=$(pwd)/$reldir/..

find $root -type f \! -path '*/.svn/*' -exec svn propset svn:keywords '' {} \;
find $root -type f \! -path '*/.svn/*' -exec /bin/rm {} \;

echo "Your repository will now show \$Id\$ tags unexpanded for quick"
echo "comparision against expanded strings hardcoded into revisions."
