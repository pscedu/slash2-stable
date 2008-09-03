/* $Id$ */

#include "psc_util/subsys.h"

#include <err.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "psc_types.h"
#include "psc_util/alloc.h"
#include "psc_util/cdefs.h"
#include "psc_util/lock.h"
#include "psc_util/thread.h"
#include "psc_util/threadtable.h"

struct dynarray    pscThreads;

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
	spinlock(&thr->pscthr_lock);
	freelock(&thr->pscthr_lock);
	return (thr->pscthr_start(thr));
}

/*
 * pscthr_init - initialize a thread.
 * @thr: thread structure to be initialized, must already be allocated.
 * @type: application-specific thread type.
 * @start: thread execution routine.  By specifying a NULL routine,
 *	no pthread will be spawned (assuming that an actual pthread
 *	already exists or will be taken care of).
 * @namearg: application-specific name for thread.
 */
void
pscthr_init(struct psc_thread *thr, int type,
    void *(*start)(void *), void *private, const char *namefmt, ...)
{
	va_list ap;
	int rc, n;

	/*
	 * Ensure that the thr is initialized before the new thread
	 *  attempts to access its data structures.
	 */
	LOCK_INIT(&thr->pscthr_lock);
	spinlock(&thr->pscthr_lock);

	va_start(ap, namefmt);
	rc = vsnprintf(thr->pscthr_name, sizeof(thr->pscthr_name),
	    namefmt, ap);
	va_end(ap);

	if (rc == -1)
		psc_fatal("vsnprintf");

	thr->pscthr_loglevels = PSCALLOC(psc_nsubsys *
	    sizeof(*thr->pscthr_loglevels));

	for (n = 0; n < psc_nsubsys; n++)
		thr->pscthr_loglevels[n] = psc_log_getlevel(n);

	thr->pscthr_type  = type;
	thr->pscthr_id    = dynarray_len(&pscThreads); /* XXX lockme? */
	thr->pscthr_start = start;

	if (start) {
		if ((rc = pthread_create(&thr->pscthr_pthread, NULL,
		    pscthr_begin, thr)) != 0)
			psc_fatalx("pthread_create: %s", strerror(rc));
	} else
		thr->pscthr_pthread = pthread_self();

	thr->pscthr_hashid = (u64)thr->pscthr_pthread;

	psc_threadtbl_put(thr);

	if (dynarray_add(&pscThreads, thr) == -1)
		psc_fatal("dynarray_add");

	thr->pscthr_run = 1;
	thr->pscthr_private = private;
	freelock(&thr->pscthr_lock);

	psc_info("spawned %s [thread %zu] [id %"_P_U64"x] [pthrid %lx] thr=%p"
		 " thr->type %d, passed type %d",
		 thr->pscthr_name, thr->pscthr_id,
		 thr->pscthr_hashid, thr->pscthr_pthread,
		 thr, thr->pscthr_type, type);
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

	thr = psc_threadtbl_get_canfail();
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

	thr = psc_threadtbl_get_canfail();
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

	thr = psc_threadtbl_get();
	while (tryreqlock(&thr->pscthr_lock, &locked))
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

	thr = psc_threadtbl_get();
	while (tryreqlock(&thr->pscthr_lock, &locked))
		sched_yield();
	thr->pscthr_flags &= ~PTF_PAUSED;
	ureqlock(&thr->pscthr_lock, locked);
}

/*
 * pscthr_destroy - remove thread resources from the process.
 * @thr: thread that is going away.
 * Notes: make sure psc_thread memory itself and any
 * threadtype-specific memory (pscthr_private) is released.
 */
void
pscthr_destroy(struct psc_thread *thr)
{
	struct hash_entry *e;

	free(thr->pscthr_loglevels);
	e = del_hash_entry(thrHTable, thr->pscthr_hashid);
	if (e)
		free(e);

	/*
	 * I don't think we can do this unless must disallow any
	 * external indexing into this array.  Things that need to
	 * reference a thread should maintain a pointer to the pscthr
	 * and not a pscThreads index.
	 */
	dynarray_remove(&pscThreads, thr);
}
