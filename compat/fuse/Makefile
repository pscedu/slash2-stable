# $Id$

ROOTDIR=../..
include ${ROOTDIR}/Makefile.path

PROG=		fuse_compat
SRCS+=		fuse_compat.c
MODULES+=	fuse

include ${MAINMK}

prereq-hook:
	@if ! [ -f ${FUSE_BASE}/config.log ]; then			\
		cd ${FUSE_BASE};					\
		./configure;						\
	fi
