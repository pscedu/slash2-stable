# $Id$

ROOTDIR=..
include ${ROOTDIR}/Makefile.path

PROG=		sft
MAN+=		sft.1
SRCS+=		sft.c

ifdef MPI
MODULES+=	mpi
endif

ifdef QK
MODULES+=	mpi qk
endif

MODULES+=	pfl str pthread gcrypt

include ${MAINMK}
