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
 * Multiwait: for waiting on any of a number of conditions to become
 * available.
 */

#include <sys/time.h>

#include <errno.h>
#include <pthread.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "pfl/cdefs.h"
#include "psc_ds/vbitmap.h"
#include "psc_util/alloc.h"
#include "psc_util/bitflag.h"
#include "psc_util/log.h"
#include "psc_util/multiwait.h"
#include "psc_util/pthrutil.h"
#include "psc_util/time.h"

/**
 * psc_multiwait_cmp - Compare two multiwaits, used for sorting.
 *	Sorting is necessary to avoid deadlocking as everyone will
 *	try to quire locks in the same order instead of in the
 *	order they were registered.
 * @a: a multiwait.
 * @b: another multiwait.
 */
__static int
psc_multiwait_cmp(const void *a, const void *b)
{
	struct psc_multiwait *const *mwa = a, *const *mwb = b;

	return (CMP(*mwa, *mwb));
}

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
	psc_pthread_mutex_init(&mwc->mwc_mutex);
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
	int rc, j, k;

	DYNARRAY_FOREACH(mw, j, &mwc->mwc_multiwaits) {
		rc = psc_pthread_mutex_trylock(&mw->mw_mutex);
		if (rc == EBUSY) {
			/*
			 * Unable to lock them all; release what we did
			 * lock and give up.
			 */
			for (k = 0; k < j; k++) {
				mw = psc_dynarray_getpos(&mwc->mwc_multiwaits, k);
				psc_pthread_mutex_unlock(&mw->mw_mutex);
			}
			return (-1);
		}
		if (rc)
			psc_fatalx("pthread_mutex_trylock: %s", strerror(rc));
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
		psc_pthread_mutex_unlock(&mw->mw_mutex);
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
	psc_pthread_mutex_lock(&mwc->mwc_mutex);
	DYNARRAY_FOREACH(mw, j, &mwc->mwc_multiwaits) {
		if (psc_pthread_mutex_trylock(&mw->mw_mutex)) {
			if (count++ == 300000)
				psc_errorx("mwcond %s failed to lock his "
				    "multiwaits after many attempts, "
				    "possible deadlock", mwc->mwc_name);
			psc_pthread_mutex_unlock(&mwc->mwc_mutex);
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

		k = psc_dynarray_remove(&mw->mw_conds, mwc);
		pfl_bitstr_copy(&mw->mw_condmask, k, &mw->mw_condmask,
		    k + 1, psc_vbitmap_getsize(mw->mw_condmask) - k);
		psc_vbitmap_resize(mw->mw_condmask,
		    psc_dynarray_len(&mw->mw_conds));
		psc_pthread_mutex_unlock(&mw->mw_mutex);

		psc_dynarray_remove(&mwc->mwc_multiwaits, mw);
	}
	/* XXX: ensure no one is waiting on this mutex? */
	psc_dynarray_free(&mwc->mwc_multiwaits);
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

	psc_pthread_mutex_ensure_locked(&mw->mw_mutex);
	DYNARRAY_FOREACH(c, j, &mw->mw_conds)
		if (c == mwc)
			return (psc_vbitmap_get(mw->mw_condmask, j));
	psc_fatalx("could not find condition %s in multiwait %s",
	    mwc->mwc_name, mw->mw_name);
}

/**
 * psc_multiwaitcond_wakeup - wakeup multiwaits waiting on a condition.
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
	psc_pthread_mutex_lock(&mwc->mwc_mutex);
	if (psc_multiwaitcond_trylockallmw(mwc)) {
		if (count++ == 300000)
			psc_errorx("mwcond %s failed to lock his "
			    "multiwaits after attempting many times,"
			    "possible deadlock", mwc->mwc_name);
		psc_pthread_mutex_unlock(&mwc->mwc_mutex);
		sched_yield();
		goto restart;
	}
	DYNARRAY_FOREACH(mw, j, &mwc->mwc_multiwaits)
		if (psc_multiwait_iscondwakeable(mw, mwc)) {
			mw->mw_waker = mwc;
			pthread_cond_signal(&mw->mw_cond);
		}
	psc_multiwaitcond_unlockallmw(mwc);
	pthread_cond_signal(&mwc->mwc_cond);
	mwc->mwc_winner = NULL;
	psc_pthread_mutex_unlock(&mwc->mwc_mutex);
}

/**
 * psc_multiwaitcond_wait - Wait for one condition to occur.
 * @mwc: the multiwait condition to wait for.
 * @mutex: an optional mutex that will be unlocked in the critical section,
 *	for avoiding missed wakeups from races.
 */
void
psc_multiwaitcond_wait(struct psc_multiwaitcond *mwc, pthread_mutex_t *mutex)
{
	int rc;

	psc_pthread_mutex_lock(&mwc->mwc_mutex);
	if (mutex)
		psc_pthread_mutex_unlock(mutex);
	rc = pthread_cond_wait(&mwc->mwc_cond, &mwc->mwc_mutex);
	if (rc)
		psc_fatalx("pthread_cond_wait: %s", strerror(rc));
	psc_pthread_mutex_unlock(&mwc->mwc_mutex);
}

/**
 * psc_multiwait_addcond - add a condition to a multiwait.
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
	int rc, j;

	/* Acquire locks. */
	for (;;) {
		psc_pthread_mutex_lock(&mwc->mwc_mutex);
		rc = psc_pthread_mutex_trylock(&mw->mw_mutex);
		if (rc == 0)
			break;
		if (rc != EBUSY)
			psc_fatalx("pthread_mutex_trylock: %s",
			    strerror(rc));
		psc_pthread_mutex_unlock(&mwc->mwc_mutex);
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

	/* Associate each with each other. */
	if (psc_dynarray_add(&mw->mw_conds, mwc) == -1) {
		rc = -1;
		goto done;
	}
	if (psc_dynarray_add(&mwc->mwc_multiwaits, mw) == -1) {
		rc = -1;
		psc_dynarray_remove(&mw->mw_conds, mwc);
		goto done;
	}

	j = psc_dynarray_len(&mw->mw_conds);
	if (psc_vbitmap_resize(mw->mw_condmask, j) == -1) {
		rc = -1;
		psc_dynarray_remove(&mwc->mwc_multiwaits, mw);
		psc_dynarray_remove(&mw->mw_conds, mwc);
		goto done;
	}
	qsort(psc_dynarray_get(&mwc->mwc_multiwaits),
	    psc_dynarray_len(&mwc->mwc_multiwaits),
	    sizeof(void *), psc_multiwait_cmp);
	psc_vbitmap_setval(mw->mw_condmask, j - 1, active);

 done:
	psc_pthread_mutex_unlock(&mw->mw_mutex);
	psc_pthread_mutex_unlock(&mwc->mwc_mutex);
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
	psc_pthread_mutex_init(&mw->mw_mutex);
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
	psc_dynarray_free(&mw->mw_conds);
	psc_vbitmap_free(mw->mw_condmask);
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

	psc_pthread_mutex_lock(&mw->mw_mutex);
	DYNARRAY_FOREACH(c, j, &mw->mw_conds)
		if (c == mwc) {
			psc_vbitmap_setval(mw->mw_condmask, j, active);
			psc_pthread_mutex_unlock(&mw->mw_mutex);
			return;
		}
	psc_fatalx("couldn't find mwcond %s in multiwait %s",
	    mwc->mwc_name, mw->mw_name);
}

int
psc_multiwaitus(struct psc_multiwait *mw, void *datap, int usec)
{
	struct psc_multiwaitcond *mwc;
	int rc, won, j;

	won = 0;
 restart:
	psc_pthread_mutex_lock(&mw->mw_mutex);

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
	if (usec) {
		struct timeval tv, res, adj;
		struct timespec ntv;

		PFL_GETTIME(&tv);
		adj.tv_sec = usec / 1000000;
		adj.tv_usec = usec % 1000000;
		timeradd(&tv, &adj, &res);
		ntv.tv_sec = res.tv_sec;
		ntv.tv_nsec = res.tv_usec * 1000;

		rc = pthread_cond_timedwait(&mw->mw_cond,
		    &mw->mw_mutex, &ntv);
		if (rc == ETIMEDOUT) {
			psc_pthread_mutex_unlock(&mw->mw_mutex);
			return (-ETIMEDOUT);
		}
		if (rc)
			psc_fatalx("pthread_cond_timedwait: %s",
			    strerror(rc));
	} else {
		rc = pthread_cond_wait(&mw->mw_cond, &mw->mw_mutex);
		if (rc)
			psc_fatalx("pthread_cond_wait: %s", strerror(rc));
	}

 checkwaker:
	mwc = mw->mw_waker;
	mw->mw_waker = NULL;
	mw->mw_flags |= PMWF_CRITSECT;
	psc_pthread_mutex_unlock(&mw->mw_mutex);

	psc_pthread_mutex_lock(&mwc->mwc_mutex);
	if (mwc->mwc_flags & PMWCF_WAKEALL) {
		*(void **)datap = (void *)mwc->mwc_data;
		won = 1;
	} else if (mwc->mwc_winner == NULL) {
		*(void **)datap = (void *)mwc->mwc_data;
		mwc->mwc_winner = mw;
		won = 1;
	}

	psc_pthread_mutex_unlock(&mwc->mwc_mutex);

	if (!won) {
		sched_yield();
		goto restart;
	}
	psc_multiwait_leavecritsect(mw);
	return (0);
}

/**
 * psc_multiwait_reset - Release all conditions and release all*
 *	associated memory rom a multiwait.
 * @mw: the multiwait to reset.
 */
void
psc_multiwait_reset(struct psc_multiwait *mw)
{
	struct psc_multiwaitcond *mwc;

 restart:
	psc_pthread_mutex_lock(&mw->mw_mutex);
	while (psc_dynarray_len(&mw->mw_conds) > 0) {
		mwc = psc_dynarray_getpos(&mw->mw_conds, 0);

		/* XXX we violate the locking order of mwc then mw */
		if (psc_pthread_mutex_trylock(&mwc->mwc_mutex)) {
			psc_pthread_mutex_unlock(&mw->mw_mutex);
			sched_yield();
			goto restart;
		}

		psc_dynarray_remove(&mwc->mwc_multiwaits, mw);

		/*
		 * psc_dynarray_remove() will swap the last elem with
		 * the new empty slot, so we should resort to peserve
		 * ordering semantics.
		 */
		qsort(psc_dynarray_get(&mwc->mwc_multiwaits),
		    psc_dynarray_len(&mwc->mwc_multiwaits),
		    sizeof(void *), psc_multiwait_cmp);
		psc_pthread_mutex_unlock(&mwc->mwc_mutex);
		/* Remove it so we don't process it twice. */
		psc_dynarray_remove(&mw->mw_conds, mwc);
	}
	psc_dynarray_reset(&mw->mw_conds);
	psc_vbitmap_resize(mw->mw_condmask, 0);
	mw->mw_flags = 0;
	mw->mw_waker = NULL;
	psc_pthread_mutex_unlock(&mw->mw_mutex);
}

/*
 * psc_multiwaitcond_nwaiters - count the number of waiters sleeping
 *	on a multiwait condition.
 * @mwc: the multiwait condition to check.
 */
size_t
psc_multiwaitcond_nwaiters(struct psc_multiwaitcond *mwc)
{
	int n;

	psc_pthread_mutex_lock(&mwc->mwc_mutex);
	n = psc_dynarray_len(&mwc->mwc_multiwaits);
	psc_pthread_mutex_unlock(&mwc->mwc_mutex);
	return (n);
}

/**
 * psc_multiwait_entercritsect - Enter a multiwait critical section.
 * @mw: the multiwait.
 */
void
psc_multiwait_entercritsect(struct psc_multiwait *mw)
{
	psc_pthread_mutex_lock(&mw->mw_mutex);
	mw->mw_flags |= PMWF_CRITSECT;
	mw->mw_waker = NULL;
	psc_pthread_mutex_unlock(&mw->mw_mutex);
}

/**
 * psc_multiwait_leavecritsect - Leave a multiwait critical section.
 * @mw: the multiwait.
 */
void
psc_multiwait_leavecritsect(struct psc_multiwait *mw)
{
	psc_pthread_mutex_lock(&mw->mw_mutex);
	mw->mw_flags &= ~PMWF_CRITSECT;
	mw->mw_waker = NULL;
	psc_pthread_mutex_unlock(&mw->mw_mutex);
}

/**
 * psc_multiwait_hascond - Determine if a condition has been registered in a
 *	multiwait.
 * @mw: the multiwait.
 * @mwc: the multiwait condition to check the existence of.
 */
int
psc_multiwait_hascond(struct psc_multiwait *mw, struct psc_multiwaitcond *mwc)
{
	struct psc_multiwaitcond *c;
	int j, rc;

	rc = 0;
	psc_pthread_mutex_lock(&mw->mw_mutex);
	DYNARRAY_FOREACH(c, j, &mw->mw_conds)
		if (c == mwc) {
			rc = 1;
			break;
		}
	psc_pthread_mutex_unlock(&mw->mw_mutex);
	return (rc);
}

/**
 * psc_multiwait_prconds - Print list of conditions registered in a multiwait.
 * @mw: the multiwait to dump.
 */
void
psc_multiwait_prconds(struct psc_multiwait *mw)
{
	struct psc_multiwaitcond *mwc;
	int locked, j;

	locked = psc_pthread_mutex_reqlock(&mw->mw_mutex);
	DYNARRAY_FOREACH(mwc, j, &mw->mw_conds)
		printf(" multiwait %s has mwc %s (%s)\n",
		    mw->mw_name, mwc->mwc_name,
		    psc_multiwait_iscondwakeable(mw, mwc) ?
		    "enabled" : "disabled");
	psc_pthread_mutex_ureqlock(&mw->mw_mutex, locked);
}
