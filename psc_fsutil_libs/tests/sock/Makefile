# $Id: Makefile 4838 2009-01-05 22:51:24Z yanovich $

include ../Makefile.path

PROG=		sock
SRCS=		sock.c
SRCS+=		${PFL_BASE}/psc_util/alloc.c
SRCS+=		${PFL_BASE}/psc_util/log.c
SRCS+=		${PFL_BASE}/psc_util/strlcpy.c
LDFLAGS+=	${THREAD_LIBS}

include ${PFLMK}
