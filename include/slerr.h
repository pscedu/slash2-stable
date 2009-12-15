/* $Id$ */

#ifndef _SLERR_H_
#define _SLERR_H_

char *slstrerror(int);

#define _SLERR_START			1000		/* must be >max errno */
#define SLERR_REPL_ALREADY_ACT		(_SLERR_START)
#define SLERR_REPL_ALREADY_INACT	(_SLERR_START + 1)
#define SLERR_REPLS_ALL_INACT		(_SLERR_START + 2)
#define SLERR_INVALID_BMAP		(_SLERR_START + 3)
#define SLERR_BMAP_ZERO			(_SLERR_START + 4)
#define SLERR_UNKNOWN_IOS		(_SLERR_START + 5)
#define SLERR_ION_UNKNOWN		(_SLERR_START + 6)
#define SLERR_ION_OFFLINE		(_SLERR_START + 7)
#define SLERR_ION_NOTREPL		(_SLERR_START + 8)
#define SLERR_XACT_FAIL			(_SLERR_START + 9)
#define SLERR_SHORTIO			(_SLERR_START + 10)
#define SLERR_RES_UNKNOWN		(_SLERR_START + 11)

#endif /* _SLERR_H_ */
