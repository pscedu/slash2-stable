# $Id$

ROOTDIR=..
include ${ROOTDIR}/Makefile.path

PROG=		sft
MAN+=		sft.1
SRCS+=		sft.c
SRCS+=		${PFL_BASE}/alloc.c
SRCS+=		${PFL_BASE}/crc.c
SRCS+=		${PFL_BASE}/dynarray.c
SRCS+=		${PFL_BASE}/fmt.c
SRCS+=		${PFL_BASE}/init.c
SRCS+=		${PFL_BASE}/iostats.c
SRCS+=		${PFL_BASE}/listcache.c
SRCS+=		${PFL_BASE}/log.c
SRCS+=		${PFL_BASE}/pool.c
SRCS+=		${PFL_BASE}/pthrutil.c
SRCS+=		${PFL_BASE}/timerthr.c

ifdef MPI
MODULES+=	mpi
endif

ifdef QK
MODULES+=	mpi qk
endif

MODULES+=	pfl str pthread gcrypt

include ${MAINMK}
