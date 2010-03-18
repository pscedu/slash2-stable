# $Id$

ROOTDIR=..
include ${ROOTDIR}/Makefile.path

PROG=		fio
SRCS+=		fio.c
SRCS+=		fio_config_lex.l
SRCS+=		fio_config_parser.y
SRCS+=		fio_pthread_barrier.c
SRCS+=		fio_sym.c
SRCS+=		fio_symtable.c

DEBUG?=		0
LDFLAGS=	-lm
MODULES+=	pfl

ifdef QK
MODULES+=	mpi qk
SKIPTHR=	1
endif

ifdef ZCC
MODULES+=	zcc
endif

ifdef MPI
MODULES+=	mpi
SKIPTHR=	1
endif

ifneq (${SKIPTHR},1)
MODULES+=	pthread
endif

include ${MAINMK}
