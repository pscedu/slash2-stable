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

#include <err.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "pfl/cdefs.h"
#include "pfl/pfl.h"
#include "pfl/log.h"

#define _PATH_CMDLINE "/proc/self/cmdline"

__dead void
usage(void)
{
	extern const char *__progname;

	fprintf(stderr, "usage: %s\n", __progname);
	exit(1);
}

int
main(__unusedx int argc, char *argv[])
{
	char buf[BUFSIZ];
	int dopipe = 0;
	ssize_t rc;
	FILE *fp;

	pfl_init();
	if (getopt(argc, argv, "") != -1)
		usage();
	argc -= optind;
	if (argc)
		usage();

	pfl_setprocesstitle(argv, "foobar %d", 13);

	fp = fopen(_PATH_CMDLINE, "r");
	if (fp == NULL) {
		dopipe = 1;
		snprintf(buf, sizeof(buf),
		    "ps -o command -p %d | tail -1", getpid());
		fp = popen(buf, "r");
		if (fp == NULL)
			err(1, "popen %s", buf);
	}
	rc = fread(buf, 1, sizeof(buf), fp);
	if (!feof(fp))
		err(1, _PATH_CMDLINE);
	if (ferror(fp))
		err(1, _PATH_CMDLINE);
	if (dopipe)
		pclose(fp);
	else
		fclose(fp);

	buf[rc] = '\0';
	buf[strcspn(buf, "\n")] = '\0';
	psc_assert(strstr(buf, "foobar 13"));
	exit(0);
}
