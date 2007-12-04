# $Id: $

ROOTDIR = ..
include ${ROOTDIR}/mk/defs.mk

LIBRARY=	libpsc_rpc.a
SRCS=	connection.c
SRCS+=	events.c
SRCS+=	export.c
SRCS+=	import.c
SRCS+=	nb.c
SRCS+=	niobuf.c
SRCS+=	packgeneric.c
SRCS+=	rpcclient.c
SRCS+=	service.c

INCLUDES+=	-I${LNET_BASE}/include
DEFINES+=	-DHAVE_GETHOSTBYNAME_DEFINED -DHAVE_GETHOSTBYNAME -DHAVE_LIBPTHREAD_DEFINED

include	${MAINMK}

CFLAGS+= -O2
