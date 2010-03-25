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
 * XXX: i386 atomic recursively depends on this to implement 64-bit
 * cmpxchg on machines that do not have the cmpxchg8b extension.
 */

#ifndef _PFL_SPINLOCK_H_
#define _PFL_SPINLOCK_H_

#include <sys/time.h>

#include <pthread.h>
#include <sched.h>
#include <unistd.h>

#include "psc_util/atomic.h"
#include "psc_util/log.h"
#include "psc_util/time.h"

struct psc_spinlock {
	psc_atomic32_t	psl_value;
	pthread_t	psl_who;
	struct timeval	psl_time;
};

#define PSL_UNLOCKED		40
#define PSL_LOCKED		41

#define PRSL_WASLOCKED		42
#define PRSL_WASNOTLOCKED	43

#define PSL_SLEEP_NSPINS	100			/* #tries before sleep */
#define PSL_SLEEP_INTV		(5000 - 1)		/* usec */

#define PSC_SPINLOCK_INIT	{ ATOMIC_INIT(PSL_UNLOCKED), 0, { 0, 0 } }

/**
 * psc_spin_init - Initialize a spinlock.
 * @psl: spinlock.
 */
static __inline void
psc_spin_init(struct psc_spinlock *psl)
{
	memset(psl, 0, sizeof(*psl));
	psc_atomic32_set(&psl->psl_value, PSL_UNLOCKED);
}

/**
 * psc_spin_ensure - Ensure that a spinlock is owned by the caller.
 * @psl: spinlock.
 */
static __inline int
psc_spin_ensure(struct psc_spinlock *psl)
{
	int v;

	v = psc_atomic32_read(&psl->psl_value);
	if (v == PSL_UNLOCKED)
		psc_fatalx("psc_spin_ensure: not locked");
	if (v != PSL_LOCKED)
		psc_fatalx("psc_spin_ensure: invalid value");
	if (psl->psl_who != pthread_self())
		psc_fatalx("psc_spin_ensure: not our lock; "
		    "psl=%p, owner=%lu, self=%lu", (psl),
		    psl->psl_who, pthread_self());
	return (1);
}

/**
 * psc_spin_unlock - Release a spinlock.
 * @psl: spinlock.
 */
static __inline void
psc_spin_unlock(struct psc_spinlock *psl)
{
	struct timeval now, max, diff;

	psc_spin_ensure(psl);
	if (psl->psl_who != pthread_self())
		psc_fatalx("psc_spin_unlock: not owner; "
		    "psl=%p, owner=%lu, self=%lu",
		    psl, psl->psl_who, pthread_self());
	PFL_GETTIME(&now);
	max.tv_sec = 0;
	max.tv_usec = 500;
	timersub(&now, &psl->psl_time, &diff);
	if (timercmp(&diff, &max, >))
		psc_errorx("psc_spin_unlock: lock held long; "
		    "psl=%p, len=%luus", psl, diff.tv_sec *
		    1000 * 1000 + diff.tv_usec);
	psl->psl_who = 0;
	psc_atomic32_set(&psl->psl_value, PSL_UNLOCKED);
}

/**
 * psc_spin_trylock - Attempt to acquire a spinlock.
 * @psl: spinlock.
 * Returns Boolean true if attempt was successful or false otherwise.
 */
static __inline int
psc_spin_trylock(struct psc_spinlock *psl)
{
	int v;

	v = psc_atomic32_xchg(&psl->psl_value, PSL_LOCKED);
	if (v == PSL_LOCKED) {
		if (psl->psl_who == pthread_self())
			psc_fatalx("already holding the lock");
		return (0);			/* someone else has it */
	} else if (v == PSL_UNLOCKED) {
		psl->psl_who = pthread_self();	/* we got it */
		PFL_GETTIME(&psl->psl_time);
		return (1);
	}
	psc_fatalx("psc_spin_trylock: invalid value; psl=%p, value=%d",
	    psl, v);
}

/**
 * psc_spin_lock - Acquire a spinlock, waiting as long as necessary.
 * @psl: spinlock.
 */
static __inline void
psc_spin_lock(struct psc_spinlock *psl)
{
	int i;

	for (i = 0; !psc_spin_trylock(psl); i++, sched_yield())
		if (i > PSL_SLEEP_NSPINS) {
			usleep(PSL_SLEEP_INTV);
			i = 0;
		}
}

/**
 * psc_spin_reqlock - Require spinlock ownership.  If already owned,
 *	this is effectively a no-op but returns a value for symmetry
 *	with psc_spin_ureqlock().
 * @psl: spinlock.
 * Returns Boolean true if the lock is already locked, false otherwise;
 *	this value should usually be passed to a corresponding
 *	psc_spin_ureqlock() call.
 */
static __inline int
psc_spin_reqlock(struct psc_spinlock *psl)
{
	if (psc_atomic32_read(&psl->psl_value) == PSL_LOCKED &&
	    psl->psl_who == pthread_self())
		return (PRSL_WASLOCKED);	/* we've already locked it */
	psc_spin_lock(psl);			/* someone else has it, wait */
	return (PRSL_WASNOTLOCKED);
}

/**
 * psc_spin_tryreqlock - Try to require a spinlock.
 * @psl: spinlock.
 * @locked: value-result Boolean of whether we already own the lock.
 * Returns Boolean true if lock attempt was successful, false otherwise.
 */
static __inline int
psc_spin_tryreqlock(struct psc_spinlock *psl, int *locked)
{
	if (psc_atomic32_read(&psl->psl_value) == PSL_LOCKED &&
	    psl->psl_who == pthread_self()) {
		*locked = PRSL_WASLOCKED;
		return (1);
	}
	*locked = PRSL_WASNOTLOCKED;
	return (psc_spin_trylock(psl));
}

/*
 * psc_spin_ureqlock - "Unrequire" a spinlock; i.e. perform an actual
 *	unlock unless the lock was owned by us prior to the last
 *	encompassing "psc_spin_reqlock() ... psc_spin_ureqlock()"
 *	section.
 * @psl: the lock.
 * @waslocked: return value from psc_spin_reqlock().
 */
static __inline void
psc_spin_ureqlock(struct psc_spinlock *psl, int waslocked)
{
	if (waslocked == PRSL_WASNOTLOCKED)
		psc_spin_unlock(psl);
	else if (waslocked != PRSL_WASLOCKED)
		psc_fatalx("psc_spin_ureqlock: bad value");
}

#endif /* _PFL_SPINLOCK_H_ */
