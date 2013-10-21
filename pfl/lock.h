/* $Id$ */
/*
 * %PSC_START_COPYRIGHT%
 * -----------------------------------------------------------------------------
 * Copyright (c) 2006-2013, Pittsburgh Supercomputing Center (PSC).
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

#include "pfl/_atomic32.h"
#include "pfl/time.h"
#include "pfl/types.h"
#include "psc_util/log.h"

#ifndef HAVE_LIBPTHREAD
#  define pthread_self() ((pthread_t)1)
#endif

#define PSLRV_WASLOCKED		42
#define PSLRV_WASNOTLOCKED	43

enum psc_spinlock_val {
	PSL_UNLOCKED = 2,
	PSL_LOCKED = 3
};

typedef struct psc_spinlock {
	psc_atomic32_t		psl_value;
	int			psl_flags;
	pthread_t		psl_owner;
#ifdef LOCK_TIMING
	struct timeval		psl_time;
#endif
} psc_spinlock_t;

#define PSLF_NOLOG		(1 << 0)	/* don't psclog locks/unlocks */
#define PSLF_LOGTMP		(1 << 1)	/* psclog to tmp subsystem */

#define PSL_SLEEP_NTRIES	256
#define PSL_SLEEP_NSEC		5001

#define _SPIN_GETATOM(psl)	((psc_atomic32_t *)(void *)&(psl)->psl_value)

#define INIT_SPINLOCK_FLAGS(psl, flg)					\
	do {								\
		memset((psl), 0, sizeof(*(psl)));			\
		psc_atomic32_set(_SPIN_GETATOM(psl), PSL_UNLOCKED);	\
		(psl)->psl_flags = (flg);				\
	} while (0)

#define INIT_SPINLOCK(psl)	 INIT_SPINLOCK_FLAGS((psl), 0)
#define INIT_SPINLOCK_NOLOG(psl) INIT_SPINLOCK_FLAGS((psl), PSLF_NOLOG)
#define INIT_SPINLOCK_LOGTMP(psl)INIT_SPINLOCK_FLAGS((psl), PSLF_LOGTMP)

#ifdef LOCK_TIMING
#  define SPINLOCK_INIT		{ PSC_ATOMIC32_INIT(PSL_UNLOCKED), 0, 0, { 0, 0 } }
#  define SPINLOCK_INIT_NOLOG	{ PSC_ATOMIC32_INIT(PSL_UNLOCKED), PSLF_NOLOG, 0, { 0, 0 } }
#  define SPINLOCK_INIT_LOGTMP	{ PSC_ATOMIC32_INIT(PSL_UNLOCKED), PSLF_LOGTMP, 0, { 0, 0 } }
#else
#  define SPINLOCK_INIT		{ PSC_ATOMIC32_INIT(PSL_UNLOCKED), 0, 0 }
#  define SPINLOCK_INIT_NOLOG	{ PSC_ATOMIC32_INIT(PSL_UNLOCKED), PSLF_NOLOG, 0 }
#  define SPINLOCK_INIT_LOGTMP	{ PSC_ATOMIC32_INIT(PSL_UNLOCKED), PSLF_LOGTMP, 0 }
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
		if ((psl)->psl_owner != pthread_self())			\
			psc_fatalx("%s: not owner "			\
			    "(lock %p, owner %"PSCPRI_PTHRT", "		\
			    "self %"PSCPRI_PTHRT")", (name),		\
			    (psl), (psl)->psl_owner, pthread_self());	\
	} while (0)

#define _SPIN_TEST_AND_SET(pci, name, psl)				\
	{								\
		enum psc_spinlock_val _val;				\
		int _lrc;						\
									\
		_val = PSC_ATOMIC32_XCHG(_SPIN_GETATOM(psl),		\
		    PSL_LOCKED);					\
		if ((_val) == PSL_LOCKED) {				\
			if ((psl)->psl_owner == pthread_self())		\
				_psclog_pci((pci), PLL_FATAL, 0,	\
				    "%s %p: already locked", (name),	\
				    (psl));				\
			/* PFL_GETTIMEVAL(&(psl)->psl_time); */		\
			_lrc = 0;					\
		} else if ((_val) == PSL_UNLOCKED) {			\
			psc_assert((psl)->psl_owner == 0);		\
			(psl)->psl_owner = pthread_self();		\
			if (((psl)->psl_flags & PSLF_NOLOG) == 0)	\
				_psclog_pci((pci), PLL_VDEBUG, 0,	\
				    "lock %p acquired",	(psl));		\
			_lrc = 1;					\
		} else							\
			_psclog_pci((pci), PLL_FATAL, 0,		\
			    "%s: lock %p has invalid value %#x",	\
			    (name), (psl), (_val));			\
		_lrc;							\
	}

/**
 * trylock - Try to acquire a lock; will not block if spinlock is not
 *	immediately available.
 * @psl: the spinlock.
 */
#define trylock_pci(pci, psl)	(_SPIN_TEST_AND_SET((pci), "trylock", (psl)))

/**
 * freelock - Block until the caller locks a spinlock for a critical
 *	section.
 * @psl: the spinlock.
 */
#define spinlock_pci(pci, psl)						\
	do {								\
		struct timespec _tm;					\
		int _i;							\
									\
		for (_i = 0;						\
		    !(_SPIN_TEST_AND_SET((pci), "spinlock", (psl)));	\
		    sched_yield(), _i++)				\
			if (_i >= PSL_SLEEP_NTRIES) {			\
				_tm.tv_sec  = 0;			\
				_tm.tv_nsec = PSL_SLEEP_NSEC;		\
				nanosleep(&_tm, 0);			\
				_i = 0;					\
			}						\
	} while (0)

