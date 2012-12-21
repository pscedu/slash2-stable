# $Id$

ROOTDIR=.
include ${ROOTDIR}/Makefile.path

SUBDIRS+=	${PFL_BASE}
SUBDIRS+=	tools
SUBDIRS+=	apps
SUBDIRS+=	mfio
SUBDIRS+=	${_ZFS_BASE}
SUBDIRS+=	${_SLASH_BASE}

_SLASH_BASE=	${SLASH_BASE}

ifneq ($(wildcard ${ZEST_BASE}),)
_ZEST_BASE=	${ZEST_BASE}
endif

include ${MAINMK}
-include local.mk
