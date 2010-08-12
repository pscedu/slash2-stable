# $Id$

ROOTDIR=.
include ${ROOTDIR}/Makefile.path

SUBDIRS+=	apps/sft
SUBDIRS+=	fio
SUBDIRS+=	${PFL_BASE}/tests
SUBDIRS+=	${PFL_BASE}/utils
SUBDIRS+=	${SLASH_BASE}
SUBDIRS+=	${ZEST_BASE}

MAN+=		${ROOTDIR}/doc/pflenv.7

include ${MAINMK}

zbuild:
	@(cd ${SLASH_BASE}/slashd && ${MAKE} zbuild)

rezbuild:
	@(cd ${SLASH_BASE}/slashd && ${MAKE} rezbuild)
