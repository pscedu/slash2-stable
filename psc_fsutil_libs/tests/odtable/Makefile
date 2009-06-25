# $Id: Makefile 5779 2009-04-07 19:07:58Z yanovich $

include ../Makefile.path

PROG=		odtable
SRCS=		odtable_test.c
SRCS+=		${PFL_BASE}/psc_ds/vbitmap.c
SRCS+=		${PFL_BASE}/psc_ds/dynarray.c
SRCS+=		${PFL_BASE}/psc_util/alloc.c
SRCS+=		${PFL_BASE}/psc_util/log.c
SRCS+=		${PFL_BASE}/psc_util/odtable.c
SRCS+=		${PFL_BASE}/psc_util/crc.c
SRCS+=		${PFL_BASE}/psc_util/init.c
DEFINES+=	-DHAVE_LIBPTHREAD
LDFLAGS+=	${THREAD_LIBS} -lrt ${NUMA_LIBS}

include ${PFLMK}
