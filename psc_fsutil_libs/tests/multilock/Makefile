# $Id$

include ../Makefile.path

PROG=		multilock
SRCS=		multilock.c
SRCS+=		${PFL_BASE}/psc_util/log.c
SRCS+=		${PFL_BASE}/psc_util/multilock.c
DEFINES+=	-DHAVE_LIBPTHREAD
LDFLAGS+=	${THREAD_LIBS}

include ${PFLMK}
