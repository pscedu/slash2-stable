# $Id$

include ../Makefile.path

PROG=		zrpcwait-sa
SRCS=		zrpcwait-sa.c
SRCS+=		${ZEST_BASE}/zestion/zestWaitq.c
SRCS+=		${ZEST_BASE}/zestion/log.c
INCLUDES+=	-I${ZEST_BASE}/zestrpc -I${LNET_BASE}/include

include ${ZESTMK}
