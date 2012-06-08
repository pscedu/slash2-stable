/* $Id$ */
/*
 * %PSC_START_COPYRIGHT%
 * -----------------------------------------------------------------------------
 * Copyright (c) 2006-2012, Pittsburgh Supercomputing Center (PSC).
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
 * Multiwait is an API for waiting on any of a number of conditions to
 * become available.
 */

#include <sys/time.h>

#include <errno.h>
#include <pthread.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "pfl/cdefs.h"
#include "pfl/time.h"
#include "psc_ds/vbitmap.h"
#include "psc_util/bitflag.h"
#include "psc_util/log.h"
#include "psc_util/multiwait.h"
#include "psc_util/pthrutil.h"

/**
 * psc_multiwaitcond_init - Initialize a multiwait condition.
 * @mwc: the condition to initialize.
 * @data: pointer to user data.
 * @flags: behavioral flags.
 * @name: printf(3)-like name.
 */
void
psc_multiwaitcond_init(struct psc_multiwaitcond *mwc, const void *data,
    int flags, const char *name, ...)
{
	va_list ap;

	memset(mwc, 0, sizeof(*mwc));
	psc_dynarray_init(&mwc->mwc_multiwaits);
	psc_mutex_init(&mwc->mwc_mutex);
	pthread_cond_init(&mwc->mwc_cond, NULL);
	mwc->mwc_data = data;
	mwc->mwc_flags = flags;

	va_start(ap, name);
	vsnprintf(mwc->mwc_name, sizeof(mwc->mwc_name), name, ap);
	va_end(ap);
}

/**
 * psc_multiwaitcond_trylockallmw - Try to lock all multiwaits this
 *	condition is a member of.
 * @mwc: a multiwaitcond, which must itself be locked.
 * Returns zero on success, -1 on failure.
 */
__static int
psc_multiwaitcond_trylockallmw(struct psc_multiwaitcond *mwc)
{
	struct psc_multiwait *mw;
	int j, k;

	DYNARRAY_FOREACH(mw, j, &mwc->mwc_multiwaits)
		if (!psc_mutex_trylock(&mw->mw_mutex)) {
			/*
			 * Unable to lock them all; release what we did
			 * lock and give up.
			 */
			for (k = 0; k < j; k++) {
				mw = psc_dynarray_getpos(
				    &mwc->mwc_multiwaits, k);
				psc_mutex_unlock(&mw->mw_mutex);
			}
			return (-1);
		}
	return (0);
}

/**
 * psc_multiwaitcond_unlockallmw - Unlock all multiwaits this
 *	condition is a member of.
 * @mwc: a multiwait condition, which must itself be locked.
 */
__static void
psc_multiwaitcond_unlockallmw(struct psc_multiwaitcond *mwc)
{
	struct psc_multiwait *mw;
	int j;

	DYNARRAY_FOREACH(mw, j, &mwc->mwc_multiwaits)
		psc_mutex_unlock(&mw->mw_mutex);
}

__static int
psc_multiwaitcond_cmp(const void *a, const void *b)
{
	return (CMP(a, b));
}

/**
 * psc_multiwaitcond_destroy - Release a multiwait condition.
 * @mwc: the condition to release.
 */
void
psc_multiwaitcond_destroy(struct psc_multiwaitcond *mwc)
{
	struct psc_multiwait *mw;
	int j, k, count;

	count = 0;
 restart:
	psc_mutex_lock(&mwc->mwc_mutex);
	DYNARRAY_FOREACH_REVERSE(mw, j, &mwc->mwc_multiwaits) {
		if (!psc_mutex_trylock(&mw->mw_mutex)) {
			if (count++ == 300000)
				psclog_errorx("mwcond %s failed to lock his "
				    "multiwaits after many attempts, "
				    "possible deadlock", mwc->mwc_name);
			psc_mutex_unlock(&mwc->mwc_mutex);
			sched_yield();
			goto restart;
		}
		if (mw->mw_flags & PMWF_CRITSECT &&
		    mw->mw_waker == mwc)
			psc_fatalx("waking condition %s wants to go away "
			    "but may corrupt integrity of multiwait %s",
			    mwc->mwc_name, mw->mw_name);
		psc_assert(psc_dynarray_len(&mw->mw_conds) ==
		    (int)psc_vbitmap_getsize(mw->mw_condmask));

		k = psc_dynarray_bsearch(&mwc->mwc_multiwaits, mw,
		    psc_multiwaitcond_cmp);
		psc_dynarray_splice(&mwc->mwc_multiwaits, k, 1, NULL, 0);

		k = psc_dynarray_removeitem(&mw->mw_conds, mw);
		pfl_bitstr_copy(&mw->mw_condmask, k, &mw->mw_condmask,
		    k + 1, psc_vbitmap_getsize(mw->mw_condmask) - k);
		psc_vbitmap_resize(mw->mw_condmask,
		    psc_dynarray_len(&mw->mw_conds));

		psc_mutex_unlock(&mw->mw_mutex);
	}
	psc_dynarray_free(&mwc->mwc_multiwaits);
	/* XXX: ensure no one is waiting on this mutex? */
	psc_mutex_unlock(&mwc->mwc_mutex);
	psc_mutex_destroy(&mwc->mwc_mutex);
	pthread_cond_destroy(&mwc->mwc_cond);

#if PFL_DEBUG > 0
	memset(mwc, 0, sizeof(*mwc));
#endif
}

