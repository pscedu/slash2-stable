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

#ifndef _PFL_LOCK_H_
#define _PFL_LOCK_H_

#include "pfl/types.h"
#include "psc_util/log.h"

#ifdef __ia64

#include "psc_util/spinlock.h"

typedef struct psc_spinlock psc_spinlock_t;

#define SPINLOCK_INIT		PSC_SPINLOCK_INIT
#define INIT_SPINLOCK(psl)	psc_spin_init(psl)
#define LOCK_ENSURE(psl)	psc_spin_ensure(psl)
#define freelock(psl)		psc_spin_unlock(psl)
#define trylock(psl)		psc_spin_trylock(psl)
#define spinlock(psl)		psc_spin_lock(psl)
#define reqlock(psl)		psc_spin_reqlock(psl)
#define tryreqlock(psl, lk)	psc_spin_tryreqlock((psl), (lk))
#define ureqlock(psl, lk)	psc_spin_ureqlock((psl), (lk))

#else

#define SL_UNLOCKED		2
#define SL_LOCKED		3

#if HAVE_LIBPTHREAD

#include <pthread.h>
#include <sched.h>
#include <time.h>

typedef struct {
	int			sl_lock;
	int			sl_flags;
	pthread_t		sl_who;
} psc_spinlock_t;

#define SPINF_LOG		(1 << 0)	/* log locks and unlocks */

#define MAX_SPIN_CNT		256
#define SPIN_SLEEP_DURATION     5001

#define PSC_SPIN_RLV_LOCKED	42
#define PSC_SPIN_RLV_NOTLOCKED	43

#define INIT_SPINLOCK(s)						\
	do {								\
		(s)->sl_lock = SL_UNLOCKED;				\
		(s)->sl_flags = 0;					\
	} while (0)

#define INIT_SPINLOCK_LOG(psl)						\
	do {								\
		(s)->sl_lock = SL_UNLOCKED;				\
		(s)->sl_flags = SPINF_LOG;				\
	} while (0)

#define SPINLOCK_INIT		{ SL_UNLOCKED, 0, 0 }

#define _SPIN_CHECK(name, s)						\
	do {								\
		int _val = (s)->sl_lock;				\
									\
		if (_val != SL_LOCKED && _val != SL_UNLOCKED)		\
			psc_fatalx("%s: %#x: invalid value (lock %p)",	\
			    (name), (s)->sl_lock, (s));			\
	} while (0)

#define _SPIN_ENSURELOCKED(name, s)					\
	{								\
		_SPIN_CHECK((name), (s));				\
		if ((s)->sl_lock == SL_UNLOCKED)			\
			psc_fatalx("%s: not locked (lock %p)", (name),	\
			    (s));					\
		if ((s)->sl_who != pthread_self())			\
			psc_fatalx("%s: not owner "			\
			    "(lock %p, owner %"PSCPRI_PTHRT", "		\
			    "self %"PSCPRI_PTHRT")", (name),		\
			    (s), (s)->sl_who, pthread_self());		\
		1;							\
	}

#define _P_SPIN_TEST_AND_SET(name, s)					\
	{								\
		int _v;							\
									\
		(_v) = SL_LOCKED;					\
		__asm__(						\
		    "xchg %0, %1"					\
		    : "=r" (_v), "=m" ((s)->sl_lock)			\
		    : "0"  (_v), "m"  ((s)->sl_lock)			\
		);							\
		if ((_v) == SL_LOCKED) {				\
			if ((s)->sl_who == pthread_self())		\
				psc_fatalx("%s: already locked",	\
				    (name));				\
			(_v) = 0;					\
		} else if ((_v) == SL_UNLOCKED) {			\
			(s)->sl_who = pthread_self();			\
			psc_log((s)->sl_flags & SPINF_LOG ?		\
			    PLL_NOTICE : PLL_TRACE,			\
			    "lock %p acquired", (s));			\
			(_v) = 1;					\
		} else							\
			psc_fatalx("%s: %#x: invalid value (lock %p)",	\
			    (name), (_v), (s));				\
		(_v);							\
	}
#define _SPIN_TEST_AND_SET(name, s)	(_P_SPIN_TEST_AND_SET((name), (s)))

#define _TRYLOCK(s)		_SPIN_TEST_AND_SET("trylock", (s))

#define _SPINLOCK(s)							\
	do {								\
		struct timespec _tm;					\
		int _i = 0;						\
									\
		while (!_SPIN_TEST_AND_SET("spinlock", (s))) {		\
			if (_i < MAX_SPIN_CNT) {			\
				sched_yield();				\
				_i++;					\
			} else {					\
				_tm.tv_sec  = 0;			\
				_tm.tv_nsec = SPIN_SLEEP_DURATION;	\
				nanosleep(&_tm, 0);			\
				_i = 0;					\
			}						\
		}							\
	} while (0)
#define _P_SPINLOCK(s)		{ _SPINLOCK(s); }

#define _FREELOCK(s)							\
	do {								\
		(void)(_SPIN_ENSURELOCKED("freelock", (s)));		\
		(s)->sl_who = 0;					\
		(s)->sl_lock = SL_UNLOCKED;				\
		psc_log((s)->sl_flags & SPINF_LOG ?			\
		    PLL_NOTICE : PLL_TRACE,				\
		    "lock %p released", (s));				\
	} while (0)

/**
 * reqlock - Require a lock for a critical section.  Locks if unlocked,
 *	doesn't if already locked (to avoid deadlock).
 * @s: the spinlock.
 * Returns a value that ureqlock() must use.
 */
