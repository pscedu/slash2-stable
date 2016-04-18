# $Id$

ROOTDIR=..
include ${ROOTDIR}/Makefile.path

PROG=		fio
SRCS+=		fio.c
SRCS+=		parse.y
SRCS+=		scan.l
SRCS+=		sym.c
SRCS+=		symtab.c

MODULES+=	m pfl barrier

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
