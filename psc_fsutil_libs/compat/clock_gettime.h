/* $Id$ */

#ifndef _PFL_COMPAT_CLOCK_GETTIME_H_
#define _PFL_COMPAT_CLOCK_GETTIME_H_

#include <time.h>

typedef int clockid_t;

long clock_gettime(clockid_t, struct timespec *);

enum {
	CLOCK_REALTIME
};

#endif /* _PFL_COMPAT_CLOCK_GETTIME_H_ */
