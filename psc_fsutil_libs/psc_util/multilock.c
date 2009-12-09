/* $Id$ */

/*
 * Multiple lock routines: for waiting on any number of
 * conditions to become available.
 *
 * XXX rename this to multiwaitq
 */

#include <sys/time.h>

#include <errno.h>
#include <pthread.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "psc_ds/vbitmap.h"
#include "psc_util/alloc.h"
#include "pfl/cdefs.h"
#include "psc_util/log.h"
#include "psc_util/multilock.h"
#include "psc_util/pthrutil.h"

/*
 * psc_multilock_cmp - Compare two multilocks, used for sorting.  Sorting is
 *	necessary to avoid deadlocking.
 * @a: a multilock
 * @b: another multilock
 */
__static int
psc_multilock_cmp(const void *a, const void *b)
{
	struct psc_multilock *const *mla = a, *const *mlb = b;

	return (CMP(*mla, *mlb));
}

/**
 * psc_multilock_cond_init - initialize a multilock condition.
 * @mlc: the condition to initialize.
 * @data: pointer to user data.
 */
void
psc_multilock_cond_init(struct psc_multilock_cond *mlc, const void *data,
    int flags, const char *name, ...)
{
	va_list ap;

	memset(mlc, 0, sizeof(*mlc));
	dynarray_init(&mlc->mlc_multilocks);
	psc_pthread_mutex_init(&mlc->mlc_mutex);
	pthread_cond_init(&mlc->mlc_cond, NULL);
	mlc->mlc_data = data;
	mlc->mlc_flags = flags;

	va_start(ap, name);
	vsnprintf(mlc->mlc_name, sizeof(mlc->mlc_name), name, ap);
	va_end(ap);
}

/*
 * psc_multilock_cond_trylockall - try to lock all multilocks this
 *	condition is a member of.
 * @mlc: a multilock condition, which must itself be locked.
 * Returns zero on success, -1 on failure.
 */
__static int
psc_multilock_cond_trylockall(struct psc_multilock_cond *mlc)
{
	struct psc_multilock **mlv;
	int nml, j, k;

	nml = dynarray_len(&mlc->mlc_multilocks);
	mlv = dynarray_get(&mlc->mlc_multilocks);
	for (j = 0; j < nml; j++)
		if (pthread_mutex_trylock(&mlv[j]->ml_mutex)) {
			for (k = 0; k < j; k++)
				psc_pthread_mutex_unlock(&mlv[k]->ml_mutex);
			return (-1);
		}
	return (0);
}

/*
 * psc_multilock_cond_unlockall - unlock all multilocks this
 *	condition is a member of.
 * @mlc: a multilock condition, which must itself be locked.
 */
__static void
psc_multilock_cond_unlockall(struct psc_multilock_cond *mlc)
{
	struct psc_multilock **mlv;
	int nml, j;

	nml = dynarray_len(&mlc->mlc_multilocks);
	mlv = dynarray_get(&mlc->mlc_multilocks);
	for (j = 0; j < nml; j++)
		psc_pthread_mutex_unlock(&mlv[j]->ml_mutex);
}

/**
 * psc_multilock_cond_destroy - release a multilock condition.
 * @mlc: the condition to release.
 */
void
psc_multilock_cond_destroy(struct psc_multilock_cond *mlc)
{
	struct psc_multilock *ml, **mlv;
	int nml, i, count;

	count = 0;
 restart:
	psc_pthread_mutex_lock(&mlc->mlc_mutex);
	nml = dynarray_len(&mlc->mlc_multilocks);
	mlv = dynarray_get(&mlc->mlc_multilocks);
	for (i = 0; i < nml; i++) {
		ml = mlv[i];
		if (pthread_mutex_trylock(&ml->ml_mutex)) {
			if (count++ == 300000)
				psc_errorx("mlcond %s failed to lock his "
				    "multilocks - possible deadlock if this "
				    "message repeats", mlc->mlc_name);
			psc_pthread_mutex_unlock(&mlc->mlc_mutex);
			sched_yield();
			goto restart;
		}
		dynarray_remove(&ml->ml_conds, mlc);
		dynarray_remove(&mlc->mlc_multilocks, ml);
		psc_pthread_mutex_unlock(&ml->ml_mutex);

	}
	dynarray_free(&mlc->mlc_multilocks);
}

