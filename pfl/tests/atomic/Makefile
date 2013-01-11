# $Id$

ROOTDIR=../../..
include ${ROOTDIR}/Makefile.path

TEST=		atomic_test
SRCS+=		atomic_test.c
SRCS+=		${PFL_BASE}/psc_util/alloc.c
SRCS+=		${PFL_BASE}/psc_util/init.c
SRCS+=		${PFL_BASE}/psc_util/log.c
SRCS+=		${PFL_BASE}/psc_util/pthrutil.c

MODULES+=	pthread pfl barrier

include ${PFLMK}
