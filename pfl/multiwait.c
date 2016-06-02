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
 * Multiwait is an API for waiting on any of a number of conditions to
 * become available.
 *
 * XXX since we keep the multiwaits/conds sorted on each others' lists,
 * we shouldnt use a dynarray.  It does make debugging access easier,
 * though.
 */

#include <sys/time.h>

#include <errno.h>
#include <pthread.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "pfl/bitflag.h"
#include "pfl/cdefs.h"
#include "pfl/log.h"
#include "pfl/multiwait.h"
#include "pfl/pthrutil.h"
#include "pfl/thread.h"
#include "pfl/time.h"
#include "pfl/vbitmap.h"

/*
 * Initialize a multiwait condition.
 * @mwc: the condition to initialize.
 * @data: pointer to user data.
 * @flags: behavioral flags.
 * @name: printf(3)-like name.
 */
void
pfl_multiwaitcond_init(struct pfl_multiwaitcond *mwc, const void *data,
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

/*
 * Try to lock all multiwaits this condition is a member of.
 * @mwc: a multiwaitcond, which must itself be locked.
 * Returns zero on success, -1 on failure.
 */
__static int
pfl_multiwaitcond_trylockallmw(struct pfl_multiwaitcond *mwc)
{
	struct pfl_multiwait *mw;
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

/*
 * Unlock all multiwaits this condition is a member of.
 * @mwc: a multiwait condition, which must itself be locked.
 */
__static void
pfl_multiwaitcond_unlockallmw(struct pfl_multiwaitcond *mwc)
{
	struct pfl_multiwait *mw;
	int j;

	DYNARRAY_FOREACH(mw, j, &mwc->mwc_multiwaits)
		psc_mutex_unlock(&mw->mw_mutex);
}

__static int
pfl_multiwaitcond_cmp(const void *a, const void *b)
{
	return (CMP(a, b));
}

/*
 * Release a multiwait condition.
 * @mwc: the condition to release.
 */
void
pfl_multiwaitcond_destroy(struct pfl_multiwaitcond *mwc)
{
	struct pfl_multiwait *mw;
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
			pscthr_yield();
			goto restart;
		}
		if (mw->mw_flags & PMWF_CRITSECT &&
		    mw->mw_waker == mwc)
			psc_fatalx("waking condition %s wants to go away "
			    "but may corrupt integrity of multiwait %s",
			    mwc->mwc_name, mw->mw_name);
		psc_assert(psc_dynarray_len(&mw->mw_conds) ==
		    (int)psc_vbitmap_getsize(mw->mw_condmask));

		DLOG_MULTIWAIT(PLL_DEBUG, mw,
		    "disassociating cond %s@%p", mwc->mwc_name, mwc);
		k = psc_dynarray_bsearch(&mwc->mwc_multiwaits, mw,
		    pfl_multiwaitcond_cmp);
		psc_assert(psc_dynarray_getpos(&mwc->mwc_multiwaits,
		    k) == mw);
		psc_dynarray_splice(&mwc->mwc_multiwaits, k, 1, NULL, 0);

		k = psc_dynarray_removeitem(&mw->mw_conds, mwc);
		pfl_bitstr_copy(&mw->mw_condmask, k, &mw->mw_condmask,
		    k + 1, psc_vbitmap_getsize(mw->mw_condmask) - k);
		psc_vbitmap_resize(mw->mw_condmask,
		    psc_dynarray_len(&mw->mw_conds));

		psc_mutex_unlock(&mw->mw_mutex);
	}
	psc_dynarray_free(&mwc->mwc_multiwaits);
	/* XXX: ensure no one is waiting on this mutex? */
	// XXX need refcnt and wait until release before we can destroy it
	psc_mutex_unlock(&mwc->mwc_mutex);
	psc_mutex_destroy(&mwc->mwc_mutex);
	pthread_cond_destroy(&mwc->mwc_cond);

#if PFL_DEBUG > 0
	memset(mwc, 0, sizeof(*mwc));
#endif
}

/*
 * Determine if a condition is masked off in a multiwait.
 * @mw: multiwait to check in, which must be locked.
 * @mwc: condition to check for.
 */
int
pfl_multiwait_iscondwakeable(struct pfl_multiwait *mw,
    const struct pfl_multiwaitcond *mwc)
{
	struct pfl_multiwaitcond *c;
	int j;

	psc_mutex_ensure_locked(&mw->mw_mutex);
	DYNARRAY_FOREACH(c, j, &mw->mw_conds)
		if (c == mwc)
			return (psc_vbitmap_get(mw->mw_condmask, j));
	psc_fatalx("could not find condition %s in multiwait %s",
	    mwc->mwc_name, mw->mw_name);
}

