/* $Id$ */
/*
 * %PSC_START_COPYRIGHT%
 * -----------------------------------------------------------------------------
 * Copyright (c) 2006-2009, Pittsburgh Supercomputing Center (PSC).
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

#include <errno.h>
#include <pthread.h>
#include <string.h>

#include "psc_util/log.h"
#include "psc_util/pthrutil.h"
#include "psc_util/time.h"

void
psc_pthread_mutex_init(pthread_mutex_t *mut)
{
	pthread_mutexattr_t attr;
	int rc;

	rc = pthread_mutexattr_init(&attr);
	if (rc)
		psc_fatalx("pthread_mutexattr_init: %s", strerror(rc));
	rc = pthread_mutexattr_settype(&attr,
	    PTHREAD_MUTEX_ERRORCHECK_NP);
	if (rc)
		psc_fatalx("pthread_mutexattr_settype: %s",
		    strerror(rc));
	rc = pthread_mutex_init(mut, &attr);
	if (rc)
		psc_fatalx("pthread_mutex_init: %s", strerror(rc));
}

void
psc_pthread_mutex_lock(pthread_mutex_t *mut)
{
	int rc;

	rc = pthread_mutex_lock(mut);
	if (rc)
		psc_fatalx("pthread_mutex_lock: %s", strerror(rc));
}

void
psc_pthread_mutex_unlock(pthread_mutex_t *mut)
{
	int rc;

	rc = pthread_mutex_unlock(mut);
	if (rc)
		psc_fatalx("pthread_mutex_unlock: %s", strerror(rc));
}

int
psc_pthread_mutex_reqlock(pthread_mutex_t *mut)
{
	int rc;

	rc = pthread_mutex_lock(mut);
	if (rc == EDEADLK)
		rc = 1;
	else if (rc)
		psc_fatalx("pthread_mutex_unlock: %s", strerror(rc));
	return (rc);
}

void
psc_pthread_mutex_ureqlock(pthread_mutex_t *mut, int waslocked)
{
	if (!waslocked)
		psc_pthread_mutex_unlock(mut);
}

int
psc_pthread_mutex_trylock(pthread_mutex_t *mut)
{
	struct timespec ts;
	int rc;

	/*
	 * pthread_mutex_trylock() may not return EDEADLK while we are
	 * holding the lock, so use an immediate timeout instead.
	 */
	memset(&ts, 0, sizeof(ts));
	rc = pthread_mutex_timedlock(mut, &ts);
	if (rc == 0)
		return (0);
	if (rc == ETIMEDOUT)
		return (EBUSY);
	psc_fatalx("psc_pthread_mutex_trylock: %s", strerror(rc));
}

void
psc_pthread_mutex_ensure_locked(pthread_mutex_t *mut)
{
	struct timespec ts;
	int rc;

	memset(&ts, 0, sizeof(ts));
	rc = pthread_mutex_timedlock(mut, &ts);
	psc_assert(rc == EDEADLK);
}
