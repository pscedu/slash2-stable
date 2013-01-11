# $Id$

ROOTDIR=../../..
include ${ROOTDIR}/Makefile.path

TEST=		waitq_test
SRCS+=		waitq_test.c
SRCS+=		${PFL_BASE}/dynarray.c
SRCS+=		${PFL_BASE}/vbitmap.c
SRCS+=		${PFL_BASE}/alloc.c
SRCS+=		${PFL_BASE}/init.c
SRCS+=		${PFL_BASE}/log.c
SRCS+=		${PFL_BASE}/pthrutil.c
SRCS+=		${PFL_BASE}/subsys.c
SRCS+=		${PFL_BASE}/thread.c
SRCS+=		${PFL_BASE}/waitq.c

MODULES+=	pthread

include ${PFLMK}