/*
 * Wakeup multiwaits waiting on a condition.
 * @mwc: a multiwait condition, which must be unlocked on entry
 *	and will be locked on exit.
 */
void
pfl_multiwaitcond_wakeup(struct pfl_multiwaitcond *mwc)
{
	struct pfl_multiwait *mw;
	int j, count = 0;

 restart:
	psc_mutex_lock(&mwc->mwc_mutex);
	if (pfl_multiwaitcond_trylockallmw(mwc)) {
		if (count++ == 300000)
			psclog_errorx("mwcond %s failed to lock his "
			    "multiwaits after attempting many times; "
			    "possible deadlock", mwc->mwc_name);
		psc_mutex_unlock(&mwc->mwc_mutex);
		pscthr_yield();
		goto restart;
	}
	DYNARRAY_FOREACH(mw, j, &mwc->mwc_multiwaits)
		if (pfl_multiwait_iscondwakeable(mw, mwc)) {
			mw->mw_waker = mwc;
			DLOG_MULTIWAIT(PLL_DEBUG, mw,
			    "condition %s@%p tentatively woke us",
			    mwc->mwc_name, mwc);
			pthread_cond_signal(&mw->mw_cond);
		}
	pfl_multiwaitcond_unlockallmw(mwc);
	psclog_debug("wake cond %s@%p", mwc->mwc_name, mwc);
	pthread_cond_broadcast(&mwc->mwc_cond);
	mwc->mwc_winner = NULL;
	psc_mutex_unlock(&mwc->mwc_mutex);
}

/*
 * Wait for one condition to occur.
 * @mwc: the multiwait condition to wait for.
 * @mutex: an optional mutex that will be unlocked in the critical
 *	section, for avoiding missed wakeups from races.
 * @reltime: amount of time to wait (relative to "now") or NULL for
 *	forever.
 */
int
pfl_multiwaitcond_waitrel_ts(struct pfl_multiwaitcond *mwc,
    struct pfl_mutex *mutex, const struct timespec *reltime)
{
	struct timespec abstime;
	int rc;

	psc_mutex_lock(&mwc->mwc_mutex);
	if (mutex)
		psc_mutex_unlock(mutex);

	psclog_debug("wait cond %s@%p", mwc->mwc_name, mwc);
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

/*
 * Add a condition to a multiwait.
 * @mw: a multiwait.
 * @mwc: the condition to add.
 * @active: whether condition is active.
 */
int
_pfl_multiwait_addcond(struct pfl_multiwait *mw,
    struct pfl_multiwaitcond *mwc, int active)
{
	struct pfl_multiwaitcond *c;
	int rc = 0, k, j;

	/* Acquire locks. */
	for (;;) {
		psc_mutex_lock(&mwc->mwc_mutex);
		if (psc_mutex_trylock(&mw->mw_mutex))
			break;
		psc_mutex_unlock(&mwc->mwc_mutex);
		pscthr_yield();
	}

	/* Ensure no associations already exist. */
	// XXX bsearch
	DYNARRAY_FOREACH(c, j, &mw->mw_conds)
		if (c == mwc)
			psc_fatalx("mwc %s already registered in multiwait %s",
			    mwc->mwc_name, mw->mw_name);

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

	DLOG_MULTIWAIT(PLL_DEBUG, mw, "associating cond %s@%p",
	    mwc->mwc_name, mwc);
	k = psc_dynarray_bsearch(&mwc->mwc_multiwaits, mw,
	    pfl_multiwaitcond_cmp);

	if (k < psc_dynarray_len(&mwc->mwc_multiwaits) &&
	    psc_dynarray_getpos(&mwc->mwc_multiwaits, k) == mw)
		psc_fatalx("mw %s already registered multiwaitcond %s",
		    mw->mw_name, mwc->mwc_name);

	if (psc_dynarray_splice(&mwc->mwc_multiwaits, k, 0, &mw, 1) ==
	    -1) {
		rc = -1;
		if (psc_vbitmap_resize(mw->mw_condmask, j - 1) == -1)
			psc_fatalx("unable to undo bitmask changes");
		psc_dynarray_removeitem(&mw->mw_conds, mwc);
		goto done;
	}
	// refcnt

	psc_vbitmap_setval(mw->mw_condmask, j - 1, active);

