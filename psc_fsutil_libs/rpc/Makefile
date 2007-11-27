# $Id$

# This Makefile is for building with the client only.
# The sources are included directly in zestiond and
# not linking with the zestrpc.a library.

ROOTDIR = ..
include ${ROOTDIR}/mk/defs.mk

LIBRARY=	libzestrpc.a
SRCS=		zestConnection.c
SRCS+=		zestClientRpc.c
SRCS+=		zestEvents.c
SRCS+=		zestExport.c
SRCS+=		zestImport.c
SRCS+=		zestNiobuf.c
SRCS+=		zestPackGeneric.c
SRCS+=		zestRpcClient.c
SRCS+=		zestRpcNbReq.c
SRCS+=		zestService.c
SRCS+=		${ROOTDIR}/zestion/log.c
SRCS+=		${ROOTDIR}/zestion/zestWaitq.c
INCLUDES+=	-I${LNET_BASE}/include
INCLUDES+=	-I${ROOTDIR}/zestion
INCLUDES+=	-I${ROOTDIR}/intercept
LDFLAGS+=	${THREAD_LIBS} ${LNET_LIBS}

TARGET=		${LIBRARY}

include ${MAINMK}
