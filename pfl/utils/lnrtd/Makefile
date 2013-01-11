# $Id$

ROOTDIR=../../..
include ${ROOTDIR}/Makefile.path

PROG=		lnrtd
FORCE_INST=	1
BIN=		lnrtd.sh
SRCS+=		ctl.c
SRCS+=		lnrtd.c
MAN=		lnrtd.8
SRCS+=		${PFL_BASE}/listcache.c
SRCS+=		${PFL_BASE}/hashtbl.c
SRCS+=		${PFL_BASE}/stree.c
SRCS+=		${PFL_BASE}/vbitmap.c
SRCS+=		${PFL_BASE}/ctlsvr.c
SRCS+=		${PFL_BASE}/iostats.c
SRCS+=		${PFL_BASE}/pool.c
SRCS+=		${PFL_BASE}/pthrutil.c
SRCS+=		${PFL_BASE}/random.c
SRCS+=		${PFL_BASE}/rlimit.c
SRCS+=		${PFL_BASE}/thread.c
SRCS+=		${PFL_BASE}/usklndthr.c
MODULES+=	pthread pfl lnet ctl

include ${PFLMK}
