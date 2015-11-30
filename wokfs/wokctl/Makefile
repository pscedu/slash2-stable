# $Id$

ROOTDIR=../..
include ${ROOTDIR}/Makefile.path

PROG=		wokctl
INCLUDES+=	-I${ROOTDIR}/wokfs
MAN+=		wokctl.8
SRCS+=		wokctl.c
SRCS+=		${PFL_BASE}/ctlcli.c
MODULES+=	pthread pscfs-hdrs curses clock pfl lnet-hdrs

include ${PFLMK}
