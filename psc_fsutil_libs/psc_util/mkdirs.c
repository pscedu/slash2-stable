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
#include <sys/stat.h>

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "pfl/str.h"
#include "psc_util/alloc.h"

/**
 * mkdirs - Simple recursive "mkdir -p" type functionality.
 * @dir: path to be created
 * @mode: dir creation mode
 */
int
mkdirs(const char *s, mode_t mode)
{
	char *p, *path;
	int rc;

	rc = -1;

	/* XXX realpath() must be used in here */

	/* Sanity check(s) */
	if (strlen(s) == 0) {
		errno = ENOENT;
		return (-1);
	}

	if (s[0] != '/') {
		errno = EINVAL;
		return (-1);
	}

	if (strcmp(s, "/") == 0)
		return (0);

	path = pfl_strdup(s);
	for (p = path; p; ) {
		*p++ = '/';
		if ((p = strchr(p, '/')) != NULL)
			*p = '\0';
		if (mkdir(path, mode) == -1 &&
		    errno != EEXIST)
			goto done;
	}
	rc = 0;
 done:
	PSCFREE(path);
	return (rc);
}
