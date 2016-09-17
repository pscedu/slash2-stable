/* $Id$ */
/*
 * %ISC_START_LICENSE%
 * ---------------------------------------------------------------------
 * Copyright 2015-2016, Google, Inc.
 * Copyright 2009-2016, Pittsburgh Supercomputing Center
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

#include <errno.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>

#include "pfl/pthrutil.h"
#include "pfl/thread.h"
#include "pfl/time.h"
#include "pfl/vbitmap.h"

#if PFL_DEBUG > 1
# include "pfl/str.h"
#endif

#define PFLOG_MUTEX(level, mut, fmt, ...)				\
	psclog((level), "mutex@%p: " fmt, (mut), ##__VA_ARGS__)

#define PMUT_LOG(mut, fmt, ...)						\
	do {								\
		if (((mut)->pm_flags & PMTXF_NOLOG) == 0)		\
			PFLOG_MUTEX((mut)->pm_flags & PMTXF_DEBUG ?	\
			    PLL_MAX : PLL_VDEBUG, (mut), fmt,		\
			    ##__VA_ARGS__);				\
	} while (0)

void
_psc_mutex_init(struct pfl_mutex *mut, int flags)
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

	mut->pm_flags = flags;

	PMUT_LOG(mut, "initialized");
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

#if PFL_DEBUG > 1
	psc_assert(!pfl_memchk(mut, 0, sizeof(*mut)));
#endif

	rc = pthread_mutex_lock(&mut->pm_mutex);
	/* asm-generic/errno.h: EDEADLK = 35 (lock against myself) */
	if (rc)
		psc_fatalx("pthread_mutex_lock: %s", strerror(rc));
	mut->pm_owner = pthread_self();
	mut->pm_lineno = pci ? pci->pci_lineno : __LINE__;
	PMUT_LOG(mut, "acquired");
}

void
_psc_mutex_unlock(const struct pfl_callerinfo *pci,
    struct pfl_mutex *mut)
{
	int rc, dolog = 0, loglevel = PLL_VDEBUG;

	mut->pm_owner = 0;
	mut->pm_lineno = 0;
	PMUT_LOG(mut, "releasing log=%d level=%d",
	    dolog = ((mut->pm_flags & PMTXF_NOLOG) == 0),
	    loglevel = (mut->pm_flags & PMTXF_DEBUG ? PLL_MAX :
	    PLL_VDEBUG));
	rc = pthread_mutex_unlock(&mut->pm_mutex);
	if (rc)
		PFLOG_MUTEX(PLL_FATAL, mut,
		    "pthread_mutex_unlock: %s", strerror(rc));
	if (dolog)
		PFLOG_MUTEX(loglevel, mut, "released");
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
		psc_fatalx("pthread_mutex_lock: %s", strerror(rc));
	mut->pm_owner = pthread_self();
	PMUT_LOG(mut, "acquired, req=%d", rc);
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
		PMUT_LOG(mut, "acquired");
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
pfl_rwlock_init(struct pfl_rwlock *rw)
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
pfl_rwlock_destroy(struct pfl_rwlock *rw)
{
	int rc;

	rc = pthread_rwlock_destroy(&rw->pr_rwlock);
	if (rc)
		psc_fatalx("pthread_rwlock_destroy: %s", strerror(rc));
	psc_dynarray_free(&rw->pr_readers);
}

void
_pfl_rwlock_rdlock(const struct pfl_callerinfo *pci,
    struct pfl_rwlock *rw)
{
	pthread_t p;
	void *pa;
	int rc;

	p = pthread_self();
	psc_assert(rw->pr_writer != p);

	pa = (void *)(uintptr_t)p;
	spinlock(&rw->pr_lock);
	psc_assert(!psc_dynarray_exists(&rw->pr_readers, pa));
	psc_dynarray_add(&rw->pr_readers, pa);
	freelock(&rw->pr_lock);

	rc = pthread_rwlock_rdlock(&rw->pr_rwlock);
	if (rc)
		psc_fatalx("pthread_rwlock_rdlock: %s", strerror(rc));
	psclog_vdebug("rwlock@%p reader lock acquired", rw);
}

void
_pfl_rwlock_wrlock(const struct pfl_callerinfo *pci,
    struct pfl_rwlock *rw)
{
	pthread_t p;
	int rc;

	p = pthread_self();
	psc_assert(rw->pr_writer != p);

	rc = pthread_rwlock_wrlock(&rw->pr_rwlock);
	if (rc)
		psc_fatalx("pthread_rwlock_wrlock: %s", strerror(rc));
	rw->pr_writer = p;
	psclog_vdebug("rwlock@%p writer lock acquired", rw);
}

int
pfl_rwlock_hasrdlock(struct pfl_rwlock *rw)
{
	pthread_t p;
	void *pa;
	int rc;

	p = pthread_self();

	if (rw->pr_writer == p)
		return (1);

	pa = (void *)(uintptr_t)p;

	spinlock(&rw->pr_lock);
	rc = psc_dynarray_exists(&rw->pr_readers, pa);
	freelock(&rw->pr_lock);
	return (rc);
}

int
pfl_rwlock_haswrlock(struct pfl_rwlock *rw)
{
	return (rw->pr_writer == pthread_self());
}

int
_pfl_rwlock_reqrdlock(const struct pfl_callerinfo *pci,
    struct pfl_rwlock *rw)
{
	if (pfl_rwlock_hasrdlock(rw))
		return (1);
	pfl_rwlock_rdlock(rw);
	return (0);
}

int
_pfl_rwlock_reqwrlock(const struct pfl_callerinfo *pci,
    struct pfl_rwlock *rw)
{
	if (pfl_rwlock_haswrlock(rw))
		return (1);
	pfl_rwlock_wrlock(rw);
	return (0);
}

void
_pfl_rwlock_ureqlock(const struct pfl_callerinfo *pci,
    struct pfl_rwlock *rw, int waslocked)
{
	if (!waslocked)
		pfl_rwlock_unlock(rw);
}

void
_pfl_rwlock_unlock(const struct pfl_callerinfo *pci,
    struct pfl_rwlock *rw)
{
	int rc, wr = 0;
	pthread_t p;

	(void)wr;
	p = pthread_self();
	if (rw->pr_writer == p) {
		rw->pr_writer = 0;
		wr = 1;
	} else {
		void *pa;

		pa = (void *)(uintptr_t)p;

		spinlock(&rw->pr_lock);
		psc_dynarray_remove(&rw->pr_readers, pa);
		freelock(&rw->pr_lock);
	}
	rc = pthread_rwlock_unlock(&rw->pr_rwlock);
	if (rc)
		psc_fatalx("pthread_rwlock_unlock: %s", strerror(rc));
	psclog_vdebug("rwlock@%p %s lock released", rw,
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