/**
 * psc_multiwait_iscondwakeable - Determine if a condition is masked
 *	off in a multiwait.
 * @mw: multiwait to check in, which must be locked.
 * @mwc: condition to check for.
 */
int
psc_multiwait_iscondwakeable(struct psc_multiwait *mw,
    const struct psc_multiwaitcond *mwc)
{
	struct psc_multiwaitcond *c;
	int j;

	psc_mutex_ensure_locked(&mw->mw_mutex);
	DYNARRAY_FOREACH(c, j, &mw->mw_conds)
		if (c == mwc)
			return (psc_vbitmap_get(mw->mw_condmask, j));
	psc_fatalx("could not find condition %s in multiwait %s",
	    mwc->mwc_name, mw->mw_name);
}

/**
 * psc_multiwaitcond_wakeup - Wakeup multiwaits waiting on a condition.
 * @mwc: a multiwait condition, which must be unlocked on entry
 *	and will be locked on exit.
 */
void
psc_multiwaitcond_wakeup(struct psc_multiwaitcond *mwc)
{
	struct psc_multiwait *mw;
	int j, count;

	count = 0;

 restart:
	psc_mutex_lock(&mwc->mwc_mutex);
	if (psc_multiwaitcond_trylockallmw(mwc)) {
		if (count++ == 300000)
			psclog_errorx("mwcond %s failed to lock his "
			    "multiwaits after attempting many times, "
			    "possible deadlock", mwc->mwc_name);
		psc_mutex_unlock(&mwc->mwc_mutex);
		sched_yield();
		goto restart;
	}
	DYNARRAY_FOREACH(mw, j, &mwc->mwc_multiwaits)
		if (psc_multiwait_iscondwakeable(mw, mwc)) {
			mw->mw_waker = mwc;
			psclog_info("wake mw %p %s", mw, mw->mw_name);
			pthread_cond_signal(&mw->mw_cond);
		}
	psc_multiwaitcond_unlockallmw(mwc);
	psclog_info("wake cond %p %s", mwc, mwc->mwc_name);
	pthread_cond_broadcast(&mwc->mwc_cond);
	mwc->mwc_winner = NULL;
	psc_mutex_unlock(&mwc->mwc_mutex);
}

/**
 * psc_multiwaitcond_waitrel_ts - Wait for one condition to occur.
 * @mwc: the multiwait condition to wait for.
 * @mutex: an optional mutex that will be unlocked in the critical
 *	section, for avoiding missed wakeups from races.
 * @reltime: amount of time to wait (relative to "now") or NULL for
 *	forever.
 */
int
psc_multiwaitcond_waitrel_ts(struct psc_multiwaitcond *mwc,
    struct pfl_mutex *mutex, const struct timespec *reltime)
{
	struct timespec abstime;
	int rc;

	psc_mutex_lock(&mwc->mwc_mutex);
	if (mutex)
		psc_mutex_unlock(mutex);

