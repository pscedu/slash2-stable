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

#include <fcntl.h>
#include <libgen.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "pfl/cdefs.h"
#include "pfl/pfl.h"
#include "pfl/str.h"
#include "psc_util/acsvc.h"
#include "psc_util/alloc.h"
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
	char *dir, fn[PATH_MAX];
	int fd;

	pfl_init();
	progname = argv[0];
	while (getopt(argc, argv, "") != -1)
		usage();
	argc -= optind;
	if (argc)
		usage();

	acsvc_init(0, "test", argv);

	dir = pfl_strdup(progname);
	if (dirname(dir) == NULL)
		psc_fatal("dirname");
	snprintf(fn, sizeof(fn), "%s%s%s",
	    __FILE__[0] == '/' ? "" : dir,
	    __FILE__[0] == '/' ? "" : "/", __FILE__);
	PSCFREE(dir);

	fd = access_fsop(ACSOP_OPEN, geteuid(), getegid(), fn,
	    O_RDONLY);
	if (fd == -1)
		psc_fatal("access_fsop: %s", fn);
	close(fd);
	exit(0);
}
