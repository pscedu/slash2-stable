# $Id$

ROOTDIR=../../..
include ${ROOTDIR}/Makefile.path

PROG=		typedump
SRCS+=		typedump.c
INCLUDES+=	-I${OBJDIR}

# pthread is specified here to use correct struct sizes
MODULES+=	lnet-hdrs pthread pfl

include ${PFLMK}

typedump.c: ${OBJDIR}/typedump.h

${OBJDIR}/typedump.h: ${OBJDIR} ${PFL_BASE}/*.h
	${GENTYPES} -h '../../*.h' $(				\
	    ) -x '../../{fsmod,refmgr,queue,tree}.h' > $@
