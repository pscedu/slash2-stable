# $Id$

ROOTDIR=..
include ${ROOTDIR}/Makefile.path

SRC_PATH+=	.

PROG=		fio
SRCS+=		fio.c
SRCS+=		fio_config_lex.l
SRCS+=		fio_config_parser.y
SRCS+=		fio_sym.c
SRCS+=		fio_symtable.c
SRCS+=		${PFL_BASE}/psc_util/alloc.c
SRCS+=		${PFL_BASE}/psc_util/log.c
SRCS+=		${PFL_BASE}/psc_util/pthrutil.c

DEBUG?=		0
LDFLAGS=	-lm
MODULES+=	pfl

ifdef QK
MODULES+=	qk
endif

ifdef ZCC
MODULES+=	zcc
endif

ifdef MPI
MODULES+=	mpi
else
MODULES+=	pthread
endif

include ${MAINMK}
