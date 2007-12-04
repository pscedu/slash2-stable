# $Id$

include Makefile.path

LIBRARY=	${LNET_BASE}/lib/liblnet.a
SRCS=		acceptor.c
SRCS+=		api-errno.c
SRCS+=		api-ni.c
SRCS+=		config.c
SRCS+=		lib-eq.c
SRCS+=		lib-md.c
SRCS+=		lib-me.c
SRCS+=		lib-move.c
SRCS+=		lib-msg.c
SRCS+=		lo.c
SRCS+=		peer.c
SRCS+=		router.c
SRCS+=		router_proc.c
INCLUDES+=	-I${LNET_BASE}/include

TARGET=		${LIBRARY}

include ${LLMK}
