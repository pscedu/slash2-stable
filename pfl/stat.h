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

#ifndef _PFL_STAT_H_
#define _PFL_STAT_H_

#include <sys/types.h>
#include <sys/stat.h>

#include "pfl/time.h"
#include "pfl/types.h"
#include "pfl/log.h"

struct stat;

#if defined(HAVE_STB_TIM) || defined(HAVE_STB_TIMESPEC)

# ifdef HAVE_STB_TIM
#  define st_pfl_atim st_atim
#  define st_pfl_ctim st_ctim
#  define st_pfl_mtim st_mtim
# else
#  define st_pfl_atim st_atimespec
#  define st_pfl_ctim st_ctimespec
#  define st_pfl_mtim st_mtimespec
# endif

# define PFL_STB_ATIME_SET(s, ns, stb)	do { (stb)->st_pfl_atim.tv_sec = (s); (stb)->st_pfl_atim.tv_nsec = (ns); } while (0)
# define PFL_STB_MTIME_SET(s, ns, stb)	do { (stb)->st_pfl_mtim.tv_sec = (s); (stb)->st_pfl_mtim.tv_nsec = (ns); } while (0)
# define PFL_STB_CTIME_SET(s, ns, stb)	do { (stb)->st_pfl_ctim.tv_sec = (s); (stb)->st_pfl_ctim.tv_nsec = (ns); } while (0)

# define PFL_STB_ATIME_GET(stb, s, ns)	do { *(s) = (stb)->st_pfl_atim.tv_sec; *(ns) = (stb)->st_pfl_atim.tv_nsec; } while (0)
# define PFL_STB_MTIME_GET(stb, s, ns)	do { *(s) = (stb)->st_pfl_mtim.tv_sec; *(ns) = (stb)->st_pfl_mtim.tv_nsec; } while (0)
# define PFL_STB_CTIME_GET(stb, s, ns)	do { *(s) = (stb)->st_pfl_ctim.tv_sec; *(ns) = (stb)->st_pfl_ctim.tv_nsec; } while (0)
#else
# define PFL_STB_ATIME_SET(s, ns, stb)	do { (stb)->st_atime = (s); } while (0)
# define PFL_STB_MTIME_SET(s, ns, stb)	do { (stb)->st_mtime = (s); } while (0)
# define PFL_STB_CTIME_SET(s, ns, stb)	do { (stb)->st_ctime = (s); } while (0)

# define PFL_STB_ATIME_GET(stb, s, ns)	do { *(s) = (stb)->st_atime; *(ns) = 0; } while (0)
# define PFL_STB_MTIME_GET(stb, s, ns)	do { *(s) = (stb)->st_mtime; *(ns) = 0; } while (0)
# define PFL_STB_CTIME_GET(stb, s, ns)	do { *(s) = (stb)->st_ctime; *(ns) = 0; } while (0)
#endif

/* XXX this is enough to wipe the cacheline */
#define DEBUG_STATBUF(level, stb, fmt, ...)					\
	do {									\
		struct timespec _atime, _ctime, _mtime;				\
										\
		PFL_STB_ATIME_GET((stb), &_atime.tv_sec, &_atime.tv_nsec);	\
		PFL_STB_CTIME_GET((stb), &_ctime.tv_sec, &_ctime.tv_nsec);	\
		PFL_STB_MTIME_GET((stb), &_mtime.tv_sec, &_mtime.tv_nsec);	\
										\
		psclog((level),							\
		    "stb@%p dev:%"PRIu64" inode:%"PSCPRIuINOT" "		\
		    "mode:%#o nlink:%"PRIu64" "					\
		    "uid:%u gid:%u "						\
		    "rdev:%"PRIu64" sz:%"PSCPRIdOFFT" "				\
		    "blksz:%"PSCPRI_BLKSIZE_T" blkcnt:%"PRId64" "		\
		    "atime:"PSCPRI_TIMESPEC" "					\
		    "mtime:"PSCPRI_TIMESPEC" "					\
		    "ctime:"PSCPRI_TIMESPEC" " fmt,				\
		    (stb), (uint64_t)(stb)->st_dev, (stb)->st_ino,		\
		    (stb)->st_mode, (uint64_t)(stb)->st_nlink,			\
		    (stb)->st_uid, (stb)->st_gid,				\
		    (uint64_t)(stb)->st_rdev, (stb)->st_size,			\
		    (stb)->st_blksize, (stb)->st_blocks,			\
		    PSCPRI_TIMESPEC_ARGS(&_atime),				\
		    PSCPRI_TIMESPEC_ARGS(&_mtime),				\
		    PSCPRI_TIMESPEC_ARGS(&_ctime), ## __VA_ARGS__);		\
	} while (0)

#define _S_IRUGO (S_IRUSR | S_IRGRP | S_IROTH)
#define _S_IWUGO (S_IWUSR | S_IWGRP | S_IWOTH)
#define _S_IXUGO (S_IXUSR | S_IXGRP | S_IXOTH)

#ifndef ALLPERMS
#define ALLPERMS	(S_ISUID | S_ISGID | S_ISVTX | S_IRWXU | S_IRWXG | S_IRWXO)
#endif

/* XXX we should be careful here */
#ifndef HAVE_BLKSIZE_T
typedef uint64_t blksize_t;
typedef uint64_t blkcnt_t;
#endif

void pfl_dump_statbuf(const struct stat *);
void pfl_dump_mode(mode_t);

#endif /* _PFL_STAT_H_ */
