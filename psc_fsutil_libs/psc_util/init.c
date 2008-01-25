/* $Id$ */

#include <unistd.h>

#include "psc_ds/dynarray.h"
#include "psc_util/threadtable.h"
#include "psc_util/subsys.h"
#include "psc_util/thread.h"
#include "psc_util/alloc.h"

extern struct dynarray psc_subsystems;

struct psclist_head pscListCaches = PSCLIST_HEAD_INIT(pscListCaches);
psc_spinlock_t pscListCachesLock = LOCK_INITIALIZER;
long pscPageSize;

void
pfl_init(int thrtabsz)
{
	dynarray_init(&psc_subsystems);

	psc_subsys_register(PSS_LOG, "log");
	psc_subsys_register(PSS_JOURNAL, "journal");
	psc_subsys_register(PSS_RPC, "rpc");
	psc_subsys_register(PSS_LNET, "lnet");
	psc_subsys_register(PSS_OTHER, "other");

	psc_threadtbl_init(thrtabsz);

	dynarray_init(&pscThreads);

	pscPageSize = sysconf(_SC_PAGESIZE);
	if (pscPageSize == -1)
		psc_fatal("sysconf");
}
