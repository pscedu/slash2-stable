# $Id$

ROOTDIR=../../..
include ${ROOTDIR}/Makefile.path

PROG=		lnrtctl
FORCE_INST=	1
MAN+=		lnrtctl.8
SRCS+=		lnrtctl.c
SRCS+=		${PFL_BASE}/dynarray.c
SRCS+=		${PFL_BASE}/alloc.c
SRCS+=		${PFL_BASE}/ctlcli.c
SRCS+=		${PFL_BASE}/fmt.c
SRCS+=		${PFL_BASE}/init.c
SRCS+=		${PFL_BASE}/log.c
SRCS+=		${PFL_BASE}/pthrutil.c
SRCS+=		${PFL_BASE}/subsys.c
SRCS+=		${PFL_BASE}/thread.c
SRCS+=		${PFL_BASE}/waitq.c

MODULES+=	lnet-hdrs pthread

include ${SLASHMK}
