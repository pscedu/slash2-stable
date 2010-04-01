/* $Id$ */

#include <sys/types.h>
#include <sys/time.h>

#include <err.h>
#include <time.h>

#include "pfl/compat/clock_gettime.h"

long
clock_gettime(clockid_t cid, struct timespec *ts)
{
	struct timeval tv;
	int rc;

	switch (cid) {
	case CLOCK_REALTIME:
		rc = gettimeofday(&tv, NULL);
		break;
	default:
		errx(1, "invalid clock ID: %d", cid);
	}

	if (rc == 0) {
		ts->tv_sec = tv.tv_sec;
		ts->tv_nsec = tv.tv_usec * 1000;
	}
	return (-rc);
}