	psclog_info("wait cond %p %s", mwc, mwc->mwc_name);
	if (reltime) {
		PFL_GETTIMESPEC(&abstime);
		timespecadd(&abstime, reltime, &abstime);

		rc = pthread_cond_timedwait(&mwc->mwc_cond,
		    &mwc->mwc_mutex.pm_mutex, &abstime);
		if (rc && rc != ETIMEDOUT)
			psc_fatalx("pthread_cond_timedwait: %s",
			    strerror(rc));
	} else {
		rc = pthread_cond_wait(&mwc->mwc_cond,
		    &mwc->mwc_mutex.pm_mutex);
		if (rc)
			psc_fatalx("pthread_cond_wait: %s",
			    strerror(rc));
	}
	psc_mutex_unlock(&mwc->mwc_mutex);
	return (rc);
}

/**
 * psc_multiwait_addcond - Add a condition to a multiwait.
 * @mw: a multiwait.
 * @mwc: the condition to add.
 * @active: whether condition is active.
 */
int
_psc_multiwait_addcond(struct psc_multiwait *mw,
    struct psc_multiwaitcond *mwc, int active)
{
	struct psc_multiwaitcond *c;
	struct psc_multiwait *m;
	int rc = 0, k, j;

	/* Acquire locks. */
	for (;;) {
		psc_mutex_lock(&mwc->mwc_mutex);
		if (psc_mutex_trylock(&mw->mw_mutex))
			break;
		psc_mutex_unlock(&mwc->mwc_mutex);
		sched_yield();
	}

	/* Ensure no associations already exist. */
	DYNARRAY_FOREACH(c, j, &mw->mw_conds)
		if (c == mwc)
			psc_fatalx("mwc %s already registered in multiwait %s",
			    mwc->mwc_name, mw->mw_name);
	DYNARRAY_FOREACH(m, j, &mwc->mwc_multiwaits)
		if (m == mw)
			psc_fatalx("mw %s already registered multiwaitcond %s",
			    mw->mw_name, mwc->mwc_name);

	psc_assert(psc_dynarray_len(&mw->mw_conds) ==
	    (int)psc_vbitmap_getsize(mw->mw_condmask));

	/* Associate condition and multiwait with each other. */
	if (psc_dynarray_add(&mw->mw_conds, mwc) == -1) {
		rc = -1;
		goto done;
	}

	j = psc_dynarray_len(&mw->mw_conds);
	if (psc_vbitmap_resize(mw->mw_condmask, j) == -1) {
		rc = -1;
		psc_dynarray_removeitem(&mw->mw_conds, mwc);
		goto done;
	}

	k = psc_dynarray_bsearch(&mwc->mwc_multiwaits, mw,
	    psc_multiwaitcond_cmp);
	if (psc_dynarray_splice(&mwc->mwc_multiwaits,
	    k, 0, &mw, 1) == -1) {
		rc = -1;
		if (psc_vbitmap_resize(mw->mw_condmask, j - 1) == -1)
			psc_fatalx("unable to undo bitmask changes");
		psc_dynarray_removeitem(&mw->mw_conds, mwc);
		goto done;
	}

	psc_vbitmap_setval(mw->mw_condmask, j - 1, active);

 done:
	psc_mutex_unlock(&mw->mw_mutex);
	psc_mutex_unlock(&mwc->mwc_mutex);
	return (rc);
}

/**
 * psc_multiwait_init - Initialize a multiwait.
 * @mw: the multiwait to initialize.
 */
void
psc_multiwait_init(struct psc_multiwait *mw, const char *name, ...)
{
	va_list ap;

	memset(mw, 0, sizeof(*mw));
	psc_dynarray_init(&mw->mw_conds);
	psc_mutex_init(&mw->mw_mutex);
	pthread_cond_init(&mw->mw_cond, NULL);
	mw->mw_condmask = psc_vbitmap_new(0);
	if (mw->mw_condmask == NULL)
		psc_fatal("psc_vbitmap_new");

	va_start(ap, name);
	vsnprintf(mw->mw_name, sizeof(mw->mw_name), name, ap);
	va_end(ap);
}

/**
 * psc_multiwait_free - Destroy a multiwait.
 * @mw: the multiwait to release.
 */
void
psc_multiwait_free(struct psc_multiwait *mw)
{
	psc_multiwait_reset(mw);
	psc_mutex_destroy(&mw->mw_mutex);
	psc_dynarray_free(&mw->mw_conds);
	psc_vbitmap_free(mw->mw_condmask);

#if PFL_DEBUG > 0
	memset(mw, 0, sizeof(*mw));
#endif
}

