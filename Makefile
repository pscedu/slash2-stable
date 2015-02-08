# $Id$

ROOTDIR=..
include ${ROOTDIR}/Makefile.path

SUBDIRS+=	benchfs
SUBDIRS+=	mount_wokfs
SUBDIRS+=	wokctl

include ${PFLMK}
