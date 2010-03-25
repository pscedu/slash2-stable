# $Id$

ROOTDIR=../../..
include ${ROOTDIR}/Makefile.path

PROG=		random_test
SRCS=		random_test.c
SRCS+=		${PFL_BASE}/psc_util/alloc.c
SRCS+=		${PFL_BASE}/psc_util/log.c
SRCS+=		${PFL_BASE}/psc_util/random.c
SRCS+=		${PFL_BASE}/psc_util/strlcpy.c

include ${PFLMK}

graph: all
	./${PROG} | tail -n +3 > data
	gnuplot plot.gp
