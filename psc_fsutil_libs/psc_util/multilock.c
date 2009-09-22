/* $Id$ */

/*
 * Multiple lock routines: for waiting on any number of
 * conditions to become available.
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
#include "psc_util/cdefs.h"
#include "psc_util/log.h"
#include "psc_util/multilock.h"
#include "psc_util/pthrutil.h"

/*
 * multilock_cmp - Compare two multilocks, used for sorting.  Sorting is
 *	necessary to avoid deadlocking.
 * @a: a multilock
 * @b: another multilock
 */
__static int
multilock_cmp(const void *a, const void *b)
{
	struct multilock *const *mla = a, *const *mlb = b;

	if (*mla < *mlb)
		return (-1);
	else if (*mla > *mlb)
		return (1);
	return (0);
}

/**
 * multilock_cond_init - initialize a multilock condition.
 * @mlc: the condition to initialize.
 * @data: pointer to user data.
 */
void
multilock_cond_init(struct multilock_cond *mlc, const void *data,
    int flags, const char *name, ...)
{
	va_list ap;

	memset(mlc, 0, sizeof(*mlc));
	dynarray_init(&mlc->mlc_multilocks);
	psc_pthread_mutex_init(&mlc->mlc_mutex);
	mlc->mlc_data = data;
	mlc->mlc_flags = flags;

	va_start(ap, name);
	vsnprintf(mlc->mlc_name, sizeof(mlc->mlc_name), name, ap);
	va_end(ap);
}

/*
 * multilock_cond_trylockall - try to lock all multilocks this
 *	condition is a member of.
 * @mlc: a multilock condition, which must itself be locked.
 * Returns zero on success, -1 on failure.
 */
__static int
multilock_cond_trylockall(struct multilock_cond *mlc)
{
	struct multilock **mlv;
	int nml, j, k;

	nml = dynarray_len(&mlc->mlc_multilocks);
	mlv = dynarray_get(&mlc->mlc_multilocks);
	for (j = 0; j < nml; j++)
		if (pthread_mutex_trylock(&mlv[j]->ml_mutex)) {
			for (k = 0; k < j; k++)
				pthread_mutex_unlock(&mlv[k]->ml_mutex);
			return (-1);
		}
	return (0);
}

/*
 * multilock_cond_unlockall - unlock all multilocks this
 *	condition is a member of.
 * @mlc: a multilock condition, which must itself be locked.
 */
__static void
multilock_cond_unlockall(struct multilock_cond *mlc)
{
	struct multilock **mlv;
	int nml, j;

	nml = dynarray_len(&mlc->mlc_multilocks);
	mlv = dynarray_get(&mlc->mlc_multilocks);
	for (j = 0; j < nml; j++)
		pthread_mutex_unlock(&mlv[j]->ml_mutex);
}

/**
 * multilock_cond_destroy - release a multilock condition.
 * @mlc: the condition to release.
 */
void
multilock_cond_destroy(struct multilock_cond *mlc)
{
	struct multilock *ml, **mlv;
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
			pthread_mutex_unlock(&mlc->mlc_mutex);
			sched_yield();
			goto restart;
		}
		dynarray_remove(&ml->ml_conds, mlc);
		dynarray_remove(&mlc->mlc_multilocks, ml);
		pthread_mutex_unlock(&ml->ml_mutex);

	}
	dynarray_free(&mlc->mlc_multilocks);
}

/*
 * multilock_masked_cond - determine if a condition is masked
 *	off in a multilock.
 * @ml: multilock to check in, which must be locked.
 * @mlc: condition to check for.
 */
int
multilock_masked_cond(const struct multilock *ml,
    const struct multilock_cond *mlc)
{
	struct multilock_cond **mlcv;
	int nmlc, j;

	mlcv = dynarray_get(&ml->ml_conds);
	nmlc = dynarray_len(&ml->ml_conds);
	for (j = 0; j < nmlc; j++)
		if (mlcv[j] == mlc)
			return (vbitmap_get(ml->ml_mask, j));
	psc_fatalx("could not find condition in multilock");
}

