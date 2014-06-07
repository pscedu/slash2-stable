/* $Id$ */

#ifndef _PFL_ERR_H_
#define _PFL_ERR_H_

#define _PFLERR_START			500

#define PFLERR_BADMSG			(_PFLERR_START +  0)
#define PFLERR_KEYEXPIRED		(_PFLERR_START +  1)
#define PFLERR_NOTCONN			(_PFLERR_START +  2)
#define PFLERR_ALREADY			(_PFLERR_START +  3)
#define PFLERR_NOTSUP			(_PFLERR_START +  4)
#define PFLERR_NOSYS			(_PFLERR_START +  5)
#define PFLERR_CANCELED			(_PFLERR_START +  6)
#define PFLERR_STALE			(_PFLERR_START +  7)

const char *pfl_strerror(int);

#define strerror(rc)			pfl_strerror(rc)

#endif /* _PFL_ERR_H_ */
