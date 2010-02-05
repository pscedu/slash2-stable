# $Id$

ROOTDIR=.
include Makefile.path

SUBDIRS+=	apps/sft
SUBDIRS+=	fio
SUBDIRS+=	psc_fsutil_libs/tests
SUBDIRS+=	psc_fsutil_libs/utils
SUBDIRS+=	slash_nara
SUBDIRS+=	slash_nara/tests
SUBDIRS+=	slash_nara/utils
SUBDIRS+=	zest
SUBDIRS+=	zest/client/linux
SUBDIRS+=	zest/tests
SUBDIRS+=	zest/utils

include mk/main.mk

build-prereq:
	(cd zest/utils/typedump && ${MAKE} regen)
	(cd psc_fsutil_libs/utils/typedump && ${MAKE} regen)
	(cd slash_nara/utils/typedump && ${MAKE} regen)
