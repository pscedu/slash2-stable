SFT_BASE=.
PROJECT_BASE=${SFT_BASE}
include Makefile.path

PROG=		sft
SRCS+=		${PFL_BASE}/psc_util/crc.c
SRCS+=		sft.c
LDFLAGS+=	${MPI_LIBS}

include ${SFTMK}
