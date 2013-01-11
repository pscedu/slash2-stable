# $Id$

ROOTDIR=../../..
include ${ROOTDIR}/Makefile.path

TEST=		multiwait_test
SRCS+=		multiwait_test.c
SRCS+=		${PFL_BASE}/psc_ds/dynarray.c
SRCS+=		${PFL_BASE}/psc_ds/vbitmap.c
SRCS+=		${PFL_BASE}/psc_util/alloc.c
SRCS+=		${PFL_BASE}/psc_util/init.c
SRCS+=		${PFL_BASE}/psc_util/log.c
SRCS+=		${PFL_BASE}/psc_util/multiwait.c
SRCS+=		${PFL_BASE}/psc_util/pthrutil.c

MODULES+=	pthread

include ${PFLMK}
