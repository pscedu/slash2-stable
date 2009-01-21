# $Id$

ROOTDIR=../..
PROJECT_BASE=.
include ${ROOTDIR}/Makefile.path

PROG?=		sft.qk
SRCS+=		sft.c
SRCS+=		${PFL_BASE}/psc_util/crc.c

CFLAGS+=	-Wall -W -g
CFLAGS+=	-D_LARGEFILE_SOURCE -D_FILE_OFFSET_BITS=64
CFLAGS+=	-I${PFL_BASE}/include

sft.mpi:	CC=		mpicc
sft.mpi:	CFLAGS+=	-DMPI
sft.mpi:	LDFLAGS+=	-lmpi

sft.qk:		CC=		qk-gcc
sft.qk:		CFLAGS+=	-I/opt/xt-mpt/default/mpich2-64/P2/include
sft.qk:		CFLAGS+=	-DMPI
sft.qk:		LDFLAGS+=	-L/opt/xt-mpt/default/mpich2-64/P2/lib -lmpich

include ${MAINMK}

sft:
	@PROG=sft ${MAKE}

mpi qk:
	@PROG=sft.$@ ${MAKE}
