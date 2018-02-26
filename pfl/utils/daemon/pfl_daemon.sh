#!/bin/sh
# $Id$
# %ISC_START_LICENSE%
# ---------------------------------------------------------------------
# Copyright 2015-2016, Google, Inc.
# Copyright 2013-2018, Pittsburgh Supercomputing Center
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
coredir=core/$host
 
# This is the default source code directory for the slash2 software. 
# It can be overridden by the daemon config file (i.e., $prof.dcfg) 
# if necessary.
 
srcdir=/local/src/p
uname=$(uname)
filter=
verbose=0
testmail=0
prof=
allow_logfiles_over_nfs=0

# Print a message if verbose (-v) mode is enabled.

vprint()
{
	[ $verbose -eq 1 ] && echo "$@"
}

# Extract something out of a key1=val1,key2=val2,... expression.

extract_value()
{
	key=$1
	perl -MEnglish -Wle '
		while (<>) {
			for (split /,/, $_) {
				/=/ or next;
				$h{$PREMATCH} = $POSTMATCH;
			}
		}
		if (exists($h{'$key'})) {
			print $h{'$key'};
		}'
}

# Load profile for the host where invoked.

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
		allow_logfiles_over_nfs=*|\
		ctl=*|\
		mod=*|\
		mp=*|\
		name=*|\
		narg=*|\
		prog=*|\
		srcdir=*)
			local newvalue=${fl#*=}
			eval ${fl%%=*}=\$newvalue;;
		args=*)	
			# xargs is a comma separated list of arguments.
			# Some of them can have values.

			xargs+=("${fl#args=}")
			ctlsock=$(echo $xargs | extract_value ctlsock)
			[ -n "$ctlsock" ] && export CTL_SOCK_FILE=$ctlsock
			;;
		bounce)	;;
		share)	;;
		tag=*)	[ x"$1" = x"${fl#tag=}" ] || return 1 ;;
		[A-Z][A-Z_]*=*)

			# Handle environment variables

			export "$fl";;
		*)	warn "unknown setting $fl";;
		esac
		[ $dobreak -eq 1 ] && break
	done

	[ $# -gt $narg ] && usage

	vprint "profile applied: $t0"

	return 0
}

# Apply settings to the shell interpretter environment from the loaded
# profile.
#
# Called from slashd.sh, sliod.sh, and mount_slash.sh

apply_host_prefs()
{
	local narg=0 tmpbase fn

	vprint "searching for profile; prof=$prof; host=$host; prog=$prog"

	for dir in %%INST_BASE%%; do
		tmpbase=$dir/pfl_daemon.cfg
		[ -d $tmpbase ] || continue

		fn=$tmpbase/local
		[ -f $fn ] && . $fn

		fn=$tmpbase/$prof.dcfg
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
		warn "no profile for host $host; assuming defaults"
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

# Launch my gdb with some custom settings.

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
	export GDBHISTFILE=$coredir/$prog.$id.gdbhist

	# hack for some systems
	[ -e /bin/bash ] && export SHELL=/bin/bash

	exec gdb -f -q -x $tmpfn $prog
}

# Perform daemon post processing (i.e. after the daemon exits).

postproc()
{
	ex=$1

	trap '' EXIT

	cf=$prog.$id.cdump

	# Rename the core to our "unique" name

	mv -f *core* $cf 2>/dev/null

	# Send a message via logger as well. On CentOS, logger will
	# write log messages to /var/log/messages by default.

	if [ -f "/usr/bin/logger" ]
	then
		if [ $ex -gt 128 ]
		then
			echo "$prog ($base) receive signal $((ex-128))" | logger
		else
			echo "$prog ($base) exit with code $ex" | logger
		fi  
	fi

	# If the core file exists and the mail address is set, send an email.
	# The mail address is set in the so-called daemon configuration file
	# (e.g., pylon2.dcfg).

	# PSC_LOG_FILE_LINK is set up in mount_slash.sh, sliod.sh, or slashd.sh
	# This only works if each daemon has its own log directory or the link
	# itself is different among different daemon instance.

	if [ -e "$cf" -a -n "$mail_to" ]; then
		chmod og+r $cf

		cmdfile=/tmp/gdbcmd.$id
		echo thr ap all bt > $cmdfile
		{
			echo From: $mail_from
			echo To: $mail_to
			echo "Subject: [sysbug] $prof $host $name down"
			echo
			#
			# This is done manually to match the version of the
			# slash2 code base. It should be the last submit to
			# to be accurate. Use + to allow lazy update.
			#
			echo slash2 version is 45183+
			echo core file is $base/$coredir/$cf
			echo binary is $base/$coredir/$prog.$id
			
			# As long as our our daemon open the file within the same
			# second, the log file name will match.

			echo log is most likely $base/log/$host.$name/$tm

			[ $ex -gt 128 ] && echo exited via signal $((ex-128))
			echo --------------------------------------------------
			tail $PSC_LOG_FILE_LINK
			echo --------------------------------------------------
			gdb -batch -c $cf -x $cmdfile $prog.$id 2>&1 | $srcdir/tools/filter-pstack
		} | sendmail -t

		echo binary was $base/$coredir/$prog.$id

		echo log file was most likely $base/log/$host.$name/$tm
		rm -f $cmdfile
	else
		rm -f $prog.$id
	fi
}

