#!/bin/sh
# $Id$

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
