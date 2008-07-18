/* $Id$ */

#include <errno.h>

#include "psc_util/assert.h"
#include "psc_util/atomic.h"
#include "psc_util/cdefs.h"
#include "psc_util/lock.h"
#include "psc_util/log.h"
#include "psc_util/waitq.h"

#if HAVE_LIBPTHREAD

#include <pthread.h>

/**
 * psc_waitq_init - prepare the queue struct for use.
 * @q: the struct to be initialized.
 */
void
psc_waitq_init(psc_waitq_t *q)
{
	int rc;

	atomic_set(&q->wq_nwaitors, 0);
	rc = pthread_mutex_init(&q->wq_mut, NULL);
	rc |= pthread_cond_init(&q->wq_cond, NULL);
	psc_assert(rc == 0);
}

/**
 * psc_waitq_wait - wait until resource managed by wq_cond is available.
 * @q: wait queue.
 * @k: optional lock needed to protect the list.
 * Notes: hopefully the freelock() call is not a source of deadlock.  It's here
 * to prevent a race between a process trying to wait and a process that free's
 * the resource also guarded by 'k'.  If this were to occur then the waiting
 * process would never wake up.  Using this method guarantees a free won't
 * happen before this process is put to sleep.
 */
void
psc_waitq_wait(psc_waitq_t *q, psc_spinlock_t *k)
{
	int rc;

	rc = pthread_mutex_lock(&q->wq_mut);
	atomic_inc(&q->wq_nwaitors);

	if (k != NULL)
		freelock(k);

	rc |= pthread_cond_wait(&q->wq_cond, &q->wq_mut);
//	if (k)
//		spinlock(k);
	atomic_dec(&q->wq_nwaitors);
	rc |= pthread_mutex_unlock(&q->wq_mut);

	psc_assert(rc == 0);
}

/*
 * psc_waitq_timedwait - wait a maximum amount of time for the resource managed by
 *	wq_cond to become available.
 * @q: the wait queue.
 * @k: optional lock needed to protect the list.
 * @abstime: maximum amount of time to wait before giving up.
 * Notes: returns ETIMEDOUT if the resource has not become available.
 */
int
psc_waitq_timedwait(psc_waitq_t *q, psc_spinlock_t *k,
		     const struct timespec *abstime)
{
	int rc;

	rc = pthread_mutex_lock(&q->wq_mut);
	atomic_inc(&q->wq_nwaitors);

	if (k != NULL)
		freelock(k);

	rc |= pthread_cond_timedwait(&q->wq_cond, &q->wq_mut, abstime);
//	if (k)
//		spinlock(k);
	atomic_dec(&q->wq_nwaitors);
	rc |= pthread_mutex_unlock(&q->wq_mut);

	psc_assert(rc == 0 || rc == ETIMEDOUT);
	return rc;
}

/*
 * psc_waitq_wakeone - unblock one waiting thread.
 * @q: pointer to the wait queue struct.
 */
void
psc_waitq_wakeone(psc_waitq_t *q)
{
	int rc;

	rc =  pthread_mutex_lock(&q->wq_mut);
	rc |= pthread_cond_signal(&q->wq_cond);
	rc |= pthread_mutex_unlock(&q->wq_mut);

	psc_assert(rc == 0);
}

/*
 * psc_waitq_wakeall - a method for implementing a pseudo-barrier.
 * @q: pointer to the wait queue struct.
 */
void
psc_waitq_wakeall(psc_waitq_t *q)
{
	int rc;

	rc =  pthread_mutex_lock(&q->wq_mut);
	rc |= pthread_cond_broadcast(&q->wq_cond);
	rc |= pthread_mutex_unlock(&q->wq_mut);

	psc_assert(rc == 0);
}

#else /* HAVE_LIBPTHREAD */

void
psc_waitq_init(__unusedx psc_waitq_t *q)
{
	psc_fatalx("waitqs not supported");
}

void
psc_waitq_wait(__unusedx psc_waitq_t *q, __unusedx psc_spinlock_t *k)
{
	psc_fatalx("waitqs not supported");
}

int
psc_waitq_timedwait(__unusedx psc_waitq_t *q, __unusedx psc_spinlock_t *k,
    __unusedx const struct timespec *abstime)
{
	psc_fatalx("waitqs not supported");
}

void
psc_waitq_wakeone(__unusedx psc_waitq_t *q)
{
	psc_fatalx("waitqs not supported");
}

void
psc_waitq_wakeall(__unusedx psc_waitq_t *q)
{
	psc_fatalx("waitqs not supported");
}

#endif /* HAVE_LIBPTHREAD */
