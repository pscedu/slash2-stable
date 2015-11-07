# $Id$

ROOTDIR=../../..
include ${ROOTDIR}/Makefile.path

PROG=		typedump
SRCS+=		typedump.c

# pthread is specified here to use correct struct sizes
MODULES+=	lnet-hdrs pthread pfl

include ${PFLMK}

regen-hook:
	${GENTYPES} -h '../../include/*/*.h' -h '../../../pfl/*.h' $(	\
	    ) -x '../../../pfl/{fsmod,refmgr,queue,tree}.h' $(		\
	    ) typedump.c