/*
 * psc_multilock_masked_cond - determine if a condition is masked
 *	off in a multilock.
 * @ml: multilock to check in, which must be locked.
 * @mlc: condition to check for.
 */
int
psc_multilock_masked_cond(const struct psc_multilock *ml,
    const struct psc_multilock_cond *mlc)
{
	struct psc_multilock_cond **mlcv;
	int nmlc, j;

	/* XXX ensure pthread mutex is held on ml */

	mlcv = dynarray_get(&ml->ml_conds);
	nmlc = dynarray_len(&ml->ml_conds);
	for (j = 0; j < nmlc; j++)
		if (mlcv[j] == mlc)
			return (vbitmap_get(ml->ml_mask, j));
	psc_fatalx("could not find condition in multilock");
}

/*
 * psc_multilock_cond_wakeup - wakeup multilocks waiting on a condition.
 * @mlc: a multilock condition, which must be unlocked on entry
 *	and will be locked on exit.
 */
void
psc_multilock_cond_wakeup(struct psc_multilock_cond *mlc)
{
	struct psc_multilock **mlv;
	int nml, j, count;

	count = 0;
 restart:
	psc_pthread_mutex_lock(&mlc->mlc_mutex);
	if (psc_multilock_cond_trylockall(mlc)) {
		if (count++ == 300000)
			psc_errorx("mlcond %s failed to lock his "
			    "multilocks - possible deadlock if this "
			    "message repeats", mlc->mlc_name);
		psc_pthread_mutex_unlock(&mlc->mlc_mutex);
		sched_yield();
		goto restart;
	}
	nml = dynarray_len(&mlc->mlc_multilocks);
	mlv = dynarray_get(&mlc->mlc_multilocks);
	for (j = 0; j < nml; j++)
		if (psc_multilock_masked_cond(mlv[j], mlc)) {
			mlv[j]->ml_waker = mlc;
			pthread_cond_signal(&mlv[j]->ml_cond);
		}
	psc_multilock_cond_unlockall(mlc);
	pthread_cond_signal(&mlc->mlc_cond);
	psc_pthread_mutex_unlock(&mlc->mlc_mutex);
}

/*
 * psc_multilock_cond_wait - Wait for one condition to occur.
 * @mlc: the multilockable condition to wait for.
 * @mutex: an optional mutex that will be unlocked in the critical section,
 *	for avoiding missed wakeups from races.
 */
void
psc_multilock_cond_wait(struct psc_multilock_cond *mlc, pthread_mutex_t *mutex)
{
	int rc;

	psc_pthread_mutex_lock(&mlc->mlc_mutex);
	psc_pthread_mutex_unlock(mutex);
	rc = pthread_cond_wait(&mlc->mlc_cond, &mlc->mlc_mutex);
	if (rc)
		psc_fatalx("pthread_cond_wait: %s", strerror(rc));
	psc_pthread_mutex_unlock(&mlc->mlc_mutex);
}

/*
 * psc_multilock_addcond - add a condition to a multilock.
 * @ml: a multilock.
 * @mlc: the multilock condition to add.
 * @masked: whether condition is active.
 */
int
psc_multilock_addcond(struct psc_multilock *ml,
    struct psc_multilock_cond *mlc, int masked)
{
	struct psc_multilock_cond **mlcv;
	struct psc_multilock **mlv;
	int rc, nmlc, nml, j;

	rc = 0;

 restart:
	psc_pthread_mutex_lock(&mlc->mlc_mutex);

	if (pthread_mutex_trylock(&ml->ml_mutex)) {
		psc_pthread_mutex_unlock(&mlc->mlc_mutex);
		sched_yield();
		goto restart;
	}

