# $Id$

ROOTDIR=..
include ${ROOTDIR}/Makefile.path

SRCS+=		fio.c
SRCS+=		fio_config_lex.l
SRCS+=		fio_config_parser.y
SRCS+=		fio_pthread_barrier.c
SRCS+=		fio_sym.c
SRCS+=		fio_symtable.c

ifdef DEBUG
CFLAGS+=	-g
else
CFLAGS+=	-O2
endif

CFLAGS+=	-Wall -W
CFLAGS+=	-I${PFL_BASE}/include
LINUXFLAGS=	-D_LARGEFILE_SOURCE -D_FILE_OFFSET_BITS=64 -DYY_NO_UNPUT
LDFLAGS=	-lm
YFLAGS=		-d -o $@

OBJS+=		$(patsubst %.c,%.o,$(filter %.c,${SRCS}))
OBJS+=		$(patsubst %.y,%.o,$(filter %.y,${SRCS}))
OBJS+=		$(patsubst %.l,%.o,$(filter %.l,${SRCS}))

_YACCINTM=	$(patsubst %.y,%.c,$(filter %.y,${SRCS}))
_LEXINTM=	$(patsubst %.l,%.c,$(filter %.l,${SRCS}))

all:
	@echo "no target specified, pick one of:"
	@echo "  mpi zest zmpi debian_mpi pthreads qk"
	@exit 1

qk:		CC=		qk-gcc
qk:		CFLAGS+=	-DQK -DMPI ${LINUXFLAGS}
qk:		CFLAGS+=	-I/opt/xt-mpt/default/mpich2-64/P2/include/
qk:		LDFLAGS+=	-lmpich -L/opt/xt-mpt/default/mpich2-64/P2/lib

zest:		CC=		ZINCPATH=../zest/intercept/include ZLIBPATH=../zest/client/linux-mt ../zest/scripts/zcc
zest:		CFLAGS+=	-DHAVE_LIBPTHREAD ${LINUXFLAGS}
zest:		LDFLAGS+=	-lpthread

pthreads:	CFLAGS+=	-DHAVE_LIBPTHREAD ${LINUXFLAGS}
pthreads:	LDFLAGS+=	-lpthread

mpi:		CC=		mpicc
mpi:		CFLAGS+=	-DMPI

zmpi:		CC=		ZINCPATH=../zest/intercept/include ZLIBPATH=../zest/client/linux-mt ../zest/scripts/zcc
zmpi:		CFLAGS+=	-DMPI ${LINUXFLAGS}
zmpi:		LDFLAGS+=	-lmpi

debian_mpi:	CC=		mpicc.mpich
debian_mpi:	CFLAGS+=	-DMPI

debian_mpi mpi zmpi pthreads zest qk: ${OBJS}
	${CC} -o fio.$@ ${OBJS} ${LDFLAGS}

.c.o:
	${CC} ${CFLAGS} -c -o $@ $<

.y.c:
	${YACC} ${YFLAGS} $<

.PRECIOUS: %.c

clean_all clean:
	rm -f ${OBJS} ${_YACCINTM} ${_LEXINTM} \
	    fio.debian_mpi fio.pthreads fio.zest fio.qk fio.mpi fio.zmpi
