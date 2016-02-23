#!/bin/sh
# $Id$
# %ISC_START_LICENSE%
# ---------------------------------------------------------------------
# Copyright 2015-2016, Google, Inc.
# Copyright 2013-2016, Pittsburgh Supercomputing Center
# All rights reserved.
#
# Permission to use, copy, modify, and distribute this software for any
# purpose with or without fee is hereby granted, provided that the
# above copyright notice and this permission notice appear in all
# copies.
#
# THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL
# WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS.  IN NO EVENT SHALL THE
# AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
# DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
# PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
# TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
# PERFORMANCE OF THIS SOFTWARE.
# --------------------------------------------------------------------
# %END_LICENSE%

# pfl_daemon.sh: routines for launching daemons, including
# features such as: auto-restart on crash; e-mailing of coredump
# and stack traces; avoidance of multiple daemon instances
#
# see http://github.com/pscedu/pfl

host=$(hostname -s)
nodaemonize=0
name=$prog
srcdir=/local/src/p
uname=$(uname)
filter=
verbose=0
prof=

vprint()
{
	[ $verbose -eq 1 ] && echo "$@"
}

loadprof()
{
	local _h=${1%%%*}
	local t0=${1#*%}
	local _p=${t0%%%*}
	local _fl=${t0#*%}
	local dobreak=0
	shift

	[ x"$_p" = x"mount_slash" ] && _p=mount_wokfs

	vprint "considering host $_h, prog $_p"

	[ x"${_h%%.*}" = x"$host" ] || return 1
	[ x"$_p" = x"$prog" ] || return 1
	[ -n "$_fl" ] || return 0

	vprint "applying profile"

	while :; do
		fl=${_fl%%%*}
		_fl=${_fl#*%}
		[ x"$fl" = x"$_fl" ] && dobreak=1
		fl=$(echo $fl | perl -pe 's/\\x(..)/chr hex $1/ge')

		vprint "  + processing setting: $fl"

		case $fl in
		args=*)		xargs+=("${fl#args=}");;
		bounce)		;;
		ctl=*)		ctl=${fl#ctl=};;
		mod=*)		mod=${fl#mod=};;
		mp=*)		mp=${fl#mp=};;
		name=*)		name=${fl#name=};;
		narg=*)		narg=${fl#narg=};;
		prog=*)		prog=${fl#prog=};;
		share)		;;
		srcdir=*)	srcdir=${fl#srcdir=};;
		tag=*)		[ x"$1" = x"${fl#tag=}" ] || return 1 ;;
		[A-Z][A-Z_]*=*)	export "$fl";;
		*)		warn "unknown setting $fl";;
		esac
		[ $dobreak -eq 1 ] && break
	done

	[ $# -gt $narg ] && usage

	vprint "profile applied: $t0"

	return 0
}

