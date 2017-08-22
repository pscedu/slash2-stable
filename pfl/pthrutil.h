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

#ifndef _PFL_PTHRUTIL_H_
#define _PFL_PTHRUTIL_H_

#include <pthread.h>

#include "pfl/dynarray.h"
#include "pfl/list.h"
#include "pfl/lock.h"
#include "pfl/pfl.h"

/* The last "unable" case happens on FreeBSD 9.0-CURRENT */
 
#ifdef PTHREAD_MUTEX_ERRORCHECK_INITIALIZER
# define PSC_MUTEX_INIT			{ PTHREAD_MUTEX_ERRORCHECK_INITIALIZER, 0, 0, 0 }
#elif defined(PTHREAD_MUTEX_ERRORCHECK_INITIALIZER_NP)
# define PSC_MUTEX_INIT			{ PTHREAD_MUTEX_ERRORCHECK_INITIALIZER_NP, 0, 0, 0 }
#elif defined(PTHREAD_ERRORCHECK_MUTEX_INITIALIZER)
# define PSC_MUTEX_INIT			{ PTHREAD_ERRORCHECK_MUTEX_INITIALIZER, 0, 0, 0 }
#elif defined(PTHREAD_ERRORCHECK_MUTEX_INITIALIZER_NP)
# define PSC_MUTEX_INIT			{ PTHREAD_ERRORCHECK_MUTEX_INITIALIZER_NP, 0, 0, 0 }
#else
# warning "unable to find an error checking mutex; beware"
# define PSC_MUTEX_INIT			{ PTHREAD_MUTEX_INITIALIZER, 0, 0, 0 }
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
	int			pm_lineno;
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

/* The last "unable" case happens on FreeBSD 9.0-CURRENT */

#ifdef PTHREAD_RWLOCK_WRITER_NONRECURSIVE_INITIALIZER_NP
# define pfl_rwlock_INIT		{ PTHREAD_RWLOCK_WRITER_NONRECURSIVE_INITIALIZER_NP, \
					  0, DYNARRAY_INIT, SPINLOCK_INIT }
#else
# warning "unable to find an nonrecursive rw writer; beware"
# define pfl_rwlock_INIT		{ PTHREAD_RWLOCK_INITIALIZER, 0, DYNARRAY_INIT, SPINLOCK_INIT }
#endif

#define pfl_rwlock_rdlock(rw)		_pfl_rwlock_rdlock(PFL_CALLERINFO(), (rw))
#define pfl_rwlock_unlock(rw)		_pfl_rwlock_unlock(PFL_CALLERINFO(), (rw))
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

enum pfl_lockprim {
	PFL_LOCKPRIMT_MUTEX = 10,
	PFL_LOCKPRIMT_RWLOCK,
	PFL_LOCKPRIMT_SPIN,
};

#define PFL_LOCKPRIM_ULOCK(type, lockp)					\
	do {								\
		if (lockp) {						\
			switch (type) {					\
			case PFL_LOCKPRIMT_SPIN:			\
				freelock((struct psc_spinlock *)lockp);	\
				break;					\
			case PFL_LOCKPRIMT_MUTEX:			\
				psc_mutex_unlock(lockp);		\
				break;					\
			case PFL_LOCKPRIMT_RWLOCK:			\
				pfl_rwlock_unlock(lockp);		\
				break;					\
			default:					\
				psc_fatalx("invalid locking primitive "	\
				    "type: %d", (type));		\
			}						\
		}							\
	} while (0)

#endif /* _PFL_PTHRUTIL_H_ */
