# $Id$

ROOTDIR=../../..
include ${ROOTDIR}/Makefile.path

PROG=		lnrtd
FORCE_INST=	1
BIN=		lnrtd.sh
MAN=		lnrtd.8
SRCS+=		ctl.c
SRCS+=		lnrtd.c
SRCS+=		${PFL_BASE}/usklndthr.c
MODULES+=	pthread pfl lnet ctl

include ${PFLMK}