#define _REQLOCK(s)							\
	(havelock(s) ?							\
	    PSC_SPIN_RLV_LOCKED :					\
	    ((_P_SPINLOCK(s)), PSC_SPIN_RLV_NOTLOCKED))

#define _TRYREQLOCK(s, waslkdp)						\
	(havelock(s) ?							\
	    (*(waslkdp) = PSC_SPIN_RLV_LOCKED, 1) :			\
	    (*(waslkdp) = PSC_SPIN_RLV_NOTLOCKED, trylock(s)))

/**
 * ureqlock - "Unrequire" a lock -- unlocks the lock if it was locked
 *	for the nearest "reqlock ... ureqlock" section and doesn't if
 *	the lock was already locked before the critical section began.
 * @s: the spinlock.
 * @waslkd: return value from reqlock().
 */
#define _UREQLOCK(s, waslkd)						\
	do {								\
		if ((waslkd) == PSC_SPIN_RLV_NOTLOCKED)			\
			freelock(s);					\
		else							\
			_SPIN_CHECK("ureqlock", (s));			\
	} while (0)

static __inline int
havelock(psc_spinlock_t *s)
{
	_SPIN_CHECK("havelock", s);
	/*
	 * This code is thread safe because even if sl_who changes, it
	 * won't be set to us.
	 */
	return (s->sl_lock == SL_LOCKED && s->sl_who == pthread_self());
}

#ifdef LOCK_PROFILING
static __inline void
spinlock(psc_spinlock_t *s)
{
	_SPINLOCK(s);
}

static __inline void
freelock(psc_spinlock_t *s)
{
	_FREELOCK(s);
}

static __inline void
trylock(psc_spinlock_t *s)
{
	_TRYLOCK(s);
}

static __inline int
reqlock(psc_spinlock_t *s)
{
	return (_REQLOCK(s));
}

static __inline int
tryreqlock(psc_spinlock_t *s, int *waslkd)
{
	return (_TRYREQLOCK(s, waslkd));
}

static __inline void
ureqlock(psc_spinlock_t *s, int waslkd)
{
	_UREQLOCK(s, waslkd);
}
#else
#  define spinlock(s)		_SPINLOCK(s)
#  define trylock(s)		_TRYLOCK(s)
#  define freelock(s)		_FREELOCK(s)
#  define reqlock(s)		_REQLOCK(s)
#  define tryreqlock(s, waslkd)	_TRYREQLOCK((s), (waslkd))
#  define ureqlock(s, waslkd)	_UREQLOCK((s), (waslkd))
#endif

#define LOCK_ENSURE(s)		(_SPIN_ENSURELOCKED("LOCK_ENSURE", (s)))

#else /* !HAVE_LIBPTHREAD */

#include "pfl/cdefs.h"

typedef int psc_spinlock_t;

/* XXX provide some way to detect reinitializing already-initialized locks. */
#define INIT_SPINLOCK(s)	(*(s) = SL_UNLOCKED)
#define SPINLOCK_INIT		SL_UNLOCKED

#define _LOCK_VALID(s)		(*(s) == SL_LOCKED || *(s) == SL_UNLOCKED)

static __inline int
LOCK_ENSURE(psc_spinlock_t *s)
{
	if (!_LOCK_VALID(s))
		psc_fatalx("LOCK_ENSURE: %x: invalid value (lock %p)", *s, s);
	if (*s != SL_LOCKED)
		psc_fatalx("lock is not locked (%p)!", s);
	return (1);
}

static __inline void
freelock(psc_spinlock_t *s)
{
	if (!_LOCK_VALID(s))
		psc_fatalx("lock %p has invalid value", s);
	if (*s == SL_UNLOCKED)
		psc_fatalx("lock %p not locked", s);
	*s = SL_UNLOCKED;
}

static __inline void
spinlock(psc_spinlock_t *s)
{
	if (!_LOCK_VALID(s))
		psc_fatalx("lock %p has invalid value", s);
	if (*s == SL_LOCKED)
		psc_fatalx("lock %p already locked", s);
	*s = SL_LOCKED;
}

static __inline int
trylock(psc_spinlock_t *s)
{
	if (*s == SL_LOCKED)
		psc_fatalx("lock %p already locked", s);
	else if (*s != SL_UNLOCKED)
		psc_fatalx("lock %p has invalid value", s);
	*s = SL_LOCKED;
	return (1);
}

static __inline int
reqlock(psc_spinlock_t *s)
{
	if (*s == SL_LOCKED)
		return (1);
	else if (*s != SL_UNLOCKED)
		psc_fatalx("lock %p has invalid value", s);
	*s = SL_LOCKED;
	return (0);
}

static __inline void
ureqlock(psc_spinlock_t *s, int waslocked)
{
	if (!waslocked)
		freelock(s);
	else if (!_LOCK_VALID(s))
		psc_fatalx("lock %p has invalid value", s);
}

static __inline int
tryreqlock(psc_spinlock_t *s, int *locked)
{
	if (*s == SL_LOCKED) {
		*locked = 1;
		return (1);
	} else if (*s != SL_UNLOCKED)
		psc_fatalx("lock %p has invalid value", s);
	*s = SL_LOCKED;
	*locked = 0;
	return (1);
}

#endif /* HAVE_LIBPTHREAD */

#endif /* __ia64 */

#endif /* _PFL_LOCK_H_ */
