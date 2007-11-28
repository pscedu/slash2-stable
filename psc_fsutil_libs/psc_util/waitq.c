/* $Id: zestWaitq.c 2137 2007-11-05 18:41:27Z yanovich $ */

#include <errno.h>

#include "psc_util/assert.h"
#include "psc_util/lock.h"
#include "psc_util/psc_util/log.h"
#include "psc_util/waitq.h"
#include "psc_util/psc_util/cdefs.h"

#if HAVE_LIBPTHREAD

#include <pthread.h>

/**
 * psc_waitq_init - prepare the queue struct for use.
 * @q: the struct to be initialized.
 * Notes:  the initializer macros must be cast this way or the file will not compile.
 */
void psc_waitq_init(psc_waitq_t *q)
{
	int rc;

	rc = pthread_mutex_init(&q->psc_waitq_mut, NULL);
	/* XXX cond_attr is uninitialized */
	rc |= pthread_cond_init(&q->psc_waitq_cond, &q->psc_waitq_cond_attr);

	psc_assert(rc == 0);
}

/**
 * psc_waitq_wait - wait until resource managed by psc_waitq_cond is available.
 * @q: pointer to the queue struct which holds the necessary pthreads elements.
 * @k: pointer to spinlock needed to protect the list
 * Notes: hopefully the freelock() call is not a source of deadlock.  It's here
 * to prevent a race between a process trying to wait and a process that free's
 * the resource also guarded by 'k'.  If this were to occur then the waiting
 * process would never wake up.  Using this method guarantees a free won't
 * happen before this process is put to sleep.
 */
void psc_waitq_wait(psc_waitq_t *q, psc_spinlock_t *k)
{
	int rc;

	rc = pthread_mutex_lock(&q->psc_waitq_mut);

	if (k != NULL)
		freelock(k);

	rc |= pthread_cond_wait(&q->psc_waitq_cond, &q->psc_waitq_mut);
	rc |= pthread_mutex_unlock(&q->psc_waitq_mut);

	psc_assert(rc == 0);
}

/*
 * psc_waitq_timedwait - wait a maximum amount of time for the resource managed by
 *	psc_waitq_cond to become available.
 * @q: the wait queue.
 * @k: lock needed to protect the list.
 * @abstime: maximum amount of time to wait before giving up.
 * Notes: returns ETIMEDOUT if the resource has not become available.
 */
int psc_waitq_timedwait(psc_waitq_t *q, psc_spinlock_t *k,
		     const struct timespec *abstime)
{
	int rc;

	//psc_trace("abstime.sec %lu abstime.usec %ld", 
	//       abstime->tv_sec, abstime->tv_nsec);

	rc = pthread_mutex_lock(&q->psc_waitq_mut);

	if (k != NULL)
		freelock(k);

	rc |= pthread_cond_timedwait(&q->psc_waitq_cond, &q->psc_waitq_mut, abstime);
	rc |= pthread_mutex_unlock(&q->psc_waitq_mut);

	psc_assert(rc == 0 || rc == ETIMEDOUT);
	return rc;
}

/*
 * psc_waitq_wakeup - unblock one waiting thread
 * @q: pointer to the wait queue struct.
 */
void psc_waitq_wakeup(psc_waitq_t *q)
{
	int rc;

	rc =  pthread_mutex_lock(&q->psc_waitq_mut);
	rc |= pthread_cond_signal(&q->psc_waitq_cond);
	rc |= pthread_mutex_unlock(&q->psc_waitq_mut);

	psc_assert(rc == 0);
}

/*
 * psc_waitq_wakeall - a method for implementing a psuedo-barrier.
 * @q: pointer to the wait queue struct.
 */
void psc_waitq_wakeall(psc_waitq_t *q)
{
	int rc = 0;
	rc =  pthread_mutex_lock(&q->psc_waitq_mut);
	rc |= pthread_cond_broadcast(&q->psc_waitq_cond);
	rc |= pthread_mutex_unlock(&q->psc_waitq_mut);

	psc_assert(rc == 0);
}

#else /* HAVE_LIBPTHREAD */

void
psc_waitq_init(__unusedx psc_waitq_t *q)
{
  //psc_fatalx("waitqs not supported");
  return;
}

void
psc_waitq_wait(__unusedx psc_waitq_t *q, __unusedx psc_spinlock_t *k)
{
  //psc_fatalx("waitqs not supported");
  return;
}

int
psc_waitq_timedwait(__unusedx psc_waitq_t *q, __unusedx psc_spinlock_t *k,
__unusedx const struct timespec *abstime)
{
  //psc_fatalx("waitqs not supported");
  return;
}

void
psc_waitq_wakeup(__unusedx psc_waitq_t *q)
{
  //psc_fatalx("waitqs not supported");
	return;
}

void
psc_waitq_wakeall(__unusedx psc_waitq_t *q)
{
  //psc_fatalx("waitqs not supported");
	return;
}

#endif /* HAVE_LIBPTHREAD */
