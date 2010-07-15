/* $Id$ */

#ifndef _PFL_SETRESUID_H_
#define _PFL_SETRESUID_H_

#ifdef HAVE_SETRESUID
#include <sys/types.h>

int setresuid(uid_t, uid_t, uid_t);
#endif

#endif /* _PFL_SETRESUID_H_ */
