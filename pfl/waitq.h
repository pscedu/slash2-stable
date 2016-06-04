/* $Id$ */
/*
 * %ISC_START_LICENSE%
 * ---------------------------------------------------------------------
 * Copyright 2015-2016, Google, Inc.
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

#ifndef _PFL_WAITQ_H_
#define _PFL_WAITQ_H_

#include <time.h>

#include "pfl/lock.h"
#include "pfl/pthrutil.h"

struct pfl_mutex;

#ifdef HAVE_LIBPTHREAD

# include <pthread.h>

#define	MAX_WQ_NAME		32

struct psc_waitq {
	struct pfl_mutex	wq_mut;
	pthread_cond_t		wq_cond;
	char			wq_name[MAX_WQ_NAME];
	int			wq_nwaiters;
	int			wq_flags;
};

#define PWQF_NOLOG		(1 << 0)

# define PSC_WAITQ_INIT(name)	{ PSC_MUTEX_INIT, PTHREAD_COND_INITIALIZER, (name), 0, 0 }

#else /* HAVE_LIBPTHREAD */

struct psc_waitq {
	int			wq_nwaiters;
};

# define PSC_WAITQ_INIT	{ 0 }

#endif

/*
 * Wait until resource managed by wq_cond is available.
 * @wq: wait queue.
 * @lk: optional lock to prevent race condition in waiting.
 */
#define psc_waitq_wait(wq, lk)		 _psc_waitq_waitabs((wq), PFL_LOCKPRIMT_SPIN, (lk), NULL)
#define psc_waitq_waitf(wq, fl, p)	 _psc_waitq_waitabs((wq), (fl), (p), NULL)

/*
 * Wait at most the amount of time specified (relative to calling time)
 * for the resource managed by wq_cond to become available.
 * @wq: the wait queue to wait on.
 * @lk: optional lock needed to protect the list (optional).
 * @s: number of seconds to wait for (optional).
 * @ns: number of nanoseconds to wait for (optional).
 * Returns: ETIMEDOUT if the resource did not become available if
 * @s or @ns was specififed.
 */
#define psc_waitq_waitrel(wq, lk, s, ns) _psc_waitq_waitrel((wq), PFL_LOCKPRIMT_SPIN, (lk), (s), (ns))

#define psc_waitq_waitrel_s(wq, lk, s)	 psc_waitq_waitrel((wq), (lk), (s), 0L)
#define psc_waitq_waitrel_us(wq, lk, us) psc_waitq_waitrel((wq), (lk), 0L, (us) * 1000L)
#define psc_waitq_waitrel_ms(wq, lk, ms) psc_waitq_waitrel((wq), (lk), 0L, (ms) * 1000L * 1000L)
#define psc_waitq_waitrel_tv(wq, lk, tv) psc_waitq_waitrel((wq), (lk), (tv)->tv_sec, (tv)->tv_usec * 1000L)
#define psc_waitq_waitrel_ts(wq, lk, tv) psc_waitq_waitrel((wq), (lk), (tv)->tv_sec, (tv)->tv_nsec)

#define psc_waitq_waitrelf_us(wq, fl, p, us)	\
					_psc_waitq_waitrel((wq), (fl), (p), 0L, (us) * 1000L)

#define psc_waitq_waitabs(wq, lk, ts)	_psc_waitq_waitabs((wq), PFL_LOCKPRIMT_SPIN, (lk), (ts))

/*
 * Determine number of threads waiting on a waitq.
 * @wq: wait queue.
 */
#define psc_waitq_nwaiters(wq)		(wq)->wq_nwaiters

#define psc_waitq_init(wq, name)	_psc_waitq_init((wq), (name), 0)
#define psc_waitq_init_nolog(wq, name)	_psc_waitq_init((wq), (name), PWQF_NOLOG)

void	_psc_waitq_init(struct psc_waitq *, const char *, int);
void	 psc_waitq_destroy(struct psc_waitq *);
void	 psc_waitq_wakeone(struct psc_waitq *);
void	 psc_waitq_wakeall(struct psc_waitq *);
int	_psc_waitq_waitrel(struct psc_waitq *, enum pfl_lockprim, void *, long, long);
int	_psc_waitq_waitabs(struct psc_waitq *, enum pfl_lockprim, void *, const struct timespec *);

#endif /* _PFL_WAITQ_H_ */
