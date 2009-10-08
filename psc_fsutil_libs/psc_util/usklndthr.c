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

int
cfs_create_thread(cfs_thread_t startf, void *arg,
    const char *namefmt, ...)
{
	char name[PSC_THRNAME_MAX];
	struct psc_usklndthr *put;
	struct psc_thread *thr;
	va_list ap;

	va_start(ap, namefmt);
	psc_usklndthr_get_namev(name, namefmt, ap);
	va_end(ap);

	thr = pscthr_init(psc_usklndthr_get_type(namefmt), PTF_FREE,
	    psc_usklndthr_begin, NULL, sizeof(*put),
	    name);
	put = thr->pscthr_private;
	put->put_startf = startf;
	put->put_arg = arg;
	pscthr_setready(thr);
	return (0);
}

#endif
