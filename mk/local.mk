CC=		qk-gcc
CFLAGS+=        -Wall -W -g
DEFINES+=       -D_LARGEFILE_SOURCE -D_FILE_OFFSET_BITS=64 -DYY_NO_UNPUT
INCLUDES+=	-I/opt/xt-mpt/default/mpich2-64/P2/include/


MPI_LIBS?=   -L/opt/xt-mpt/default/mpich2-64/P2/lib/ -lmpich