/*
 * multilock_cond_wakeup - wakeup multilocks waiting on a condition.
 * @mlc: a multilock condition, which must be unlocked on entry
 *	and will be locked on exit.
 */
void
multilock_cond_wakeup(struct multilock_cond *mlc)
{
	struct multilock **mlv;
	int nml, j, count;

	count = 0;
 restart:
	psc_pthread_mutex_lock(&mlc->mlc_mutex);
	if (multilock_cond_trylockall(mlc)) {
		if (count++ == 300000)
			psc_errorx("mlcond %s failed to lock his "
			    "multilocks - possible deadlock if this "
			    "message repeats", mlc->mlc_name);
		pthread_mutex_unlock(&mlc->mlc_mutex);
		sched_yield();
		goto restart;
	}
	nml = dynarray_len(&mlc->mlc_multilocks);
	mlv = dynarray_get(&mlc->mlc_multilocks);
	for (j = 0; j < nml; j++)
		if (multilock_masked_cond(mlv[j], mlc)) {
			mlv[j]->ml_waker = mlc;
			pthread_cond_signal(&mlv[j]->ml_cond);
		}
	multilock_cond_unlockall(mlc);
	pthread_mutex_unlock(&mlc->mlc_mutex);
}

/*
 * multilock_addcond - add a condition to a multilock.
 * @ml: a multilock.
 * @mlc: the multilock condition to add.
 * @masked: whether condition is active.
 */
int
multilock_addcond(struct multilock *ml, struct multilock_cond *mlc,
    int masked)
{
	struct multilock_cond **mlcv;
	struct multilock **mlv;
	int rc, nmlc, nml, j;

	rc = 0;

 restart:
	psc_pthread_mutex_lock(&mlc->mlc_mutex);

	if (pthread_mutex_trylock(&ml->ml_mutex)) {
		pthread_mutex_unlock(&mlc->mlc_mutex);
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
	    sizeof(void *), multilock_cmp);
	if (masked)
		vbitmap_set(ml->ml_mask, nmlc - 1);
	else
		vbitmap_unset(ml->ml_mask, nmlc - 1);

 done:
	pthread_mutex_unlock(&ml->ml_mutex);
	pthread_mutex_unlock(&mlc->mlc_mutex);
	return (rc);
}

/*
 * multilock_init - initialize a multilock.
 * @ml: the multilock to initialize.
 */
void
multilock_init(struct multilock *ml, const char *name, ...)
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
 * multilock_free - destroy a multilock.
 * @ml: the multilock to release.
 */
void
multilock_free(struct multilock *ml)
{
	multilock_reset(ml);
	dynarray_free(&ml->ml_conds);
	vbitmap_free(ml->ml_mask);
}

/*
 * multilock_mask_cond - update multilock active condition mask.
 * @ml: the multilock.
 * @mlc: the condition to mask.
 * @set: whether to add or remove it from the mask.
 */
void
multilock_mask_cond(struct multilock *ml,
    const struct multilock_cond *mlc, int set)
{
	struct multilock_cond **mlcv;
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
			pthread_mutex_unlock(&ml->ml_mutex);
			return;
		}
	psc_fatalx("couldn't find cond %s in multilock %s\n",
	    mlc->mlc_name, ml->ml_name);
}

/*
 * multilock_wait - wait for any condition in a multilock to change.
 * @ml: the multilock to wait on.
 * @data: pointer to user data filled in from the multilock_cond.
 * @usec: # microseconds till timeout.
 */
