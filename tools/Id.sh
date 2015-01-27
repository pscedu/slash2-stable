#!/bin/sh
# $Id$
# %PSC_START_COPYRIGHT%
# -----------------------------------------------------------------------------
# Copyright (c) 2007-2010, Pittsburgh Supercomputing Center (PSC).
#
# Permission to use, copy, modify, and distribute this software
# for any purpose with or without fee is hereby granted, provided
# that the above copyright notice and this permission notice
# appear in all copies.
#
# THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL
# WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS.  IN NO EVENT SHALL
# THE AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR
# CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
# LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT,
# NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
# CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
#
# Pittsburgh Supercomputing Center	phone: 412.268.4960  fax: 412.268.5832
# 300 S. Craig Street			e-mail: remarks@psc.edu
# Pittsburgh, PA 15213			web: http://www.psc.edu/
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
