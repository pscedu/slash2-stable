/* $Id$ */
/*
 * %PSC_START_COPYRIGHT%
 * -----------------------------------------------------------------------------
 * Copyright (c) 2009-2012, Pittsburgh Supercomputing Center (PSC).
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

#include <errno.h>
#include <pthread.h>
#include <string.h>

#include "pfl/time.h"
#include "psc_ds/vbitmap.h"
#include "psc_util/log.h"
#include "psc_util/pthrutil.h"
#include "psc_util/thread.h"

void
psc_mutex_init(struct pfl_mutex *mut)
{
	pthread_mutexattr_t attr;
	int rc;

	memset(mut, 0, sizeof(*mut));
	rc = pthread_mutexattr_init(&attr);
	if (rc)
		psc_fatalx("pthread_mutexattr_init: %s", strerror(rc));
	rc = pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_ERRORCHECK);
	if (rc)
		psc_fatalx("pthread_mutexattr_settype: %s",
		    strerror(rc));
	rc = pthread_mutex_init(&mut->pm_mutex, &attr);
	if (rc)
		psc_fatalx("pthread_mutex_init: %s", strerror(rc));

	rc = pthread_mutexattr_destroy(&attr);
	if (rc)
		psc_fatalx("pthread_mutexattr_destroy: %s",
		    strerror(rc));
}

void
psc_mutex_destroy(struct pfl_mutex *mut)
{
	int rc;

	rc = pthread_mutex_destroy(&mut->pm_mutex);
	if (rc)
		psc_fatalx("pthread_mutex_destroy: %s", strerror(rc));
}

void
_psc_mutex_lock(const struct pfl_callerinfo *pci,
    struct pfl_mutex *mut)
{
	int rc;

	rc = pthread_mutex_lock(&mut->pm_mutex);
	if (rc)
		psc_fatalx("pthread_mutex_lock: %s", strerror(rc));
	mut->pm_owner = pthread_self();
	psclog_vdbg("mutex@%p acquired", mut);
}

void
_psc_mutex_unlock(const struct pfl_callerinfo *pci,
    struct pfl_mutex *mut)
{
	int rc;

	mut->pm_owner = 0;
	rc = pthread_mutex_unlock(&mut->pm_mutex);
	if (rc)
		psc_fatalx("pthread_mutex_unlock: %s", strerror(rc));
	psclog_vdbg("mutex@%p released", mut);
}

int
_psc_mutex_reqlock(const struct pfl_callerinfo *pci,
    struct pfl_mutex *mut)
{
	int rc;

	rc = pthread_mutex_lock(&mut->pm_mutex);
	if (rc == EDEADLK)
		rc = 1;
	else if (rc)
		psc_fatalx("pthread_mutex_unlock: %s", strerror(rc));
	mut->pm_owner = pthread_self();
	psclog_vdbg("mutex@%p acquired req=%d", mut, rc);
	return (rc);
}

void
_psc_mutex_ureqlock(const struct pfl_callerinfo *pci,
    struct pfl_mutex *mut, int waslocked)
{
	if (!waslocked)
		psc_mutex_unlock(mut);
}

int
_psc_mutex_trylock(const struct pfl_callerinfo *pci,
    struct pfl_mutex *mut)
{
	int rc;

	psc_assert(!psc_mutex_haslock(mut));
	rc = pthread_mutex_trylock(&mut->pm_mutex);
	if (rc == 0) {
		mut->pm_owner = pthread_self();
		psclog_vdbg("mutex=%p acquired", mut);
		return (1);
	}
	if (rc == EBUSY)
		return (0);
	psc_fatalx("pthread_mutex_trylock: %s", strerror(rc));
}

int
psc_mutex_haslock(struct pfl_mutex *mut)
{
	return (mut->pm_owner == pthread_self());
}

int
_psc_mutex_tryreqlock(const struct pfl_callerinfo *pci,
    struct pfl_mutex *mut, int *waslocked)
{
	if (psc_mutex_haslock(mut)) {
		*waslocked = 1;
		return (1);
	}
	*waslocked = 0;
	return (psc_mutex_trylock(mut));
}

void
_psc_mutex_ensure_locked(const struct pfl_callerinfo *pci,
    struct pfl_mutex *m)
{
	psc_assert(psc_mutex_haslock(m));
}

void
psc_rwlock_init(struct psc_rwlock *rw)
{
	int rc;

	memset(rw, 0, sizeof(*rw));
	rc = pthread_rwlock_init(&rw->pr_rwlock, NULL);
	if (rc)
		psc_fatalx("pthread_rwlock_init: %s", strerror(rc));
	INIT_SPINLOCK(&rw->pr_lock);
	psc_dynarray_init(&rw->pr_readers);
}

void
psc_rwlock_destroy(struct psc_rwlock *rw)
{
	int rc;

	rc = pthread_rwlock_destroy(&rw->pr_rwlock);
	if (rc)
		psc_fatalx("pthread_rwlock_destroy: %s", strerror(rc));
	psc_dynarray_free(&rw->pr_readers);
}

void
_psc_rwlock_rdlock(const struct pfl_callerinfo *pci,
    struct psc_rwlock *rw)
{
	pthread_t p;
	int rc;

	p = pthread_self();
	psc_assert(rw->pr_writer != p);

	spinlock(&rw->pr_lock);
	psc_assert(!psc_dynarray_exists(&rw->pr_readers, (void *)p));
	psc_dynarray_add(&rw->pr_readers, (void *)p);
	freelock(&rw->pr_lock);

	rc = pthread_rwlock_rdlock(&rw->pr_rwlock);
	if (rc)
		psc_fatalx("pthread_rwlock_rdlock: %s", strerror(rc));
	psclog_vdbg("rwlock@%p reader lock acquired", rw);
}

void
_psc_rwlock_wrlock(const struct pfl_callerinfo *pci,
    struct psc_rwlock *rw)
{
	pthread_t p;
	int rc;

	p = pthread_self();
	psc_assert(rw->pr_writer != p);

	spinlock(&rw->pr_lock);
	psc_assert(!psc_dynarray_exists(&rw->pr_readers, (void *)p));
	freelock(&rw->pr_lock);

	rc = pthread_rwlock_wrlock(&rw->pr_rwlock);
	if (rc)
		psc_fatalx("pthread_rwlock_wrlock: %s", strerror(rc));
	rw->pr_writer = p;
	psclog_vdbg("rwlock@%p writer lock acquired", rw);
}

int
psc_rwlock_hasrdlock(struct psc_rwlock *rw)
{
	pthread_t p;
	int rc;

	p = pthread_self();

	if (rw->pr_writer == p)
		return (1);

	spinlock(&rw->pr_lock);
	rc = psc_dynarray_exists(&rw->pr_readers, (void *)p);
	freelock(&rw->pr_lock);
	return (rc);
}

int
psc_rwlock_haswrlock(struct psc_rwlock *rw)
{
	return (rw->pr_writer == pthread_self());
}

int
_psc_rwlock_reqrdlock(const struct pfl_callerinfo *pci,
    struct psc_rwlock *rw)
{
	if (psc_rwlock_hasrdlock(rw))
		return (1);
	psc_rwlock_rdlock(rw);
	return (0);
}

int
_psc_rwlock_reqwrlock(const struct pfl_callerinfo *pci,
    struct psc_rwlock *rw)
{
	int rc = 0;

	if (psc_rwlock_haswrlock(rw))
		return (1);
	if (psc_rwlock_hasrdlock(rw)) {
		psc_rwlock_unlock(rw);
		rc = 1;
	}
	psc_rwlock_wrlock(rw);
	return (rc);
}

void
_psc_rwlock_ureqlock(const struct pfl_callerinfo *pci,
    struct psc_rwlock *rw, int waslocked)
{
	if (!waslocked)
		psc_rwlock_unlock(rw);
}

void
_psc_rwlock_unlock(const struct pfl_callerinfo *pci,
    struct psc_rwlock *rw)
{
	int rc, wr = 0;
	pthread_t p;

	p = pthread_self();
	if (rw->pr_writer == p) {
		rw->pr_writer = 0;
		wr = 1;
	} else {
		spinlock(&rw->pr_lock);
		psc_dynarray_remove(&rw->pr_readers, (void *)p);
		freelock(&rw->pr_lock);
	}
	rc = pthread_rwlock_unlock(&rw->pr_rwlock);
	if (rc)
		psc_fatalx("pthread_rwlock_unlock: %s", strerror(rc));
	psclog_vdbg("rwlock@%p %s lock released", rw,
	    wr ? "writer" : "reader");
}

int
psc_cond_timedwait(pthread_cond_t *c, struct pfl_mutex *m,
    const struct timespec *tm)
{
	int rc;

	rc = pthread_cond_timedwait(c, &m->pm_mutex, tm);
	if (rc && rc != ETIMEDOUT)
		psc_fatalx("pthread_cond_timedwait: %s", strerror(rc));
	return (rc);
}
