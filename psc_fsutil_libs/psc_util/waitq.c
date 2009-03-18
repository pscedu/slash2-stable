/* $Id$ */

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <errno.h>
#include <string.h>

#include "psc_util/assert.h"
#include "psc_util/atomic.h"
#include "psc_util/cdefs.h"
#include "psc_util/lock.h"
#include "psc_util/log.h"
#include "psc_util/pthrutil.h"
#include "psc_util/waitq.h"

#if HAVE_LIBPTHREAD

#include <pthread.h>

#ifndef timespecadd
#define timespecadd(tsp, usp, vsp)					\
	do {								\
		(vsp)->tv_sec = (tsp)->tv_sec + (usp)->tv_sec;		\
		(vsp)->tv_nsec = (tsp)->tv_nsec + (usp)->tv_nsec;	\
		if ((vsp)->tv_nsec >= 1000000000L) {			\
			(vsp)->tv_sec++;				\
			(vsp)->tv_nsec -= 1000000000L;			\
		}							\
	} while (0)
#endif

/**
 * psc_waitq_init - prepare the queue struct for use.
 * @q: the struct to be initialized.
 */
void
psc_waitq_init(struct psc_waitq *q)
{
	int rc;

	memset(q, 0, sizeof(*q));
	atomic_set(&q->wq_nwaitors, 0);

	psc_pthread_mutex_init(&q->wq_mut);
	rc = pthread_cond_init(&q->wq_cond, NULL);
	if (rc)
		psc_fatalx("pthread_cond_init: %s", strerror(rc));
}

/*
 * psc_waitq_waitabs - wait until the time specified for the
 *	resource managed by wq_cond to become available.
 * @q: the wait queue to wait on.
 * @k: optional lock needed to protect the list.
 * @reltime: amount of time to wait for.
 * Notes: returns ETIMEDOUT if the resource has not become available.
 */
int
psc_waitq_waitabs(struct psc_waitq *q, psc_spinlock_t *k,
    const struct timespec *abstime)
{
	int rc, rv;

	rc = pthread_mutex_lock(&q->wq_mut);
	if (rc)
		psc_fatalx("pthread_mutex_lock: %s", strerror(rc));

	if (k != NULL)
		freelock(k);

	atomic_inc(&q->wq_nwaitors);
	rv = pthread_cond_timedwait(&q->wq_cond, &q->wq_mut, abstime);
	if (rv && rv != ETIMEDOUT)
		psc_fatalx("pthread_cond_timedwait: %s", strerror(rv));
	atomic_dec(&q->wq_nwaitors);
	rc = pthread_mutex_unlock(&q->wq_mut);
	if (rc)
		psc_fatalx("pthread_mutex_unlock: %s", strerror(rc));
	return (rv);
}

/*
 * psc_waitq_waitrel - wait at most the amount of time specified for the
 *	resource managed by wq_cond to become available.
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
	int rc, rv;

	rc = pthread_mutex_lock(&q->wq_mut);
	if (rc)
		psc_fatalx("pthread_mutex_lock: %s", strerror(rc));

	if (k != NULL)
		freelock(k);

	atomic_inc(&q->wq_nwaitors);
	if (reltime) {
		if (clock_gettime(CLOCK_REALTIME, &abstime) == -1)
			psc_fatal("clock_gettime");
		timespecadd(&abstime, reltime, &abstime);
		rv = pthread_cond_timedwait(&q->wq_cond, &q->wq_mut, &abstime);
		if (rv && rv != ETIMEDOUT)
			psc_fatalx("pthread_cond_timedwait: %s", strerror(rv));
	} else {
		rv = pthread_cond_wait(&q->wq_cond, &q->wq_mut);
		if (rv)
			psc_fatalx("pthread_cond_wait: %s", strerror(rv));
	}
	atomic_dec(&q->wq_nwaitors);
	rc = pthread_mutex_unlock(&q->wq_mut);
	if (rc)
		psc_fatalx("pthread_mutex_unlock: %s", strerror(rc));
	return (rv);
}

/*
 * psc_waitq_wakeone - unblock one waiting thread.
 * @q: wait queue to operate on.
 */
void
psc_waitq_wakeone(struct psc_waitq *q)
{
	int rc;

	rc = pthread_mutex_lock(&q->wq_mut);
	if (rc)
		psc_fatalx("pthread_mutex_lock: %s", strerror(rc));
	rc = pthread_cond_signal(&q->wq_cond);
	if (rc)
		psc_fatalx("pthread_cond_signal: %s", strerror(rc));
	rc = pthread_mutex_unlock(&q->wq_mut);
	if (rc)
		psc_fatalx("pthread_mutex_unlock: %s", strerror(rc));
}

/*
 * psc_waitq_wakeall - wake everyone waiting on a wait queue.
 * @q: wait queue to operate on.
 */
void
psc_waitq_wakeall(struct psc_waitq *q)
{
	int rc;

	rc = pthread_mutex_lock(&q->wq_mut);
	if (rc)
		psc_fatalx("pthread_mutex_lock: %s", strerror(rc));
	rc = pthread_cond_broadcast(&q->wq_cond);
	if (rc)
		psc_fatalx("pthread_cond_broadcast: %s", strerror(rc));
	rc = pthread_mutex_unlock(&q->wq_mut);
	if (rc)
		psc_fatalx("pthread_mutex_unlock: %s", strerror(rc));
}

#else /* HAVE_LIBPTHREAD */

void
psc_waitq_init(struct psc_waitq *q)
{
	atomic_set(&q->wq_nwaitors, 0);
}

int
psc_waitq_waitrel(__unusedx struct psc_waitq *q,
    __unusedx psc_spinlock_t *k,
    __unusedx const struct timespec *reltime)
{
	psc_fatalx("waitqs not supported");
}

int
psc_waitq_waitabs(__unusedx struct psc_waitq *q,
    __unusedx psc_spinlock_t *k,
    __unusedx const struct timespec *abstime)
{
	psc_fatalx("waitqs not supported");
}

void
psc_waitq_wakeone(__unusedx struct psc_waitq *q)
{
	psc_fatalx("waitqs not supported");
}

void
psc_waitq_wakeall(__unusedx struct psc_waitq *q)
{
	psc_fatalx("waitqs not supported");
}

#endif /* HAVE_LIBPTHREAD */
