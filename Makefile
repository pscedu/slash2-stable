# $Id$

ROOTDIR=../..
include ${ROOTDIR}/Makefile.path

PROG=		sft
SRCS+=		sft.c
SRCS+=		${PFL_BASE}/psc_util/alloc.c
SRCS+=		${PFL_BASE}/psc_util/crc.c
SRCS+=		${PFL_BASE}/psc_util/log.c
SRCS+=		${PFL_BASE}/psc_util/strlcpy.c

ifdef MPI
MODULES+=	mpi
endif

ifdef QK
MODULES+=	mpi qk
endif

MODULES+=	pfl

include ${MAINMK}
