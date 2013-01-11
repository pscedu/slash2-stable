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

#ifndef _PFL_PTHRUTIL_H_
#define _PFL_PTHRUTIL_H_

#include <pthread.h>

#include "pfl/pfl.h"
#include "psc_ds/dynarray.h"
#include "psc_util/lock.h"

#ifdef PTHREAD_MUTEX_ERRORCHECK_INITIALIZER
# define PSC_MUTEX_INIT			{ PTHREAD_MUTEX_ERRORCHECK_INITIALIZER, 0 }
#elif defined(PTHREAD_MUTEX_ERRORCHECK_INITIALIZER_NP)
# define PSC_MUTEX_INIT			{ PTHREAD_MUTEX_ERRORCHECK_INITIALIZER_NP, 0 }
#else
# define PSC_MUTEX_INIT			{ PTHREAD_MUTEX_INITIALIZER, 0 }
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
};

void	_psc_mutex_ensure_locked(const struct pfl_callerinfo *, struct pfl_mutex *);
int	 psc_mutex_haslock(struct pfl_mutex *);
void	 psc_mutex_init(struct pfl_mutex *);
void	 psc_mutex_destroy(struct pfl_mutex *);
void	_psc_mutex_lock(const struct pfl_callerinfo *, struct pfl_mutex *);
int	_psc_mutex_reqlock(const struct pfl_callerinfo *, struct pfl_mutex *);
int	_psc_mutex_trylock(const struct pfl_callerinfo *, struct pfl_mutex *);
int	_psc_mutex_tryreqlock(const const struct pfl_callerinfo *, struct pfl_mutex *, int *);
void	_psc_mutex_unlock(const struct pfl_callerinfo *, struct pfl_mutex *);
void	_psc_mutex_ureqlock(const struct pfl_callerinfo *, struct pfl_mutex *, int);

struct psc_rwlock {
	pthread_rwlock_t	pr_rwlock;
	pthread_t		pr_writer;
	struct psc_dynarray	pr_readers;
	psc_spinlock_t		pr_lock;
};

#ifdef PTHREAD_RWLOCK_WRITER_NONRECURSIVE_INITIALIZER_NP
# define PSC_RWLOCK_INIT		{ PTHREAD_RWLOCK_WRITER_NONRECURSIVE_INITIALIZER_NP, \
					  0, DYNARRAY_INIT, SPINLOCK_INIT }
#else
# define PSC_RWLOCK_INIT		{ PTHREAD_RWLOCK_INITIALIZER, 0, DYNARRAY_INIT, SPINLOCK_INIT }
#endif

#define psc_rwlock_rdlock(rw)		_psc_rwlock_rdlock(PFL_CALLERINFO(), (rw))
#define psc_rwlock_reqrdlock(rw)	_psc_rwlock_reqrdlock(PFL_CALLERINFO(), (rw))
#define psc_rwlock_reqwrlock(rw)	_psc_rwlock_reqwrlock(PFL_CALLERINFO(), (rw))
#define psc_rwlock_unlock(rw)		_psc_rwlock_unlock(PFL_CALLERINFO(), (rw))
#define psc_rwlock_ureqlock(rw, lk)	_psc_rwlock_ureqlock(PFL_CALLERINFO(), (rw), (lk))
#define psc_rwlock_wrlock(rw)		_psc_rwlock_wrlock(PFL_CALLERINFO(), (rw))

void	 psc_rwlock_destroy(struct psc_rwlock *);
int	 psc_rwlock_hasrdlock(struct psc_rwlock *);
int	 psc_rwlock_haswrlock(struct psc_rwlock *);
void	 psc_rwlock_init(struct psc_rwlock *);
void	_psc_rwlock_rdlock(const struct pfl_callerinfo *, struct psc_rwlock *);
int	_psc_rwlock_reqrdlock(const struct pfl_callerinfo *, struct psc_rwlock *);
int	_psc_rwlock_reqwrlock(const struct pfl_callerinfo *, struct psc_rwlock *);
void	_psc_rwlock_unlock(const struct pfl_callerinfo *, struct psc_rwlock *);
void	_psc_rwlock_ureqlock(const struct pfl_callerinfo *, struct psc_rwlock *, int);
void	_psc_rwlock_wrlock(const struct pfl_callerinfo *, struct psc_rwlock *);

#ifndef HAVE_PTHREAD_BARRIER
# include "pfl/compat/pthread_barrier.h"
#endif

#endif /* _PFL_PTHRUTIL_H_ */
