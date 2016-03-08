/* $Id$ */
/*
 * %ISC_START_LICENSE%
 * ---------------------------------------------------------------------
 * Copyright 2015-2016, Google, Inc.
 * Copyright (c) 2007-2015, Pittsburgh Supercomputing Center (PSC).
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

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "pfl/alloc.h"
#include "pfl/str.h"

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
	int rc = 0;

	path = pfl_strdup(s);

	/* XXX the path should be canonicalized. */

	/* skip past each existing subdir */
	for (p = path; p; ) {
		p = strchr(p, '/');
		if (p)
			*p = '\0';
		if (path[0] && mkdir(path, mode) == -1 &&
		    (rc = errno) != EEXIST)
			break;
		if (p)
			*p++ = '/';
	}

	PSCFREE(path);
	return (rc);
}