/**
 * psc_multiwait_setcondwakeable - Update a multiwait's active
 *	conditions mask.
 * @mw: the multiwait whose mask to modify.
 * @mwc: the condition in the mask to modify.
 * @active: whether the condition can wake the multiwait.
 */
void
psc_multiwait_setcondwakeable(struct psc_multiwait *mw,
    const struct psc_multiwaitcond *mwc, int active)
{
	struct psc_multiwaitcond *c;
	int j;

	psc_mutex_lock(&mw->mw_mutex);
	DYNARRAY_FOREACH(c, j, &mw->mw_conds)
		if (c == mwc) {
			psc_vbitmap_setval(mw->mw_condmask, j, active);
			psc_mutex_unlock(&mw->mw_mutex);
			return;
		}
	psc_fatalx("couldn't find mwcond %s in multiwait %s",
	    mwc->mwc_name, mw->mw_name);
}

int
psc_multiwait_usecs(struct psc_multiwait *mw, void *datap, int usec)
{
	struct psc_multiwaitcond *mwc;
	int rc, won, j;

	won = 0;

 restart:
	psc_mutex_lock(&mw->mw_mutex);

	/* Check for missed wakeups during a critical section. */
	if (mw->mw_flags & PMWF_CRITSECT) {
		mw->mw_flags &= ~PMWF_CRITSECT;
		if (mw->mw_waker)
			goto checkwaker;
	}

	if (psc_dynarray_len(&mw->mw_conds) == 0)
		psc_fatalx("multiwait %s has no conditions and "
		    "will never wake up", mw->mw_name);

	/* check for no active conditions */
	for (j = 0; j < psc_dynarray_len(&mw->mw_conds); j++)
		if (psc_vbitmap_get(mw->mw_condmask, j))
			goto wait;
	psc_fatalx("multiwait %s has all conditions masked and "
	    "will never wake up", mw->mw_name);

 wait:
	psclog_info("wait mw %p %s", mw, mw->mw_name);
	if (usec) {
		struct timeval tv, res, adj;
		struct timespec ntv;

		PFL_GETTIMEVAL(&tv);
		adj.tv_sec = usec / 1000000;
		adj.tv_usec = usec % 1000000;
		timeradd(&tv, &adj, &res);
		ntv.tv_sec = res.tv_sec;
		ntv.tv_nsec = res.tv_usec * 1000;

		rc = pthread_cond_timedwait(&mw->mw_cond,
		    &mw->mw_mutex.pm_mutex, &ntv);
		if (rc == ETIMEDOUT) {
			psc_mutex_unlock(&mw->mw_mutex);
			return (-ETIMEDOUT);
		}
		if (rc)
			psc_fatalx("pthread_cond_timedwait: %s",
			    strerror(rc));
	} else {
		rc = pthread_cond_wait(&mw->mw_cond,
		    &mw->mw_mutex.pm_mutex);
		if (rc)
			psc_fatalx("pthread_cond_wait: %s",
			    strerror(rc));
	}

 checkwaker:
	mwc = mw->mw_waker;
	mw->mw_waker = NULL;
	mw->mw_flags |= PMWF_CRITSECT;
	psc_mutex_unlock(&mw->mw_mutex);

	psc_mutex_lock(&mwc->mwc_mutex);
	if (mwc->mwc_flags & PMWCF_WAKEALL) {
		*(void **)datap = (void *)mwc->mwc_data;
		won = 1;
	} else if (mwc->mwc_winner == NULL) {
		*(void **)datap = (void *)mwc->mwc_data;
		mwc->mwc_winner = mw;
		won = 1;
	}

	psc_mutex_unlock(&mwc->mwc_mutex);

	if (!won) {
		//sched_yield();
		/* XXX decrement usecs */
		goto restart;
	}
	psc_multiwait_leavecritsect(mw);
	return (0);
}

/**
 * psc_multiwait_reset - Release all conditions and release all
 *	associated memory from a multiwait.
 * @mw: the multiwait to reset.
 */
