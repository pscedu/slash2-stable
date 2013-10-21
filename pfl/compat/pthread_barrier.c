/* $Id$ */

/*
 * barrier.c
 *
 * This file implements the "barrier" synchronization construct.
 *
 * A barrier causes threads to wait until a set of threads has all
 * "reached" the barrier.  The number of threads required is set when the
 * barrier is initialized, and cannot be changed except by
 * reinitializing.
 *
 * The barrier_init() and barrier_destroy() functions, respectively,
 * allow you to initialize and destroy the barrier.
 *
 * The barrier_wait() function allows a thread to wait for a barrier to
 * be completed.  One thread (the one that happens to arrive last) will
 * return from barrier_wait() with the status -1 on success -- others
 * will return with 0.  The special status makes it easy for the calling
 * code to cause one thread to do something in a serial region before
 * entering another parallel section of code.
 */

#include <errno.h>
#include <pthread.h>
#include <stdio.h>

#include "pfl/cdefs.h"
#include "pfl/compat/pthread_barrier.h"
#include "pfl/log.h"
#include "pfl/pthrutil.h"

/**
 * pthread_barrier_init - Initialize a barrier for use.
 */
int
pthread_barrier_init(pthread_barrier_t *barrier, __unusedx
    const pthread_barrierattr_t *attr, unsigned count)
{
	int status;

	barrier->threshold = barrier->counter = count;
	barrier->cycle = 0;
	psc_mutex_init(&barrier->mutex);
	status = pthread_cond_init(&barrier->cv, NULL);
	if (status != 0) {
		psc_mutex_destroy(&barrier->mutex);
		return (status);
	}
	barrier->valid = BARRIER_VALID;
	return (0);
}

/**
 * pthread_barrier_destroy - Destroy a barrier when done using it.
 */
int
pthread_barrier_destroy(pthread_barrier_t *barrier)
{
	int rc;

	if (barrier->valid != BARRIER_VALID)
		return (EINVAL);

	psc_mutex_lock(&barrier->mutex);

	/*
	 * Check whether any threads are known to be waiting; report
	 * "BUSY" if so.
	 */
	if (barrier->counter != barrier->threshold) {
		psc_mutex_unlock(&barrier->mutex);
		return (EBUSY);
	}

	barrier->valid = 0;
	psc_mutex_unlock(&barrier->mutex);

	/*
	 * If unable to destroy either 1003.1c synchronization
	 * object, return the error status.
	 */
	psc_mutex_destroy(&barrier->mutex);
	rc = pthread_cond_destroy(&barrier->cv);
	return (rc);
}

/**
 * pthread_barrier_wait - Wait for all members of a barrier to reach the
 *	barrier.  When the count (of remaining members) reaches 0,
 *	broadcast to wake all threads waiting.
 */
int
pthread_barrier_wait(pthread_barrier_t *barrier)
{
	int status = 0, cancel, tmp;
	unsigned long cycle;

	if (barrier->valid != BARRIER_VALID)
		return (EINVAL);

	psc_mutex_lock(&barrier->mutex);

	cycle = barrier->cycle;   /* Remember which cycle we're on */

	if (--barrier->counter == 0) {
		barrier->cycle++;
		barrier->counter = barrier->threshold;
		status = pthread_cond_broadcast(&barrier->cv);

		/*
		 * The last thread into the barrier will return status
		 * -1 rather than 0, so that it can be used to perform
		 * some special serial code following the barrier.
		 */
		if (status == 0)
			status = -1;
	} else {
		/*
		 * Wait with cancellation disabled, because barrier_wait
		 * should not be a cancellation point.
		 */
		pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, &cancel);

		/*
		 * Wait until the barrier's cycle changes, which means
		 * that it has been broadcast, and we don't want to wait
		 * anymore.
		 */
		while (cycle == barrier->cycle) {
			status = pthread_cond_wait(&barrier->cv,
			    &barrier->mutex.pm_mutex);
			if (status != 0)
				break;
		}

		pthread_setcancelstate(cancel, &tmp);
	}

	/*
	 * Ignore an error in unlocking. It shouldn't happen, and
	 * reporting it here would be misleading -- the barrier wait
	 * completed, after all, whereas returning, for example,
	 * EINVAL would imply the wait had failed. The next attempt
	 * to use the barrier *will* return an error, or hang, due
	 * to whatever happened to the mutex.
	 */
	psc_mutex_unlock(&barrier->mutex);
	return status;          /* error, -1 for waker, or 0 */
}
