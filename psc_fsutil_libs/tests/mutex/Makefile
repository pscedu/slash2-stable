# $Id$

include ../Makefile.path

PROG=		mutex_test
SRCS=		mutex_test.c
SRCS+=		${PFL_BASE}/psc_util/alloc.c
SRCS+=		${PFL_BASE}/psc_util/log.c
SRCS+=		${PFL_BASE}/psc_util/pthrutil.c
LDFLAGS+=	${THREAD_LIBS}

include ${PFLMK}
