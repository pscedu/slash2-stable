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

#include "psc_util/log.h"

#ifdef __ia64

#include "psc_util/spinlock.h"

typedef struct psc_spinlock psc_spinlock_t;

#define LOCK_INITIALIZER	PSC_SPINLOCK_INIT
#define LOCK_INIT(psl)		psc_spin_init(psl)
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
	int		sl_lock;
	pthread_t	sl_who;
} psc_spinlock_t;

#define MAX_SPIN_CNT		100
#define SPIN_SLEEP_DURATION	2000001

/* XXX provide some way to detect reinitializing already-initialized locks. */
#define LOCK_INIT(l)		((l)->sl_lock = SL_UNLOCKED)
#define LOCK_INITIALIZER	{ SL_UNLOCKED, 0 }

static __inline int
validlock(const psc_spinlock_t *sl)
{
	int v;

	v = sl->sl_lock;
	return (v == SL_LOCKED || v == SL_UNLOCKED);
}

#define _LOCK_ENSURE(name, sl) {					\
		if (!validlock(sl))					\
			psc_fatalx("%s: %#x: invalid value (lock %p)",	\
			    (name), (sl)->sl_lock, (sl));		\
		if ((sl)->sl_lock == SL_UNLOCKED)			\
			psc_fatalx("%s: not locked (lock %p)", (name),	\
			    (sl));					\
		if ((sl)->sl_who != pthread_self())			\
			psc_fatalx("%s: not owner "			\
			    "(lock %p, owner %lu, self %lu)", (name),	\
			    (sl), (sl)->sl_who, pthread_self());	\
		1;							\
	}

#define LOCK_ENSURE(sl)		(_LOCK_ENSURE("LOCK_ENSURE", (sl)))

#define freelock(sl)							\
	do {								\
		(void)(_LOCK_ENSURE("freelock", (sl)));			\
		(sl)->sl_who = 0;					\
		(sl)->sl_lock = SL_UNLOCKED;				\
	} while (0)

#define _TEST_AND_SET_GUTS(name, sl)					\
	{								\
		int _v;							\
									\
		(_v) = SL_LOCKED;					\
		__asm__(						\
		    "xchg %0, %1"					\
		    : "=r" (_v), "=m" ((sl)->sl_lock)			\
		    : "0"  (_v), "m"  ((sl)->sl_lock)			\
		);							\
		if ((_v) == SL_LOCKED) {				\
			if ((sl)->sl_who == pthread_self())		\
				psc_fatalx("%s: already locked",	\
				    (name));				\
			(_v) = 0;					\
		} else if ((_v) == SL_UNLOCKED) {			\
			(sl)->sl_who = pthread_self();			\
			(_v) = 1;					\
		} else {						\
			psc_fatalx("%s: %#x: invalid value (lock %p)",	\
			    (name), (_v), (sl));			\
		}							\
		(_v);							\
	}

#define _TEST_AND_SET(name, sl)	(_TEST_AND_SET_GUTS((name), (sl)))

#define trylock(sl)	_TEST_AND_SET("trylock", (sl))

