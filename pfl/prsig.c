/* $Id$ */
/*
 * %ISC_START_LICENSE%
 * ---------------------------------------------------------------------
 * Copyright 2015-2016, Google, Inc.
 * Copyright (c) 2006-2015, Pittsburgh Supercomputing Center (PSC).
 * All rights reserved.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the
 * above copyright notice and this permission notice appear in all
 * copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL
 * WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS.  IN NO EVENT SHALL THE
 * AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
 * PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 * --------------------------------------------------------------------
 * %END_LICENSE%
 */

#include <sys/param.h>

#include <inttypes.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>

#include "pfl/str.h"
#include "pfl/log.h"
#include "pfl/prsig.h"

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
