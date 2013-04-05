/* $Id$ */
/*
 * %PSC_START_COPYRIGHT%
 * -----------------------------------------------------------------------------
 * Copyright (c) 2006-2013, Pittsburgh Supercomputing Center (PSC).
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

#ifndef _PFL_STAT_H_
#define _PFL_STAT_H_

#include <sys/types.h>
#include <sys/stat.h>

#include "pfl/time.h"
#include "pfl/types.h"
#include "psc_util/log.h"

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

struct pfl_stat {
	dev_t			st_dev;
	ino_t			st_ino;
	mode_t			st_mode;
	nlink_t			st_nlink;
	uid_t			st_uid;
	gid_t			st_gid;
	dev_t			st_rdev;
	off_t			st_size;
	blksize_t		st_blksize;
	blkcnt64_t		st_blocks;
	struct pfl_timespec	st_atim;
	struct pfl_timespec	st_mtim;
	struct pfl_timespec	st_ctim;
};

void pfl_dump_statbuf(const struct stat *);
void pfl_dump_mode(mode_t);

#define PFL_STAT_EXPORT(stb, pst)					\
	do {								\
		uint64_t _s, _ns;					\
									\
		(pst)->st_dev = (stb)->st_dev;				\
		(pst)->st_ino = (stb)->st_ino;				\
		(pst)->st_mode = (stb)->st_mode;			\
		(pst)->st_nlink = (stb)->st_nlink;			\
		(pst)->st_uid = (stb)->st_uid;				\
		(pst)->st_gid = (stb)->st_gid;				\
		(pst)->st_rdev = (stb)->st_rdev;			\
		(pst)->st_size = (stb)->st_size;			\
		(pst)->st_blksize = (stb)->st_blksize;			\
		(pst)->st_blocks = (stb)->st_blocks;			\
									\
		PFL_STB_ATIME_GET((stb), &_s, &_ns);			\
		PFL_STB_ATIME_SET(_s, _ns, (pst));			\
									\
		PFL_STB_MTIME_GET((stb), &_s, &_ns);			\
		PFL_STB_MTIME_SET(_s, _ns, (pst));			\
									\
		PFL_STB_CTIME_GET((stb), &_s, &_ns);			\
		PFL_STB_CTIME_SET(_s, _ns, (pst));			\
	} while (0)

#define PFL_STAT_IMPORT(pst, stb)					\
	do {								\
		uint64_t _s, _ns;					\
									\
		(stb)->st_dev = (pst)->st_dev;				\
		(stb)->st_ino = (pst)->st_ino;				\
		(stb)->st_mode = (pst)->st_mode;			\
		(stb)->st_nlink = (pst)->st_nlink;			\
		(stb)->st_uid = (pst)->st_uid;				\
		(stb)->st_gid = (pst)->st_gid;				\
		(stb)->st_rdev = (pst)->st_rdev;			\
		(stb)->st_size = (pst)->st_size;			\
		(stb)->st_blksize = (pst)->st_blksize;			\
		(stb)->st_blocks = (pst)->st_blocks;			\
									\
		PFL_STB_ATIME_GET((pst), &_s, &_ns);			\
		PFL_STB_ATIME_SET(_s, _ns, (stb));			\
									\
		PFL_STB_MTIME_GET((pst), &_s, &_ns);			\
		PFL_STB_MTIME_SET(_s, _ns, (stb));			\
									\
		PFL_STB_CTIME_GET((pst), &_s, &_ns);			\
		PFL_STB_CTIME_SET(_s, _ns, (stb));			\
	} while (0)

#endif /* _PFL_STAT_H_ */