cleanup()
{
	$ctl stop
	postproc 1
	exit 0
}

# Determine if the given directory is remotely mounted.

is_on_nfs()
{
	local dir=$1

	# get the Mounted on column of df output
	mp=$(df $dir | { read; sed 's/.* //'; })
	mount | grep " on $mp " | grep -qw nfs
}

# Perform daemon launch pre processing.

preproc()
{
	PSC_TIMEOUT=5 $ctl -p sys.version >/dev/null && \
	    die "another instance detected; exiting"

	local logdir=$(dirname "$PSC_LOG_FILE")

	is_on_nfs "$logdir" && [ $allow_logfiles_over_nfs -eq 0 ] && \
	    warn "$logdir: attempting to write log files on NFS"

	mkdir -p $logdir
	cd $base
	mkdir -p $coredir

	find log/$host.$name/ -type f -mtime +30 -exec rm -f {} \;
	find $coredir -type f -size 0 -exec rm -f {} \;

	# Running in my own directory to avoid conflicts

	cd $coredir

	while :; do
		id=$RANDOM
		[ -e $prog.$id ] || break
		sleep 1
	done

	# Make a copy before hand.

	cp `which $prog` $prog.$id
	tm=$(date +%s)
	trap cleanup EXIT
}

# Backoff-sensitive sleep, invoked before relaunch of a daemon instance.

vsleep()
{
	local amt=0

	# If this is not the first time of execution, we will use the
	# current value exported to us.  Otherwise, we avoid using the 
	# wildcard match in the following case statement.

	if [ ! -v ATTEMPT ]
	then
		ATTEMPT=1
	fi

	# If we have been running for 60+ seconds, we assume a successful
	# restart and reset the ATTEMPT variable.
	#
	# Note that the value of SECONDS is reset after each exec call.

	if [ $SECONDS -gt 60 ]
	then
		ATTEMPT=1
		return
	fi

	# Cap the maximum delay between retry at 1920 seconds (or 32 minutes)

	case $ATTEMPT in
	1)	let amt=30	ATTEMPT++	;;
	2)	let amt=60	ATTEMPT++	;;
	3)	let amt=120	ATTEMPT++	;;
	4)	let amt=240	ATTEMPT++	;;
	5)	let amt=480	ATTEMPT++	;;
	6)	let amt=960	ATTEMPT++	;;
	*)	let amt=1920	ATTEMPT++
	esac
	export ATTEMPT

	echo attempt $ATTEMPT: restarting after $amt seconds...
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

	# This should also pick up a new version of the wrapper script that
	# has been installed.

	exec $0 "${bkav[@]}"
}

# Launch a daemon, doing any "daemonization" necessary.

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

# Utility routine for generating a batch of profiles for hosts offering
# the SLASH2 I/O service.

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

		# Add info for each sliod to match a host profile. 
		# Each option is separated by %.

		echo -n $host
		echo -n %sliod

		# tag is used to match instance, such as sliod.sh 1

		echo -n %tag=$i
		echo -n %narg=1
		echo -n %ctl=slictl$i

		# We only override name for sliod because we can run
		# multiple sliod daemons on the same host.  We want
		# each daemon to have its own log directory.

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

# Utility routine for generating a batch of profiles for hosts offering
# the SLASH2 client service.

mkclients()
{
	# The underscore (_) assigns/localizes the $_, which is the last 
	# argument of the previous command. 

	local opts=$1 i _ start=0
	shift

	[ -n "$opts" ] && opts=%$opts

	# Execute once for each positional parameter (e.g., $1, $2) that 
	# is set.
	#
	# This loop parses client specifications in a slash2 daemon 
	# configuration file such as dxcgpu%02d:1-3, dxclsm%02d:3, etc.

	for hspec; do

		# Remove shortest suffix matching ":*"
		local hclass=${hspec%:*}

		# Remove shortest prefix matching "*:"
		local range=${hspec#*:}

		# If the host class matches the host specification, then
		# there is only one host in the class.

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

preinit()
{
	if [ $testmail -eq 1 ]; then
		postproc 0
		exit
	fi
}
