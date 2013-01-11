/* $Id$ */
/*
 * %PSC_START_COPYRIGHT%
 * -----------------------------------------------------------------------------
 * Copyright (c) 2006-2012, Pittsburgh Supercomputing Center (PSC).
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
#include <sys/stat.h>

#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <unistd.h>

#include "pfl/cdefs.h"
#include "pfl/fcntl.h"
#include "pfl/pfl.h"
#include "pfl/stat.h"
#include "psc_util/fmtstr.h"
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
	char b[LINE_MAX];
	struct stat stb;
	int ch;

	progname = argv[0];
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
