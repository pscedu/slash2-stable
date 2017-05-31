# $Id$
#
# If you have problem to build, try remove the old pickle file under mk
# (e.g., mk/gen-localdefs-polybius-pickle.mk)
#

ROOTDIR=.
include ${ROOTDIR}/Makefile.path

SUBDIRS+=	$(filter-out zfs-fuse compat,$(foreach i,$(wildcard */Makefile inf/*/Makefile),$(patsubst %/Makefile,%,$i)))

include ${MAINMK}
-include local.mk

DISTCLEANFILES+=${PICKLEHOSTMK}