int
multilock_wait(struct multilock *ml, void *datap, int usec)
{
	struct multilock_cond *mlc, **mlcv;
	int allmasked, won, nmlc, j;

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
		pthread_mutex_unlock(&mlcv[j]->mlc_mutex);
	}

	if (allmasked)
		psc_fatalx("multilock has all conditions masked and will never wake up");

	if (usec) {
		struct timeval tv, res, adj;
		struct timespec ntv;
		int rc;

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
			pthread_mutex_unlock(&ml->ml_mutex);
			return (-ETIMEDOUT);
		}
		if (rc)
			psc_fatalx("pthread_cond_timedwait: %s",
			    strerror(rc));
	} else
		if (pthread_cond_wait(&ml->ml_cond, &ml->ml_mutex) == -1)
			psc_fatal("pthread_cond_wait");

 checkwaker:
	mlc = ml->ml_waker;
	ml->ml_owner = 0;
	pthread_mutex_unlock(&ml->ml_mutex);

	psc_pthread_mutex_lock(&mlc->mlc_mutex);
	if (mlc->mlc_flags & PMLCF_WAKEALL) {
		*(void **)datap = (void *)mlc->mlc_data;
		won = 1;
	} else if (mlc->mlc_winner == NULL) {
		*(void **)datap = (void *)mlc->mlc_data;
		mlc->mlc_winner = ml;
		won = 1;
	}

	pthread_mutex_unlock(&mlc->mlc_mutex);

	if (!won) {
		sched_yield();
		goto restart;
	}
	return (0);
}

/*
 * multilock_reset - release all conditions from a multilock and reclaim
 *	all of a multilock's associated memory.
 * @ml: the multilock to reset.
 */
void
multilock_reset(struct multilock *ml)
{
	struct multilock_cond **mlcv;
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
			pthread_mutex_unlock(&ml->ml_mutex);
			sched_yield();
			goto restart;
		}

		dynarray_remove(&mlcv[0]->mlc_multilocks, ml);
		qsort(dynarray_get(&mlcv[0]->mlc_multilocks),
		    dynarray_len(&mlcv[0]->mlc_multilocks),
		    sizeof(void *), multilock_cmp);
		pthread_mutex_unlock(&mlcv[0]->mlc_mutex);
		/* Remove it so we don't process it twice. */
		dynarray_remove(&ml->ml_conds, mlcv[0]);
	}

	dynarray_reset(&ml->ml_conds);
	vbitmap_resize(ml->ml_mask, 0);
	ml->ml_flags = 0;
	ml->ml_owner = 0;
	pthread_mutex_unlock(&ml->ml_mutex);
}

/*
 * multilock_cond_nwaitors - count the number of waitors sleeping
 *	on a multilock condition.  The count may differ from the
 *	value in mlc->mlc_nmultilocks since there may be gaps in
 *	the array of multilocks when they are coming and going.
 * @mlc: the multilock condition to check.
 */
size_t
multilock_cond_nwaitors(struct multilock_cond *mlc)
{
	int n;

	psc_pthread_mutex_lock(&mlc->mlc_mutex);
	n = dynarray_len(&mlc->mlc_multilocks);
	pthread_mutex_unlock(&mlc->mlc_mutex);
	return (n);
}

/*
 * multilock_enter_critsect - enter a critical section.
 * @ml: the multilock.
 */
void
multilock_enter_critsect(struct multilock *ml)
{
	psc_pthread_mutex_lock(&ml->ml_mutex);
	ml->ml_flags |= PMLF_CRITSECT;
	ml->ml_waker = NULL;
	pthread_mutex_unlock(&ml->ml_mutex);
}

/*
 * multilock_leave_critsect - leave a critical section.
 * @ml: the multilock.
 */
void
multilock_leave_critsect(struct multilock *ml)
{
	psc_pthread_mutex_lock(&ml->ml_mutex);
	ml->ml_flags &= ~PMLF_CRITSECT;
	pthread_mutex_unlock(&ml->ml_mutex);
}

/*
 * multilock_hascond - determine if a condition has been registered in a
 *	multilock.
 * @ml: the multilock.
 * @mlc: the multilock condition to check the existence of.
 */
int
multilock_hascond(struct multilock *ml, struct multilock_cond *mlc)
{
	struct multilock_cond **mlcv;
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
	pthread_mutex_unlock(&ml->ml_mutex);
	return (rc);
}
