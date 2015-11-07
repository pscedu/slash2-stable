# $Id$

ROOTDIR=../../..
include ${ROOTDIR}/Makefile.path

PROG=		random_test
SRCS=		random_test.c
MODULES+=	pfl

include ${PFLMK}

graph: all
	./${PROG} | tail -n +3 > data
	gnuplot plot.gp
