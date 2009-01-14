# $Id: Makefile 4897 2009-01-14 17:14:28Z yanovich $

include ../Makefile.path

PROG=		typedump
SRCS=		typedump.c
SRCS+=		${PFL_BASE}/psc_util/log.c
SRCS+=		${PFL_BASE}/psc_util/alloc.c
INCLUDES+=	-I${LNET_BASE}/include
DEFINES+=	-DHAVE_LIBPTHREAD -DHAVE_LIBPTHREAD_DEFINED \
		-DHAVE_GETHOSTBYNAME -DHAVE_GETHOSTBYNAME_DEFINED

include ${PFLMK}
