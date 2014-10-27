/* $Id$ */
/*
 * %PSC_START_COPYRIGHT%
 * -----------------------------------------------------------------------------
 * Copyright (c) 2006-2014, Pittsburgh Supercomputing Center (PSC).
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
#include "pfl/alloc.h"

/**
 * mkdirs - Simple recursive "mkdir -p" type functionality.
 * @dir: path to be created
 * @mode: dir creation mode
 *
 *	mkdirs("foo")		-> mkdir("foo")
 *	mkdirs("foo/bar")	-> mkdir("foo"); mkdir("foo/bar")
 *	mkdirs("/foo")		-> mkdir("foo")
 *	mkdirs("/")		->
 *	mkdirs("/foo/bar")	-> mkdir("/foo"); mkdir("/foo/bar")
 *	mkdirs("../foo")	-> mkdir("../foo")
 */
int
mkdirs(const char *s, mode_t mode)
{
	char *p, *path;
	struct stat stb;
	int rc = -1;

	path = pfl_strdup(s);

	/* skip past each existing subdir */
	for (p = path; p; ) {
		p = strchr(p, '/');
		if (p)
			*p = '\0';
		if (path[0] && stat(path, &stb) == -1) {
			if (errno == ENOENT)
				goto create;
			goto out;
		}
		if (p)
			*p++ = '/';
	}
	errno = EEXIST;
	goto out;

 create:
	/* now create each component */
	for (;;) {
		if (mkdir(path, mode) == -1)
			goto out;
		if (p == NULL)
			break;
		*p++ = '/';
		p = strchr(p, '/');
		if (p)
			*p = '\0';
	}
	rc = 0;

 out:
	PSCFREE(path);
	return (rc);
}
