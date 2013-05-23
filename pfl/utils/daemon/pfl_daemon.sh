#!/bin/sh
# $Id$
# TODO
#  - add a checker to prevent multiple instances simultaneously

host=$(hostname -s)
name=$prog
ud=/usr/local
dir=/local
mygdb=
mystrace=
verbose=0
prof=

verbose()
{
	[ $verbose -eq 1 ] && echo "$@"
}

loadprof()
{
	local _h=${1%%%*}
	local t0=${1#*%}
	local _p=${t0%%%*}
	local _fl=${t0#*%}
	shift

	[ x"${_h%%.*}" = x"$host" ] || return 1
	[ x"$_p" = x"$prog" ] || return 1
	[ -n "$_fl" ] || return 0

	while :; do
		fl=${_fl%%%*}
		_fl=${_fl#*%}
		case $fl in
		args=*)	xargs=${fl#args=};;
		bounce)	;;
		ctl=*)	ctl=${fl#ctl=};;
		dir=*)	dir=${fl#dir=};;
		name=*)	name=${fl#name=};;
		narg=*)	narg=${fl#narg=};;
		share)	;;
		tag=*)	[ x"$1" = x"${fl#tag=}" ] || return 1 ;;
		*)	export $fl;;
		esac
		[ x"$fl" = x"$_fl" ] && break
	done

	[ $# -gt $narg ] && usage

	return 0
}

apply_host_prefs()
{
	local narg=0
	for fn in							\
	    /local/pfl_daemon.cfg					\
	    /usr/local/pfl_daemon.cfg
	do
		if [ -f $fn ]; then
			av="$@"
			[ -f $fn.local ] && . $fn.local
			. $fn
			[ $# -eq 0 ] && die "unknown deployment: ${av[0]}"
			for ln; do
				loadprof $ln ${av[@]} || continue
				verbose "deployment $prof, host $host"
				return
			done
			warn "no profile for this host; assuming defaults"
			[ ${#av[@]} -gt $narg ] && usage
			return
		fi
	done
	die "cannot find pfl_daemon.cfg"
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
	# gdb hack
	export SHELL=/bin/bash
	exec gdb -f -q -x $tmpfn $prog
}

postproc()
{
	ex=$1

	cf=c/$prog.$id.core
	mv -f *core* $cf 2>/dev/null

	if [ -e "$cf" -a -n "$mail_to" ]; then
		chmod og+r $cf

		frompre=
		frompost=
		if mail -V >/dev/null 2>&1; then
			# GNU mailx -- use native flag
			frompre="-r $mail_from"
		else
			# BSD mailx -- use sendmail flag
			frompost="-f $mail_from"
		fi

		cmdfile=/tmp/gdbcmd.$id
		echo thr ap all bt > $cmdfile
		{
			echo core file is $base/$cf
			echo binary is $base/c/$prog.$id
			echo log is $base/log/$host.$name/$tm
			echo --------------------------------------------------
			tail $PSC_LOG_FILE_LINK
			echo --------------------------------------------------
			gdb -batch -c $cf -x $cmdfile c/$prog.$id 2>&1 | $src/tools/filter-pstack
		} | mail -s "$host $name down" $frompre $mail_to $frompost
		echo binary was $base/c/$prog.$id
		rm $cmdfile
	else
		rm c/$prog.$id
	fi

	trap '' EXIT
	[ $ex -eq 0 ] && exit
}

cleanup()
{
	$ctl stop
	postproc 1
	sleep 10
	exit 0
}

preproc()
{
	src=$dir/src/p
	mkdir -p $(dirname $PSC_LOG_FILE)
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
