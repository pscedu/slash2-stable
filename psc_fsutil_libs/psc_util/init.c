/* $Id$ */
/*
 * %PSC_START_COPYRIGHT%
 * -----------------------------------------------------------------------------
 * Copyright (c) 2007-2010, Pittsburgh Supercomputing Center (PSC).
 *
 * Permission to use, copy, and modify this software and its documentation
 * without fee for personal use or non-commercial use within your organization
 * is hereby granted, provided that the above copyright notice is preserved in
 * all copies and that the copyright and this permission notice appear in
 * supporting documentation.  Permission to redistribute this software to other
 * organizations or individuals is not permitted without the written permission
 * of the Pittsburgh Supercomputing Center.  PSC makes no representations about
 * the suitability of this software for any purpose.  It is provided "as is"
 * without express or implied warranty.
 * -----------------------------------------------------------------------------
 * %PSC_END_COPYRIGHT%
 */

#include <err.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "pfl/cdefs.h"
#include "pfl/pfl.h"
#include "psc_util/alloc.h"
#include "psc_util/atomic.h"
#include "psc_util/lock.h"
#include "psc_util/log.h"

psc_spinlock_t			 psc_umask_lock = SPINLOCK_INIT;
__threadx struct pfl_callerinfo	*pfl_callerinfo;
__static void			*_pfl_tls[PFL_TLSIDX_MAX];

__weak void
pscthrs_init(void)
{
}

__weak void
psc_memnode_init(void)
{
}

__weak void
psc_faults_init(void)
{
}

__weak int
pfl_tls_get(int idx, size_t len, void *p)
{
	int rc = 0;

	if (_pfl_tls[idx] == NULL) {
		rc = 1;
		_pfl_tls[idx] = psc_alloc(len, PAF_NOLOG | PAF_NOGUARD);
	}
	*(void **)p = _pfl_tls[idx];
	return (rc);
}

__weak void
psc_subsys_register(__unusedx int level, __unusedx const char *name)
{
}

void
psc_dumpstack(int sig)
{
	static psc_spinlock_t lock = SPINLOCK_INIT_NOLOG;
	char buf[BUFSIZ];

//	if (!trylock(&lock)) {
//		write(STDERR_FILENO, );
//		_exit(1);
//	}

	spinlock(&lock);
	fflush(stderr);
	printf("\n\nSignal %d received, attempting to "
	    "generate stack trace...\n", sig);
	snprintf(buf, sizeof(buf), "pstack %d 2>/dev/null || gstack %d",
	    getpid(), getpid());
	if (system(buf) == -1)
		warn("%s", buf);
	kill(0, SIGQUIT);
	_exit(1);
}

void
psc_dumpstack0(void)
{
	psc_dumpstack(0);
}

__weak void
psc_enter_debugger(const char *str)
{
	psclog(PLL_MAX, "enter debugger (%s)", str);
	kill(0, SIGINT);
}

void
pfl_init(void)
{
	static atomic_t init = ATOMIC_INIT(0);
	char *p;

	if (atomic_xchg(&init, 1))
		errx(1, "pfl_init: already initialized");

	psc_log_init();
	pscthrs_init();
	psc_memnode_init();

	psc_pagesize = sysconf(_SC_PAGESIZE);
	if (psc_pagesize == -1)
		psc_fatal("sysconf");

	psc_memallocs_init();

	psc_subsys_register(PSS_DEF, "def");
	psc_subsys_register(PSS_TMP, "tmp");
	psc_subsys_register(PSS_MEM, "mem");
	psc_subsys_register(PSS_LNET, "lnet");
	psc_subsys_register(PSS_RPC, "rpc");

	p = getenv("PSC_DUMPSTACK");
	if (p && strcmp(p, "0")) {
		if (signal(SIGSEGV, psc_dumpstack) == SIG_ERR)
			psc_fatal("signal");
		if (signal(SIGABRT, psc_dumpstack) == SIG_ERR)
			psc_fatal("signal");
	}
	p = getenv("PSC_FORCE_DUMPSTACK");
	if (p && strcmp(p, "0"))
		atexit(psc_dumpstack0);

	psc_faults_init();
}
