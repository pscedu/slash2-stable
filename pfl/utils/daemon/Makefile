# $Id$

ROOTDIR=../../..
include ${ROOTDIR}/Makefile.path

BIN+=		pfl_daemon.sh
INSTDIR=	${INST_SBINDIR}
FORCE_INST=	1

include ${PFLMK}
-include local.mk

install-hook:
	@if [ -n "${CFG_DST}" ]; then					\
		${ECHORUN} cp -f pfl_daemon.cfg ${CFG_DST};		\
	fi
