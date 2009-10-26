# $Id$

include ../Makefile.path

PROG=		atomic
SRCS=		atomic.c
SRCS+=		${PFL_BASE}/psc_util/alloc.c
SRCS+=		${PFL_BASE}/psc_util/init.c
SRCS+=		${PFL_BASE}/psc_util/log.c
DEFINES+=	-DHAVE_LIBPTHREAD
LDFLAGS+=	${THREAD_LIBS}

include ${PFLMK}
