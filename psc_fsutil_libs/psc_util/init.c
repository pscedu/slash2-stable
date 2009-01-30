/* $Id$ */

#include <err.h>
#include <unistd.h>

#include "psc_util/alloc.h"
#include "psc_util/atomic.h"
#include "psc_util/cdefs.h"
#include "psc_util/log.h"

extern long pscPageSize;

__weak void
pscthrs_init(void)
{
}

__weak void
psc_memnode_init(void)
{
}

__weak void
psc_subsys_register(__unusedx int level, __unusedx const char *name)
{
}

void
pfl_init(void)
{
	static atomic_t init = ATOMIC_INIT(0);

	if (atomic_xchg(&init, 1))
		errx(1, "pfl_init: already initialized");

	pscthrs_init();
	psc_memnode_init();
	psc_log_init();

	psc_subsys_register(PSS_LOG, "log");
	psc_subsys_register(PSS_JOURNAL, "journal");
	psc_subsys_register(PSS_RPC, "rpc");
	psc_subsys_register(PSS_LNET, "lnet");
	psc_subsys_register(PSS_MEMALLOC, "memalloc");
	psc_subsys_register(PSS_OTHER, "other");

	pscPageSize = sysconf(_SC_PAGESIZE);
	if (pscPageSize == -1)
		psc_fatal("sysconf");
}
