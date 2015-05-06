# $Id$

ROOTDIR=.
include ${ROOTDIR}/Makefile.path

SUBDIRS+=	$(foreach i,$(wildcard */Makefile),$(patsubst %/Makefile,%,$i))

include ${MAINMK}
-include local.mk

DISTCLEANFILES+=${PICKLEHOSTMK}
