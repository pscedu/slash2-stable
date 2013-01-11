/* $Id$ */
/*
 * %PSC_START_COPYRIGHT%
 * -----------------------------------------------------------------------------
 * Copyright (c) 2006-2012, Pittsburgh Supercomputing Center (PSC).
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

#ifndef _PFL_WAITQ_H_
#define _PFL_WAITQ_H_

#include <time.h>

#include "psc_util/atomic.h"
#include "psc_util/lock.h"
#include "psc_util/pthrutil.h"

struct pfl_mutex;

#if HAVE_LIBPTHREAD

# include <pthread.h>

struct psc_waitq {
	struct pfl_mutex	wq_mut;
	pthread_cond_t		wq_cond;
	atomic_t		wq_nwaiters;
};

# define PSC_WAITQ_INIT	{ PSC_MUTEX_INIT,				\
			  PTHREAD_COND_INITIALIZER, ATOMIC_INIT(0) }

#else /* HAVE_LIBPTHREAD */

struct psc_waitq {
	atomic_t		wq_nwaiters;
};

# define PSC_WAITQ_INIT	{ ATOMIC_INIT(0) }

#endif

/**
 * psc_waitq_wait - Wait until resource managed by wq_cond is available.
 * @wq: wait queue.
 * @lk: optional lock to prevent race condition in waiting.
 */
#define psc_waitq_wait(wq, lk)		 _psc_waitq_waitrel((wq), (lk), NULL, NULL)
#define psc_waitq_wait_mutex(wq, mx)	 _psc_waitq_waitrel((wq), NULL, (mx), NULL)

#define psc_waitq_waitrel_s(wq, lk, n)	 _psc_waitq_waitrelv((wq), (lk), (n), 0L)
#define psc_waitq_waitrel_us(wq, lk, n)	 _psc_waitq_waitrelv((wq), (lk), 0L, (n) * 1000L)
#define psc_waitq_waitrel_ms(wq, lk, n)	 _psc_waitq_waitrelv((wq), (lk), 0L, (n) * 1000L * 1000L)
#define psc_waitq_waitrel_tv(wq, lk, tv) _psc_waitq_waitrelv((wq), (lk), (tv)->tv_sec, (tv)->tv_usec * 1000L)
#define psc_waitq_waitrel(wq, lk, tv)	 _psc_waitq_waitrel((wq), (lk), NULL, (tv))

/**
 * psc_waitq_nwaiters - Determine number of threads waiting on a waitq.
 * @wq: wait queue.
 */
#define psc_waitq_nwaiters(wq)		atomic_read(&(wq)->wq_nwaiters)

#define psc_waitq_timedwait(wq, lk, ts)	psc_waitq_waitabs((wq), (lk), (ts))

void	 psc_waitq_init(struct psc_waitq *);
void	 psc_waitq_destroy(struct psc_waitq *);
void	 psc_waitq_wakeone(struct psc_waitq *);
void	 psc_waitq_wakeall(struct psc_waitq *);
int	_psc_waitq_waitrel(struct psc_waitq *, psc_spinlock_t *,
	    struct pfl_mutex *, const struct timespec *);
int	_psc_waitq_waitrelv(struct psc_waitq *, psc_spinlock_t *,
	    long, long);
int	 psc_waitq_waitabs(struct psc_waitq *, psc_spinlock_t *,
	    const struct timespec *);

#endif /* _PFL_WAITQ_H_ */
