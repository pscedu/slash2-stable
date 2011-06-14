# $Id$

ROOTDIR=../../..
include ${ROOTDIR}/Makefile.path

PROG=		lnrtd
SRCS=		lnrtd.c
SRCS+=		${PFL_BASE}/psc_ds/vbitmap.c
SRCS+=		${PFL_BASE}/psc_util/pthrutil.c
SRCS+=		${PFL_BASE}/psc_util/thread.c
SRCS+=		${PFL_BASE}/psc_util/usklndthr.c
MODULES+=	pthread pfl lnet

include ${PFLMK}
