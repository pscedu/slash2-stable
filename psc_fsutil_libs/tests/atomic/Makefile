# $Id$

ROOTDIR=../../..
include ${ROOTDIR}/Makefile.path

PROG=		atomic_test
SRCS+=		atomic_test.c
SRCS+=		${PFL_BASE}/psc_util/alloc.c
SRCS+=		${PFL_BASE}/psc_util/init.c
SRCS+=		${PFL_BASE}/psc_util/log.c
SRCS+=		${PFL_BASE}/psc_util/pthrutil.c
SRCS+=		${PFL_BASE}/psc_util/strlcpy.c

MODULES+=	pthread pfl

include ${PFLMK}
