# $Id$

ROOTDIR=../..
include ${ROOTDIR}/Makefile.path

PROG=		mount_wokfs
#BIN=		mount_wokfs.sh
MAN+=		mount_wokfs.8

SRCS+=		ctl.c
SRCS+=		main.c
SRCS+=		${PFL_BASE}/fuse.c

LDFLAGS+=	-rdynamic -Wl,--whole-archive
MODULES+=	pthread pscfs ctl clock pfl-whole acl dl

include ${PFLMK}
