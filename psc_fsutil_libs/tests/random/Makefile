# $Id$

include ../Makefile.path

PROG=		random
SRCS=		random.c
SRCS+=		${PFL_BASE}/psc_util/random.c
SRCS+=		${PFL_BASE}/psc_util/log.c

include ${PFLMK}

graph: all
	./${PROG} | tail -n +3 > data
	gnuplot plot.gp
