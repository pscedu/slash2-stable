# $Id$

#CFLAGS = -O2 -Wall -D_LARGEFILE_SOURCE -D_FILE_OFFSET_BITS=64  -D_XOPEN_SOURCE -D_REENTRANT -D_THREAD_SAFE -DOS64 -DNEED_YYLVAL
#CFLAGS = -O2 -Wall -D_LARGEFILE_SOURCE -D_FILE_OFFSET_BITS=64
CFLAGS = -O2 -Wall
# CFLAGS += -W
LINUXFLAGS = -D_LARGEFILE_SOURCE -D_FILE_OFFSET_BITS=64 -DYY_NO_UNPUT
LIBS = -lm

OBJS = fio.o fio_sym.o lex.yy.o fio_config_parser.tab.o
CC   = gcc


lex_yacc:
	lex fio_config_parser.lex
	bison -d fio_config_parser.y

base:   lex_yacc $(OBJS)

qk:	INC = /opt/xt-mpt/default/mpich2-64/P2/include/
qk:     LIB = /opt/xt-mpt/default/mpich2-64/P2/lib/
qk:     CC = qk-gcc
qk:     CFLAGS += -DQK -DMPI $(LINUXFLAGS) -I $(INC)
qk:     LIBS   += -lmpich
#qk:     LIBS   += -lmpich -liobuf
qk:	base
	$(CC) $(CFLAGS) -o fio.qk $(OBJS) -L $(LIB) -L .  $(LIBS)


pthreads: OBJS   += fio_pthread_barrier.o
pthreads: CFLAGS += -DPTHREADS $(LINUXFLAGS)
pthreads: LIBS   += -lpthread
pthreads: base fio_pthread_barrier.o
	$(CC) $(CFLAGS) -o fio.pthreads $(OBJS) $(LIBS)

mpi:
	flex fio_config_parser.lex
	bison -d fio_config_parser.y
	mpicc $(CFLAGS) -DMPI -c -o fio_config_parser.lex.o lex.yy.c
	mpicc $(CFLAGS) -DMPI -c -o fio_config_parser.y.o fio_config_parser.tab.c
	mpicc $(CFLAGS) -DMPI -c -o ./fio_sym.o ./fio_sym.c
	mpicc $(CFLAGS) -DMPI -c -o ./fio.o fio.c
	mpicc $(CFLAGS) -DMPI -o fio.mpi fio.o fio_sym.o fio_config_parser.lex.o fio_config_parser.y.o  -ll -lm

debian_mpi:
	flex fio_config_parser.lex
	bison -d fio_config_parser.y
	mpicc.mpich $(CFLAGS) -DMPI -c -o fio_config_parser.lex.o lex.yy.c
	mpicc.mpich $(CFLAGS) -DMPI -c -o fio_config_parser.y.o fio_config_parser.tab.c
	mpicc.mpich $(CFLAGS) -DMPI -c -o ./fio_sym.o ./fio_sym.c
	mpicc.mpich $(CFLAGS) -DMPI -c -o ./fio.o fio.c
	mpicc.mpich $(CFLAGS) -DMPI -o fio.mpi fio.o fio_sym.o fio_config_parser.lex.o fio_config_parser.y.o  -ll -lm

clean_all:
	rm -f *.o fio.pthreads fio.qk fio.mpi

clean:
	rm -f *.o


.c.o:
	$(CC) $(CFLAGS) $(LINUXFLAGS) -c -o $@ $<
