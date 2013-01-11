# $Id$

ROOTDIR=../../..
include ${ROOTDIR}/Makefile.path

TEST=		atomic_test
SRCS+=		atomic_test.c
SRCS+=		${PFL_BASE}/alloc.c
SRCS+=		${PFL_BASE}/init.c
SRCS+=		${PFL_BASE}/log.c
SRCS+=		${PFL_BASE}/pthrutil.c

MODULES+=	pthread pfl barrier

include ${PFLMK}
