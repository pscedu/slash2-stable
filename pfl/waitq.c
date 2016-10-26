/* $Id$ */
/*
 * %ISC_START_LICENSE%
 * ---------------------------------------------------------------------
 * Copyright 2015, Google, Inc.
 * Copyright (c) 2006-2015, Pittsburgh Supercomputing Center (PSC).
 * All rights reserved.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the
 * above copyright notice and this permission notice appear in all
 * copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL
 * WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS.  IN NO EVENT SHALL THE
 * AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
 * PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 * --------------------------------------------------------------------
 * %END_LICENSE%
 */

/*
 * Wait queue: suspend execution until notifiction from another thread.
 */

#include <errno.h>
#include <string.h>

#include "pfl/atomic.h"
#include "pfl/cdefs.h"
#include "pfl/lock.h"
#include "pfl/log.h"
#include "pfl/waitq.h"
#include "pfl/thread.h"

#ifdef HAVE_LIBPTHREAD

#include "pfl/str.h"
#include "pfl/time.h"
#include "pfl/pthrutil.h"

#include <pthread.h>

/*
 * Prepare a wait queue for use.
 * @q: the struct to be initialized.
 */
void
_psc_waitq_init(struct psc_waitq *q, const char *name, int flags)
{
	int rc;

	memset(q, 0, sizeof(*q));
	strlcpy(q->wq_name, name, MAX_WQ_NAME);
	_psc_mutex_init(&q->wq_mut, flags & PWQF_NOLOG ?
	    PMTXF_NOLOG : 0);
	rc = pthread_cond_init(&q->wq_cond, NULL);
	if (rc)
		psc_fatalx("pthread_cond_init: %s", strerror(rc));
}

/*
 * Release resources associated with a waitq.
 * @q: waitq to destroy.
 */
void
psc_waitq_destroy(struct psc_waitq *q)
{
	int rc;

	psc_assert(q->wq_nwaiters == 0);

	psc_mutex_destroy(&q->wq_mut);
	rc = pthread_cond_destroy(&q->wq_cond);
	if (rc)
		psc_fatalx("pthread_cond_destroy: %s", strerror(rc));
#if PFL_DEBUG > 0
	memset(q, 0, sizeof(*q));
#endif
}

/*
 * Wait until the time specified for the resource managed by wq_cond to
 * become available.
 * @q: the wait queue to wait on.
 * @lockp: optional lock needed to protect the list.
 * @abstime: time to wait till.
 * Note: returns ETIMEDOUT if the resource has not become available.
 * Note: this code could be merged with waitrel() but we try to make
 *	the timing calculations accurate.
 */
int
_psc_waitq_waitabs(struct psc_waitq *q, enum pfl_lockprim type,
    void *lockp, const struct timespec *abstime)
{
	int rc;
	struct psc_thread *thr;

	thr = pscthr_get();
	psc_mutex_lock(&q->wq_mut);
	q->wq_nwaiters++;

	PFL_LOCKPRIM_ULOCK(type, lockp);

	thr->pscthr_waitq = q->wq_name;
	if (abstime) {
		rc = pthread_cond_timedwait(&q->wq_cond,
		    &q->wq_mut.pm_mutex, abstime);

		/* 10/26/2016: Hit rc = 1 with RPC thread */
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

	thr->pscthr_waitq = NULL;
	q->wq_nwaiters--;
	psc_mutex_unlock(&q->wq_mut);

	return (rc);
}

int
_psc_waitq_waitrel(struct psc_waitq *q, enum pfl_lockprim type,
    void *lockp, long s, long ns)
{
	struct timespec reltime, abstime;

	if (ns || s) {
		PFL_GETTIMESPEC(&abstime);
		reltime.tv_sec = s;
		reltime.tv_nsec = ns;
		timespecadd(&abstime, &reltime, &abstime);
		return (_psc_waitq_waitabs(q, type, lockp, &abstime));
	}
	return (_psc_waitq_waitabs(q, type, lockp, NULL));
}

/*
 * Unblock one thread waiting on a wait queue.
 * @q: wait queue to operate on.
 */
void
psc_waitq_wakeone(struct psc_waitq *q)
{
	psc_mutex_lock(&q->wq_mut);
	if (q->wq_nwaiters) {
		int rc;

		rc = pthread_cond_signal(&q->wq_cond);
		if (rc)
			psc_fatalx("pthread_cond_signal: %s",
			    strerror(rc));
	}
	psc_mutex_unlock(&q->wq_mut);
}

/*
 * Wake everyone waiting on a wait queue.
 * @q: wait queue to operate on.
 */
void
psc_waitq_wakeall(struct psc_waitq *q)
{
	psc_mutex_lock(&q->wq_mut);
	if (q->wq_nwaiters) {
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
psc_waitq_init(struct psc_waitq *q, char *name)
{
	memset(q, 0, sizeof(*q));
	strlcpy(q->wq_name, name, MAX_WQ_NAME);
}

int
_psc_waitq_waitrel(__unusedx struct psc_waitq *q,
    __unusedx enum pfl_lockprim type, __unusedx void *lockp,
    __unusedx long s, __unusedx long ns)
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
