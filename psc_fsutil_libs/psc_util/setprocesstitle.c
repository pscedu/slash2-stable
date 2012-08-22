/* $Id$ */
/*
 * %PSC_START_COPYRIGHT%
 * -----------------------------------------------------------------------------
 * Copyright (c) 2006-2011, Pittsburgh Supercomputing Center (PSC).
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

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "pfl/pfl.h"
#include "pfl/str.h"
#include "psc_util/log.h"

void
pfl_setprocesstitle(char **av, const char *fmt, ...)
{
#ifdef HAVE_SETPROCTITLE
	va_list ap;
	char *p;
	int rc;

	va_start(ap, fmt);
	rc = vasprintf(&p, fmt, ap);
	va_end(ap);

	if (rc == -1)
		psc_fatal("vasprintf");

	setproctitle("%s", p);
	free(p);
	(void)av;
#else
	char buf[2048];
	size_t len, newlen;
	va_list ap;
	int j;

	/* XXX handle fmt==NULL which trims args */

	va_start(ap, fmt);
	vsnprintf(buf, sizeof(buf), fmt, ap);
	va_end(ap);

	newlen = strlen(buf);

	if (av[1])
		len = av[1] - av[0];
	else
		len = strlen(av[0]);
	strlcpy(av[0], buf, len + 1);
	if (newlen < len)
		memset(av[0] + newlen, '\0', len - newlen);
	for (j = 1; av[j]; j++)
		memset(av[j], '\0', strlen(av[j]));
#endif
}
