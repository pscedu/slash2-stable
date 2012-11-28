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

apply_host_prefs()
{
	for i in							\
	    /local/pfl_daemon.cfg					\
	    /usr/local/pfl_daemon.cfg
	 do
		if [ -f $i ]; then
			. $i
			[ -f $i.local ] && . $i.local
			return
		fi
	done
	die "cannot find pfl_daemon.cfg"
}

die()
{
	echo "$0: $@"
	exit 1
}

mygdb()
{
	echo program arguments are: $@
	echo ----------------------------------------------------------
	exec gdb $prog
}

postproc()
{
	cf=c/$prog.$id.core
	mv -f *core* c/

	if [ -e "$cf" -a -n "$mail_to" ]; then
		chmod og+r $cf

		case $(uname) in
		FreeBSD)	frompre= frompost="-- -f $mail_from"	;;
		*)		frompre="-r $mail_from" frompost=	;;
		esac

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
		rm $cmdfile
	else
		rm c/$prog.$id
	fi
}

cleanup()
{
	$ctl stop
	postproc
	sleep 10
	exit 0
}

do_exec()
{
	# Execute
	src=$dir/src/p
	mkdir -p $(dirname $PSC_LOG_FILE)
	cd $base
	mkdir -p c

	find log/$host.$name/ -mtime +30 -exec rm {} \;
	find c/ -size 0 -exec rm {} \;

	# delete core files with no accompanying executable
	# for i in c/*.[0-9][0-9][0-9] c/*.[0-9][0-9][0-9][0-9] c/*.[0-9][0-9][0-9][0-9][0-9]; do
	#  if ! [ -e $i.core ]; then rm $i; fi; done
	#

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
