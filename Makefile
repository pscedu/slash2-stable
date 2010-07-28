# $Id$

ROOTDIR=.
include ${ROOTDIR}/Makefile.path

SUBDIRS+=	apps/sft
SUBDIRS+=	fio
SUBDIRS+=	psc_fsutil_libs/tests
SUBDIRS+=	psc_fsutil_libs/utils
SUBDIRS+=	slash_nara
SUBDIRS+=	zest

MAN+=		${ROOTDIR}/doc/pflenv.7

include ${MAINMK}
