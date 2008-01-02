/* $Id$ */

#include "psc_util/subsys.h"

#include <err.h>
#include <stdio.h>
#include <unistd.h>

#include "psc_util/alloc.h"
#include "psc_util/threadtable.h"
#include "psc_util/lock.h"
#include "psc_util/thread.h"
#include "psc_types.h"

/*
 * Keep track of the threads here
 */
struct dynarray    pscThreads;

void *
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
	    void *(*start)(void *), const char *name)
{
	int *loglevels, error, n;

	/*
	 * Ensure that the thr is initialized before the new thread
	 *  attempts to access its data structures.
	 */
	LOCK_INIT(&thr->pscthr_lock);
	spinlock(&thr->pscthr_lock);

	snprintf(thr->pscthr_name, sizeof(thr->pscthr_name), "%s",
		 name);

	thr->pscthr_loglevels = PSCALLOC(psc_nsubsys *
	    sizeof(*thr->pscthr_loglevels));
	for (n = 0; n < psc_nsubsys; n++)
		loglevels[n] = psc_getloglevel();

	thr->pscthr_type  = type;
	thr->pscthr_id    = dynarray_len(&pscThreads); /* XXX lockme? */
	thr->pscthr_start = start;

	if (start)
		if ((error = pthread_create(&thr->pscthr_pthread, NULL,
					    pscthr_begin, thr)) != 0)
			psc_errorx("pthread_create: %s", strerror(error));

	thr->pscthr_hashid = (u64)thr->pscthr_pthread;

	psc_threadtbl_put(thr);

	if (dynarray_add(&pscThreads, thr) == -1)
		psc_fatal("dynarray_add");

	freelock(&thr->pscthr_lock);

	psc_info("spawned %s [thread %zu] [id %"ZLPX64"] [pthrid %lx] thr=%p"
		 " thr->type %d, passed type %d",
		 thr->pscthr_name, thr->pscthr_id,
		 thr->pscthr_hashid, thr->pscthr_pthread,
		 thr, thr->pscthr_type, type);
}

/*
 * pscthr_getloglevel - get thread logging level for the specified subsystem.
 * This routine is not intended for general-purpose usage.
 * @subsys: subsystem ID.
 */
int
pscthr_getloglevel(int subsys)
{
	struct psc_thread *thr;

	thr = psc_threadtbl_get_canfail();
	if (thr == NULL)
		return (PLL_TRACE);
	if (subsys >= psc_nsubsys)
		psc_fatalx("request subsystem out of bounds (%d)",
		    subsys);
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
