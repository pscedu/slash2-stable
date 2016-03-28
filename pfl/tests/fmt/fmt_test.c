/* $Id$ */
/*
 * %ISC_START_LICENSE%
 * ---------------------------------------------------------------------
 * Copyright 2015-2016, Google, Inc.
 * Copyright (c) 2010-2015, Pittsburgh Supercomputing Center (PSC).
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

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "pfl/cdefs.h"
#include "pfl/fmt.h"
#include "pfl/log.h"
#include "pfl/pfl.h"
#include "pfl/str.h"

__dead void
usage(void)
{
	extern const char *__progname;

	fprintf(stderr, "usage: %s\n", __progname);
	exit(1);
}

int
main(int argc, char *argv[])
{
	char buf[PSCFMT_RATIO_BUFSIZ], hbuf[PSCFMT_HUMAN_BUFSIZ];
	char *cp, fn[PATH_MAX];
	int c;

	while ((c = getopt(argc, argv, "")) != -1)
		switch (c) {
		default:
			usage();
		}
	argc -= optind;
	if (argc)
		usage();

	pfl_fmt_ratio(buf,  9998, 10001); psc_assert(strcmp(buf, "99.97%") == 0);
	pfl_fmt_ratio(buf,  9999, 10001); psc_assert(strcmp(buf, "99.98%") == 0);
	pfl_fmt_ratio(buf, 10000, 10001); psc_assert(strcmp(buf, "99.99%") == 0);
	pfl_fmt_ratio(buf, 10001, 10001); psc_assert(strcmp(buf, "100%") == 0);

	pfl_fmt_human(hbuf, 12); psc_assert(strcmp(hbuf, "    12B") == 0);

	pfl_dirname("", fn); psc_assert(strcmp(fn, ".") == 0);
	pfl_dirname(NULL, fn); psc_assert(strcmp(fn, ".") == 0);
	pfl_dirname("sdfdf", fn); psc_assert(strcmp(fn, ".") == 0);
	pfl_dirname("/", fn); psc_assert(strcmp(fn, "/") == 0);
	pfl_dirname("/foo", fn); psc_assert(strcmp(fn, "/") == 0);
	pfl_dirname("//", fn); psc_assert(strcmp(fn, "/") == 0);
	pfl_dirname("////", fn); psc_assert(strcmp(fn, "/") == 0);
	pfl_dirname("/foo/bar", fn); psc_assert(strcmp(fn, "/foo") == 0);
	pfl_dirname("/foo/bar/glar", fn); psc_assert(strcmp(fn, "/foo/bar") == 0);

	psc_assert(pfl_humantonum("24g") == INT64_C(24) * 1024 * 1024 * 1024);

	printf("%"PRIx64"\n", psc_str_hashify("foobar"));
	printf("%"PRIx64"\n", psc_str_hashify("/"));
	printf("%"PRIx64"\n", psc_str_hashify("//"));
	printf("%"PRIx64"\n", psc_str_hashify("/foo/bar"));
	printf("%"PRIx64"\n", psc_str_hashify("fmt/fmt_test.c"));
	printf("%"PRIx64"\n", psc_str_hashify("Yesterday, all of my troubles seemed so far away."));

	cp = "msl_rmc_bmlreassign_cb";
	printf("0: %s\n", pfl_strrastr(cp, '_', 0));
	printf("1: %s\n", pfl_strrastr(cp, '_', 1));
	printf("2: %s\n", pfl_strrastr(cp, '_', 2));
	printf("3: %s\n", pfl_strrastr(cp, '_', 3));
	printf("4: %s\n", pfl_strrastr(cp, '_', 4));
	cp = pfl_strrastr(cp, '_', 3);
	printf("s: %s\n", cp + strspn(cp, "_"));

	exit(0);
}
