/* $Id$ */
/*
 * %PSC_START_COPYRIGHT%
 * -----------------------------------------------------------------------------
 * Copyright (c) 2006-2014, Pittsburgh Supercomputing Center (PSC).
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

#ifndef _PFL_WAITQ_H_
#define _PFL_WAITQ_H_

#include <time.h>

#include "pfl/atomic.h"
#include "pfl/lock.h"
#include "pfl/pthrutil.h"

struct pfl_mutex;

#ifdef HAVE_LIBPTHREAD

# include <pthread.h>

struct psc_waitq {
	struct pfl_mutex	wq_mut;
	pthread_cond_t		wq_cond;
	atomic_t		wq_nwaiters;
	int			wq_flags;
};

#define PWQF_NOLOG		(1 << 0)

# define PSC_WAITQ_INIT	{ PSC_MUTEX_INIT, PTHREAD_COND_INITIALIZER,	\
			  ATOMIC_INIT(0), 0 }

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
#define psc_waitq_wait_mutex(wq, mtx)	 _psc_waitq_waitrel((wq), NULL, (mtx), NULL)

#define psc_waitq_waitrel_s(wq, lk, n)	 _psc_waitq_waitrelv((wq), (lk), (n), 0L)
#define psc_waitq_waitrel_us(wq, lk, n)	 _psc_waitq_waitrelv((wq), (lk), 0L, (n) * 1000L)
#define psc_waitq_waitrel_ms(wq, lk, n)	 _psc_waitq_waitrelv((wq), (lk), 0L, (n) * 1000L * 1000L)
#define psc_waitq_waitrel_tv(wq, lk, tv) _psc_waitq_waitrelv((wq), (lk), (tv)->tv_sec, (tv)->tv_usec * 1000L)
#define psc_waitq_waitrel(wq, lk, tv)	 _psc_waitq_waitrel((wq), (lk), NULL, (tv))

#define psc_waitq_waitabs(wq, lk, ts)	 _psc_waitq_waitabs((wq), (lk), NULL, (ts))
#define psc_waitq_waitabs_mutex(wq, mtx, ts) \
					 _psc_waitq_waitabs((wq), NULL, (mtx), (ts))

/**
 * psc_waitq_nwaiters - Determine number of threads waiting on a waitq.
 * @wq: wait queue.
 */
#define psc_waitq_nwaiters(wq)		atomic_read(&(wq)->wq_nwaiters)

#define psc_waitq_init(wq)		_psc_waitq_init(wq, 0)
#define psc_waitq_init_nolog(wq)	_psc_waitq_init(wq, PWQF_NOLOG)

void	_psc_waitq_init(struct psc_waitq *, int);
void	 psc_waitq_destroy(struct psc_waitq *);
void	 psc_waitq_wakeone(struct psc_waitq *);
void	 psc_waitq_wakeall(struct psc_waitq *);
int	_psc_waitq_waitrel(struct psc_waitq *, psc_spinlock_t *,
	    struct pfl_mutex *, const struct timespec *);
int	_psc_waitq_waitrelv(struct psc_waitq *, psc_spinlock_t *,
	    long, long);
int	_psc_waitq_waitabs(struct psc_waitq *, psc_spinlock_t *,
	    struct pfl_mutex *, const struct timespec *);

#endif /* _PFL_WAITQ_H_ */
