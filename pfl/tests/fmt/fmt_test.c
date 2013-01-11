/* $Id$ */
/*
 * %PSC_START_COPYRIGHT%
 * -----------------------------------------------------------------------------
 * Copyright (c) 2010-2011, Pittsburgh Supercomputing Center (PSC).
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "pfl/cdefs.h"
#include "pfl/pfl.h"
#include "pfl/str.h"
#include "psc_util/fmt.h"
#include "psc_util/log.h"

const char *progname;

__dead void
usage(void)
{
	fprintf(stderr, "usage: %s\n", progname);
	exit(1);
}

int
main(int argc, char *argv[])
{
	char buf[PSCFMT_RATIO_BUFSIZ], hbuf[PSCFMT_HUMAN_BUFSIZ], fn[PATH_MAX];
	int c;

	while ((c = getopt(argc, argv, "")) != -1)
		switch (c) {
		default:
			usage();
		}
	argc -= optind;
	if (argc)
		usage();

	psc_fmt_ratio(buf,  9998, 10001); psc_assert(strcmp(buf, "99.97%") == 0);
	psc_fmt_ratio(buf,  9999, 10001); psc_assert(strcmp(buf, "99.98%") == 0);
	psc_fmt_ratio(buf, 10000, 10001); psc_assert(strcmp(buf, "99.99%") == 0);
	psc_fmt_ratio(buf, 10001, 10001); psc_assert(strcmp(buf, "100%") == 0);

	psc_fmt_human(hbuf, 12); psc_assert(strcmp(hbuf, "    12B") == 0);

	pfl_dirname("", fn); psc_assert(strcmp(fn, ".") == 0);
	pfl_dirname(NULL, fn); psc_assert(strcmp(fn, ".") == 0);
	pfl_dirname("sdfdf", fn); psc_assert(strcmp(fn, ".") == 0);
	pfl_dirname("/", fn); psc_assert(strcmp(fn, "/") == 0);
	pfl_dirname("/foo", fn); psc_assert(strcmp(fn, "/") == 0);
	pfl_dirname("//", fn); psc_assert(strcmp(fn, "/") == 0);
	pfl_dirname("////", fn); psc_assert(strcmp(fn, "/") == 0);
	pfl_dirname("/foo/bar", fn); psc_assert(strcmp(fn, "/foo") == 0);
	pfl_dirname("/foo/bar/glar", fn); psc_assert(strcmp(fn, "/foo/bar") == 0);
	exit(0);
}
