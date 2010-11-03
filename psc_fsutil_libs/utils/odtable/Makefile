# $Id$

ROOTDIR=../../..
include ${ROOTDIR}/Makefile.path

PROG=		odtable
FORCE_INST=	1
MAN+=		odtable.1
SRCS=		odtable_main.c
SRCS+=		${PFL_BASE}/psc_ds/dynarray.c
SRCS+=		${PFL_BASE}/psc_ds/lockedlist.c
SRCS+=		${PFL_BASE}/psc_ds/vbitmap.c
SRCS+=		${PFL_BASE}/psc_util/alloc.c
SRCS+=		${PFL_BASE}/psc_util/crc.c
SRCS+=		${PFL_BASE}/psc_util/init.c
SRCS+=		${PFL_BASE}/psc_util/log.c
SRCS+=		${PFL_BASE}/psc_util/odtable.c

include ${PFLMK}
