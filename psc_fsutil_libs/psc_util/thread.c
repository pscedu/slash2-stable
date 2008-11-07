/* $Id$ */

#include "psc_util/subsys.h"

#include <sys/syscall.h>

#include <err.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "psc_types.h"
#include "psc_ds/dynarray.h"
#include "psc_ds/hash.h"
#include "psc_util/alloc.h"
#include "psc_util/cdefs.h"
#include "psc_util/lock.h"
#include "psc_util/thread.h"

struct dynarray	pscThreads;
psc_spinlock_t	pscThreadsLock = LOCK_INITIALIZER;
pthread_key_t	psc_thrkey;
int		psc_thrinit;
pthread_once_t	psc_thr_once = PTHREAD_ONCE_INIT;

void
pscthr_destroy(void *arg)
{
	struct psc_thread *thr = arg;

	reqlock(&thr->pscthr_lock);
	PSCFREE(thr->pscthr_loglevels);

	/*
	 * I don't think we can do this unless we disallow any
	 * external indexing into this array.  Things that need to
	 * reference a thread should maintain a pointer to the pscthr
	 * and not a pscThreads index.
	 *
	 * At this point, pscThreads is only used by things like ctlapi.
	 */
	spinlock(&pscThreadsLock);
	dynarray_remove(&pscThreads, thr);
	freelock(&pscThreadsLock);

	if (thr->pscthr_dtor)
		thr->pscthr_dtor(thr->pscthr_private);
	if (thr->pscthr_flags & PTF_FREE)
		free(thr);
}

void
pscthrs_init(void)
{
	int rc;

	rc = pthread_key_create(&psc_thrkey, pscthr_destroy);
	if (rc)
		psc_fatalx("pthread_key_create: %s", strerror(rc));
	psc_thrinit = 1;
}

/*
 * pscthr_begin: each new thread begins its life here.
 *	This routine invokes the thread's real start routine once
 *	it's safe after the thread who created us has finished our
 *	(external) initialization.
 * @arg: thread structure.
 */
__static void *
pscthr_begin(void *arg)
{
	struct psc_thread *thr = arg;
	int rc;

	spinlock(&thr->pscthr_lock);
	thr->pscthr_pthread = pthread_self();
	thr->pscthr_thrid = syscall(SYS_gettid);
	rc = pthread_setspecific(psc_thrkey, thr);
	if (rc)
		psc_fatalx("pthread_setspecific: %s", strerror(rc));
	freelock(&thr->pscthr_lock);
	return (thr->pscthr_start(thr));
}

struct psc_thread *
pscthr_get_canfail(void)
{
	if (!psc_thrinit)
		pthread_once(&psc_thr_once, pscthrs_init);
	return (pthread_getspecific(psc_thrkey));
}

struct psc_thread *
pscthr_get(void)
{
	struct psc_thread *thr;

	thr = pscthr_get_canfail();
	psc_assert(thr);
	return (thr);
}

/*
 * pscthr_init - initialize a thread.
 * @thr: thread structure to be initialized, must already be allocated.
 * @type: application-specific thread type.
 * @start: thread execution routine.  By specifying a NULL routine,
 *	no pthread will be spawned (assuming that an actual pthread
 *	already exists or will be taken care of).
 * @private: thread-specific data.
 * @flags: operational flags.
 * @dtor: optional destructor for thread.
 * @namearg: application-specific name for thread.
 */
