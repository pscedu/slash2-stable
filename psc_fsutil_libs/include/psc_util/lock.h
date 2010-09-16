/* $Id$ */
/*
 * %PSC_START_COPYRIGHT%
 * -----------------------------------------------------------------------------
 * Copyright (c) 2006-2010, Pittsburgh Supercomputing Center (PSC).
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

/*
 * Spinlock routines: busy wait until another thread is done with a
 * critical section.
 *
 * Note: these routines depend on 32-bit atomic operations and may
 * supply higher precision (64-bit) atomic operations on some
 * architectures.
 */

#ifndef _PFL_SPINLOCK_H_
#define _PFL_SPINLOCK_H_

#include <sys/types.h>
#include <sys/time.h>

#include <pthread.h>
#include <sched.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "pfl/time.h"
#include "pfl/types.h"
#include "psc_util/log.h"

#define PSLRV_WASLOCKED		42
#define PSLRV_WASNOTLOCKED	43

enum psc_spinlock_val {
	PSL_UNLOCKED = 2,
	PSL_LOCKED = 3,
};

typedef struct psc_spinlock {
	enum psc_spinlock_val	psl_value;
	int			psl_flags;
	pthread_t		psl_who;
#ifdef LOCK_TIMING
	struct timeval		psl_time;
#endif
} psc_spinlock_t;

#define PSLF_LOG		(1 << 0)	/* log locks and unlocks */

#define PSL_SLEEP_NTRIES	256
#define PSL_SLEEP_NSEC		5001

#define PSL_SLEEP_NSEC		5001

#define _SPIN_GETATOM(psl)	((psc_atomic32_t *)(void *)&(psl)->psl_value)

#define INIT_SPINLOCK_FLAGS(psl, flg)					\
	do {								\
		psc_atomic32_set(_SPIN_GETATOM(psl), PSL_UNLOCKED);	\
		(psl)->psl_flags = (flg);				\
	} while (0)

#define INIT_SPINLOCK(psl)	INIT_SPINLOCK_FLAGS((psl), 0)
#define INIT_SPINLOCK_LOG(psl)	INIT_SPINLOCK_FLAGS((psl), PSLF_LOG)

#ifdef LOCK_TIMING
#  define SPINLOCK_INIT		{ PSL_UNLOCKED, 0, 0, { 0, 0 } }
#else
#  define SPINLOCK_INIT		{ PSL_UNLOCKED, 0, 0 }
#endif

#define _SPIN_GETVAL(psl)	psc_atomic32_read(_SPIN_GETATOM(psl))

#define _SPIN_CHECK(name, psl)						\
	do {								\
		enum psc_spinlock_val _val = _SPIN_GETVAL(psl);		\
									\
		if (_val != PSL_LOCKED && _val != PSL_UNLOCKED)		\
			psc_fatalx("%s: lock %p has invalid value %#x",	\
			    (name), (psl), _val);			\
	} while (0)

#define _SPIN_ENSURELOCKED(name, psl)					\
	do {								\
		_SPIN_CHECK((name), (psl));				\
		if (_SPIN_GETVAL(psl) == PSL_UNLOCKED)			\
			psc_fatalx("%s: not locked (lock %p)", (name),	\
			    (psl));					\
		if ((psl)->psl_who != pthread_self())			\
			psc_fatalx("%s: not owner "			\
			    "(lock %p, owner %"PSCPRI_PTHRT", "		\
			    "self %"PSCPRI_PTHRT")", (name),		\
			    (psl), (psl)->psl_who, pthread_self());	\
	} while (0)

#define _SPIN_TEST_AND_SET(name, psl)					\
	{								\
		enum psc_spinlock_val _val;				\
									\
		_val = psc_atomic32_xchg(_SPIN_GETATOM(psl),		\
		    PSL_LOCKED);					\
		if ((_val) == PSL_LOCKED) {				\
			if ((psl)->psl_who == pthread_self())		\
				psc_fatalx("%s: already locked",	\
				    (name));				\
			/* PFL_GETTIMEVAL(&(psl)->psl_time); */		\
			(_val) = 0;					\
		} else if ((_val) == PSL_UNLOCKED) {			\
			(psl)->psl_who = pthread_self();		\
			psc_log((psl)->psl_flags & PSLF_LOG ?		\
			    PLL_NOTICE : PLL_TRACE,			\
			    "lock %p acquired", (psl));			\
			(_val) = 1;					\
		} else							\
			psc_fatalx("%s: lock %p has invalid value %#x",	\
			    (name), (psl), (_val));			\
		(_val);							\
	}

