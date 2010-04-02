# $Id$

ifeq ($(filter $(realpath ${ROOTDIR})/compat/%,${CURDIR}),)

PICKLELOCALMK=${ROOTDIR}/mk/gen-localdefs-pickle.mk

-include ${PICKLELOCALMK}

PICKLE_NEED_VERSION=					2

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

ifndef PICKLE_HAVE_HOST_NAME_MAX
  DEFINES+=						-DHOST_NAME_MAX=MAXHOSTNAMELEN
endif

ifneq ($(filter pthread,${MODULES}),)
 ifndef PICKLE_HAVE_PTHREAD_BARRIER
  SRCS+=						${ROOTDIR}/psc_fsutil_libs/compat/pthread_barrier.c
 else
  DEFINES+=						-DHAVE_PTHREAD_BARRIER
 endif
endif

endif
