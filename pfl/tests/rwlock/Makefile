# $Id$

ROOTDIR=../../..
include ${ROOTDIR}/Makefile.path

TEST=		rwlock_test
SRCS+=		rwlock_test.c
SRCS+=		${PFL_BASE}/alloc.c
SRCS+=		${PFL_BASE}/init.c
SRCS+=		${PFL_BASE}/log.c
SRCS+=		${PFL_BASE}/pthrutil.c
SRCS+=		${PFL_BASE}/random.c

MODULES+=	pthread

include ${PFLMK}
