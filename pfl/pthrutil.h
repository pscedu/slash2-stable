/* $Id$ */
/*
 * %PSC_START_COPYRIGHT%
 * -----------------------------------------------------------------------------
 * Copyright (c) 2006-2015, Pittsburgh Supercomputing Center (PSC).
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

#ifndef _PFL_PTHRUTIL_H_
#define _PFL_PTHRUTIL_H_

#include <pthread.h>

#include "pfl/dynarray.h"
#include "pfl/list.h"
#include "pfl/lock.h"
#include "pfl/pfl.h"

#ifdef PTHREAD_MUTEX_ERRORCHECK_INITIALIZER
# define PSC_MUTEX_INIT			{ PTHREAD_MUTEX_ERRORCHECK_INITIALIZER, 0, 0 }
#elif defined(PTHREAD_MUTEX_ERRORCHECK_INITIALIZER_NP)
# define PSC_MUTEX_INIT			{ PTHREAD_MUTEX_ERRORCHECK_INITIALIZER_NP, 0, 0 }
#else
# define PSC_MUTEX_INIT			{ PTHREAD_MUTEX_INITIALIZER, 0, 0 }
#endif

#define psc_mutex_ensure_locked(m)	_psc_mutex_ensure_locked(PFL_CALLERINFO(), (m))
#define psc_mutex_lock(m)		_psc_mutex_lock(PFL_CALLERINFO(), (m))
#define psc_mutex_reqlock(m)		_psc_mutex_reqlock(PFL_CALLERINFO(), (m))
#define psc_mutex_trylock(m)		_psc_mutex_trylock(PFL_CALLERINFO(), (m))
#define psc_mutex_tryreqlock(m, lk)	_psc_mutex_tryreqlock(PFL_CALLERINFO(), (m), (lk))
#define psc_mutex_unlock(m)		_psc_mutex_unlock(PFL_CALLERINFO(), (m))
#define psc_mutex_ureqlock(m, lk)	_psc_mutex_ureqlock(PFL_CALLERINFO(), (m), (lk))

struct pfl_mutex {
	pthread_mutex_t		pm_mutex;
	pthread_t		pm_owner;
	int			pm_flags;
};

#define PMTXF_DEBUG		(1 << 0)
#define PMTXF_NOLOG		(1 << 1)

#define psc_mutex_init(m)		_psc_mutex_init((m), 0)
#define psc_mutex_init_debug(m)		_psc_mutex_init((m), PMTXF_DEBUG)
#define psc_mutex_init_nolog(m)		_psc_mutex_init((m), PMTXF_NOLOG)

void	_psc_mutex_ensure_locked(const struct pfl_callerinfo *, struct pfl_mutex *);
int	 psc_mutex_haslock(struct pfl_mutex *);
void	_psc_mutex_init(struct pfl_mutex *, int);
void	 psc_mutex_destroy(struct pfl_mutex *);
void	_psc_mutex_lock(const struct pfl_callerinfo *, struct pfl_mutex *);
int	_psc_mutex_reqlock(const struct pfl_callerinfo *, struct pfl_mutex *);
int	_psc_mutex_trylock(const struct pfl_callerinfo *, struct pfl_mutex *);
int	_psc_mutex_tryreqlock(const struct pfl_callerinfo *, struct pfl_mutex *, int *);
void	_psc_mutex_unlock(const struct pfl_callerinfo *, struct pfl_mutex *);
void	_psc_mutex_ureqlock(const struct pfl_callerinfo *, struct pfl_mutex *, int);

struct pfl_rwlock {
	pthread_rwlock_t	pr_rwlock;
	pthread_t		pr_writer;
	struct psc_dynarray	pr_readers;
	psc_spinlock_t		pr_lock;
};

#ifdef PTHREAD_RWLOCK_WRITER_NONRECURSIVE_INITIALIZER_NP
# define pfl_rwlock_INIT		{ PTHREAD_RWLOCK_WRITER_NONRECURSIVE_INITIALIZER_NP, \
					  0, DYNARRAY_INIT, SPINLOCK_INIT }
#else
# define pfl_rwlock_INIT		{ PTHREAD_RWLOCK_INITIALIZER, 0, DYNARRAY_INIT, SPINLOCK_INIT }
#endif

#define pfl_rwlock_rdlock(rw)		_pfl_rwlock_rdlock(PFL_CALLERINFO(), (rw))
#define pfl_rwlock_reqrdlock(rw)	_pfl_rwlock_reqrdlock(PFL_CALLERINFO(), (rw))
#define pfl_rwlock_reqwrlock(rw)	_pfl_rwlock_reqwrlock(PFL_CALLERINFO(), (rw))
#define pfl_rwlock_unlock(rw)		_pfl_rwlock_unlock(PFL_CALLERINFO(), (rw))
#define pfl_rwlock_ureqlock(rw, lk)	_pfl_rwlock_ureqlock(PFL_CALLERINFO(), (rw), (lk))
#define pfl_rwlock_wrlock(rw)		_pfl_rwlock_wrlock(PFL_CALLERINFO(), (rw))

void	 pfl_rwlock_destroy(struct pfl_rwlock *);
int	 pfl_rwlock_hasrdlock(struct pfl_rwlock *);
int	 pfl_rwlock_haswrlock(struct pfl_rwlock *);
void	 pfl_rwlock_init(struct pfl_rwlock *);
void	_pfl_rwlock_rdlock(const struct pfl_callerinfo *, struct pfl_rwlock *);
int	_pfl_rwlock_reqrdlock(const struct pfl_callerinfo *, struct pfl_rwlock *);
int	_pfl_rwlock_reqwrlock(const struct pfl_callerinfo *, struct pfl_rwlock *);
void	_pfl_rwlock_unlock(const struct pfl_callerinfo *, struct pfl_rwlock *);
void	_pfl_rwlock_ureqlock(const struct pfl_callerinfo *, struct pfl_rwlock *, int);
void	_pfl_rwlock_wrlock(const struct pfl_callerinfo *, struct pfl_rwlock *);

static __inline int
psc_listhd_empty_mutex_locked(struct pfl_mutex *m, struct psclist_head *hd)
{
	int locked, empty;

	locked = psc_mutex_reqlock(m);
	empty = psc_listhd_empty(hd);
	psc_mutex_ureqlock(m, locked);
	return (empty);
}

#ifndef HAVE_PTHREAD_BARRIER
# include "pfl/compat/pthread_barrier.h"
#endif

#endif /* _PFL_PTHRUTIL_H_ */
