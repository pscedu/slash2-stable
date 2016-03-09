/* $Id$ */
/*
 * %ISC_START_LICENSE%
 * ---------------------------------------------------------------------
 * Copyright 2015-2016, Google, Inc.
 * Copyright (c) 2006-2015, Pittsburgh Supercomputing Center (PSC).
 * All rights reserved.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the
 * above copyright notice and this permission notice appear in all
 * copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL
 * WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS.  IN NO EVENT SHALL THE
 * AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
 * PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 * --------------------------------------------------------------------
 * %END_LICENSE%
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
#elif defined(sun)
# define PSCPRI_PTHRT		"x"
#else
# define PSCPRI_PTHRT		"lx"
#endif

#if defined(__APPLE__) || defined(__OpenBSD__) || defined(__FreeBSD__) || defined(sun)
# define PSCPRI_BLKSIZE_T	"u"
#else
# define PSCPRI_BLKSIZE_T	"ld"
#endif

#ifdef __OpenBSD__
# define PSCPRI_TIMET		"d"
#else
# define PSCPRI_TIMET		"ld"
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
# define PSCPRI_PTIMESPEC	"%"PRIu64":%"PRIu64

#define PSCPRI_TIMEVAL_ARGS(tv)	  (tv)->tv_sec, (tv)->tv_usec
#define PSCPRI_TIMESPEC_ARGS(ts)  (ts)->tv_sec, (ts)->tv_nsec

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
#elif defined(__linux) || defined(sun)
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