	/* Ensure no associations already exist. */
	nmlc = dynarray_len(&ml->ml_conds);
	mlcv = dynarray_get(&ml->ml_conds);
	nml = dynarray_len(&mlc->mlc_multilocks);
	mlv = dynarray_get(&mlc->mlc_multilocks);

	for (j = 0; j < nmlc; j++)
		if (mlcv[j] == mlc)
			psc_fatalx("mlc %s already registered in multilock %s",
			    mlc->mlc_name, ml->ml_name);
	for (j = 0; j < nml; j++)
		if (mlv[j] == ml)
			psc_fatalx("ml %s already registered multilock_cond %s",
			    ml->ml_name, mlc->mlc_name);

	/* Associate multilock with the condition. */
	if (dynarray_add(&ml->ml_conds, mlc) == -1) {
		rc = -1;
		goto done;
	}
	if (dynarray_add(&mlc->mlc_multilocks, ml) == -1) {
		rc = -1;
		dynarray_remove(&ml->ml_conds, mlc);
		goto done;
	}

	nmlc = dynarray_len(&ml->ml_conds);
	if (vbitmap_resize(ml->ml_mask, nmlc) == -1) {
		rc = -1;
		dynarray_remove(&mlc->mlc_multilocks, ml);
		dynarray_remove(&ml->ml_conds, mlc);
		goto done;
	}
	qsort(dynarray_get(&mlc->mlc_multilocks),
	    dynarray_len(&mlc->mlc_multilocks),
	    sizeof(void *), psc_multilock_cmp);
	psc_vbitmap_setval(ml->ml_mask, nmlc - 1, masked);

 done:
	psc_pthread_mutex_unlock(&ml->ml_mutex);
	psc_pthread_mutex_unlock(&mlc->mlc_mutex);
	return (rc);
}

/*
 * psc_multilock_init - initialize a multilock.
 * @ml: the multilock to initialize.
 */
void
psc_multilock_init(struct psc_multilock *ml, const char *name, ...)
{
	va_list ap;

	memset(ml, 0, sizeof(*ml));
	dynarray_init(&ml->ml_conds);
	psc_pthread_mutex_init(&ml->ml_mutex);
	pthread_cond_init(&ml->ml_cond, NULL);
	ml->ml_mask = vbitmap_new(0);
	if (ml->ml_mask == NULL)
		psc_fatal("vbitmap_new");

	va_start(ap, name);
	vsnprintf(ml->ml_name, sizeof(ml->ml_name), name, ap);
	va_end(ap);
}

/*
 * psc_multilock_free - destroy a multilock.
 * @ml: the multilock to release.
 */
void
psc_multilock_free(struct psc_multilock *ml)
{
	psc_multilock_reset(ml);
	dynarray_free(&ml->ml_conds);
	vbitmap_free(ml->ml_mask);
}

/*
 * psc_multilock_mask_cond - update multilock active condition mask.
 * @ml: the multilock.
 * @mlc: the condition to mask.
 * @set: whether to add or remove it from the mask.
 */
void
psc_multilock_mask_cond(struct psc_multilock *ml,
    const struct psc_multilock_cond *mlc, int set)
{
	struct psc_multilock_cond **mlcv;
	int nmlc, j;

	psc_pthread_mutex_lock(&ml->ml_mutex);
	ml->ml_owner = pthread_self();
	nmlc = dynarray_len(&ml->ml_conds);
	mlcv = dynarray_get(&ml->ml_conds);
	for (j = 0; j < nmlc; j++)
		if (mlcv[j] == mlc) {
			if (set)
				vbitmap_set(ml->ml_mask, j);
			else
				vbitmap_unset(ml->ml_mask, j);
			ml->ml_owner = 0;
			psc_pthread_mutex_unlock(&ml->ml_mutex);
			return;
		}
	psc_fatalx("couldn't find cond %s in multilock %s\n",
	    mlc->mlc_name, ml->ml_name);
}

/*
 * psc_multilock_wait - wait for any condition in a multilock to change.
 * @ml: the multilock to wait on.
 * @data: pointer to user data filled in from the multilock_cond.
 * @usec: # microseconds till timeout.
 */
