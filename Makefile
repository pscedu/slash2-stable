# $Id$

ROOTDIR=.
include ${ROOTDIR}/Makefile.path

SUBDIRS+=	${PFL_BASE}
SUBDIRS+=	tools
SUBDIRS+=	inf
SUBDIRS+=	mfio
SUBDIRS+=	psync
SUBDIRS+=	${SLASH_BASE}
SUBDIRS+=	sft
SUBDIRS+=	src-upd
#SUBDIRS+=	${ZEST_BASE}

include ${MAINMK}
-include local.mk

DISTCLEANFILES+=${PICKLEHOSTMK}