apply_host_prefs()
{
	local narg=0 base fn

	vprint "searching for profile; prof=$prof; host=$host; prog=$prog"

	for dir in %%INST_BASE%%; do
		base=$dir/pfl_daemon.cfg
		[ -d $base ] || continue

		fn=$base/local
		[ -f $fn ] && . $fn

		fn=$base/$prof.dcfg
		[ -f $fn ] || continue

		av="$@"

		export PFL_SYSLOG_IDENT=%n-$prof

		. $fn
		vprint "scanning profiles from $fn; args ${av[@]}"
		[ $# -eq 0 ] && die "unknown deployment: ${av[0]}"
		for ln; do
			vprint "checking: $ln"
			loadprof $ln ${av[@]} || continue
			vprint "deployment $prof, host $host"
			return
		done
		warn "no profile for this host; assuming defaults"
		[ ${#av[@]} -gt $narg ] && usage
		return
	done
	die "cannot find $prof.dcfg"
}

warn()
{
	echo "$@" >&2
}

die()
{
	echo "$0: $@"
	exit 1
}

mygdb()
{
	shift
	local tmpfn=/tmp/gdbinv.$RANDOM
	{
		echo set args $@
		echo set confirm off
		echo set height 0
		echo set history save on
		echo set history size 10000
		echo set print pretty
		echo r
	} > $tmpfn
	export GDBHISTFILE=c/$prog.$id.gdbhist

	# hack for some systems
	[ -e /bin/bash ] && export SHELL=/bin/bash

	exec gdb -f -q -x $tmpfn $prog
}

postproc()
{
	ex=$1

	trap '' EXIT

	cf=c/$prog.$id.core
	mv -f *core* $cf 2>/dev/null

	if [ -e "$cf" -a -n "$mail_to" ]; then
		chmod og+r $cf

		cmdfile=/tmp/gdbcmd.$id
		echo thr ap all bt > $cmdfile
		{
			echo From: $mail_from
			echo To: $mail_to
			echo "Subject: [sysbug] $prof $host $name down"
			echo
			echo core file is $base/$cf
			echo binary is $base/c/$prog.$id
			echo log is $base/log/$host.$name/$tm
			[ $ex -gt 128 ] && echo exited via signal $((ex-128))
			echo --------------------------------------------------
			tail $PSC_LOG_FILE_LINK
			echo --------------------------------------------------
			gdb -batch -c $cf -x $cmdfile c/$prog.$id 2>&1 | $srcdir/tools/filter-pstack
		} | sendmail -t
		echo binary was $base/c/$prog.$id
		echo log file was $base/log/$host.$name/$tm
		rm $cmdfile
	else
		rm c/$prog.$id
	fi

	[ $ex -eq 0 ] && exit
}

cleanup()
{
	$ctl stop
	postproc 1
	exit 0
}

is_on_nfs()
{
	local dir=$1

	# get the Mounted on column of df output
	mp=$(df $dir | { read; sed 's/.* //'; })
	mount | grep " on $mp " | grep -qw nfs
	[ $? -eq 0 ]
}

preproc()
{
	PSC_TIMEOUT=5 $ctl -p sys.version >/dev/null && \
	    die "another instance detected; exiting"

	local logdir=$(dirname "$PSC_LOG_FILE")

	is_on_nfs "$logdir" && \
	    die "$logdir: refusing to write log files on NFS"

	mkdir -p $logdir
	cd $base
	mkdir -p c

	find log/$host.$name/ -type f -mtime +30 -exec rm {} \;
	find c/ -type f -size 0 -exec rm {} \;

	# delete core files with no accompanying executable
#	n=[0-9]
#	for i in c/*.$n$n$n c/*.$n$n$n$n c/*.$n$n$n$n$n; do
#		[ -e $i.core ] || rm $i
#	done

	while :; do
		id=$RANDOM
		[ -e c/$prog.$id ] || break
		sleep 1
	done

	mv -f *core* c/ 2>/dev/null
	cp `which $prog` c/$prog.$id
	tm=$(date +%s)
	trap cleanup EXIT
}

vsleep()
{
	local amt=0

	[ $SECONDS -gt 60 ] && return

	case $ATTEMPT in
	1)	let amt=10	ATTEMPT++	;;
	2)	let amt=30	ATTEMPT++	;;
	3)	let amt=60	ATTEMPT++	;;
	4)	let amt=600	ATTEMPT++	;;
	5)	let amt=1800	ATTEMPT++	;;
	6)	let amt=3600			;;
	*)	ATTEMPT=1;
	esac
	export ATTEMPT

	echo restarting after $amt seconds...
	sleep $amt
}

# This script runs within another service script using the source or dot
# operator.  The backup argv (bkav) is saved by the service script and
# is used here to re-execute ourself when the service daemon dies.
_rundaemon()
{
	preproc
	"$@"
	postproc $?
	vsleep

	# Do we need to somehow clean up the socket here? zfs-fuse listener
	# sometimes reads garbage and abort. We have to kill slashd.sh to
	# restart slashd.

	exec $0 "${bkav[@]}"
}

rundaemon()
{
	vprint "launching daemon: $@"

	if [ $nodaemonize -eq 0 ]; then
		(_rundaemon "$@" 0<&- &>/dev/null &) &
		disown -a
	else
		_rundaemon "$@"
	fi
}

mksliods()
{
	local noif0=0 OPTIND c i _

	# Linux does not have if:0, so make a special exception to just
	# use `if'
	case $uname in
	Linux) noif0=1;;
	esac

	local host=$1
	local np=$2
	local if=$3
	local ctlsock=$4
	local opts=$5
	local nif

	[ -n "$opts" ] && opts=%$opts

	for i in $(seq 0 $((np - 1))); do
		echo -n $host
		echo -n %sliod
		echo -n %tag=$i
		echo -n %narg=1
		echo -n %ctl=slictl$i
		echo -n %name=sliod$i
		echo -n %CTL_SOCK_FILE=
		printf $ctlsock $i
		echo -n %LNET_NETWORKS=

		if [ $noif0 -eq 1 -a $i -eq 0 ]; then
			nif=$(echo $if | sed 's/:%d//g')
		else
			nif=$(echo $if | sed "s/%d/$i/g")
		fi
		echo -n $nif

		[ $i -gt 0 ] && echo -n %share
		echo $opts
	done
}

mkclients()
{
	local opts=$1 i _ start=0
	shift

	[ -n "$opts" ] && opts=%$opts

	for hspec; do
		local hclass=${hspec%:*}
		local range=${hspec#*:}
		if [ x"$hclass" = x"$hspec" ]; then
			start=1
			end=1
		else
			if [ x"${range%-*}" = x"$range" ]; then
				# range is not set; start at zero and interpret
				# range as number of hosts
				start=0
				end=$range
			else
				# range (`start'-`end') is specified; interpret
				# directly
				start=${range%-*}
				end=${range#*-}
			fi
		fi
		for i in $(seq $start $end); do
			printf -- $hclass $i
			echo -n %mount_slash
			[ $i -gt 0 ] && echo -n %share
			echo $opts
		done
	done
}
