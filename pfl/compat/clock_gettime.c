/* $Id$ */
/*
 * %PSC_START_COPYRIGHT%
 * -----------------------------------------------------------------------------
 * Copyright (c) 2010-2013, Pittsburgh Supercomputing Center (PSC).
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

#include <sys/types.h>
#include <sys/time.h>

#include <err.h>
#include <time.h>

#include "pfl/compat/clock_gettime.h"

int
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
