# $Id$

CFLAGS+=	-Wall -W -g
#CFLAGS+=	-Wshadow -Wunused -Wuninitialized -O
DEFINES+=	-D_LARGEFILE_SOURCE -D_FILE_OFFSET_BITS=64
INCLUDES+=	-I${ROOTDIR}/zest/trunk/include/kernel/2.6.9-42.0.8.EL_lustre.1.4.9.1/include/
INCLUDES+=	-I${KERNEL_BASE}/include

THREAD_LIBS?=	-lpthread

ifneq ($(wildcard /opt/sgi),)
# On altix
SRCS+=		${ROOTDIR}/zest/trunk/compat/ia64.c
DEFINES+=	-DCONFIG_NR_CPUS=2 -DLINUX -D_GNU_SOURCE -DHAVE_CPUSET
CPUSET_LIBS=	-lcpuset -lbitmask -lnuma
else
# regular linux
DEFINES+=	-DLINUX
endif
