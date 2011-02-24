# $Id$

ROOTDIR=.
include ${ROOTDIR}/Makefile.path

SUBDIRS+=	${PFL_BASE}
SUBDIRS+=	apps/sft
SUBDIRS+=	fio
SUBDIRS+=	${SLASH_BASE}
SUBDIRS+=	${ZEST_BASE}

COPYRIGHT_PATS+=distrib zfs

include ${MAINMK}
