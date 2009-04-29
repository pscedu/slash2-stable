/* $Id$ */

#include <err.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "pfl.h"
#include "psc_util/alloc.h"
#include "psc_util/atomic.h"
#include "psc_util/cdefs.h"
#include "psc_util/lock.h"
#include "psc_util/log.h"

psc_spinlock_t psc_umask_lock = LOCK_INITIALIZER;
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
psc_dumpstack(__unusedx int sig)
{
	static psc_spinlock_t lock = LOCK_INITIALIZER;
	char buf[BUFSIZ];

	spinlock(&lock);
	fflush(stderr);
	printf("\n\nAttempting to generating stack trace...\n");
	snprintf(buf, sizeof(buf), "pstack %d || gstack %d",
	    getpid(), getpid());
	system(buf);
	kill(getpid(), SIGQUIT);
	_exit(1);
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

	if (getenv("PSC_DUMPSTACK")) {
		if (signal(SIGSEGV, psc_dumpstack) == SIG_ERR)
			psc_fatal("signal");
		if (signal(SIGABRT, psc_dumpstack) == SIG_ERR)
			psc_fatal("signal");
	}

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
