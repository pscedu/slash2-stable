# $Id$

ROOTDIR=../../..
include ${ROOTDIR}/Makefile.path

PROG=		sock
SRCS=		sock.c
SRCS+=		${PFL_BASE}/dynarray.c
SRCS+=		${PFL_BASE}/vbitmap.c
SRCS+=		${PFL_BASE}/alloc.c
SRCS+=		${PFL_BASE}/fmt.c
SRCS+=		${PFL_BASE}/init.c
SRCS+=		${PFL_BASE}/iostats.c
SRCS+=		${PFL_BASE}/log.c
SRCS+=		${PFL_BASE}/netutil.c
SRCS+=		${PFL_BASE}/pthrutil.c
SRCS+=		${PFL_BASE}/subsys.c
SRCS+=		${PFL_BASE}/thread.c
SRCS+=		${PFL_BASE}/timerthr.c
SRCS+=		${PFL_BASE}/waitq.c

MODULES+=	pthread pfl

include ${PFLMK}
