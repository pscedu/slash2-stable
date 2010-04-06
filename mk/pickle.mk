# $Id$

ifeq ($(filter $(realpath ${ROOTDIR})/compat/%,${CURDIR}),)

PICKLELOCALMK=${ROOTDIR}/mk/gen-localdefs-pickle.mk

-include ${PICKLELOCALMK}

PICKLE_NEED_VERSION=					4

 ifneq (${PICKLE_NEED_VERSION},${PICKLE_HAS_VERSION})
  $(shell ${PICKLEGEN} "${ROOTDIR}" "${PICKLE_NEED_VERSION}" "${MAKE}" "${PICKLELOCALMK}" >&2)
  include ${PICKLELOCALMK}
 endif

 ifndef PICKLE_HAVE_POSIX_MEMALIGN
  SRCS+=						${ROOTDIR}/psc_fsutil_libs/compat/posix_memalign.c
 endif

 ifdef PICKLE_HAVE_PTHREAD_MUTEX_TIMEDLOCK
  DEFINES+=						-DHAVE_PTHREAD_MUTEX_TIMEDLOCK
 endif

 ifdef PICKLE_HAVE_CLOCK_GETTIME
  DEFINES+=						-DHAVE_CLOCK_GETTIME
 else
  SRCS+=						${ROOTDIR}/psc_fsutil_libs/compat/clock_gettime.c
 endif

 ifdef PICKLE_HAVE_PTHREAD_BARRIER
  DEFINES+=						-DHAVE_PTHREAD_BARRIER
 else
  ifneq ($(filter pthread,${MODULES}),)
   SRCS+=						${ROOTDIR}/psc_fsutil_libs/compat/pthread_barrier.c
   SRCS+=						${ROOTDIR}/psc_fsutil_libs/psc_util/pthrutil.c
  endif
 endif

 ifdef PICKLE_HAVE_STRLCPY
  DEFINES+=						-DHAVE_STRLCPY
 else
  SRCS+=						${ROOTDIR}/psc_fsutil_libs/compat/strlcpy.c
 endif

 ifdef PICKLE_HAVE_SYS_SIGABBREV
  DEFINES+=						-DHAVE_SYS_SIGABBREV
 endif

 ifdef PICKLE_HAVE_GETHOSTBYNAME
  DEFINES+=						-DHAVE_GETHOSTBYNAME
 endif

endif
