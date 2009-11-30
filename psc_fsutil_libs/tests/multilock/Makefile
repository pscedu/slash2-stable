# $Id$

include ../Makefile.path

PROG=		multilock_test
SRCS=		multilock_test.c
SRCS+=		${PFL_BASE}/psc_ds/dynarray.c
SRCS+=		${PFL_BASE}/psc_ds/vbitmap.c
SRCS+=		${PFL_BASE}/psc_util/alloc.c
SRCS+=		${PFL_BASE}/psc_util/init.c
SRCS+=		${PFL_BASE}/psc_util/log.c
SRCS+=		${PFL_BASE}/psc_util/multilock.c
SRCS+=		${PFL_BASE}/psc_util/pthrutil.c

DEFINES+=	-DHAVE_LIBPTHREAD
LDFLAGS+=	${THREAD_LIBS}

include ${PFLMK}
