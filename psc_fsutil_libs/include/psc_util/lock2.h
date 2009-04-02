/* $Id$ */

/*
 * Implementation of spin locks, which spin in a tight loop
 * continuously checking to own while someone else is holding
 * them.
 */

#ifndef _PFL_LOCK2_H_
#define _PFL_LOCK2_H_

#define PSL_MAGIC

#include <sched.h>
#include <time.h>

#include "psc_util/atomic.h"
#include "psc_util/log.h"
#include "psc_util/thread.h"

struct psc_spinlock {
	psc_atomic16_t	psl_value;
#ifdef PSL_MAGIC
	int		psl_magic;
#endif
};

#define PSL_LOCKMASK	(1 << 15)	/* 1000_0000_0000_0000 */
#define PSL_OWNERMASK	(0x7fff)	/* 0111_1111_1111_1111 */

#ifdef PSL_MAGIC
# define PSL_MAGICVAL	0x43218765

# define PSC_MAGIC_INIT(psl)	((psl)->psl_magic = PSL_MAGICVAL)

# define PSL_MAGIC_CHECK(psl)						\
	do {								\
		if ((psl)->psl_magic != PSL_MAGICVAL)			\
			psc_fatalx("invalid lock value");		\
	} while (0)

# define PSL_LOCK_INIT		{ PSC_ATOMIC16_INIT(0), PSL_MAGICVAL }
#else
# define PSC_MAGIC_INIT(psl)	do { } while (0)
# define PSL_MAGIC_CHECK(psl)	do { } while (0)

# define PSL_LOCK_INIT		{ PSC_ATOMIC16_INIT(0) }
#endif

#define psc_spin_init(psl)						\
	do {								\
		PSL_MAGIC_INIT(psl);					\
		psc_atomic16_set((psl)->psl_value, 0);			\
	} while (0)

#define psc_spin_ensure(psl)						\
	do {								\
		int _v;							\
									\
		PSL_MAGIC_CHECK(psl);					\
		_v = psc_atomic16_read(&(psl)->psl_value);		\
		if ((_v & PSL_LOCKMASK) == 0)				\
			psc_fatalx("freelock: not locked (%p)", (psl));	\
		if ((_v & PSL_OWNERMASK) != pscthr_gettid())		\
			psc_fatalx("freelock: not owner "		\
			    "(%p, owner=%d, self=%d)!",	(psl),		\
			    (_v & PSL_OWNERMASK), pscthr_gettid());	\
	} while (0)

#define psc_spin_unlock(psl)						\
	do {								\
		psc_spin_ensure(psl);					\
		psc_atomic16_set(&(psl)->psl_value, 0);			\
	} while (0)

static inline int
psc_spin_trylock(struct psc_spinlock *psl)
{
	int16_t oldval;

	PSL_MAGIC_CHECK(psl);
	oldval = psc_atomic16_setmask_ret(&psl->psl_value, PSL_LOCKMASK);
	if (oldval & PSL_LOCKMASK) {
		if ((oldval & PSL_OWNERMASK) == pscthr_gettid())
			psc_fatalx("already holding the lock");
		return (0);				/* someone else has it */
	}
	oldval = pscthr_gettid() | PSL_LOCKMASK;
	psc_atomic16_set(&psl->psl_value, oldval);	/* we got it */
	return (1);
}

#define PSL_SPIN_THRES		100
#define PSL_SPIN_SLEEP		(2 * 1000 * 1000 + 1)	/* nsec */

static inline void
psc_spin_lock(struct psc_spinlock *psl)
{
	struct timespec tm;
	int i = 0;

	while (!psc_spin_trylock(psl)) {
		if (i < PSL_SPIN_THRES) {
			sched_yield();
			i++;
		} else {
			tm.tv_sec = 0;
			tm.tv_nsec = PSL_SPIN_SLEEP;
			nanosleep(&tm, NULL);
			i = 0;
		}
	}
}

static inline int
psc_spin_reqlock(struct psc_spinlock *psl)
{
	int v;

	v = psc_atomic16_read(&psl->psl_value);
	if ((v & PSL_LOCKMASK) &&
	    (v & PSL_OWNERMASK) == pscthr_gettid())
		return (1);
	psc_spin_lock(psl);
	return (0);
}

static inline int
psc_spin_tryreqlock(struct psc_spinlock *psl, int *locked)
{
	int v;

	v = psc_atomic16_read(&psl->psl_value);
	if ((v & PSL_LOCKMASK) &&
	    (v & PSL_OWNERMASK) == pscthr_gettid()) {
		*locked = 1;
		return (1);
	}
	*locked = 0;
	return (psc_spin_trylock(psl));
}

#define psc_spin_ureqlock(psl, locked)					\
	do {								\
		if (!(locked))						\
			psc_spin_unlock(psl);				\
	} while (0)

#endif /* _PFL_LOCK2_H_ */
