# $Id$

ROOTDIR=.
include Makefile.path

fullbuild:
	(cd zest/utils && ${MAKE} build)
	(cd zest/tests && ${MAKE} build)
	(cd zest && ${MAKE} build)
	(cd zest/client/linux && ${MAKE} build)
	(cd psc_fsutil_libs/utils && ${MAKE} build)
	(cd psc_fsutil_libs/tests && ${MAKE} build)
	(cd slash_nara/utils && ${MAKE} build)
	(cd slash_nara/tests && ${MAKE} build)
	(cd slash_nara && ${MAKE} build)
	(cd fio && ${MAKE} clean && ${MAKE} pthreads)
	(cd apps/sft && ${MAKE} clean && ${MAKE})
