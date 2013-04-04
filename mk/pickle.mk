# $Id$
# %PSC_START_COPYRIGHT%
# -----------------------------------------------------------------------------
# Copyright (c) 2010-2013, Pittsburgh Supercomputing Center (PSC).
#
# Permission to use, copy, and modify this software and its documentation
# without fee for personal use or non-commercial use within your organization
# is hereby granted, provided that the above copyright notice is preserved in
# all copies and that the copyright and this permission notice appear in
# supporting documentation.  Permission to redistribute this software to other
# organizations or individuals is not permitted without the written permission
# of the Pittsburgh Supercomputing Center.  PSC makes no representations about
# the suitability of this software for any purpose.  It is provided "as is"
# without express or implied warranty.
# -----------------------------------------------------------------------------
# %PSC_END_COPYRIGHT%

ifeq ($(filter $(realpath ${ROOTDIR})/compat/%,${CURDIR}),)

 PICKLEHOSTMK=${ROOTDIR}/mk/gen-localdefs-$(word 1,$(subst ., ,$(shell hostname)))-pickle.mk
 PICKLELOCALMK=${ROOTDIR}/mk/local.mk.pckl-$(word 1,$(subst ., ,$(shell hostname)))

 -include ${PICKLEHOSTMK}

 PICKLE_NEED_VERSION=					$(word 2,$$Rev$$)

 ifneq (${PICKLE_NEED_VERSION},${PICKLE_HAS_VERSION})
  $(shell ${PICKLEGEN} "${ROOTDIR}" "${PICKLE_NEED_VERSION}" "${MAKE}" "${PICKLEHOSTMK}" >&2)
  include ${PICKLEHOSTMK}
 endif

 -include ${PICKLELOCALMK}

 ifdef PICKLE_HAVE_POSIX_MEMALIGN
  DEFINES+=						-DHAVE_POSIX_MEMALIGN
 else
  SRCS+=						${PFL_BASE}/compat/posix_memalign.c
 endif

 ifdef PICKLE_HAVE_PTHREAD_MUTEX_TIMEDLOCK
  DEFINES+=						-DHAVE_PTHREAD_MUTEX_TIMEDLOCK
 endif

 ifdef PICKLE_HAVE_PTHREAD_RWLOCK_TIMEDRDLOCK
  DEFINES+=						-DHAVE_PTHREAD_RWLOCK_TIMEDRDLOCK
 endif

 ifdef PICKLE_HAVE_CLOCK_GETTIME
  DEFINES+=						-DHAVE_CLOCK_GETTIME
 else
  CLOCK_SRCS+=						${PFL_BASE}/compat/clock_gettime.c
 endif

 ifdef PICKLE_HAVE_PTHREAD_BARRIER
  DEFINES+=						-DHAVE_PTHREAD_BARRIER
 else
  BARRIER_SRCS+=					${PFL_BASE}/compat/pthread_barrier.c
  BARRIER_SRCS+=					${PFL_BASE}/pthrutil.c
 endif

 ifdef PICKLE_HAVE_STRLCPY
  DEFINES+=						-DHAVE_STRLCPY
 else
  STR_SRCS+=						${PFL_BASE}/compat/strlcpy.c
 endif

 ifdef PICKLE_HAVE_STRNLEN
  DEFINES+=						-DHAVE_STRNLEN
 else
  STR_SRCS+=						${PFL_BASE}/compat/strnlen.c
 endif

 ifdef PICKLE_HAVE_SYS_SIGABBREV
  DEFINES+=						-DHAVE_SYS_SIGABBREV
 endif

 ifdef PICKLE_HAVE_GETHOSTBYNAME
  DEFINES+=						-DHAVE_GETHOSTBYNAME
 endif

 ifdef PICKLE_HAVE_GETIFADDRS
  DEFINES+=						-DHAVE_GETIFADDRS
 endif

 ifdef PICKLE_HAVE_GETPEEREID
  DEFINES+=						-DHAVE_GETPEEREID
 endif

 ifdef PICKLE_HAVE_RTNETLINK
  DEFINES+=						-DHAVE_RTNETLINK
 endif

 ifdef PICKLE_HAVE_I386_CMPXCHG8B
  DEFINES+=						-DHAVE_I386_CMPXCHG8B
 endif

 ifdef PICKLE_HAVE_NET_IF_DL_H
  DEFINES+=						-DHAVE_NET_IF_DL_H
 endif

 ifdef PICKLE_HAVE_RTM_HDRLEN
  DEFINES+=						-DHAVE_RTM_HDRLEN
 endif

 ifdef PICKLE_HAVE_MACH_MACH_TYPES_H
  DEFINES+=						-DHAVE_MACH_MACH_TYPES_H
 endif

 ifdef PICKLE_HAVE_LIBKERN_OSBYTEORDER_H
  DEFINES+=						-DHAVE_LIBKERN_OSBYTEORDER_H
 endif

 ifdef PICKLE_HAVE_INOTIFY
  DEFINES+=						-DHAVE_INOTIFY
 endif

 ifdef PICKLE_HAVE_ATSYSCALLS
  DEFINES+=						-DHAVE_ATSYSCALLS
 endif

 ifdef PICKLE_HAVE_STB_TIM
  DEFINES+=						-DHAVE_STB_TIM
 endif

 ifdef PICKLE_HAVE_STB_TIMESPEC
  DEFINES+=						-DHAVE_STB_TIMESPEC
 endif

 ifdef PICKLE_HAVE_SETRESUID
  DEFINES+=						-DHAVE_SETRESUID
 else
  SETRESUID_SRC=					${PFL_BASE}/compat/setresuid.c
 endif

 ifdef PICKLE_HAVE_SYNC_FILE_RANGE
  DEFINES+=						-DHAVE_SYNC_FILE_RANGE
 endif

 ifdef PICKLE_HAVE_FUSE_DEBUGLEVEL
  DEFINES+=						-DHAVE_FUSE_DEBUGLEVEL
 endif

 ifdef PICKLE_HAVE_FUSE_REQ_GETGROUPS
  DEFINES+=						-DHAVE_FUSE_REQ_GETGROUPS
 endif

 ifdef PICKLE_HAVE_FUSE_REQ_GETCHANNEL
  DEFINES+=						-DHAVE_FUSE_REQ_GETCHANNEL
 endif

 ifdef PICKLE_HAVE_FUSE
  DEFINES+=						-DHAVE_FUSE
  PSCFS_SRCS+=						${PFL_BASE}/fuse.c
  PSCFS_SRCS+=						${PFL_BASE}/sys.c
 endif

 ifdef PICKLE_HAVE_DOKAN
  DEFINES+=						-DHAVE_DOKAN
  PSCFS_SRCS+=						${PFL_BASE}/dokan.c
 endif

 ifdef PICKLE_HAVE_SETPROCTITLE
  DEFINES+=						-DHAVE_SETPROCTITLE
 endif

 ifdef PICKLE_HAVE_SGIO
  DEFINES+=						-DHAVE_SGIO
 endif

 ifdef PICKLE_HAVE_FUTIMENS
  DEFINES+=						-DHAVE_FUTIMENS
 else
  FUTIMENS_SRCS+=					${PFL_BASE}/compat/futimens.c
 endif

 ifdef PICKLE_HAVE_ST_INO_64BIT
  DEFINES+=						-DHAVE_ST_INO_64BIT
 endif

 ifdef PICKLE_HAVE_TLS
  DEFINES+=						-DHAVE_TLS
 endif

 ifdef PICKLE_HAVE_YYERRLAB
   ifndef PICKLE_HAVE_YYERRLAB_ERROR
     ifdef PICKLE_HAVE_CFLAGS_WERROR
       PCPP_FLAGS+=					-H yyerrlab
     endif
   endif
 endif

 ifdef PICKLE_HAVE_YYLEX_RETURN
   ifndef PICKLE_HAVE_YYLEX_RETURN_ERROR
     ifdef PICKLE_HAVE_CFLAGS_WERROR
       PCPP_FLAGS+=					-H yylex_return
     endif
   endif
 endif

 ifdef PICKLE_HAVE_YYSYNTAX_ERROR
   ifndef PICKLE_HAVE_YYSYNTAX_ERROR_ERROR
     ifdef PICKLE_HAVE_CFLAGS_WERROR
       PCPP_FLAGS+=					-H yysyntax_error
     endif
   endif
 endif

 ifdef PICKLE_HAVE_LIBNUMA
  DEFINES+=						-DHAVE_NUMA
  NUMA_LIBS=						-lcpuset -lnuma
 endif

 ifdef PICKLE_HAVE_OPTRESET
  DEFINES+=						-DHAVE_OPTRESET
 endif

 ifdef PICKLE_HAVE_DKIOC
  DEFINES+=						-DHAVE_DKIOC
 endif

 ifdef PICKLE_HAVE_SRAND48_R
  DEFINES+=						-DHAVE_SRAND48_R
 else
  RND_SRCS+=						${PFL_BASE}/compat/srand48_r.c
 endif

 ifdef PICKLE_HAVE_AIO
  DEFINES+=						-DHAVE_AIO
 endif

 ifdef PICKLE_HAVE_GETMNTINFO
  DEFINES+=						-DHAVE_GETMNTINFO
 endif

 ifdef PICKLE_HAVE_GETXATTR
  DEFINES+=						-DHAVE_GETXATTR
 endif

 ifdef PICKLE_HAVE_STATFS_FSTYPE
  DEFINES+=						-DHAVE_STATFS_FSTYPE
 endif

 ifdef PICKLE_HAVE_GETPEERUCRED
  DEFINES+=						-DHAVE_GETPEERUCRED
 endif

 ifdef PICKLE_HAVE_FTS
  DEFINES+=						-DHAVE_FTS
 endif

 ifdef PICKLE_HAVE_RT_SYSCTL
  DEFINES+=						-DHAVE_RT_SYSCTL
 endif

 ifdef PICKLE_HAVE_SA_LEN
  DEFINES+=						-DHAVE_SA_LEN
 endif

 ifdef PICKLE_HAVE_SYS_CDEFS_H
  DEFINES+=						-DHAVE_SYS_CDEFS_H
 endif

endif
