# $Id$

ROOTDIR=.
include ${ROOTDIR}/Makefile.path

SUBDIRS+=	$(filter-out zfs-fuse compat,$(foreach i,$(wildcard */Makefile inf/*/Makefile),$(patsubst %/Makefile,%,$i)))

include ${MAINMK}
-include local.mk

DISTCLEANFILES+=${PICKLEHOSTMK}
