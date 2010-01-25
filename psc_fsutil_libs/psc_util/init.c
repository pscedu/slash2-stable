/* $Id$ */
/*
 * %PSC_START_COPYRIGHT%
 * -----------------------------------------------------------------------------
 * Copyright (c) 2006-2009, Pittsburgh Supercomputing Center (PSC).
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

#include "pfl/pfl.h"
#include "psc_util/alloc.h"
#include "psc_util/atomic.h"
#include "pfl/cdefs.h"
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
psc_fault_init(void)
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
	kill(0, SIGQUIT);
	_exit(1);
}

#include <sys/time.h>
void
psc_enter_debugger(const char *str)
{
	struct timeval	tv;

	gettimeofday(&tv, NULL);
	printf("timestamp %lu:%lu, enter debugger (%s) ...\n", tv.tv_sec, tv.tv_usec, str);
	psc_notify("timestamp %lu:%lu, enter debugger (%s) ...", tv.tv_sec, tv.tv_usec, str);
	/*
	 * Another way to drop into debugger is to use
	 *
	 *	kill(0, SIGINT);
	 *
	 * However, it does not seem to work well with backtrace (bt) command.
	 */
	__asm__ __volatile__ ("int3");
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

	if (getenv("PSC_DUMPSTACK")) {
		if (signal(SIGSEGV, psc_dumpstack) == SIG_ERR)
			psc_fatal("signal");
		if (signal(SIGABRT, psc_dumpstack) == SIG_ERR)
			psc_fatal("signal");
	}

	psc_subsys_register(PSS_JOURNAL, "jrnl");
	psc_subsys_register(PSS_RPC, "rpc");
	psc_subsys_register(PSS_LNET, "lnet");
	psc_subsys_register(PSS_MEM, "mem");
	psc_subsys_register(PSS_GEN, "gen");
	psc_subsys_register(PSS_TMP, "tmp");

	psc_fault_init();

	pscPageSize = sysconf(_SC_PAGESIZE);
	if (pscPageSize == -1)
		psc_fatal("sysconf");
}
