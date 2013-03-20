# $Id$

ROOTDIR=../../..
include ${ROOTDIR}/Makefile.path

TEST=		mutex_test
SRCS+=		mutex_test.c
SRCS+=		${PFL_BASE}/alloc.c
SRCS+=		${PFL_BASE}/completion.c
SRCS+=		${PFL_BASE}/init.c
SRCS+=		${PFL_BASE}/log.c
SRCS+=		${PFL_BASE}/pthrutil.c
SRCS+=		${PFL_BASE}/waitq.c

MODULES+=	pthread

include ${PFLMK}
