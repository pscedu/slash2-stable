/* $Id$ */
/*
 * %PSC_START_COPYRIGHT%
 * -----------------------------------------------------------------------------
 * Copyright (c) 2006-2010, Pittsburgh Supercomputing Center (PSC).
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

#include <err.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "pfl/cdefs.h"
#include "pfl/pfl.h"
#include "psc_util/log.h"
#include "psc_util/setprocesstitle.h"

#define _PATH_CMDLINE "/proc/self/cmdline"

const char *progname;

__dead void
usage(void)
{
	fprintf(stderr, "usage: %s\n", progname);
	exit(1);
}

int
main(__unusedx int argc, char *argv[])
{
	char buf[BUFSIZ];
	int pipe = 0;
	ssize_t rc;
	FILE *fp;

	pfl_init();
	progname = argv[0];
	if (getopt(argc, argv, "") != -1)
		usage();
	argc -= optind;
	if (argc)
		usage();

	setprocesstitle(argv, "foobar %d", 13);

	fp = fopen(_PATH_CMDLINE, "r");
	if (fp == NULL) {
		pipe = 1;
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
	if (pipe)
		pclose(fp);
	else
		fclose(fp);

	buf[rc] = '\0';
	buf[strcspn(buf, "\n")] = '\0';
	psc_assert(strcmp(buf, "foobar 13") == 0);
	exit(0);
}
