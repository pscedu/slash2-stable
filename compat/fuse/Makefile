# $Id$

ROOTDIR=../..
include ${ROOTDIR}/Makefile.path

PROG=		fuse_compat
SRCS+=		fuse_compat.c
MODULES+=	fuse

include ${MAINMK}

fuse_compat.c: ${FUSE_BASE}/config.log

${FUSE_BASE}/config.log:
	cd ${FUSE_BASE} && ./configure >&2
