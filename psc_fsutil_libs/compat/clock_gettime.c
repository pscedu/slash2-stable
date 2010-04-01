/* $Id$ */

#include <time.h>

#include "psc_util/log.h"
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
		psc_fatalx("invalid clock ID: %d", cid);
	}

	if (rc == 0) {
		ts->tv_sec = tv.tv_sec;
		ts->tv_nsec = tv.tv_usec * 1000;
	}
	return (-rc);
}
