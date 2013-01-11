# $Id$

ROOTDIR=../../..
include ${ROOTDIR}/Makefile.path

PROG=		random_test
SRCS=		random_test.c
SRCS+=		${PFL_BASE}/alloc.c
SRCS+=		${PFL_BASE}/log.c
SRCS+=		${PFL_BASE}/random.c

include ${PFLMK}

graph: all
	./${PROG} | tail -n +3 > data
	gnuplot plot.gp
