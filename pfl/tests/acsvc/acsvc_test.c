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

#include <fcntl.h>
#include <libgen.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "pfl/acsvc.h"
#include "pfl/alloc.h"
#include "pfl/cdefs.h"
#include "pfl/log.h"
#include "pfl/pfl.h"
#include "pfl/str.h"

extern const char *__progname;

__dead void
usage(void)
{
	fprintf(stderr, "usage: %s\n", __progname);
	exit(1);
}

int
main(int argc, char *argv[])
{
	char *dir, fn[PATH_MAX];
	int fd;

	pfl_init();
	while (getopt(argc, argv, "") != -1)
		usage();
	argc -= optind;
	if (argc)
		usage();

	acsvc_init(0, "test", argv);

	dir = pfl_strdup(__progname);
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
