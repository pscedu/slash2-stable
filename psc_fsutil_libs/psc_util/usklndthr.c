/* $Id$ */

#ifdef HAVE_LIBPTHREAD

#include "psc_util/alloc.h"
#include "psc_util/thread.h"
#include "psc_util/usklndthr.h"

void *
psc_usklndthr_begin(void *arg)
{
	struct psc_thread *thr = arg;
	struct psc_usklndthr *put;

	put = thr->pscthr_private;
	put->put_startf(put->put_arg);
	return (NULL);
}

void
psc_usklndthr_destroy(void *arg)
{
	struct psc_usklndthr *put = arg;

	free(put);
}

int
cfs_create_thread(cfs_thread_t startf, void *arg, const char *namefmt, ...)
{
	char name[PSC_THRNAME_MAX];
	struct psc_usklndthr *put;
	struct psc_thread *pt;
	va_list ap;

	pt = PSCALLOC(sizeof(*pt));
	put = PSCALLOC(sizeof(*put));
	put->put_startf = startf;
	put->put_arg = arg;

	va_start(ap, namefmt);
	psc_usklndthr_get_namev(name, namefmt, ap);
	va_end(ap);

	_pscthr_init(pt, psc_usklndthr_get_type(namefmt),
	    psc_usklndthr_begin, put, sizeof(*put), PTF_FREE,
	    psc_usklndthr_destroy, name);
	return (0);
}

#endif
