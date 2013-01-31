/* $Id$ */
/*
 * %PSC_START_COPYRIGHT%
 * -----------------------------------------------------------------------------
 * Copyright (c) 2007-2013, Pittsburgh Supercomputing Center (PSC).
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

#include <sys/types.h>
#include <sys/syscall.h>

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
#include "psc_util/thread.h"

psc_spinlock_t				 psc_umask_lock = SPINLOCK_INIT;
__threadx const struct pfl_callerinfo	*_pfl_callerinfo;
__threadx int				 _pfl_callerinfo_lvl;
__static void				*_pfl_tls[PFL_TLSIDX_MAX];

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

__weak void *
pfl_tls_get(int idx, size_t len)
{
	if (_pfl_tls[idx] == NULL)
		_pfl_tls[idx] = psc_alloc(len, PAF_NOLOG | PAF_NOGUARD);
	return (_pfl_tls[idx]);
}

__weak void
psc_subsys_register(__unusedx int level, __unusedx const char *name)
{
}

__inline pid_t
pfl_getsysthrid(void)
{
#if defined(SYS_thread_selfid)
	return (syscall(SYS_thread_selfid));
#elif defined(SYS_gettid)
	return (syscall(SYS_gettid));
#elif defined(SYS_getthrid)
	return (syscall(SYS_getthrid));
#elif defined(SYS_thr_self)
	return (syscall(SYS_thr_self));
#else
	return (pthread_self());
#endif
}

void
pfl_dump_stack(void)
{
	char buf[BUFSIZ];

	snprintf(buf, sizeof(buf),
	    "{ pstack %d 2>/dev/null || gstack %d 2>/dev/null; "
	    "  pstack %d 2>/dev/null || gstack %d 2>/dev/null; } | "
	    "{ tools/filter-pstack - 2>/dev/null; cat -; }",
	    pfl_getsysthrid(), pfl_getsysthrid(), getpid(), getpid());
	if (system(buf) == -1)
		warn("%s", buf);
}

void
pfl_dump_stack1(int sig)
{
	static psc_spinlock_t lock = SPINLOCK_INIT_NOLOG;

//	if (!trylock(&lock)) {
//		write(STDERR_FILENO, );
//		_exit(1);
//	}

	spinlock(&lock);
	fflush(stderr);
	printf("\n\n");
	if (sig)
		printf("signal %d received, ", sig);
	printf("attempting to generate stack trace...\n");
	pfl_dump_stack();
	abort();
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
	static psc_atomic32_t init = PSC_ATOMIC32_INIT(0);
	struct sigaction sa;
	char *p;

	if (psc_atomic32_xchg(&init, 1))
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
	if (p && strcmp(p, "0") &&
	    signal(SIGSEGV, pfl_dump_stack1) == SIG_ERR)
		psc_fatal("signal");
	p = getenv("PSC_FORCE_DUMPSTACK");
	if (p && strcmp(p, "0"))
		atexit(pfl_dump_stack);

	psc_faults_init();

	p = getenv("PSC_TIMEOUT");
	if (p) {
		struct itimerval it;
		long l;

		memset(&it, 0, sizeof(it));
		l = strtol(p, NULL, 10);
		it.it_value.tv_sec += l;
		if (setitimer(ITIMER_REAL, &it, NULL) == -1)
			psclog_error("setitimer");
	}

	memset(&sa, 0, sizeof(sa));
	sa.sa_handler = SIG_IGN;
	if (sigaction(SIGPIPE, &sa, NULL) == -1)
		psc_fatal("sigaction");
}
