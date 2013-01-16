/* $Id$ */
/*
 * %PSC_START_COPYRIGHT%
 * -----------------------------------------------------------------------------
 * Copyright (c) 2006-2013, Pittsburgh Supercomputing Center (PSC).
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

#include <sys/param.h>

#include <inttypes.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>

#include "pfl/str.h"
#include "psc_util/log.h"
#include "psc_util/prsig.h"

#ifdef HAVE_SYS_SIGABBREV
# define pfl_sys_signame sys_sigabbrev

extern const char * const sys_sigabbrev[];
#else
# define pfl_sys_signame sys_signame
#endif

void
psc_sigappend(char buf[LINE_MAX], const char *str)
{
	if (buf[0] != '\0')
		strlcat(buf, ",", LINE_MAX);
	strlcat(buf, str, LINE_MAX);
}

#define PNSIG 32

void
psc_prsig(void)
{
	struct sigaction sa;
	char buf[LINE_MAX];
	uint64_t mask;
	int i, j;

	i = printf("%3s %-6s %-16s %-7s", "sig", "name", "block mask", "action");
	putchar('\n');
	while (i--)
		putchar('=');
	putchar('\n');
	for (i = 1; i < PNSIG; i++) {
		if (sigaction(i, NULL, &sa) == -1)
			continue;

		buf[0] = '\0';
		if (sa.sa_handler == SIG_DFL)
			psc_sigappend(buf, "default");
		else if (sa.sa_handler == SIG_IGN)
			psc_sigappend(buf, "ignored");
		else if (sa.sa_handler == NULL)
			psc_sigappend(buf, "zero?");
		else
			psc_sigappend(buf, "caught");

		mask = 0;
		for (j = 1; j < PNSIG; j++)
			if (sigismember(&sa.sa_mask, j))
				mask |= 1 << (j - 1);
		printf("%3d %-6s %016"PRIx64" %s\n",
		    i, pfl_sys_signame[i], mask, buf);
	}
}
