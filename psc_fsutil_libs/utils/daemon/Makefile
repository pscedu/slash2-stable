# $Id$

ROOTDIR=../../..
include ${ROOTDIR}/Makefile.path

include ${PFLMK}
-include local.mk

install-hook:
	cp -f pfl_daemon.sh ${INST_SBINDIR}
	[ -n "${CFG_DST}" ] && cp -f pfl_daemon.cfg ${CFG_DST}
