# $Id$

ROOTDIR=.
include ${ROOTDIR}/Makefile.path

SUBDIRS+=	apps/sft
SUBDIRS+=	fio
SUBDIRS+=	${PFL_BASE}
SUBDIRS+=	${SLASH_BASE}
SUBDIRS+=	${ZEST_BASE}

MAN+=		${ROOTDIR}/doc/pflctl.7
MAN+=		${ROOTDIR}/doc/pflenv.7

include ${MAINMK}
