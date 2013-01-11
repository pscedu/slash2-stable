# $Id$

ROOTDIR=../..
include ${ROOTDIR}/Makefile.path

SRC_PATH+=	.

PROG=		sft
SRCS+=		sft.c
SRCS+=		${PFL_BASE}/alloc.c
SRCS+=		${PFL_BASE}/crc.c
SRCS+=		${PFL_BASE}/init.c
SRCS+=		${PFL_BASE}/log.c

ifdef MPI
MODULES+=	mpi
endif

ifdef QK
MODULES+=	mpi qk
endif

MODULES+=	pfl str

include ${MAINMK}
