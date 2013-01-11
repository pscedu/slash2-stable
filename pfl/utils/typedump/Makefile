# $Id$

ROOTDIR=../../..
include ${ROOTDIR}/Makefile.path

PROG=		typedump
SRCS+=		typedump.c

# pthread is specified here to use correct struct sizes
MODULES+=	lnet-hdrs pthread pfl

include ${PFLMK}

regen-hook:
	${GENTYPES} -h '../../include/*/*.h' $(				\
	    ) -x '../../include/pfl/fsmod.h' $(				\
	    ) -x '../../include/psc_ds/{queue,tree}.h' $(		\
	    ) -x '../../include/psc_util/refmgr.h' typedump.c
