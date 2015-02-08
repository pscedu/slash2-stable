# $Id$

ROOTDIR=../..
include ${ROOTDIR}/Makefile.path

PROG=		mount_wokfs
BIN=		mount_wokfs.sh
MAN+=		mount_wokfs.8

SRCS+=		main.c
SRCS+=		${PFL_BASE}/fuse.c

MODULES+=	pthread pscfs ctl clock pfl

include ${PFLMK}
