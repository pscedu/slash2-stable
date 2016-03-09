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
 * Spin lock implementation optimized for reduced memory footprint.
 */

#ifndef _PFL_MSPINLOCK_H_
#define _PFL_MSPINLOCK_H_

#define PMSL_MAGIC

#include <pthread.h>
#include <sched.h>
#include <string.h>
#include <unistd.h>

#include "pfl/vbitmap.h"
#include "pfl/atomic.h"
#include "pfl/lock.h"
#include "pfl/log.h"
#include "pfl/thread.h"

struct psc_mspinlock {
	psc_atomic16_t	pmsl_value;
#ifdef PMSL_MAGIC
	int		pmsl_magic;
#endif
};

#define PMSL_LOCKMASK	(1 << 15)	/* 10000000_00000000 */
#define PMSL_OWNERMASK	(0x7fff)	/* 01111111_11111111 */

#ifdef PMSL_MAGIC
# define PMSL_MAGICVAL	0x43218765

# define PSC_MAGIC_INIT(pmsl)	((pmsl)->pmsl_magic = PMSL_MAGICVAL)

# define PMSL_MAGIC_CHECK(pmsl)						\
	do {								\
		if ((pmsl)->pmsl_magic != PMSL_MAGICVAL)		\
			psc_fatalx("invalid lock value");		\
	} while (0)

# define PMSL_INIT		{ PSC_ATOMIC16_INIT(0), PMSL_MAGICVAL }
#else
# define PSC_MAGIC_INIT(pmsl)	do { } while (0)
# define PMSL_MAGIC_CHECK(pmsl)	do { } while (0)

# define PMSL_INIT		{ PSC_ATOMIC16_INIT(0) }
#endif

#define psc_mspin_init(pmsl)						\
	do {								\
		PMSL_MAGIC_INIT(pmsl);					\
		psc_atomic16_set((pmsl)->pmsl_value, 0);		\
	} while (0)

#define psc_mspin_ensure(pmsl)						\
	do {								\
		int _v;							\
									\
		PMSL_MAGIC_CHECK(pmsl);					\
		_v = psc_atomic16_read(&(pmsl)->pmsl_value);		\
		if ((_v & PMSL_LOCKMASK) == 0)				\
			psc_fatalx("psc_mspin_ensure: not locked; "	\
			    "pmsl=%p", (pmsl));				\
		if ((_v & PMSL_OWNERMASK) != pscthr_getuniqid())	\
			psc_fatalx("psc_mspin_ensure: not owner "	\
			    "(%p, owner=%d, self=%d)!",	(pmsl),		\
			    _v & PMSL_OWNERMASK,			\
			    pscthr_getuniqid());			\
	} while (0)

#define psc_mspin_unlock(pmsl)						\
	do {								\
		psc_mspin_ensure(pmsl);					\
		psc_atomic16_set(&(pmsl)->pmsl_value, 0);		\
	} while (0)

static __inline int
psc_mspin_trylock(struct psc_mspinlock *pmsl)
{
	int16_t oldval;

	PMSL_MAGIC_CHECK(pmsl);
	oldval = psc_atomic16_setmask_getold(&pmsl->pmsl_value,
	    PMSL_LOCKMASK);
	if (oldval & PMSL_LOCKMASK) {
		if ((oldval & PMSL_OWNERMASK) == pscthr_getuniqid())
			psc_fatalx("already holding the lock");
		return (0);				/* someone else has it */
	}
	oldval = pscthr_getuniqid() | PMSL_LOCKMASK;
	psc_atomic16_set(&pmsl->pmsl_value, oldval);	/* we got it */
	return (1);
}

#define PMSL_SLEEP_THRES	100		/* #spins before sleeping */
#define PMSL_SLEEP_INTV		(5000 - 1)	/* usec */

static __inline void
psc_mspin_lock(struct psc_mspinlock *pmsl)
{
	int i;

	for (i = 0; !psc_mspin_trylock(pmsl); i++, sched_yield())
		if (i > PMSL_SLEEP_THRES) {
			usleep(PMSL_SLEEP_INTV);
			i = 0;
		}
}

static __inline int
psc_mspin_reqlock(struct psc_mspinlock *pmsl)
{
	int v;

	v = psc_atomic16_read(&pmsl->pmsl_value);
	if ((v & PMSL_LOCKMASK) &&
	    (v & PMSL_OWNERMASK) == pscthr_getuniqid())
		return (1);
	psc_mspin_lock(pmsl);
	return (0);
}

static __inline int
psc_mspin_tryreqlock(struct psc_mspinlock *pmsl, int *locked)
{
	int v;

	v = psc_atomic16_read(&pmsl->pmsl_value);
	if ((v & PMSL_LOCKMASK) &&
	    (v & PMSL_OWNERMASK) == pscthr_getuniqid()) {
		*locked = 1;
		return (1);
	}
	*locked = 0;
	return (psc_mspin_trylock(pmsl));
}

#define psc_mspin_ureqlock(pmsl, locked)				\
	do {								\
		if (!(locked))						\
			psc_mspin_unlock(pmsl);				\
	} while (0)

#endif /* _PFL_MSPINLOCK_H_ */
