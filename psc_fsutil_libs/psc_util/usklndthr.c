/* $Id$ */

#ifdef HAVE_LIBPTHREAD

#include "psc_util/alloc.h"
#include "psc_util/thread.h"
#include "psc_util/usklndthr.h"

void *
psc_usklndthr_begin(void *arg)
{
	struct psc_thread *thr;
	struct psc_usklndthr *put;

	thr = arg;
	put = thr->pscthr_private;
	return (put->put_startf(put->put_arg));
}

void
psc_usklndthr_destroy(void *arg)
{
	struct psc_usklndthr *put = arg;

	free(put);
}

pthread_t
cfs_create_thread2(void *(*startf)(void *), void *arg)
{
	int usocklnd_ninstances(void);
	struct psc_usklndthr *put;
	struct psc_thread *pt;

	pt = PSCALLOC(sizeof(*pt));
	put = PSCALLOC(sizeof(*put));
	put->put_startf = startf;
	put->put_arg = arg;
	_pscthr_init(pt, psc_usklndthr_get_type(put), psc_usklndthr_begin, put,
	    PTF_FREE, psc_usklndthr_destroy, psc_usklndthr_get_name(put),
	    usocklnd_ninstances() - 1);
	pt->pscthr_private = arg;
	return (pt->pscthr_pthread);
}

int
cfs_create_thread(void *(*startf)(void *), void *arg)
{
	cfs_create_thread2(startf, arg);
	return (0);
}

#endif