int
psc_multilock_wait(struct psc_multilock *ml, void *datap, int usec)
{
	struct psc_multilock_cond *mlc, **mlcv;
	int rc, allmasked, won, nmlc, j;

	won = 0;
 restart:
	psc_pthread_mutex_lock(&ml->ml_mutex);
	ml->ml_owner = pthread_self();

	/* Check for missed wakeups during a critical section. */
	if (ml->ml_flags & PMLF_CRITSECT) {
		ml->ml_flags &= ~PMLF_CRITSECT;
		if (ml->ml_waker)
			goto checkwaker;
	}

	nmlc = dynarray_len(&ml->ml_conds);
	mlcv = dynarray_get(&ml->ml_conds);

	if (nmlc == 0)
		psc_fatalx("multilock has no conditions and will never wake up");

	allmasked = 1;
	for (j = 0; j < nmlc; j++) {
		/* check if all conditions have been masked off */
		if (allmasked && vbitmap_get(ml->ml_mask, j))
			allmasked = 0;

		psc_pthread_mutex_lock(&mlcv[j]->mlc_mutex);
		mlcv[j]->mlc_winner = NULL;
		psc_pthread_mutex_unlock(&mlcv[j]->mlc_mutex);
	}

	if (allmasked)
		psc_fatalx("multilock has all conditions masked and will never wake up");

	if (usec) {
		struct timeval tv, res, adj;
		struct timespec ntv;

		if (gettimeofday(&tv, NULL) == -1)
			psc_fatal("gettimeoday");
		adj.tv_sec = usec / 1000000;
		adj.tv_usec = usec % 1000000;
		timeradd(&tv, &adj, &res);
		ntv.tv_sec = res.tv_sec;
		ntv.tv_nsec = res.tv_usec * 1000;

		rc = pthread_cond_timedwait(&ml->ml_cond,
		    &ml->ml_mutex, &ntv);
		if (rc == ETIMEDOUT) {
			psc_pthread_mutex_unlock(&ml->ml_mutex);
			return (-ETIMEDOUT);
		}
		if (rc)
			psc_fatalx("pthread_cond_timedwait: %s",
			    strerror(rc));
	} else {
		rc = pthread_cond_wait(&ml->ml_cond, &ml->ml_mutex);
		if (rc)
			psc_fatalx("pthread_cond_wait: %s", strerror(rc));
	}

 checkwaker:
	mlc = ml->ml_waker;
	ml->ml_owner = 0;
	psc_pthread_mutex_unlock(&ml->ml_mutex);

	psc_pthread_mutex_lock(&mlc->mlc_mutex);
	if (mlc->mlc_flags & PMLCF_WAKEALL) {
		*(void **)datap = (void *)mlc->mlc_data;
		won = 1;
	} else if (mlc->mlc_winner == NULL) {
		*(void **)datap = (void *)mlc->mlc_data;
		mlc->mlc_winner = ml;
		won = 1;
	}

	psc_pthread_mutex_unlock(&mlc->mlc_mutex);

	if (!won) {
		sched_yield();
		goto restart;
	}
	return (0);
}

/*
 * psc_multilock_reset - release all conditions from a multilock and reclaim
 *	all of a multilock's associated memory.
 * @ml: the multilock to reset.
 */
