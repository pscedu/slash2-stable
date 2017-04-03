# $Id$
# %ISC_START_LICENSE%
# ---------------------------------------------------------------------
# Copyright 2015-2016, Google, Inc.
# Copyright 2010-2015, Pittsburgh Supercomputing Center
# All rights reserved.
#
# Permission to use, copy, modify, and distribute this software for any
# purpose with or without fee is hereby granted, provided that the
# above copyright notice and this permission notice appear in all
# copies.
#
# THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL
# WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS.  IN NO EVENT SHALL THE
# AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
# DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
# PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
# TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
# PERFORMANCE OF THIS SOFTWARE.
# --------------------------------------------------------------------
# %END_LICENSE%

# pickle: PFL infrastruture for configuration ... environment
#
# This file contains routines to run the pickle probe tests (via
# tools/picklegen) and glues knobs to detected system features.

# do not recursively depend on pickle when running the pickle probes!
ifeq ($(filter $(realpath ${ROOTDIR})/compat/%,${CURDIR}),)

 PICKLEHOSTMK=${ROOTDIR}/mk/gen-localdefs-$(word 1,$(subst ., ,$(shell hostname)))-pickle.mk

 -include ${PICKLEHOSTMK}

 ifeq ($(shell test "${ROOTDIR}/mk/pickle.mk" -nt "${PICKLEHOSTMK}" && echo 1),1)
  PICKLE_DONE=0
 endif

 ifneq (${PICKLE_DONE},1)
  $(shell ${PICKLEGEN} "${ROOTDIR}" "${PICKLE_NEED_VERSION}" "${MAKE}" "${PICKLEHOSTMK}" >&2)
  include ${PICKLEHOSTMK}
 endif

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

 ifdef PICKLE_HAVE_RT_SYSCTL
  DEFINES+=						-DHAVE_RT_SYSCTL
 endif

 ifdef PICKLE_HAVE_SA_LEN
  DEFINES+=						-DHAVE_SA_LEN
 endif

 ifdef PICKLE_HAVE_SYS_CDEFS_H
  DEFINES+=						-DHAVE_SYS_CDEFS_H
 endif

 ifdef PICKLE_HAVE_ASM_TYPES_H
  DEFINES+=						-DHAVE_ASM_TYPES_H
 endif

 ifdef PICKLE_HAVE_SYS_SOCKIO_H
  DEFINES+=						-DHAVE_SYS_SOCKIO_H
 endif

 ifdef PICKLE_HAVE_BLKSIZE_T
  DEFINES+=						-DHAVE_BLKSIZE_T
 endif

 ifdef PICKLE_HAVE_FALLOC_FL_PUNCH_HOLE
  DEFINES+=						-DHAVE_FALLOC_FL_PUNCH_HOLE
 endif

 ifdef PICKLE_HAVE_S3
  DEFINES+=						-DHAVE_S3
 endif

 ifdef PICKLE_HAVE_ATTR_XATTR_H
  DEFINES+=						-DHAVE_ATTR_XATTR_H
 endif

 ifdef PICKLE_HAVE_STRVIS
  DEFINES+=						-DHAVE_STRVIS
 else
  STRVIS_SRCS+=						${PFL_BASE}/compat/strvis.c
 endif

 ifdef PICKLE_HAVE_STRNVIS
  DEFINES+=						-DHAVE_STRNVIS
 else
  STRVIS_SRCS+=						${PFL_BASE}/compat/strnvis.c
 endif

 ifdef PICKLE_HAVE_PTHREAD_YIELD
  DEFINES+=						-DHAVE_PTHREAD_YIELD
 endif

 ifdef PICKLE_HAVE_PTHREAD_YIELD_NP
  DEFINES+=						-DHAVE_PTHREAD_YIELD_NP
 endif

 ifdef PICKLE_HAVE_QSORT_R
   DEFINES+=						-DHAVE_QSORT_R
  ifdef PICKLE_HAVE_QSORT_R_THUNK
   DEFINES+=						-DHAVE_QSORT_R_THUNK
  endif
 else
  QSORT_R_SRCS+=					${PFL_BASE}/compat/qsort_r.c
 endif

 ifdef PICKLE_HAVE_GETLOADAVG
  DEFINES+=						-DHAVE_GETLOADAVG
 endif

 ifdef PICKLE_HAVE_SYS_ACL_H
  DEFINES+=						-DHAVE_SYS_ACL_H
  ACL_SRCS+=						${PFL_BASE}/acl.c
 endif

 ifdef PICKLE_HAVE_ENDIAN_H
  DEFINES+=						-DHAVE_ENDIAN_H
 endif

 ifdef PICKLE_HAVE_TUNE_NATIVE
  COPT+=						-mtune=native
 endif

 ifdef PICKLE_HAVE_PTHREAD_SETSCHEDPRIO
  COPT+=						-DHAVE_PTHREAD_SETSCHEDPRIO
 endif

 ifdef PICKLE_HAVE_BACKTRACE
  DEFINES+=						-DHAVE_BACKTRACE
 endif

endif
