# $Id$

CFLAGS+=	-Wall -W -g
#CFLAGS+=	-Wshadow -Wunused -Wuninitialized -O
DEFINES+=	-D_LARGEFILE_SOURCE -D_FILE_OFFSET_BITS=64
INCLUDES+=	-I${LKERNEL_BASE}/include
INCLUDES+=	-I${KERNEL_BASE}/include

THREAD_LIBS?=	-lpthread
