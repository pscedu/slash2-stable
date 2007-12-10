# $Id$

include Makefile.path

LIBRARY=	${LNET_BASE}/lib/libzcfs.a
SRCS=		debug.c
SRCS+=		nidstrings.c
SRCS+=		user-lock.c
SRCS+=		user-prim.c
INCLUDES+=	-I${LNET_BASE}/include
INCLUDES+=	-I${PFL_BASE}/include
TARGET=		${LIBRARY}

include ${LLMK}
