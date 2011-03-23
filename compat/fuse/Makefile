# $Id$

ROOTDIR=../..
include ${ROOTDIR}/Makefile.path

PROG=		fuse_compat
SRCS+=		fuse_compat.c
MODULES+=	fuse
TARGET=		build-fuse ${PROG}

include ${MAINMK}

build-fuse:
	@if ! [ -f ${FUSE_BASE}/config.log ]; then			\
		cd ${FUSE_BASE};					\
		./configure >&2;					\
	fi
