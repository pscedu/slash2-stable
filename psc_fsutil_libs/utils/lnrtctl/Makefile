# $Id$

ROOTDIR=../../..
include ${ROOTDIR}/Makefile.path

PROG=		lnrtctl
FORCE_INST=	1
MAN+=		lnrtctl.8
SRCS+=		lnrtctl.c
SRCS+=		${PFL_BASE}/psc_ds/dynarray.c
SRCS+=		${PFL_BASE}/psc_util/alloc.c
SRCS+=		${PFL_BASE}/psc_util/ctlcli.c
SRCS+=		${PFL_BASE}/psc_util/fmt.c
SRCS+=		${PFL_BASE}/psc_util/init.c
SRCS+=		${PFL_BASE}/psc_util/log.c
SRCS+=		${PFL_BASE}/psc_util/pthrutil.c
SRCS+=		${PFL_BASE}/psc_util/subsys.c
SRCS+=		${PFL_BASE}/psc_util/thread.c
SRCS+=		${PFL_BASE}/psc_util/waitq.c

MODULES+=	lnet-hdrs pthread

include ${SLASHMK}