void
psc_multiwait_reset(struct psc_multiwait *mw)
{
	struct psc_multiwaitcond *mwc;
	int k;

 restart:
	psc_mutex_lock(&mw->mw_mutex);
	while (psc_dynarray_len(&mw->mw_conds) > 0) {
		mwc = psc_dynarray_getpos(&mw->mw_conds, 0);

		/* XXX we violate the locking order of "mwc then mw" */
		if (!psc_mutex_trylock(&mwc->mwc_mutex)) {
			psc_mutex_unlock(&mw->mw_mutex);
			sched_yield();
			goto restart;
		}

		k = psc_dynarray_bsearch(&mwc->mwc_multiwaits, mw,
		    psc_multiwaitcond_cmp);
		psc_dynarray_splice(&mwc->mwc_multiwaits, k, 1, NULL, 0);

		psc_mutex_unlock(&mwc->mwc_mutex);
		/* Remove it so we don't process it twice. */
		psc_dynarray_removeitem(&mw->mw_conds, mwc);
	}
	psc_dynarray_reset(&mw->mw_conds);
	psc_vbitmap_resize(mw->mw_condmask, 0);
	mw->mw_flags = 0;
	mw->mw_waker = NULL;
	psc_mutex_unlock(&mw->mw_mutex);
}

/**
 * psc_multiwaitcond_nwaiters - Count the number of waiters sleeping
 *	on a multiwait condition.
 * @mwc: the multiwait condition to check.
 */
size_t
psc_multiwaitcond_nwaiters(struct psc_multiwaitcond *mwc)
{
	int n;

	psc_mutex_lock(&mwc->mwc_mutex);
	n = psc_dynarray_len(&mwc->mwc_multiwaits);
	psc_mutex_unlock(&mwc->mwc_mutex);
	return (n);
}

/**
 * psc_multiwait_entercritsect - Enter a multiwait critical section.
 * @mw: the multiwait.
 */
void
psc_multiwait_entercritsect(struct psc_multiwait *mw)
{
	psc_mutex_lock(&mw->mw_mutex);
	mw->mw_flags |= PMWF_CRITSECT;
	mw->mw_waker = NULL;
	psc_mutex_unlock(&mw->mw_mutex);
}

/**
 * psc_multiwait_leavecritsect - Leave a multiwait critical section.
 * @mw: the multiwait.
 */
void
psc_multiwait_leavecritsect(struct psc_multiwait *mw)
{
	psc_mutex_lock(&mw->mw_mutex);
	mw->mw_flags &= ~PMWF_CRITSECT;
	mw->mw_waker = NULL;
	psc_mutex_unlock(&mw->mw_mutex);
}

/**
 * psc_multiwait_hascond - Determine if a condition has been registered
 *	in a multiwait.
 * @mw: the multiwait.
 * @mwc: the multiwait condition to check the existence of.
 */
int
psc_multiwait_hascond(struct psc_multiwait *mw,
    struct psc_multiwaitcond *mwc)
{
	struct psc_multiwaitcond *c;
	int j, rc;

	rc = 0;
	psc_mutex_lock(&mw->mw_mutex);
	DYNARRAY_FOREACH(c, j, &mw->mw_conds)
		if (c == mwc) {
			rc = 1;
			break;
		}
	psc_mutex_unlock(&mw->mw_mutex);
	return (rc);
}

/**
 * psc_multiwait_prconds - Print list of conditions registered in a
 *	multiwait.
 * @mw: the multiwait to dump.
 */
void
psc_multiwait_prconds(struct psc_multiwait *mw)
{
	struct psc_multiwaitcond *mwc;
	int locked, j;

	locked = psc_mutex_reqlock(&mw->mw_mutex);
	DYNARRAY_FOREACH(mwc, j, &mw->mw_conds)
		printf(" multiwait %s@%p has condition %s@%p (%s)\n",
		    mw->mw_name, mw, mwc->mwc_name, mwc,
		    psc_multiwait_iscondwakeable(mw, mwc) ?
		    "enabled" : "disabled");
	psc_mutex_ureqlock(&mw->mw_mutex, locked);
}

/**
 * psc_multiwaitcond_prmwaits - Print list of multiwaits a condition is
 *	registered in.
 * @mw: the multiwait to dump.
 */
void
psc_multiwaitcond_prmwaits(struct psc_multiwaitcond *mwc)
{
	struct psc_multiwait *mw;
	int locked, j;

	locked = psc_mutex_reqlock(&mwc->mwc_mutex);
	DYNARRAY_FOREACH(mw, j, &mwc->mwc_multiwaits)
		printf(" multiwaitcond %s@%p on multiwait %s@%p\n",
		    mwc->mwc_name, mwc, mw->mw_name, mw);
	psc_mutex_ureqlock(&mwc->mwc_mutex, locked);
}
