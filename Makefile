# $Id$

SRCS+=		fio.c
SRCS+=		fio_config_lex.l
SRCS+=		fio_config_parser.y
SRCS+=		fio_pthread_barrier.c
SRCS+=		fio_sym.c

CFLAGS=		-g -Wall -W
LINUXFLAGS=	-D_LARGEFILE_SOURCE -D_FILE_OFFSET_BITS=64 -DYY_NO_UNPUT
LDFLAGS=	-lm
YFLAGS=		-d -o $@ -t

OBJS+=		$(patsubst %.c,%.o,$(filter %.c,${SRCS}))
OBJS+=		$(patsubst %.y,%.o,$(filter %.y,${SRCS}))
OBJS+=		$(patsubst %.l,%.o,$(filter %.l,${SRCS}))

_YACCINTM=	$(patsubst %.y,%.c,$(filter %.y,${SRCS}))
_LEXINTM=	$(patsubst %.l,%.c,$(filter %.l,${SRCS}))

all:
	@echo "no target specified, pick one of:"
	@echo "  mpi zest debian_mpi pthreads qk"
	@exit 1

qk:		CC=		qk-gcc
qk:		CFLAGS+=	-DQK -DMPI ${LINUXFLAGS}
qk:		CFLAGS+=	-I/opt/xt-mpt/default/mpich2-64/P2/include/
qk:		LDFLAGS+=	-lmpich -L/opt/xt-mpt/default/mpich2-64/P2/lib

zest:		CC=		ZINCPATH=../zest/trunk/intercept/include ZLIBPATH=../zest/trunk/client/linux ../zest/trunk/scripts/zcc
zest:		CFLAGS+=	-DPTHREADS ${LINUXFLAGS}
zest:		LDFLAGS+=	-lpthread

pthreads:	CFLAGS+=	-DPTHREADS ${LINUXFLAGS}
pthreads:	LDFLAGS+=	-lpthread

mpi:		CC=		mpicc
mpi:		CFLAGS+=	-DMPI

debian_mpi:	CC=		mpicc.mpich
debian_mpi:	CFLAGS+=	-DMPI

debian_mpi mpi pthreads zest qk: ${OBJS}
	${CC} -o fio.$@ ${OBJS} ${LDFLAGS}

.c.o:
	${CC} ${CFLAGS} ${LINUXFLAGS} -c -o $@ $<

.y.c:
	${YACC} ${YFLAGS} $<

.PRECIOUS: %.c

clean_all clean:
	rm -f ${OBJS} ${_YACCINTM} ${_LEXINTM} \
	    fio.debian_mpi fio.pthreads fio.zest fio.qk fio.mpi
