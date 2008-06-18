/* $Id$ */

#ifndef __PFL_LOCK_H__
#define __PFL_LOCK_H__

#include "psc_util/log.h"

/* Compatibility for LNET. */
#define spinlock_t		psc_spinlock_t
#define SPIN_LOCK_UNLOCKED	LOCK_INITIALIZER
#define spin_lock(l)		spinlock(l)
#define spin_unlock(l)		freelock(l)
#define spin_lock_init(l)	LOCK_INIT(l)

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

	if (!sl) psc_fatalx("NULL lock pointer");
	v = sl->sl_lock;
	return (v == SL_LOCKED || v == SL_UNLOCKED);
}

#define freelock(l)							\
	do {								\
		if (!validlock(l))					\
			psc_fatalx("lock %p has invalid value", (l));	\
		if ((l)->sl_lock == SL_UNLOCKED)			\
			psc_fatalx("tried to unlock already "		\
			    "unlocked lock (%p)!", (l));		\
		if ((l)->sl_who != pthread_self())			\
			psc_fatalx("tried to unlock someone "		\
			    "else's lock (%p, %lu vs. %lu)!",		\
			    (l), (l)->sl_who, pthread_self());		\
		(l)->sl_who = 0;					\
		(l)->sl_lock = SL_UNLOCKED;				\
	} while (0)

#define LOCK_ENSURE(l)							\
	do {								\
		if (!validlock(l))					\
			psc_fatalx("lock %p has invalid value", (l));	\
		if ((l)->sl_lock == SL_LOCKED)				\
			psc_fatalx("lock is not locked (%p)!", (l));	\
		if ((l)->sl_who != pthread_self())			\
			psc_fatalx("lock is not owned by us "		\
			    "(%p, %lu vs. %lu)!",			\
			    (l), (l)->sl_who, pthread_self());		\
	} while (0)

static __inline int
_tands(volatile psc_spinlock_t *s)
{
	int r;

	r = SL_LOCKED;
	__asm__(
	     "xchg %0, %1"
	     : "=r" (r), "=m" (s->sl_lock)
	     : "0"  (r), "m"  (s->sl_lock)
	);
	if (r == SL_LOCKED) {
		if (s->sl_who == pthread_self())
			psc_fatalx("already holding the lock");
		return (1);			/* already locked */
	} else if (r == SL_UNLOCKED) {
		s->sl_who = pthread_self();	/* we got it */
		return (0);
	}
	psc_fatalx("lock %p has invalid value (%d)", s, r);
}

static __inline void
spinlock(psc_spinlock_t *s)
{
	struct timespec tm;
	int i = 0;

	while (_tands(s)) {
		if (i < MAX_SPIN_CNT) {
			sched_yield();
			i++;
		} else {
			tm.tv_sec  = 0;
			tm.tv_nsec = SPIN_SLEEP_DURATION;
			nanosleep(&tm, 0);
			i = 0;
		}
	}
}

static __inline int
trylock(psc_spinlock_t *s)
{
	return (_tands(s));
}

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
			return (1);
		else
			/* someone else has it, wait */
			spinlock(sl);
	} else
		/* not locked, grab it */
		spinlock(sl);
	return (0);
}

static __inline int
tryreqlock(psc_spinlock_t *sl, int *locked)
{
	*locked = 0;
	if (sl->sl_lock == SL_LOCKED &&
	    sl->sl_who == pthread_self()) {
		*locked = 1;
		return (0);
	}
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
	if (!waslocked)
		freelock(sl);
}

#else /* !HAVE_LIBPTHREAD */

#include "psc_util/cdefs.h"

typedef int psc_spinlock_t;

/* XXX provide some way to detect reinitializing already-initialized locks. */
#define LOCK_INIT(l)		(*(l) = SL_UNLOCKED)
#define LOCK_INITIALIZER	SL_UNLOCKED

#define _LOCK_VALID(l)		(*(l) == SL_LOCKED || *(l) == SL_UNLOCKED)

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
	        return 1;
	else if (*l != SL_UNLOCKED)
		psc_fatalx("lock %p has invalid value", l);
	*l = SL_LOCKED;
	return 0;
}

static __inline void
ureqlock(psc_spinlock_t *l, int waslocked)
{
	if (!waslocked)
		freelock(l);
	else if (!_LOCK_VALID(l))
		psc_fatalx("lock %p has invalid value", l);
}

#endif /* HAVE_LIBPTHREAD */

#endif /* __PFL_LOCK_H__ */