/**
 * freelock - Release a spinlock that is locked by the caller.
 * @psl: the spinlock.
 */
#define freelock_pci(pci, psl)						\
	do {								\
		_SPIN_ENSURELOCKED("freelock", (psl));			\
		_psc_spin_checktime(psl);				\
		(psl)->psl_owner = 0;					\
		psc_atomic32_set(_SPIN_GETATOM(psl), PSL_UNLOCKED);	\
		if (((psl)->psl_flags & PSLF_NOLOG) == 0)		\
			_psclog_pci((pci), PLL_VDEBUG, 0,		\
			    "lock %p released", (psl));			\
	} while (0)

/**
 * reqlock - Require a lock for a critical section.  Locks if unlocked,
 *	doesn't if already locked (to avoid deadlock).
 * @psl: the spinlock.
 * Returns a value that ureqlock() must use.
 */
#define reqlock_pci(pci, psl)						\
	(psc_spin_haslock(psl) ? PSLRV_WASLOCKED :			\
	    ((void)PSC_MAKETRUE(spinlock_pci((pci), (psl))), PSLRV_WASNOTLOCKED))

/**
 * tryreqlock - Try to require a lock.  Will not block if the lock
 *	cannot be obtained immediately.
 * @psl: the spinlock.
 * @waslockedp: value-result to "unrequire" lock.
 */
#define tryreqlock_pci(pci, psl, waslockedp)				\
	(psc_spin_haslock(psl) ?					\
	    (*(waslockedp) = PSLRV_WASLOCKED, 1) :			\
	    (*(waslockedp) = PSLRV_WASNOTLOCKED, trylock_pci((pci), (psl))))

/**
 * ureqlock - "Unrequire" a lock -- unlocks the lock if it was locked
 *	for the nearest "reqlock ... ureqlock" section and doesn't if
 *	the lock was already locked before the critical section began.
 * @psl: the spinlock.
 * @waslkd: return value from reqlock().
 */
#define ureqlock_pci(pci, psl, waslocked)				\
	do {								\
		if ((waslocked) == PSLRV_WASNOTLOCKED)			\
			freelock_pci((pci), (psl));			\
		else							\
			_SPIN_CHECK("ureqlock", (psl));			\
	} while (0)

#define LOCK_ENSURE_RC(psl)	PSC_MAKETRUE(_SPIN_ENSURELOCKED("LOCK_ENSURE", (psl)))
#define LOCK_ENSURE(psl)	_SPIN_ENSURELOCKED("LOCK_ENSURE", (psl))

#define _SPIN_CALLERINFO(psl)	((psl)->psl_flags & PSLF_LOGTMP ?	\
				 PFL_CALLERINFOSS(PSS_TMP) : PFL_CALLERINFO())

#define ureqlock(psl, lkd)	ureqlock_pci(_SPIN_CALLERINFO(psl), (psl), (lkd))
#define tryreqlock(psl, lkd)	tryreqlock_pci(_SPIN_CALLERINFO(psl), (psl), (lkd))
#define reqlock(psl)		reqlock_pci(_SPIN_CALLERINFO(psl), (psl))
#define freelock(psl)		freelock_pci(_SPIN_CALLERINFO(psl), (psl))
#define trylock(psl)		trylock_pci(_SPIN_CALLERINFO(psl), (psl))
#define spinlock(psl)		spinlock_pci(_SPIN_CALLERINFO(psl), (psl))

static __inline void _psc_spin_checktime(struct psc_spinlock *);

#ifndef _PFL_ATOMIC_H_
#  include "psc_util/atomic.h"
#endif

#define SETATTR_LOCKED(lk, field, fl)					\
	do {								\
		int _locked;						\
									\
		_locked = reqlock(lk);					\
		*(field) |= (fl);					\
		psclog_diag("lock %p setflags %s:%d now %#x",		\
		    (lk), #fl, (fl), *(field));				\
		ureqlock((lk), _locked);				\
	} while (0)

#define CLEARATTR_LOCKED(lk, field, fl)					\
	do {								\
		int _locked;						\
									\
		_locked = reqlock(lk);					\
		*(field) &= ~(fl);					\
		psclog_diag("lock %p clearflags %s:%d now %#x",		\
		    (lk), #fl, (fl), *(field));				\
		ureqlock((lk), _locked);				\
	} while (0)

static __inline void
_psc_spin_checktime(struct psc_spinlock *psl)
{
#ifdef LOCK_TIMING
	struct timeval now, diff;

	PFL_GETTIMEVAL(&now);
	max.tv_sec = 0;
	max.tv_usec = 500;
	timersub(&now, &psl->psl_time, &diff);
	if (timercmp(&diff, &max, >))
		psclog_errorx("lock %p held long (%luus)",
		    psl, diff.tv_sec * 1000 * 1000 + diff.tv_usec);
#else
	(void)psl;
#endif
}

static __inline int
psc_spin_haslock(psc_spinlock_t *psl)
{
	_SPIN_CHECK("psc_spin_haslock", psl);
	/*
	 * This code is thread safe because even if psl_owner changes, it
	 * won't be set to us.
	 */
	return (_SPIN_GETVAL(psl) == PSL_LOCKED &&
	    psl->psl_owner == pthread_self());
}

#endif /* _PFL_SPINLOCK_H_ */
