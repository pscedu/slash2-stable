#!/usr/bin/env bash
# $Id$

set -e

echorun()
{
	echo "$@"
	"$@"
}

clone()
{
	local url=$1
	local dir=$2
	echorun mv $dir $dir.oldsvn
	echorun git clone $url $dir
}

getrepo()
{
	local base=$1
	local name=${2//\//.}
	local dir=$2
	[ $# -eq 3 ] && dir=$3
	cd $dir && git pull || clone $base/$name $dir
}

pubrepo()
{
	(getrepo https://github.com/pscedu "$@")
}

pubrepo()
{
	(getrepo git://source.psc.edu "$@")
}

base=$(basename $0)
dir=$(dirname $0)
cd $dir
pubrepo proj p
cd p
pubrepo distrib/crcutil
pubrepo distrib/fstest
pubrepo distrib/fuse
pubrepo distrib/iozone
pubrepo distrib/libs3
pubrepo distrib/spiobench
pubrepo distrib/stumbleupon-tcollector
pubrepo distrib/zfs-fuse
pubrepo fio
pubrepo idxsearch
pubrepo mkidx
pubrepo psync
pubrepo sft
pubrepo slash2
pubrepo src-upd
pubrepo xopctl

prirepo inf
prirepo zest
