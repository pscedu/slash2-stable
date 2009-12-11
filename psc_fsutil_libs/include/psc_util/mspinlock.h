/* $Id$ */

/*
 * Spin lock implementation optimized for reduced memory footprint.
 */

#ifndef _PFL_MSPINLOCK_H_
#define _PFL_MSPINLOCK_H_

#define PMSL_MAGIC

#include <pthread.h>
#include <sched.h>
#include <unistd.h>

#include "psc_ds/vbitmap.h"
#include "psc_util/atomic.h"
#include "psc_util/lock.h"
#include "psc_util/log.h"
#include "psc_util/thread.h"

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

extern struct psc_vbitmap	*_psc_mspin_unthridmap;
extern psc_spinlock_t		 _psc_mspin_unthridmap_lock;
extern pthread_key_t		 _psc_mspin_thrkey;

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
		if ((_v & PMSL_OWNERMASK) != _psc_mspin_getunthrid())	\
			psc_fatalx("freelock: not owner "		\
			    "(%p, owner=%d, self=%d)!",	(pmsl),		\
			    _v & PMSL_OWNERMASK,			\
			    _psc_mspin_getunthrid());			\
	} while (0)

#define psc_mspin_unlock(pmsl)						\
	do {								\
		psc_mspin_ensure(pmsl);					\
		psc_atomic16_set(&(pmsl)->pmsl_value, 0);		\
	} while (0)

static __inline void
_psc_mspin_thrteardown(void *arg)
{
	uint16_t thrid = (unsigned long)arg;

	spinlock(&_psc_mspin_unthridmap_lock);
	psc_vbitmap_unset(_psc_mspin_unthridmap, thrid);
	psc_vbitmap_setnextpos(_psc_mspin_unthridmap, 0);
	freelock(&_psc_mspin_unthridmap_lock);
}

/*
 * Thread IDs are not guarenteed to be 16-bits on some systems, and since we
 * never have 2^16 threads, it is wasted space when you use lots of spinlocks,
 * so use our own unique thread ID scheme.
 */
static __inline int16_t
_psc_mspin_getunthrid(void)
{
	static int init;
	uint16_t thrid;
	int rc;

	if (!init) {
		spinlock(&_psc_mspin_unthridmap_lock);
		if (!init) {
			rc = pthread_key_create(&_psc_mspin_thrkey,
			    _psc_mspin_thrteardown);
			if (rc)
				psc_fatalx("pthread_key_create: %s",
				    strerror(rc));
			_psc_mspin_unthridmap = psc_vbitmap_newf(0, PVBF_AUTO);
			init = 1;
		}
		freelock(&_psc_mspin_unthridmap_lock);
	}

	thrid = (unsigned long)pthread_getspecific(_psc_mspin_thrkey);
	if (thrid == 0) {
		size_t arg;

		spinlock(&_psc_mspin_unthridmap_lock);
		if (psc_vbitmap_next(_psc_mspin_unthridmap, &arg) == -1)
			psc_fatal("psc_vbitmap_next");
		thrid = arg + 1;
		freelock(&_psc_mspin_unthridmap_lock);

		rc = pthread_setspecific(_psc_mspin_thrkey, (void *)(unsigned long)thrid);
		if (rc)
			psc_fatalx("pthread_setspecific: %s", strerror(rc));
	}
	return (thrid);
}

static __inline int
psc_mspin_trylock(struct psc_mspinlock *pmsl)
{
	int16_t oldval;

	PMSL_MAGIC_CHECK(pmsl);
	oldval = psc_atomic16_setmask_getold(&pmsl->pmsl_value,
	    PMSL_LOCKMASK);
	if (oldval & PMSL_LOCKMASK) {
		if ((oldval & PMSL_OWNERMASK) == _psc_mspin_getunthrid())
			psc_fatalx("already holding the lock");
		return (0);				/* someone else has it */
	}
	oldval = _psc_mspin_getunthrid() | PMSL_LOCKMASK;
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
	    (v & PMSL_OWNERMASK) == _psc_mspin_getunthrid())
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
	    (v & PMSL_OWNERMASK) == _psc_mspin_getunthrid()) {
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
