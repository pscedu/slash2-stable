/* $Id$ */
/*
 * %PSC_START_COPYRIGHT%
 * -----------------------------------------------------------------------------
 * Copyright (c) 2006-2009, Pittsburgh Supercomputing Center (PSC).
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
#include "psc_util/log.h"
#include "psc_util/setprocesstitle.h"

#define _PATH_CMDLINE "/proc/self/cmdline"

int
main(__unusedx int argc, char *argv[])
{
	char buf[BUFSIZ];
	int fd;

	setprocesstitle(argv, "foobar %d", 13);

	fd = open(_PATH_CMDLINE, O_RDONLY);
	if (fd == -1)
		err(1, _PATH_CMDLINE);
	read(fd, buf, sizeof(buf));
	close(fd);

	psc_assert(strcmp(buf, "foobar 13") == 0);
	exit(0);
}
