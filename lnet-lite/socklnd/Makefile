# $Id$

include Makefile.path

LIBRARY=	${LNET_BASE}/lib/libsocknal.a
SRCS=		connection.c
SRCS+=		pqtimer.c
SRCS+=		procapi.c
SRCS+=		proclib.c
SRCS+=		select.c
SRCS+=		table.c
SRCS+=		tcplnd.c
SRCS+=		sendrecv.c
INCLUDES+=	-I${LNET_BASE}/include
INCLUDES+=	-I${ROOTDIR}/psc_fsutil_libs/include
TARGET=		${LIBRARY}

include ${LLMK}