 done:
	psc_mutex_unlock(&mw->mw_mutex);
	psc_mutex_unlock(&mwc->mwc_mutex);
	return (rc);
}

/*
 * Initialize a multiwait.
 * @mw: the multiwait to initialize.
 */
void
pfl_multiwait_init(struct pfl_multiwait *mw, const char *name, ...)
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

/*
 * Destroy a multiwait.
 * @mw: the multiwait to release.
 */
void
pfl_multiwait_free(struct pfl_multiwait *mw)
{
	pfl_multiwait_reset(mw);
	psc_mutex_destroy(&mw->mw_mutex);
	psc_dynarray_free(&mw->mw_conds);
	psc_vbitmap_free(mw->mw_condmask);

#if PFL_DEBUG > 0
	memset(mw, 0, sizeof(*mw));
#endif
}

/*
 * Update a multiwait's active conditions mask.
 * @mw: the multiwait whose mask to modify.
 * @mwc: the condition in the mask to modify.
 * @active: whether the condition can wake the multiwait.
 */
void
pfl_multiwait_setcondwakeable(struct pfl_multiwait *mw,
    const struct pfl_multiwaitcond *mwc, int active)
{
	struct pfl_multiwaitcond *c;
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
pfl_multiwait_rel(struct pfl_multiwait *mw, void *datap, int sec,
    int nsec)
{
	struct timespec adj = { sec, nsec };
	struct pfl_multiwaitcond *mwc;
	int rc, won = 0, j;
	struct psc_thread *thr;

	thr = pscthr_get();
	/* Sanity checks. */
	if (psc_dynarray_len(&mw->mw_conds) == 0)
		psc_fatalx("multiwait %s has no conditions and "
		    "will never wake up", mw->mw_name);

	/* Check for no active conditions. */
	DYNARRAY_FOREACH(mwc, j, &mw->mw_conds)
		if (psc_vbitmap_get(mw->mw_condmask, j))
			break;
	if (!mwc)
		psc_fatalx("multiwait %s has all conditions masked and "
		    "will never wake up", mw->mw_name);

 restart:
	psc_mutex_lock(&mw->mw_mutex);

	/* Check for missed wakeups during a critical section. */
	if (mw->mw_flags & PMWF_CRITSECT) {
		mw->mw_flags &= ~PMWF_CRITSECT;
		if (mw->mw_waker)
			goto checkwaker;
	}

	DLOG_MULTIWAIT(PLL_DEBUG, mw, "entering wait; sec=%d nsec=%d",
	    sec, nsec);

	thr->pscthr_waitq = mwc->mwc_name;
	if (sec || nsec) {
		struct timespec ts;

		PFL_GETTIMESPEC(&ts);
		timespecadd(&ts, &adj, &ts);

		rc = pthread_cond_timedwait(&mw->mw_cond,
		    &mw->mw_mutex.pm_mutex, &ts);
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
	thr->pscthr_waitq = NULL;

 checkwaker:
	mwc = mw->mw_waker;
	mw->mw_waker = NULL;
	mw->mw_flags |= PMWF_CRITSECT;
	psc_mutex_unlock(&mw->mw_mutex);

	psc_mutex_lock(&mwc->mwc_mutex);
	if (mwc->mwc_flags & PMWCF_WAKEALL) {
		DLOG_MULTIWAIT(PLL_DEBUG, mw,
		    "broadcast condition %s@%p woke us out of multiwait",
		    mwc->mwc_name, mwc);
		*(void **)datap = (void *)mwc->mwc_data;
		won = 1;
	} else if (mwc->mwc_winner == NULL) {
		DLOG_MULTIWAIT(PLL_DEBUG, mw,
		    "won multiwait from condition %s@%p",
		    mwc->mwc_name, mwc);
		*(void **)datap = (void *)mwc->mwc_data;
		mwc->mwc_winner = mw;
		won = 1;
	}

	psc_mutex_unlock(&mwc->mwc_mutex);

	if (!won) {
		DLOG_MULTIWAIT(PLL_DEBUG, mw,
		    "did not win, restarting");
		//pscthr_yield();
		/* XXX decrement usecs */
		goto restart;
	}
	pfl_multiwait_leavecritsect(mw);
	return (0);
}

/*
 * Release all conditions and release all associated memory from a
 * multiwait.
 * @mw: the multiwait to reset.
 */
void
pfl_multiwait_reset(struct pfl_multiwait *mw)
{
	struct pfl_multiwaitcond *mwc;
	int k;

 restart:
	psc_mutex_lock(&mw->mw_mutex);
	while (psc_dynarray_len(&mw->mw_conds) > 0) {
		mwc = psc_dynarray_getpos(&mw->mw_conds, 0);

		/* XXX we violate the locking order of "mwc then mw" */
		if (!psc_mutex_trylock(&mwc->mwc_mutex)) {
			psc_mutex_unlock(&mw->mw_mutex);
			pscthr_yield();
			goto restart;
		}

		DLOG_MULTIWAIT(PLL_DEBUG, mw,
		    "disassociating cond %s@%p", mwc->mwc_name, mwc);
		k = psc_dynarray_bsearch(&mwc->mwc_multiwaits, mw,
		    pfl_multiwaitcond_cmp);
		psc_dynarray_splice(&mwc->mwc_multiwaits, k, 1, NULL, 0);

		psc_mutex_unlock(&mwc->mwc_mutex);
		/* Remove it so we don't process it twice. */
		psc_dynarray_removeitem(&mw->mw_conds, mwc);
	}
	// XXX mw_conds should already be reset...
	psc_dynarray_reset(&mw->mw_conds);
	psc_vbitmap_resize(mw->mw_condmask, 0);
	mw->mw_flags = 0;
	mw->mw_waker = NULL;
	psc_mutex_unlock(&mw->mw_mutex);
}

/*
 * Count the number of waiters sleeping on a multiwait condition.
 * @mwc: the multiwait condition to check.
 */
size_t
pfl_multiwaitcond_nwaiters(struct pfl_multiwaitcond *mwc)
{
	int n;

	psc_mutex_lock(&mwc->mwc_mutex);
	n = psc_dynarray_len(&mwc->mwc_multiwaits);
	psc_mutex_unlock(&mwc->mwc_mutex);
	return (n);
}

/*
 * Enter a multiwait critical section.  As multiple conditions need to
 * be checked before entering sleep, this API provides a 'critical
 * section' so all such conditions can be checked, in case an earlier
 * checked condition changes when we are checking later ones before
 * going to sleep, in which case, multiwait will return immediately.
 * @mw: the multiwait.
 */
void
pfl_multiwait_entercritsect(struct pfl_multiwait *mw)
{
	psc_mutex_lock(&mw->mw_mutex);
	mw->mw_flags |= PMWF_CRITSECT;
	mw->mw_waker = NULL;
	psc_mutex_unlock(&mw->mw_mutex);
}

/*
 * Leave a multiwait critical section.
 * @mw: the multiwait.
 */
void
pfl_multiwait_leavecritsect(struct pfl_multiwait *mw)
{
	psc_mutex_lock(&mw->mw_mutex);
	psc_assert(mw->mw_flags & PMWF_CRITSECT);
	mw->mw_flags &= ~PMWF_CRITSECT;
	psc_mutex_unlock(&mw->mw_mutex);
}

/*
 * Determine if a condition has been registered in a multiwait.
 * @mw: the multiwait.
 * @mwc: the multiwait condition to check the existence of.
 */
int
pfl_multiwait_hascond(struct pfl_multiwait *mw,
    struct pfl_multiwaitcond *mwc)
{
	struct pfl_multiwaitcond *c;
	int i;

	psc_mutex_lock(&mw->mw_mutex);
	DYNARRAY_FOREACH(c, i, &mw->mw_conds)
		if (c == mwc)
			break;
	psc_mutex_unlock(&mw->mw_mutex);
	return (c ? 1 : 0);
}

/*
 * Print list of conditions registered in a multiwait.
 * @mw: the multiwait to dump.
 */
void
pfl_multiwait_prconds(struct pfl_multiwait *mw)
{
	struct pfl_multiwaitcond *mwc;
	int locked, j;

	locked = psc_mutex_reqlock(&mw->mw_mutex);
	DYNARRAY_FOREACH(mwc, j, &mw->mw_conds)
		printf(" multiwait %s@%p has condition %s@%p (%s)\n",
		    mw->mw_name, mw, mwc->mwc_name, mwc,
		    pfl_multiwait_iscondwakeable(mw, mwc) ?
		    "enabled" : "disabled");
	psc_mutex_ureqlock(&mw->mw_mutex, locked);
}

/*
 * Print list of multiwaits a condition is registered in.
 * @mw: the multiwait to dump.
 */
void
pfl_multiwaitcond_prmwaits(struct pfl_multiwaitcond *mwc)
{
	struct pfl_multiwait *mw;
	int locked, j;

	locked = psc_mutex_reqlock(&mwc->mwc_mutex);
	DYNARRAY_FOREACH(mw, j, &mwc->mwc_multiwaits)
		printf(" multiwaitcond %s@%p on multiwait %s@%p\n",
		    mwc->mwc_name, mwc, mw->mw_name, mw);
	psc_mutex_ureqlock(&mwc->mwc_mutex, locked);
}
