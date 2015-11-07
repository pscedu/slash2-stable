#!/usr/bin/env bash
# $Id$

prog=lnrtd
ctl=lnrtctl

. ${0%/*}/pfl_daemon.sh

usage()
{
	echo "usage: $0 [-P profile] [-gs]" >&2
	exit 1
}

while getopts "gP:s" c; do
	case $c in
	g) mygdb='mygdb'	;;
	P) prof=$OPTARG		;;
	s) mystrace='strace'	;;
	*) usage		;;
	esac
done
shift $(($OPTIND - 1))

apply_host_prefs "$@"

base=$dir/$prof.s2
# Initialization/configuration
ulimit -n 100000
ulimit -c $((1024 * 1024 * 1024 * 50))
export LD_LIBRARY_PATH=/usr/local/lib
export PSC_SYSLOG=1
export PSC_LOG_LEVEL=warn
export PSC_LOG_LEVEL_info=info
export PSC_LOG_FILE=$base/log/$host.$name/%t
export PSC_LOG_FILE_LINK=$base/log/$host.$name/latest
export CONFIG_FILE=$base/slcfg

do_exec

$mystrace $mygdb $prog
[ $? -eq 0 ] && trap '' EXIT && exit

postproc

sleep 10
exec $0 "$@"