void
psc_multilock_reset(struct psc_multilock *ml)
{
	struct psc_multilock_cond **mlcv;
	int nmlc, j;

 restart:
	psc_pthread_mutex_lock(&ml->ml_mutex);
	ml->ml_owner = pthread_self();
	nmlc = dynarray_len(&ml->ml_conds);
	mlcv = dynarray_get(&ml->ml_conds);

	for (j = 0; j < nmlc; j++) {
		/*
		 * XXX the order everywhere else here is to
		 * lock the mlc then the ml, so our violation
		 * here may be bad news.
		 */
		if (pthread_mutex_trylock(&mlcv[0]->mlc_mutex)) {
			ml->ml_owner = 0;
			psc_pthread_mutex_unlock(&ml->ml_mutex);
			sched_yield();
			goto restart;
		}

		dynarray_remove(&mlcv[0]->mlc_multilocks, ml);
		qsort(dynarray_get(&mlcv[0]->mlc_multilocks),
		    dynarray_len(&mlcv[0]->mlc_multilocks),
		    sizeof(void *), psc_multilock_cmp);
		psc_pthread_mutex_unlock(&mlcv[0]->mlc_mutex);
		/* Remove it so we don't process it twice. */
		dynarray_remove(&ml->ml_conds, mlcv[0]);
	}

	dynarray_reset(&ml->ml_conds);
	vbitmap_resize(ml->ml_mask, 0);
	ml->ml_flags = 0;
	ml->ml_owner = 0;
	psc_pthread_mutex_unlock(&ml->ml_mutex);
}

/*
 * psc_multilock_cond_nwaiters - count the number of waiters sleeping
 *	on a multilock condition.  The count may differ from the
 *	value in mlc->mlc_nmultilocks since there may be gaps in
 *	the array of multilocks when they are coming and going.
 * @mlc: the multilock condition to check.
 */
size_t
psc_multilock_cond_nwaiters(struct psc_multilock_cond *mlc)
{
	int n;

	psc_pthread_mutex_lock(&mlc->mlc_mutex);
	n = dynarray_len(&mlc->mlc_multilocks);
	psc_pthread_mutex_unlock(&mlc->mlc_mutex);
	return (n);
}

/*
 * psc_multilock_enter_critsect - enter a critical section.
 * @ml: the multilock.
 */
void
psc_multilock_enter_critsect(struct psc_multilock *ml)
{
	psc_pthread_mutex_lock(&ml->ml_mutex);
	ml->ml_flags |= PMLF_CRITSECT;
	ml->ml_waker = NULL;
	psc_pthread_mutex_unlock(&ml->ml_mutex);
}

/*
 * psc_multilock_leave_critsect - leave a critical section.
 * @ml: the multilock.
 */
void
psc_multilock_leave_critsect(struct psc_multilock *ml)
{
	psc_pthread_mutex_lock(&ml->ml_mutex);
	ml->ml_flags &= ~PMLF_CRITSECT;
	/* XXX should release any captured events */
	psc_pthread_mutex_unlock(&ml->ml_mutex);
}

/*
 * psc_multilock_hascond - determine if a condition has been registered in a
 *	multilock.
 * @ml: the multilock.
 * @mlc: the multilock condition to check the existence of.
 */
int
psc_multilock_hascond(struct psc_multilock *ml, struct psc_multilock_cond *mlc)
{
	struct psc_multilock_cond **mlcv;
	int nmlc, j, rc;

	rc = 0;
	psc_pthread_mutex_lock(&ml->ml_mutex);
	nmlc = dynarray_len(&ml->ml_conds);
	mlcv = dynarray_get(&ml->ml_conds);
	for (j = 0; j < nmlc; j++)
		if (mlcv[j] == mlc) {
			rc = 1;
			break;
		}
	psc_pthread_mutex_unlock(&ml->ml_mutex);
	return (rc);
}

/*
 * psc_multilock_prconds - print list of conditions registered in a multilock.
 * @ml: the multilock to dump.
 */
void
psc_multilock_prconds(struct psc_multilock *ml)
{
	struct psc_multilock_cond **mlcv;
	int j, nmlc;

	psc_pthread_mutex_lock(&ml->ml_mutex);

	nmlc = dynarray_len(&ml->ml_conds);
	mlcv = dynarray_get(&ml->ml_conds);

	for (j = 0; j < nmlc; j++)
		printf(" ml %s has mlc %s (masked %s)\n",
		    ml->ml_name, mlcv[j]->mlc_name,
		    psc_multilock_masked_cond(ml, mlcv[j]) ? "on" : "off");

	psc_pthread_mutex_unlock(&ml->ml_mutex);
}
