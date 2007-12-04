# $Id$

PROOTDIR=.
include Makefile.path

LIBRARY=	libpfl.a
SRCS+=		psc_ds/dynarray.c
SRCS+=		psc_ds/hash.c
SRCS+=		psc_ds/vbitmap.c
SRCS+=		psc_rpc/connection.c
SRCS+=		psc_rpc/events.c
SRCS+=		psc_rpc/export.c
SRCS+=		psc_rpc/import.c
SRCS+=		psc_rpc/nb.c
SRCS+=		psc_rpc/niobuf.c
SRCS+=		psc_rpc/packgeneric.c
SRCS+=		psc_rpc/rpcclient.c
SRCS+=		psc_rpc/service.c
SRCS+=		psc_util/crc.c
SRCS+=		psc_util/log.c
SRCS+=		psc_util/myrandom.c
SRCS+=		psc_util/palloc.c
SRCS+=		psc_util/pthread_barrier.c
SRCS+=		psc_util/subsys.c
SRCS+=		psc_util/thread.c
SRCS+=		psc_util/threadtable.c
SRCS+=		psc_util/waitq.c
INCLUDES+=	-I${LNET_BASE}/include
TARGET=		${LIBRARY}

DEFINES+=	-DHAVE_GETHOSTBYNAME -DHAVE_GETHOSTBYNAME_DEFINED
DEFINES+=	-DHAVE_LIBPTHREAD -DHAVE_LIBPTHREAD_DEFINED

include	${PFLMK}
