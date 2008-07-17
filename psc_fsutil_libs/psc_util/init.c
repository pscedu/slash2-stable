/* $Id$ */

#include <unistd.h>

#include "psc_ds/dynarray.h"
#include "psc_ds/list.h"
#include "psc_util/alloc.h"
#include "psc_util/lock.h"
#include "psc_util/log.h"
#include "psc_util/pool.h"
#include "psc_util/thread.h"
#include "psc_util/threadtable.h"

extern struct dynarray psc_subsystems;

long pscPageSize;

struct psc_lockedlist psc_pools;

void
pfl_init(int thrtabsz)
{
	dynarray_init(&psc_subsystems);

	psc_subsys_register(PSS_LOG, "log");
	psc_subsys_register(PSS_JOURNAL, "journal");
	psc_subsys_register(PSS_RPC, "rpc");
	psc_subsys_register(PSS_LNET, "lnet");
	psc_subsys_register(PSS_OTHER, "other");

	pll_init(&psc_pools, struct psc_poolmgr, ppm_lentry);

	psc_threadtbl_init(thrtabsz);

	dynarray_init(&pscThreads);

	pscPageSize = sysconf(_SC_PAGESIZE);
	if (pscPageSize == -1)
		psc_fatal("sysconf");
}
