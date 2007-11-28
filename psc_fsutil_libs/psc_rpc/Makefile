# $Id$

# This Makefile is for building with the client only.
# The sources are included directly in zestiond and
# not linking with the zestrpc.a library.

ROOTDIR = ..
include ${ROOTDIR}/mk/defs.mk

LIBRARY=	libpscrpc.a
SRCS=		connection.c
SRCS+=		events.c
SRCS+=		export.c
SRCS+=		import.c
SRCS+=		niobuf.c
SRCS+=		packgeneric.c
SRCS+=		rpcclient.c
SRCS+=		nb.c
SRCS+=		service.c
SRCS+=		${ROOTDIR}/zestion/log.c
SRCS+=		${ROOTDIR}/zestion/zestWaitq.c
INCLUDES+=	-I${LNET_BASE}/include
INCLUDES+=	-I${ROOTDIR}/zestion
INCLUDES+=	-I${ROOTDIR}/intercept
LDFLAGS+=	${THREAD_LIBS} ${LNET_LIBS}

TARGET=		${LIBRARY}

include ${MAINMK}
