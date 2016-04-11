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

/*
 * Multiwait: for waiting on any of a number of conditions to become
 * available as opposed to a single pthread_cond_wait.
 */

#ifndef _PFL_MULTIWAIT_H_
#define _PFL_MULTIWAIT_H_

#include <sys/types.h>

#include <pthread.h>

#include "pfl/dynarray.h"
#include "pfl/pthrutil.h"

struct pfl_multiwait;
struct psc_vbitmap;

struct pfl_multiwaitcond {
	struct pfl_mutex		 mwc_mutex;
	pthread_cond_t			 mwc_cond;	/* for single waiters */
	struct psc_dynarray		 mwc_multiwaits;/* where cond is registered */
	struct pfl_multiwait		*mwc_winner;	/* which multiwait awoke first */
	const void			*mwc_data;	/* pointer to user data */
	int				 mwc_flags;
	char				 mwc_name[48];	/* should be on 8-byte boundary */
};

#define PMWCF_WAKEALL			(1 << 0)	/* wake all multiwaits, not just one */

#define MWCOND_INIT(data, name, flags)					\
	{ PSC_MUTEX_INIT, PTHREAD_COND_INITIALIZER,			\
	    DYNARRAY_INIT, NULL, (data), (flags), (name) }

struct pfl_multiwait {
	/*
	 * Multiwaits have mutexes not because threads may "share" them,
	 * explicitly, but because threads may modify some fields
	 * implicitly (e.g. on wakeups and condition removals).
	 */
	struct pfl_mutex		 mw_mutex;
	pthread_cond_t			 mw_cond;	/* master condition */
	struct pfl_multiwaitcond	*mw_waker;	/* which mwcond woke us */
	struct psc_dynarray		 mw_conds;	/* registered conditions */
	struct psc_vbitmap		*mw_condmask;	/* which conds can wake us */
	int				 mw_flags;
	char				 mw_name[32];	/* should be 8-byte boundary */
};

#define PMWF_CRITSECT			(1 << 0)	/* inside critical section */

#define	DLOG_MULTIWAIT(level, mw, fmt, ...)				\
	psclog((level), "%s@%p " fmt, (mw)->mw_name, (mw), ##__VA_ARGS__)

/*
 * Add a condition to a multiwait.
 * @mw: a multiwait.
 * @mwc: the condition to add.
 */
#define pfl_multiwait_addcond(mw, c)		_pfl_multiwait_addcond((mw), (c), 1)
#define pfl_multiwait_addcond_masked(mw, c, m)	_pfl_multiwait_addcond((mw), (c), (m))

/*
 * Wait for any of a number of conditions in a multiwait to become
 * available.
 * @mw: the multiwait whose registered conditions will be waited upon.
 * @data: pointer to user data filled in from the multiwaitcond.
 * @usec: # microseconds till timeout.
 */
#define pfl_multiwait_usecs(mw, p, us)		pfl_multiwait_rel((mw), (p), 0, (us) * 1000)
#define pfl_multiwait_msecs(mw, p, ms)		pfl_multiwait_rel((mw), (p), 0, (ms) * 1000 * 1000L)
#define pfl_multiwait_secs(mw, p, s)		pfl_multiwait_rel((mw), (p), (s), 0)
#define pfl_multiwait_relts(mw, p, ts)		pfl_multiwait_rel((mw), (p), (ts)->tv_sec, (ts)->tv_nsec)
#define pfl_multiwait(mw, p)			pfl_multiwait_rel((mw), (p), 0, 0)

int	_pfl_multiwait_addcond(struct pfl_multiwait *, struct pfl_multiwaitcond *, int);
void	 pfl_multiwait_entercritsect(struct pfl_multiwait *);
void	 pfl_multiwait_free(struct pfl_multiwait *);
int	 pfl_multiwait_hascond(struct pfl_multiwait *, struct pfl_multiwaitcond *);
void	 pfl_multiwait_init(struct pfl_multiwait *, const char *, ...);
void	 pfl_multiwait_leavecritsect(struct pfl_multiwait *);
void	 pfl_multiwait_setcondwakeable(struct pfl_multiwait *, const struct pfl_multiwaitcond *, int);
void	 pfl_multiwait_reset(struct pfl_multiwait *);
void	 pfl_multiwait_prconds(struct pfl_multiwait *);
int	 pfl_multiwait_rel(struct pfl_multiwait *, void *, int, int);

#define	pfl_multiwaitcond_wait(mwc, mx)		pfl_multiwaitcond_waitrel_ts((mwc), (mx), NULL)
#define	pfl_multiwaitcond_waitrel(mwc, mx, ts)	pfl_multiwaitcond_waitrel_ts((mwc), (mx), (ts))

void	 pfl_multiwaitcond_destroy(struct pfl_multiwaitcond *);
void	 pfl_multiwaitcond_init(struct pfl_multiwaitcond *, const void *, int, const char *, ...);
size_t	 pfl_multiwaitcond_nwaiters(struct pfl_multiwaitcond *);
void	 pfl_multiwaitcond_prmwaits(struct pfl_multiwaitcond *);
int	 pfl_multiwaitcond_waitrel_ts(struct pfl_multiwaitcond *, struct pfl_mutex *, const struct timespec *);
void	 pfl_multiwaitcond_wakeup(struct pfl_multiwaitcond *);

#endif /* _PFL_MULTIWAIT_H_ */
