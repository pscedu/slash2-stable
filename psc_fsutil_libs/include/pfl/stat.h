/* $Id$ */
/* %PSC_COPYRIGHT% */

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
