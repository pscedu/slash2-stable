# $Id$

ROOTDIR=../../..
include ${ROOTDIR}/Makefile.path

TEST=		rwlock_test
SRCS+=		rwlock_test.c
SRCS+=		${PFL_BASE}/psc_util/alloc.c
SRCS+=		${PFL_BASE}/psc_util/init.c
SRCS+=		${PFL_BASE}/psc_util/log.c
SRCS+=		${PFL_BASE}/psc_util/pthrutil.c
SRCS+=		${PFL_BASE}/psc_util/random.c

MODULES+=	pthread

include ${PFLMK}
