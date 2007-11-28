/* $Id: thread.c 2073 2007-11-01 17:31:07Z pauln $ */

#include "subsys.h"

#include <err.h>
#include <stdio.h>
#include <unistd.h>

#include "psc_util/threadtable.h"
#include "zestion.h"

struct dynarray pscThreads;

/* Must stay sync'd with ZTHRT_*. */
const char *zestionThreadTypeNames[] = {
	"zctlthr",
	"ziothr%d",
	"zsyncqthr",
	"zsyncwthr",
	"zprtythr%d",
	"zpgenthr%d",
	"zrpciothr%d",
	"zrpcmdsthr%d",
	"ztcplndthr%d",
	"ztintvthr",
	"ztiosthr"
};

void *
pscthr_begin(void *arg)
{
	struct psc_thread *thr = arg;
	//usleep(1000000);
	/* Wait until pscthr_init() has finished initializing us. */
	spinlock(&thr->pscthr_lock);
	freelock(&thr->pscthr_lock);
	return (thr->pscthr_start(thr));
}

/*
 * pscthr_init - initialize a zestion thread.
 * @thr: zestion thread structure to be initialized, must already be
 *	allocated.
 * @type: zestion thread type.
 * @start: thread execution routine.  By specifying a NULL routine,
 *	no pthread will be spawned (assuming that an actual pthread
 *	already exists or will be taken care of).
 * @namearg: number of `type' threads thus far.
 */
void
pscthr_init(struct psc_thread *thr, int type, void *(*start)(void *),
    int namearg)
{
	int error, n;

	if (type < 0 || type >= NZTHRT)
		psc_fatalx("invalid thread type %d", type);
	/*
	 * Ensure that the thr is initialized before the new thread
	 *  attempts to access its data structures.
	 */
	LOCK_INIT(&thr->pscthr_lock);
	spinlock(&thr->pscthr_lock);

	snprintf(thr->pscthr_name, sizeof(thr->pscthr_name),
		 zestionThreadTypeNames[type], namearg);

	for (n = 0; n < ZNSUBSYS; n++)
		thr->pscthr_log_levels[n] = psc_getloglevel();

	thr->pscthr_type = type;
	thr->pscthr_id = dynarray_len(&pscThreads); /* XXX lockme? */
	thr->pscthr_start  = start;

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
		return (LL_TRACE);
	return (thr->pscthr_log_levels[subsys]);
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
