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

#include <sys/types.h>
#include <sys/stat.h>

#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <unistd.h>

#include "pfl/cdefs.h"
#include "pfl/fcntl.h"
#include "pfl/pfl.h"
#include "pfl/stat.h"
#include "pfl/fmtstr.h"
#include "pfl/log.h"

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
	char b[LINE_MAX];
	struct stat stb;
	int ch;

	while ((ch = getopt(argc, argv, "")) != -1)
		switch (ch) {
		default:
			usage();
		}
	argc -= optind;
	if (argc)
		usage();

	(void)FMTSTR(b, sizeof(b), "[<%a>:<%b>:<%c>]",
		FMTSTRCASE('a', "d", 1)
		FMTSTRCASE('b', "s", "foobar")
		FMTSTRCASE('c', "lu", 0UL)
	);

	printf("%s\n", b);

	pfl_dump_fflags(O_RDONLY | O_NONBLOCK);
	psc_assert(stat(".", &stb) == 0);
	pfl_dump_mode(stb.st_mode);
	pfl_dump_statbuf(&stb);

	exit(0);
}
