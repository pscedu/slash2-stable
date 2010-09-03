/* $Id$ */
/*
 * %PSC_START_COPYRIGHT%
 * -----------------------------------------------------------------------------
 * Copyright (c) 2009-2010, Pittsburgh Supercomputing Center (PSC).
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

#include "psc_ds/vbitmap.h"
#include "psc_util/log.h"
#include "psc_util/pthrutil.h"
#include "psc_util/thread.h"
#include "pfl/time.h"

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
		return (0);
	if (rc == ETIMEDOUT)
		return (EBUSY);
	psc_fatalx("pthread_mutex_timedlock: %s", strerror(rc));
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

void
psc_pthread_mutex_ensure_locked(pthread_mutex_t *mut)
{
	int rc;

	rc = pthread_mutex_trylock(mut);
	psc_assert(rc == EBUSY); /* XXX XXX EDEADLK XXX XXX */
}

#endif

void
psc_pthread_rwlock_destroy(struct psc_pthread_rwlock *ppr)
{
	psc_vbitmap_free(ppr->ppr_readers);
}

void
psc_pthread_rwlock_init(struct psc_pthread_rwlock *ppr)
{
	int rc;

	LOCK_INIT(&ppr->ppr_lock);
	ppr->ppr_readers = psc_vbitmap_newf(1, PVBF_AUTO);
	rc = pthread_rwlock_init(&ppr->ppr_rwlock, NULL);
	if (rc)
		psc_fatalx("pthread_rwlock_init: %s", strerror(rc));
}

void
psc_pthread_rwlock_rdlock(struct psc_pthread_rwlock *ppr)
{
	struct psc_thread *thr;
	int rc;

	thr = pscthr_get();
#ifdef GNUC
	if (ppr->ppr_rwlock.__writer == pscthr_thrid)
		psc_fatal("deadlock: we hold writer lock");
#endif

	pscthr_getuniqid();
	spinlock(&ppr->ppr_lock);
	if ((int)psc_vbitmap_getsize(ppr->ppr_readers) <
	    thr->pscthr_uniqid + 1)
		psc_vbitmap_resize(ppr->ppr_readers,
		    thr->pscthr_uniqid + 1);
	psc_assert(psc_vbitmap_xsetval(ppr->ppr_readers,
	    thr->pscthr_uniqid, 1));
	freelock(&ppr->ppr_lock);

	rc = pthread_rwlock_rdlock(&ppr->ppr_rwlock);
	if (rc)
		psc_fatalx("pthread_rwlock_rdlock: %s", strerror(rc));
}

int
psc_pthread_rwlock_rdreqlock(struct psc_pthread_rwlock *ppr)
{
	struct psc_thread *thr;
	int rc;

	thr = pscthr_get();
#ifdef GNUC
	if (ppr->ppr_rwlock.__writer == pscthr_thrid)
		psc_fatal("deadlock: we hold writer lock");
#endif

	pscthr_getuniqid();
	spinlock(&ppr->ppr_lock);
	if ((int)psc_vbitmap_getsize(ppr->ppr_readers) <
	    thr->pscthr_uniqid + 1)
		psc_vbitmap_resize(ppr->ppr_readers,
		    thr->pscthr_uniqid + 1);
	rc = psc_vbitmap_xsetval(ppr->ppr_readers, thr->pscthr_uniqid, 1);
	freelock(&ppr->ppr_lock);

	if (!rc)
		return (1);

	rc = pthread_rwlock_rdlock(&ppr->ppr_rwlock);
	if (rc)
		psc_fatalx("pthread_rwlock_rdlock: %s", strerror(rc));
	return (0);
}

void
psc_pthread_rwlock_rdureqlock(struct psc_pthread_rwlock *ppr, int waslocked)
{
	struct psc_thread *thr;
	int rc;

	thr = pscthr_get();
	pscthr_getuniqid();
	spinlock(&ppr->ppr_lock);
	if ((int)psc_vbitmap_getsize(ppr->ppr_readers) <
	    thr->pscthr_uniqid + 1)
		psc_fatal("unlocking a rwlock we could have locked");
	rc = psc_vbitmap_xsetval(ppr->ppr_readers, thr->pscthr_uniqid, 0);
	freelock(&ppr->ppr_lock);

	psc_assert(rc);

	if (!waslocked) {
		rc = pthread_rwlock_unlock(&ppr->ppr_rwlock);
		if (rc)
			psc_fatalx("pthread_rwlock_unlock: %s", strerror(rc));
	}
}

void
psc_pthread_rwlock_rdunlock(struct psc_pthread_rwlock *ppr)
{
	struct psc_thread *thr;
	int rc;

	thr = pscthr_get();
	pscthr_getuniqid();
	spinlock(&ppr->ppr_lock);
	if ((int)psc_vbitmap_getsize(ppr->ppr_readers) <
	    thr->pscthr_uniqid + 1)
		psc_fatal("unlocking a rwlock we couldn't have locked");
	rc = psc_vbitmap_xsetval(ppr->ppr_readers, thr->pscthr_uniqid, 0);
	freelock(&ppr->ppr_lock);

	psc_assert(rc);

	rc = pthread_rwlock_unlock(&ppr->ppr_rwlock);
	if (rc)
		psc_fatalx("pthread_rwlock_unlock: %s", strerror(rc));
}

void
psc_pthread_rwlock_unlock(struct psc_pthread_rwlock *ppr)
{
	struct psc_thread *thr;
	int rc;

	thr = pscthr_get();
	pscthr_getuniqid();
	spinlock(&ppr->ppr_lock);
	if ((int)psc_vbitmap_getsize(ppr->ppr_readers) <
	    thr->pscthr_uniqid + 1)
		psc_fatal("unlocking a rwlock we couldn't have locked");
	rc = psc_vbitmap_xsetval(ppr->ppr_readers, thr->pscthr_uniqid, 0);
#ifdef GNUC
	psc_assert(rc || ppr->ppr_rwlock.__writer == thr->pscthr_thrid);
#endif
	freelock(&ppr->ppr_lock);

	rc = pthread_rwlock_unlock(&ppr->ppr_rwlock);
	if (rc)
		psc_fatalx("pthread_rwlock_unlock: %s", strerror(rc));
}

void
psc_pthread_rwlock_wrlock(struct psc_pthread_rwlock *ppr)
{
	struct psc_thread *thr;
	int rc;

	thr = pscthr_get();
	pscthr_getuniqid();
	spinlock(&ppr->ppr_lock);
	if ((int)psc_vbitmap_getsize(ppr->ppr_readers) <
	    thr->pscthr_uniqid + 1)
		psc_vbitmap_resize(ppr->ppr_readers,
		    thr->pscthr_uniqid + 1);
	if (psc_vbitmap_get(ppr->ppr_readers, thr->pscthr_uniqid))
		psc_fatal("deadlock: we hold read lock");
	freelock(&ppr->ppr_lock);

	rc = pthread_rwlock_wrlock(&ppr->ppr_rwlock);
	if (rc)
		psc_fatalx("pthread_rwlock_unlock: %s", strerror(rc));
}
