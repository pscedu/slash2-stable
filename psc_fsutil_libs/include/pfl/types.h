/* $Id$ */
/*
 * %PSC_START_COPYRIGHT%
 * -----------------------------------------------------------------------------
 * Copyright (c) 2006-2011, Pittsburgh Supercomputing Center (PSC).
 *
 * Permission to use, copy, and modify this software and its documentation
 * without fee for personal use or non-commercial use within your organization
 * is hereby granted, provided that the above copyright notice is preserved in
 * all copies and that the copyright and this permission notice appear in
 * supporting documentation.  Permission to redistribute this software to other
 * organizations or individuals is not permitted without the written permission
 * of the Pittsburgh Supercomputing Center.  PSC makes no representations about
 * the suitability of this software for any purpose.  It is provided "as is"
 * without express or implied warranty.
 * -----------------------------------------------------------------------------
 * %PSC_END_COPYRIGHT%
 */

#ifndef _PFL_TYPES_H_
#define _PFL_TYPES_H_

#include <sys/types.h>

#include <inttypes.h>

/* printf(3) specifier modifiers for custom types. */
#define PSCPRIxLNID		"#"PRIx64
#define PSCPRIxCRC32		"#010x"
#define PSCPRIxCRC64		"#018"PRIx64

#if defined(__APPLE__) || defined(__OpenBSD__) || defined(__FreeBSD__)
# define PSCPRI_PTHRT		"p"
#else
# define PSCPRI_PTHRT		"lx"
#endif

#if defined(__APPLE__) || defined(__OpenBSD__) || defined(__FreeBSD__)
# define PSCPRI_BLKSIZE_T	"u"
#else
# define PSCPRI_BLKSIZE_T	"ld"
#endif

#ifdef __OpenBSD__
# define PSCPRI_TIMET		"d"
#else
# define PSCPRI_TIMET		"lu"
#endif

#ifdef __APPLE__
# define PSCPRI_UTIMET		"06d"
#else
# define PSCPRI_UTIMET		"06ld"
#endif

#define PSCPRI_NTIMET		"09ld"

#ifdef __OpenBSD__
# define PSCPRI_TIMEVAL		"%ld:%"PSCPRI_UTIMET
#else
# define PSCPRI_TIMEVAL		"%"PSCPRI_TIMET":%"PSCPRI_UTIMET
#endif

# define PSCPRI_TIMESPEC	"%"PSCPRI_TIMET":%"PSCPRI_NTIMET

#define PSCPRI_TIMEVAL_ARGS(tv)	(tv)->tv_sec, (tv)->tv_usec
#define PSCPRI_TIMESPEC_ARGS(ts)(ts)->tv_sec, (ts)->tv_nsec

#ifdef HAVE_FILE_OFFSET32
# define _PSCPRIxOFFT		"lx"
# define PSCPRIdOFFT		"ld"
#else
# define _PSCPRIxOFFT		PRIx64
# define PSCPRIdOFFT		PRId64
#endif

# define PSCPRIxOFFT		"#"_PSCPRIxOFFT

#if !defined(HAVE_FILE_OFFSET32) && (defined(__USE_FILE_OFFSET64) || defined(__APPLE__))
# define PSCPRIuINOT		PRIu64
#elif defined(__linux)
# define PSCPRIuINOT		"lu"
#else
# define PSCPRIuINOT		"u"
#endif

#if defined(BYTE_ORDER)			/* macos */
#  if BYTE_ORDER == LITTLE_ENDIAN
#    define PFL_LITTLE_ENDIAN
#  elif BYTE_ORDER == BIG_ENDIAN
#    define PFL_BIG_ENDIAN
#  endif
#elif defined(_BYTE_ORDER)		/* bsd */
#  if _BYTE_ORDER == _LITTLE_ENDIAN
#    define PFL_LITTLE_ENDIAN
#  elif _BYTE_ORDER == _BIG_ENDIAN
#    define PFL_BIG_ENDIAN
#  endif
#elif defined(__BYTE_ORDER)		/* linux */
#  if __BYTE_ORDER == __LITTLE_ENDIAN
#    define PFL_LITTLE_ENDIAN
#  elif __BYTE_ORDER == __BIG_ENDIAN
#    define PFL_BIG_ENDIAN
#  endif
#elif defined(_LITTLE_ENDIAN)		/* solaris */
#  if defined(_BIG_ENDIAN)
#    error machine endian ambiguous
#  endif
#  define PFL_LITTLE_ENDIAN
#elif defined(_BIG_ENDIAN)
#  if defined(_LITTLE_ENDIAN)
#    error machine endian ambiguous
#  endif
#    define PFL_BIG_ENDIAN
#endif

#if !defined(PFL_LITTLE_ENDIAN) && !defined(PFL_BIG_ENDIAN)
# error cannot determine machine endianess
#endif

#include "pfl/subsys.h"

#endif /* _PFL_TYPES_H_ */
