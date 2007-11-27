# $Id$

ROOTDIR = ../..
IS_3RDPARTY =
include ${ROOTDIR}/mk/defs.mk

LIBRARY=	${LNET_BASE}/lib/libzptllnd.a
SRCS=		ptllnd.c
SRCS+=		ptllnd_cb.c
INCLUDES+=	-I${LNET_BASE}/include

TARGET=		${LIBRARY}

include ${MAINMK}
