# $Id$

ROOTDIR=../..
include ${ROOTDIR}/Makefile.path

SRC_PATH+=	.

PROG=		sft
SRCS+=		sft.c
SRCS+=		${PFL_BASE}/psc_util/alloc.c
SRCS+=		${PFL_BASE}/psc_util/crc.c
SRCS+=		${PFL_BASE}/psc_util/log.c

ifdef MPI
MODULES+=	mpi
endif

ifdef QK
MODULES+=	mpi qk
endif

MODULES+=	pfl

include ${MAINMK}
