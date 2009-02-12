/* $Id$ */

#include <err.h>
#include <string.h>
#include <unistd.h>

#include "psc_util/alloc.h"
#include "psc_util/atomic.h"
#include "psc_util/cdefs.h"
#include "psc_util/log.h"

extern long pscPageSize;
extern char **environ;

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
	char **p;

	if (atomic_xchg(&init, 1))
		errx(1, "pfl_init: already initialized");

	pscthrs_init();
	psc_memnode_init();
	psc_log_init();

	psc_subsys_register(PSS_LOG, "log");
	psc_subsys_register(PSS_JOURNAL, "journl");
	psc_subsys_register(PSS_RPC, "rpc");
	psc_subsys_register(PSS_LNET, "lnet");
	psc_subsys_register(PSS_MEMALLOC, "mem");
	psc_subsys_register(PSS_OTHER, "other");

	pscPageSize = sysconf(_SC_PAGESIZE);
	if (pscPageSize == -1)
		psc_fatal("sysconf");

	for (p = environ; *p; p++)
		if (strncmp(*p, "TCPLND", strlen("TCPLND")) == 0 ||
		    strncmp(*p, "TCPNAL", strlen("TCPNAL")) == 0)
			psc_fatalx("old-style %s not used anymore", *p);
}