#define _SPINLOCK(sl)							\
	do {								\
		struct timespec _tm;					\
		int _i = 0;						\
									\
		while (!_TEST_AND_SET("spinlock", (sl))) {		\
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

#ifdef LOCK_PROFILING
static __inline void
spinlock(psc_spinlock_t *sl)
{
	_SPINLOCK(sl);
}
#else
#define spinlock(sl)	_SPINLOCK(sl)
#endif

#define PSC_SPIN_RLV_LOCKED	42
#define PSC_SPIN_RLV_NOTLOCKED	43

/*
 * reqlock - require a lock for a critical section.
 *	locks if unlocked, doesn't if already locked
 *	(to avoid deadlock).
 * @sl: the lock.
 * Returns true if the lock is already locked.
 */
static __inline int
reqlock(psc_spinlock_t *sl)
{
	if (sl->sl_lock == SL_LOCKED) {
		/*
		 * This code is thread safe because
		 * even if sl_who changes, it won't
		 * be set to us and we will still wait
		 * for the lock when we need to.
		 */
		if (sl->sl_who == pthread_self())
			return (PSC_SPIN_RLV_LOCKED);
		else
			/* someone else has it, wait */
			spinlock(sl);
	} else
		/* not locked, grab it */
		spinlock(sl);
	return (PSC_SPIN_RLV_NOTLOCKED);
}

static __inline int
tryreqlock(psc_spinlock_t *sl, int *locked)
{
	if (sl->sl_lock == SL_LOCKED &&
	    sl->sl_who == pthread_self()) {
		*locked = PSC_SPIN_RLV_LOCKED;
		return (1);
	}
	*locked = PSC_SPIN_RLV_NOTLOCKED;
	return (trylock(sl));
}

/*
 * ureqlock - "unrequire" a lock -- unlocks the lock if
 *	it was locked for the nearest "reqlock ... ureqlock"
 *	section and doesn't if the lock was already locked
 *	before the critical section began.
 * @sl: the lock.
 * @waslocked: return value from reqlock().
 */
static __inline void
ureqlock(psc_spinlock_t *sl, int waslocked)
{
	if (waslocked == PSC_SPIN_RLV_NOTLOCKED)
		freelock(sl);
	else if (waslocked != PSC_SPIN_RLV_LOCKED)
		psc_fatalx("ureqlock: bad value");
}

#else /* !HAVE_LIBPTHREAD */

#include "pfl/cdefs.h"

typedef int psc_spinlock_t;

/* XXX provide some way to detect reinitializing already-initialized locks. */
#define LOCK_INIT(l)		(*(l) = SL_UNLOCKED)
#define LOCK_INITIALIZER	SL_UNLOCKED

#define _LOCK_VALID(l)		(*(l) == SL_LOCKED || *(l) == SL_UNLOCKED)

static __inline int
LOCK_ENSURE(psc_spinlock_t *sl)
{
	if (!_LOCK_VALID(sl))
		psc_fatalx("LOCK_ENSURE: %x: invalid value (lock %p)", *sl, sl);
	if (*sl != SL_LOCKED)
		psc_fatalx("lock is not locked (%p)!", sl);
	return (1);
}

#define freelock(l)								\
	do {									\
		if (!_LOCK_VALID(l))						\
			psc_fatalx("freelock: invalid lock value (%p)", (l));	\
		if (*(l) == SL_UNLOCKED)					\
			psc_fatalx("freelock: not locked (%p)", (l));		\
		*(l) = SL_UNLOCKED;						\
	} while (0)

static __inline void
spinlock(psc_spinlock_t *l)
{
	if (!_LOCK_VALID(l))
		psc_fatalx("lock %p has invalid value", l);
	if (*l == SL_LOCKED)
		psc_fatalx("lock %p already locked", l);
	*l = SL_LOCKED;
}

static __inline int
trylock(psc_spinlock_t *l)
{
	if (*l == SL_LOCKED)
		psc_fatalx("lock %p already locked", l);
	else if (*l != SL_UNLOCKED)
		psc_fatalx("lock %p has invalid value", l);
	*l = SL_LOCKED;
	return (1);
}

static __inline int
reqlock(psc_spinlock_t *l)
{
	if (*l == SL_LOCKED)
		return (1);
	else if (*l != SL_UNLOCKED)
		psc_fatalx("lock %p has invalid value", l);
	*l = SL_LOCKED;
	return (0);
}

static __inline void
ureqlock(psc_spinlock_t *l, int waslocked)
{
	if (!waslocked)
		freelock(l);
	else if (!_LOCK_VALID(l))
		psc_fatalx("lock %p has invalid value", l);
}

static __inline int
tryreqlock(psc_spinlock_t *l, int *locked)
{
	if (*l == SL_LOCKED) {
		*locked = 1;
		return (1);
	} else if (*l != SL_UNLOCKED)
		psc_fatalx("lock %p has invalid value", l);
	*l = SL_LOCKED;
	*locked = 0;
	return (1);
}

#endif /* HAVE_LIBPTHREAD */

#endif /* __ia64 */
#endif /* _PFL_LOCK_H_ */
