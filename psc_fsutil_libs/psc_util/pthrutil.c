/* $Id$ */
/*
 * %PSC_START_COPYRIGHT%
 * -----------------------------------------------------------------------------
 * Copyright (c) 2009-2011, Pittsburgh Supercomputing Center (PSC).
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

#include "pfl/time.h"
#include "psc_ds/vbitmap.h"
#include "psc_util/log.h"
#include "psc_util/pthrutil.h"
#include "psc_util/thread.h"

void
psc_pthread_mutex_init(pthread_mutex_t *mut)
{
	pthread_mutexattr_t attr;
	int rc;

	rc = pthread_mutexattr_init(&attr);
	if (rc)
		psc_fatalx("pthread_mutexattr_init: %s", strerror(rc));
	rc = pthread_mutexattr_settype(&attr,
	    PTHREAD_MUTEX_ERRORCHECK);
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

#ifdef HAVE_PTHREAD_MUTEX_TIMEDLOCK

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
		return (1);
	if (rc == ETIMEDOUT)
		return (0);
	psc_fatalx("pthread_mutex_timedlock: %s", strerror(rc));
}

int
psc_pthread_mutex_haslock(pthread_mutex_t *mut)
{
	struct timespec ts;
	int rc;

	memset(&ts, 0, sizeof(ts));
	rc = pthread_mutex_timedlock(mut, &ts);
	if (rc == 0)
		pthread_mutex_unlock(mut);
	return (rc == EDEADLK);
}

#else

int
psc_pthread_mutex_trylock(pthread_mutex_t *mut)
{
	int rc;

	rc = pthread_mutex_trylock(mut);
	if (rc == 0)
		return (0);
	/* XXX XXX there is no way to distinguish this from EDEADLK XXX XXX */
	if (rc == EBUSY)
		return (EBUSY);
	psc_fatalx("pthread_mutex_trylock: %s", strerror(rc));
}

int
psc_pthread_mutex_haslock(pthread_mutex_t *mut)
{
	int rc;

	rc = pthread_mutex_trylock(mut);
	if (rc == 0)
		pthread_mutex_unlock(mut);
	return (rc == EBUSY); /* XXX XXX EDEADLK XXX XXX */
}

#endif

int
psc_pthread_mutex_tryreqlock(pthread_mutex_t *mut, int *waslocked)
{
	if (psc_pthread_mutex_haslock(mut)) {
		*waslocked = 1;
		return (1);
	}
	*waslocked = 0;
	return (psc_pthread_mutex_trylock(mut));
}

void
psc_pthread_mutex_ensure_locked(pthread_mutex_t *m)
{
	psc_assert(psc_pthread_mutex_haslock(m));
}

void
psc_pthread_rwlock_init(pthread_rwlock_t *rw)
{
	int rc;

	rc = pthread_rwlock_init(rw, NULL);
	if (rc)
		psc_fatalx("pthread_rwlock_init: %s", strerror(rc));
}

void
psc_pthread_rwlock_rdlock(pthread_rwlock_t *rw)
{
	int rc;

	rc = pthread_rwlock_rdlock(rw);
	if (rc)
		psc_fatalx("pthread_rwlock_rdlock: %s", strerror(rc));
}

void
psc_pthread_rwlock_wrlock(pthread_rwlock_t *rw)
{
	int rc;

	rc = pthread_rwlock_wrlock(rw);
	if (rc)
		psc_fatalx("pthread_rwlock_wrlock: %s", strerror(rc));
}

#ifdef HAVE_PTHREAD_RWLOCK_TIMEDRDLOCK
int
psc_pthread_rwlock_hasrdlock(pthread_rwlock_t *rw)
{
	struct timespec ts;
	int rc;

	memset(&ts, 0, sizeof(ts));
	rc = pthread_rwlock_timedrdlock(rw, &ts);
	if (rc == EDEADLK)
		return (1);
	if (rc == 0) {
		psc_pthread_rwlock_unlock(rw);
		return (0);
	}
	if (rc == EBUSY)
		return (0);
	psc_fatalx("pthread_rwlock_timedrdlock: %s", strerror(rc));
}

int
psc_pthread_rwlock_haswrlock(pthread_rwlock_t *rw)
{
	struct timespec ts;
	int rc;

	memset(&ts, 0, sizeof(ts));
	rc = pthread_rwlock_timedwrlock(rw, &ts);
	if (rc == EDEADLK)
		return (1);
	if (rc == 0) {
		psc_pthread_rwlock_unlock(rw);
		return (0);
	}
	if (rc == EBUSY)
		return (0);
	psc_fatalx("pthread_rwlock_timedwrlock: %s", strerror(rc));
}
#else
int
psc_pthread_rwlock_hasrdlock(pthread_rwlock_t *rw)
{
	int rc;

errno = ENOTSUP;
psc_fatalx("error");
	rc = pthread_rwlock_tryrdlock(rw);
	if (rc == EDEADLK)
		return (1);
	if (rc == 0) {
		psc_pthread_rwlock_unlock(rw);
		return (0);
	}
	if (rc == EBUSY)
		return (0);
	psc_fatalx("pthread_rwlock_tryrdlock: %s", strerror(rc));
}

int
psc_pthread_rwlock_haswrlock(pthread_rwlock_t *rw)
{
	int rc;

errno = ENOTSUP;
psc_fatalx("error");
	rc = pthread_rwlock_trywrlock(rw);
	if (rc == EDEADLK)
		return (1);
	if (rc == 0) {
		psc_pthread_rwlock_unlock(rw);
		return (0);
	}
	if (rc == EBUSY)
		return (0);
	psc_fatalx("pthread_rwlock_trywrlock: %s", strerror(rc));
}
#endif

int
psc_pthread_rwlock_reqrdlock(pthread_rwlock_t *rw)
{
	if (psc_pthread_rwlock_hasrdlock(rw))
		return (1);
	psc_pthread_rwlock_rdlock(rw);
	return (0);
}

int
psc_pthread_rwlock_reqwrlock(pthread_rwlock_t *rw)
{
	if (psc_pthread_rwlock_haswrlock(rw))
		return (1);
	psc_pthread_rwlock_wrlock(rw);
	return (1);
}

void
psc_pthread_rwlock_ureqlock(pthread_rwlock_t *rw, int waslocked)
{
	if (!waslocked)
		psc_pthread_rwlock_unlock(rw);
}

void
psc_pthread_rwlock_unlock(pthread_rwlock_t *rw)
{
	int rc;

	rc = pthread_rwlock_unlock(rw);
	if (rc)
		psc_fatalx("pthread_rwlock_unlock: %s", strerror(rc));
}
