/* $Id$ */
/*
 * %PSC_START_COPYRIGHT%
 * -----------------------------------------------------------------------------
 * Copyright (c) 2006-2010, Pittsburgh Supercomputing Center (PSC).
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

#ifdef HAVE_STB_TIM
# define PFL_STB_ATIME_SET(s, ns, stb)	do { (stb)->st_atim.tv_sec = (s); (stb)->st_atim.tv_nsec = (ns); } while (0)
# define PFL_STB_MTIME_SET(s, ns, stb)	do { (stb)->st_mtim.tv_sec = (s); (stb)->st_mtim.tv_nsec = (ns); } while (0)
# define PFL_STB_CTIME_SET(s, ns, stb)	do { (stb)->st_ctim.tv_sec = (s); (stb)->st_ctim.tv_nsec = (ns); } while (0)

# define PFL_STB_ATIME_GET(stb, s, ns)	do { *(s) = (stb)->st_atim.tv_sec; *(ns) = (stb)->st_atim.tv_nsec; } while (0)
# define PFL_STB_MTIME_GET(stb, s, ns)	do { *(s) = (stb)->st_mtim.tv_sec; *(ns) = (stb)->st_mtim.tv_nsec; } while (0)
# define PFL_STB_CTIME_GET(stb, s, ns)	do { *(s) = (stb)->st_ctim.tv_sec; *(ns) = (stb)->st_ctim.tv_nsec; } while (0)
#else
# define PFL_STB_ATIME_SET(s, ns, stb)	do { (stb)->st_atime = (s); } while (0)
# define PFL_STB_MTIME_SET(s, ns, stb)	do { (stb)->st_mtime = (s); } while (0)
# define PFL_STB_CTIME_SET(s, ns, stb)	do { (stb)->st_ctime = (s); } while (0)

# define PFL_STB_ATIME_GET(stb, s, ns)	do { *(s) = (stb)->st_atim.tv_sec; *(ns) = 0; } while (0)
# define PFL_STB_MTIME_GET(stb, s, ns)	do { *(s) = (stb)->st_mtim.tv_sec; *(ns) = 0; } while (0)
# define PFL_STB_CTIME_GET(stb, s, ns)	do { *(s) = (stb)->st_ctim.tv_sec; *(ns) = 0; } while (0)
#endif

#endif /* _PFL_STAT_H_ */