#define _TRYLOCK(psl)		(_SPIN_TEST_AND_SET("trylock", (psl)))

#define _SPINLOCK(psl)							\
	do {								\
		struct timespec _tm;					\
		int _i;							\
									\
		for (_i = 0; !(_SPIN_TEST_AND_SET("spinlock", (psl)));	\
		    sched_yield(), _i++)				\
			if (_i >= PSL_SLEEP_NTRIES) {			\
				_tm.tv_sec  = 0;			\
				_tm.tv_nsec = PSL_SLEEP_NSEC;		\
				nanosleep(&_tm, 0);			\
				_i = 0;					\
			}						\
	} while (0)

#define _FREELOCK(psl)							\
	do {								\
		_SPIN_ENSURELOCKED("freelock", (psl));			\
		_spin_checktime(psl);					\
		(psl)->psl_who = 0;					\
		psc_atomic32_set(_SPIN_GETATOM(psl), PSL_UNLOCKED);	\
		psc_log((psl)->psl_flags & PSLF_LOG ?			\
		    PLL_NOTICE : PLL_TRACE, "lock %p released", (psl));	\
	} while (0)

/**
 * reqlock - Require a lock for a critical section.  Locks if unlocked,
 *	doesn't if already locked (to avoid deadlock).
 * @psl: the spinlock.
 * Returns a value that ureqlock() must use.
 */
#define _REQLOCK(psl)							\
	(spin_ismine(psl) ? PSLRV_WASLOCKED :				\
	    (PSC_MAKETRUE(_SPINLOCK(psl)), PSLRV_WASNOTLOCKED))

#define _TRYREQLOCK(psl, waslkdp)					\
	(spin_ismine(psl) ?						\
	    (*(waslkdp) = PSLRV_WASLOCKED, 1) :				\
	    (*(waslkdp) = PSLRV_WASNOTLOCKED, trylock(psl)))

/**
 * ureqlock - "Unrequire" a lock -- unlocks the lock if it was locked
 *	for the nearest "reqlock ... ureqlock" section and doesn't if
 *	the lock was already locked before the critical section began.
 * @psl: the spinlock.
 * @waslkd: return value from reqlock().
 */
#define _UREQLOCK(psl, waslkd)						\
	do {								\
		if ((waslkd) == PSLRV_WASNOTLOCKED)			\
			freelock(psl);					\
		else							\
			_SPIN_CHECK("ureqlock", (psl));			\
	} while (0)

#define LOCK_ENSURE(psl)	PSC_MAKETRUE(_SPIN_ENSURELOCKED("LOCK_ENSURE", (psl)))
#define spinlock(psl)		_SPINLOCK(psl)
#define trylock(psl)		_TRYLOCK(psl)
#define freelock(psl)		_FREELOCK(psl)
#define reqlock(psl)		_REQLOCK(psl)
#define tryreqlock(psl, was)	_TRYREQLOCK((psl), (was))
#define ureqlock(psl, was)	_UREQLOCK((psl), (was))

static __inline void _spin_checktime(struct psc_spinlock *);

#ifndef _PFL_ATOMIC_H_
#  include "psc_util/atomic.h"
#endif

static __inline void
_spin_checktime(struct psc_spinlock *psl)
{
#ifdef LOCK_TIMING
	struct timeval now, diff;

	PFL_GETTIMEVAL(&now);
	max.tv_sec = 0;
	max.tv_usec = 500;
	timersub(&now, &psl->psl_time, &diff);
	if (timercmp(&diff, &max, >))
		psc_errorx("lock %p held long (%luus)",
		    psl, diff.tv_sec * 1000 * 1000 + diff.tv_usec);
#else
	(void)psl;
#endif
}

static __inline int
spin_ismine(psc_spinlock_t *psl)
{
	_SPIN_CHECK("spin_ismine", psl);
	/*
	 * This code is thread safe because even if psl_who changes, it
	 * won't be set to us.
	 */
	return (_SPIN_GETVAL(psl) == PSL_LOCKED && psl->psl_who == pthread_self());
}

#endif /* _PFL_SPINLOCK_H_ */
