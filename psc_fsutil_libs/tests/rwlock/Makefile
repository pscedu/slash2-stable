# $Id: Makefile 8028 2009-09-22 18:50:25Z yanovich $

include ../Makefile.path

PROG=		rwlock_test
SRCS+=		rwlock_test.c
SRCS+=		${PFL_BASE}/psc_util/alloc.c
SRCS+=		${PFL_BASE}/psc_util/init.c
SRCS+=		${PFL_BASE}/psc_util/log.c
SRCS+=		${PFL_BASE}/psc_util/random.c
LDFLAGS+=	${THREAD_LIBS}

include ${PFLMK}
