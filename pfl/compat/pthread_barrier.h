/* $Id$ */

/*
 * barrier.h
 *
 * This header file describes the "barrier" synchronization
 * construct. The type barrier_t describes the full state of the
 * barrier including the POSIX 1003.1c synchronization objects
 * necessary.
 *
 * A barrier causes threads to wait until a set of threads has
 * all "reached" the barrier. The number of threads required is
 * set when the barrier is initialized, and cannot be changed
 * except by reinitializing.
 */

#ifndef _PFL_COMPAT_PTHREAD_BARRIER_H_
#define _PFL_COMPAT_PTHREAD_BARRIER_H_

#include <pthread.h>

#include "pfl/pthrutil.h"

typedef struct {
	struct pfl_mutex	 mutex;			/* Control access to barrier */
	pthread_cond_t		 cv;			/* wait for barrier */
	int			 valid;			/* set when valid */
	int			 threshold;		/* number of threads required */
	int			 counter;		/* current number of threads */
	unsigned long		 cycle;			/* count cycles */
} pthread_barrier_t;

typedef struct {
} pthread_barrierattr_t;

#define BARRIER_VALID   0xdbcafe

#define BARRIER_INITIALIZER(cnt)				\
    { PTHREAD_MUTEX_INITIALIZER, PTHREAD_COND_INITIALIZER,	\
      BARRIER_VALID, cnt, cnt, 0 }

int pthread_barrier_init(pthread_barrier_t *, const pthread_barrierattr_t *, unsigned);
int pthread_barrier_destroy(pthread_barrier_t *);
int pthread_barrier_wait(pthread_barrier_t *);

#endif /* _PFL_COMPAT_PTHREAD_BARRIER_H_ */