void
_pscthr_init(struct psc_thread *thr, int type, void *(*start)(void *),
    void *private, int flags, void (*dtor)(void *), const char *namefmt, ...)
{
	va_list ap;
	int rc, n;

	if (!psc_thrinit)
		pthread_once(&psc_thr_once, pscthrs_init);

	if (flags & PTF_PAUSED)
		psc_fatalx("pscthr_init: PTF_PAUSED specified");

	LOCK_INIT(&thr->pscthr_lock);
	thr->pscthr_run = 1;
	thr->pscthr_type = type;
	thr->pscthr_start = start;
	thr->pscthr_private = private;
	thr->pscthr_flags = flags;
	thr->pscthr_dtor = dtor;

	va_start(ap, namefmt);
	rc = vsnprintf(thr->pscthr_name, sizeof(thr->pscthr_name),
	    namefmt, ap);
	va_end(ap);

	if (rc == -1)
		psc_fatal("vsnprintf");
	if (rc >= (int)sizeof(thr->pscthr_name))
		psc_fatalx("pscthr_init: thread name too long: %s", namefmt);

	thr->pscthr_loglevels = PSCALLOC(psc_nsubsys *
	    sizeof(*thr->pscthr_loglevels));

	for (n = 0; n < psc_nsubsys; n++)
		thr->pscthr_loglevels[n] = psc_log_getlevel(n);

	if (start) {
		if ((rc = pthread_create(&thr->pscthr_pthread, NULL,
		    pscthr_begin, thr)) != 0)
			psc_fatalx("pthread_create: %s", strerror(rc));
	} else {
		thr->pscthr_pthread = pthread_self();
		thr->pscthr_thrid = syscall(SYS_gettid);
		rc = pthread_setspecific(psc_thrkey, thr);
		if (rc)
			psc_fatalx("pthread_setspecific: %s", strerror(rc));
	}

	spinlock(&pscThreadsLock);
	if (dynarray_add(&pscThreads, thr) == -1)
		psc_fatal("dynarray_add");
	freelock(&pscThreadsLock);
}

/*
 * psc_log_getlevel - get thread logging level for the specified subsystem.
 * This routine is not intended for general-purpose usage.
 * @subsys: subsystem ID.
 */
int
psc_log_getlevel(int subsys)
{
	struct psc_thread *thr;

	thr = pscthr_get_canfail();
	if (thr == NULL)
		return (psc_log_getlevel_ss(subsys));
	if (subsys >= psc_nsubsys)
		psc_fatalx("subsystem out of bounds (%d)", subsys);
	return (thr->pscthr_loglevels[subsys]);
}

/*
 * pscthr_getname - get thread name.
 * This routine is not intended for general-purpose usage.
 */
const char *
pscthr_getname(void)
{
	struct psc_thread *thr;

	thr = pscthr_get_canfail();
	if (thr == NULL)
		return (NULL);
	return (thr->pscthr_name);
}

/*
 * pscthr_setpause - set thread pause state.
 * @thr: the thread.
 * @pause: whether to pause or unpause the thread.
 */
void
pscthr_setpause(struct psc_thread *thr, int pause)
{
	spinlock(&thr->pscthr_lock);
	if (pause ^ (thr->pscthr_flags & PTF_PAUSED))
		pthread_kill(thr->pscthr_pthread,
		    pause ? SIGUSR1 : SIGUSR2);
	freelock(&thr->pscthr_lock);
}

/*
 * pscthr_sigusr2 - catch SIGUSR1: pause the thread.
 * @sig: signal number.
 */
void
pscthr_sigusr1(__unusedx int sig)
{
	struct psc_thread *thr;
	int locked;

	thr = pscthr_get();
	while (!tryreqlock(&thr->pscthr_lock, &locked))
		sched_yield();
	thr->pscthr_flags |= PTF_PAUSED;
	ureqlock(&thr->pscthr_lock, locked);
	while (thr->pscthr_flags & PTF_PAUSED) {
		usleep(500);
		sched_yield();
	}
}

/*
 * pscthr_sigusr2 - catch SIGUSR2: unpause the thread.
 * @sig: signal number.
 */
void
pscthr_sigusr2(__unusedx int sig)
{
	struct psc_thread *thr;
	int locked;

	thr = pscthr_get();
	while (!tryreqlock(&thr->pscthr_lock, &locked))
		sched_yield();
	thr->pscthr_flags &= ~PTF_PAUSED;
	ureqlock(&thr->pscthr_lock, locked);
}
