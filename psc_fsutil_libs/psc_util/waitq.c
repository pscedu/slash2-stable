/* $Id$ */
/*
 * %PSC_START_COPYRIGHT%
 * -----------------------------------------------------------------------------
 * Copyright (c) 2006-2011, Pittsburgh Supercomputing Center (PSC).
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
#include <string.h>

#include "pfl/cdefs.h"
#include "psc_util/atomic.h"
#include "psc_util/lock.h"
#include "psc_util/log.h"
#include "psc_util/waitq.h"

#if HAVE_LIBPTHREAD

#include "pfl/time.h"
#include "psc_util/pthrutil.h"

#include <pthread.h>

/**
 * psc_waitq_init - Prepare a wait queue for use.
 * @q: the struct to be initialized.
 */
void
psc_waitq_init(struct psc_waitq *q)
{
	int rc;

	memset(q, 0, sizeof(*q));
	atomic_set(&q->wq_nwaiters, 0);

	psc_mutex_init(&q->wq_mut);
	rc = pthread_cond_init(&q->wq_cond, NULL);
	if (rc)
		psc_fatalx("pthread_cond_init: %s", strerror(rc));
}

void
psc_waitq_destroy(struct psc_waitq *q)
{
	int rc;

	psc_mutex_destroy(&q->wq_mut);
	rc = pthread_cond_destroy(&q->wq_cond);
	if (rc)
		psc_fatalx("pthread_cond_destroy: %s", strerror(rc));
}
/**
 * psc_waitq_waitabs - Wait until the time specified for the
 *	resource managed by wq_cond to become available.
 * @q: the wait queue to wait on.
 * @k: optional lock needed to protect the list.
 * @reltime: amount of time to wait for.
 * Note: returns ETIMEDOUT if the resource has not become available.
 * Note: this code could be merged with waitrel() but we try to make
 *	the timing calculations accurate.
 */
int
psc_waitq_waitabs(struct psc_waitq *q, psc_spinlock_t *k,
    const struct timespec *abstime)
{
	int rc;

	psc_mutex_lock(&q->wq_mut);

	if (k != NULL)
		freelock(k);

	atomic_inc(&q->wq_nwaiters);
	rc = pthread_cond_timedwait(&q->wq_cond, &q->wq_mut.pm_mutex,
	    abstime);
	if (rc && rc != ETIMEDOUT)
		psc_fatalx("pthread_cond_timedwait: %s", strerror(rc));

	psc_mutex_unlock(&q->wq_mut);
	/* Bug 91: Decrease waiters after releasing the lock to guarantee
	 *    wq_mut remains intact.
	 */
	atomic_dec(&q->wq_nwaiters);

	return (rc);
}

/**
 * psc_waitq_waitrel - Wait at most the amount of time specified
 *	(relative to calling time) for the resource managed by wq_cond
 *	to become available.
 * @q: the wait queue to wait on.
 * @k: optional lock needed to protect the list.
 * @reltime: amount of time to wait for.
 * Notes: returns ETIMEDOUT if the resource has not become available.
 */
int
psc_waitq_waitrel(struct psc_waitq *q, psc_spinlock_t *k,
    const struct timespec *reltime)
{
	struct timespec abstime;
	int rc;

	psc_mutex_lock(&q->wq_mut);

	if (k != NULL)
		freelock(k);

	atomic_inc(&q->wq_nwaiters);
	if (reltime) {
		PFL_GETTIMESPEC(&abstime);
		timespecadd(&abstime, reltime, &abstime);
		rc = pthread_cond_timedwait(&q->wq_cond,
		    &q->wq_mut.pm_mutex, &abstime);
		if (rc && rc != ETIMEDOUT)
			psc_fatalx("pthread_cond_timedwait: %s", strerror(rc));
	} else {
		rc = pthread_cond_wait(&q->wq_cond,
		    &q->wq_mut.pm_mutex);
		if (rc)
			psc_fatalx("pthread_cond_wait: %s", strerror(rc));
	}
	psc_mutex_unlock(&q->wq_mut);
	atomic_dec(&q->wq_nwaiters);

	return (rc);
}

__inline int
_psc_waitq_waitrelv(struct psc_waitq *wq, psc_spinlock_t *lk, long s, long ns)
{
	struct timespec ts;

	ts.tv_sec = s;
	ts.tv_nsec = ns;
	return (psc_waitq_waitrel(wq, lk, &ts));
}

/**
 * psc_waitq_wakeone - Unblock one thread waiting on a wait queue.
 * @q: wait queue to operate on.
 */
void
psc_waitq_wakeone(struct psc_waitq *q)
{
	psc_mutex_lock(&q->wq_mut);
	if (atomic_read(&q->wq_nwaiters)) {
		int rc;

		rc = pthread_cond_signal(&q->wq_cond);
		if (rc)
			psc_fatalx("pthread_cond_signal: %s", strerror(rc));
	}
	psc_mutex_unlock(&q->wq_mut);
}

/**
 * psc_waitq_wakeall - Wake everyone waiting on a wait queue.
 * @q: wait queue to operate on.
 */
void
psc_waitq_wakeall(struct psc_waitq *q)
{
	psc_mutex_lock(&q->wq_mut);
	if (atomic_read(&q->wq_nwaiters)) {
		int rc;

		rc = pthread_cond_broadcast(&q->wq_cond);
		if (rc)
			psc_fatalx("pthread_cond_broadcast: %s", strerror(rc));
	}
	psc_mutex_unlock(&q->wq_mut);
}

#else /* HAVE_LIBPTHREAD */

void
psc_waitq_init(struct psc_waitq *q)
{
	atomic_set(&q->wq_nwaiters, 0);
}

int
psc_waitq_waitrel(__unusedx struct psc_waitq *q,
    __unusedx psc_spinlock_t *k,
    __unusedx const struct timespec *reltime)
{
	psc_fatalx("wait will sleep forever, single threaded");
}

int
_psc_waitq_waitrelv(__unusedx struct psc_waitq *wq,
    __unusedx psc_spinlock_t *lk, __unusedx long s, __unusedx long ns)
{
	psc_fatalx("wait will sleep forever, single threaded");
}

int
psc_waitq_waitabs(__unusedx struct psc_waitq *q,
    __unusedx psc_spinlock_t *k,
    __unusedx const struct timespec *abstime)
{
	psc_fatalx("wait will sleep forever, single threaded");
}

void
psc_waitq_wakeone(__unusedx struct psc_waitq *q)
{
}

void
psc_waitq_wakeall(__unusedx struct psc_waitq *q)
{
}

#endif /* HAVE_LIBPTHREAD */
