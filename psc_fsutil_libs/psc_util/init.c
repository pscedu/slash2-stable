/* $Id$ */

#include <unistd.h>

#include "psc_ds/dynarray.h"
#include "psc_ds/list.h"
#include "psc_ds/pool.h"
#include "psc_util/alloc.h"
#include "psc_util/lock.h"
#include "psc_util/log.h"
#include "psc_util/thread.h"

extern struct dynarray psc_subsystems;
extern long pscPageSize;

void
pfl_init(void)
{
	dynarray_init(&pscThreads);
	dynarray_init(&psc_subsystems);
	psc_subsys_register(PSS_LOG, "log");
	psc_subsys_register(PSS_JOURNAL, "journal");
	psc_subsys_register(PSS_RPC, "rpc");
	psc_subsys_register(PSS_LNET, "lnet");
	psc_subsys_register(PSS_OTHER, "other");

	pscPageSize = sysconf(_SC_PAGESIZE);
	if (pscPageSize == -1)
		psc_fatal("sysconf");
}
