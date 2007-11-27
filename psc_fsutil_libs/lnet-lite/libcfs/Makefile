# $Id$

ROOTDIR = ../..
include ${ROOTDIR}/mk/defs.mk

LIBRARY=	${LNET_BASE}/lib/libzcfs.a
SRCS=		debug.c
SRCS+=		nidstrings.c
SRCS+=		user-lock.c
SRCS+=		user-prim.c
INCLUDES+=	-I${LNET_BASE}/include

TARGET=		${LIBRARY}

include ${MAINMK}
