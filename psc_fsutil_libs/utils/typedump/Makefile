# $Id$

include ../Makefile.path

PROG=		typedump
SRCS=		typedump.c
SRCS+=		${PFL_BASE}/psc_util/log.c
SRCS+=		${PFL_BASE}/psc_util/alloc.c
INCLUDES+=	-I${LNET_BASE}/include
DEFINES+=	-DHAVE_LIBPTHREAD -DHAVE_LIBPTHREAD_DEFINED \
		-DHAVE_GETHOSTBYNAME -DHAVE_GETHOSTBYNAME_DEFINED

include ${PFLMK}

gentypes:
	@${GENTYPES} -h '../../include/*/*.h' -x '../../include/psc_ds/{queue,tree}.h' typedump.c
