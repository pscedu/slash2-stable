# $Id$

ROOTDIR=../../..
include ${ROOTDIR}/Makefile.path

BIN+=		pfl_daemon.sh
MAN+=		dcfg.5
INSTDIR=	${INST_SBINDIR}
FORCE_INST=	1

include ${PFLMK}

install-hook-post:
	perl -i -pe 's{%%INST_BASE%%}{${INST_BASE}}' $(strip \
	    ) ${INST_SBINDIR}/pfl_daemon.sh
