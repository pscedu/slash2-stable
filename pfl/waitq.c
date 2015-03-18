/* $Id$ */
/*
 * %PSC_START_COPYRIGHT%
 * -----------------------------------------------------------------------------
 * Copyright (c) 2006-2015, Pittsburgh Supercomputing Center (PSC).
 *
 * Permission to use, copy, modify, and distribute this software
 * for any purpose with or without fee is hereby granted, provided
 * that the above copyright notice and this permission notice
 * appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL
 * WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS.  IN NO EVENT SHALL
 * THE AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
 * LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT,
 * NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * Pittsburgh Supercomputing Center	phone: 412.268.4960  fax: 412.268.5832
 * 300 S. Craig Street			e-mail: remarks@psc.edu
 * Pittsburgh, PA 15213			web: http://www.psc.edu/
 * -----------------------------------------------------------------------------
 * %PSC_END_COPYRIGHT%
 */

#include <errno.h>
#include <string.h>

#include "pfl/atomic.h"
#include "pfl/cdefs.h"
#include "pfl/lock.h"
#include "pfl/log.h"
#include "pfl/waitq.h"

#ifdef HAVE_LIBPTHREAD

#include "pfl/time.h"
#include "pfl/pthrutil.h"

#include <pthread.h>

/**
 * psc_waitq_init - Prepare a wait queue for use.
 * @q: the struct to be initialized.
 */
void
_psc_waitq_init(struct psc_waitq *q, int flags)
{
	int rc;

	memset(q, 0, sizeof(*q));
	psc_atomic32_set(&q->wq_nwaiters, 0);

	_psc_mutex_init(&q->wq_mut, flags & PWQF_NOLOG ?
	    PMTXF_NOLOG : 0);
	rc = pthread_cond_init(&q->wq_cond, NULL);
	if (rc)
		psc_fatalx("pthread_cond_init: %s", strerror(rc));
}

/**
 * psc_waitq_destroy - Release resources associated with a waitq.
 * @q: waitq to destroy.
 */
void
psc_waitq_destroy(struct psc_waitq *q)
{
	int rc;

	psc_assert(psc_atomic32_read(&q->wq_nwaiters) == 0);

	psc_mutex_destroy(&q->wq_mut);
	rc = pthread_cond_destroy(&q->wq_cond);
	if (rc)
		psc_fatalx("pthread_cond_destroy: %s", strerror(rc));
#if PFL_DEBUG > 0
	memset(q, 0, sizeof(*q));
#endif
}

/**
 * psc_waitq_waitabs - Wait until the time specified for the
 *	resource managed by wq_cond to become available.
 * @q: the wait queue to wait on.
 * @k: optional lock needed to protect the list.
 * @abstime: time to wait till.
 * Note: returns ETIMEDOUT if the resource has not become available.
 * Note: this code could be merged with waitrel() but we try to make
 *	the timing calculations accurate.
 */
int
_psc_waitq_waitabs(struct psc_waitq *q, int flags, void *p,
    const struct timespec *abstime)
{
	int rc;

	psc_mutex_lock(&q->wq_mut);
	psc_atomic32_inc(&q->wq_nwaiters);

	if (p) {
		if (flags & PFL_WAITQWF_SPIN)
			freelock((struct psc_spinlock *)p);
		if (flags & PFL_WAITQWF_MUTEX)
			psc_mutex_unlock(p);
		if (flags & PFL_WAITQWF_RWLOCK)
			psc_rwlock_unlock(p);
	}

	if (abstime) {
		rc = pthread_cond_timedwait(&q->wq_cond,
		    &q->wq_mut.pm_mutex, abstime);
		if (rc && rc != ETIMEDOUT)
			psc_fatalx("pthread_cond_timedwait: %s",
			    strerror(rc));
	} else {
		rc = pthread_cond_wait(&q->wq_cond,
		    &q->wq_mut.pm_mutex);
		if (rc)
			psc_fatalx("pthread_cond_wait: %s",
			    strerror(rc));
	}

	psc_mutex_unlock(&q->wq_mut);
	/*
	 * BZ#91: decrease waiters after releasing the lock to guarantee
	 * wq_mut remains intact.
	 */
	psc_atomic32_dec(&q->wq_nwaiters);

	return (rc);
}

int
_psc_waitq_waitrel(struct psc_waitq *q, int flags, void *p, long s,
    long ns)
{
	struct timespec reltime, abstime;

	if (ns || s) {
		PFL_GETTIMESPEC(&abstime);
		reltime.tv_sec = s;
		reltime.tv_nsec = ns;
		timespecadd(&abstime, &reltime, &abstime);
		return (_psc_waitq_waitabs(q, flags, p, &abstime));
	}
	return (_psc_waitq_waitabs(q, flags, p, NULL));
}

/**
 * psc_waitq_wakeone - Unblock one thread waiting on a wait queue.
 * @q: wait queue to operate on.
 */
void
psc_waitq_wakeone(struct psc_waitq *q)
{
	psc_mutex_lock(&q->wq_mut);
	if (psc_atomic32_read(&q->wq_nwaiters)) {
		int rc;

		rc = pthread_cond_signal(&q->wq_cond);
		if (rc)
			psc_fatalx("pthread_cond_signal: %s",
			    strerror(rc));
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
	if (psc_atomic32_read(&q->wq_nwaiters)) {
		int rc;

		rc = pthread_cond_broadcast(&q->wq_cond);
		if (rc)
			psc_fatalx("pthread_cond_broadcast: %s",
			    strerror(rc));
	}
	psc_mutex_unlock(&q->wq_mut);
}

#else /* HAVE_LIBPTHREAD */

void
psc_waitq_init(struct psc_waitq *q)
{
	memset(q, 0, sizeof(*q));
	psc_atomic32_set(&q->wq_nwaiters, 0);
}

int
_psc_waitq_waitrel(__unusedx struct psc_waitq *wq, __unusedx int flags,
    __unusedx void *p, __unusedx long s, __unusedx long ns)
{
	psc_fatalx("wait will sleep forever, single threaded");
}

int
_psc_waitq_waitabs(__unusedx struct psc_waitq *q, __unusedx int flags,
    __unusedx void *p, __unusedx const struct timespec *abstime)
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

void
psc_waitq_destroy(__unusedx struct psc_waitq *q)
{
}

#endif /* HAVE_LIBPTHREAD */
