/* $Id$ */
/*
 * %PSC_START_COPYRIGHT%
 * -----------------------------------------------------------------------------
 * Copyright (c) 2010-2013, Pittsburgh Supercomputing Center (PSC).
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

#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "pfl/str.h"
#include "psc_util/alloc.h"

int
pfl_asprintf(char **p, const char *fmt, ...)
{
	va_list ap, apd;
	int sz;

	va_start(ap, fmt);
	va_copy(apd, ap);
	sz = vsnprintf(NULL, 0, fmt, ap);
	va_end(ap);

	*p = PSCALLOC(sz);

	vsnprintf(*p, sz, fmt, apd);
	va_end(apd);

	return (sz);
}

int
xsnprintf(char *s, size_t len, const char *fmt, ...)
{
	va_list ap;
	int rc;

	va_start(ap, fmt);
	rc = vsnprintf(s, len, fmt, ap);
	va_end(ap);

//	if (rc >= (int)len) {
//		rc = -1
//		errno = ENAMETOOLONG;
//	}
//	if (rc == -1)
//		psc_fatal();

	return (rc);
}

int
pfl_dirname(const char *s, char *buf)
{
	size_t i, cpnlen = 0;
	ssize_t sep = -1;

	if (s == NULL || strchr(s, '/') == NULL) {
		strlcpy(buf, ".", PATH_MAX);
		return (0);
	}
	for (i = 0; s[i]; i++, cpnlen++) {
		if (s[i] == '/') {
			sep = i;
			cpnlen = 0;
		}
		if (i >= PATH_MAX)
			return (ENAMETOOLONG);
		if (cpnlen > NAME_MAX)
			return (ENAMETOOLONG);
		buf[i] = s[i];
	}
	if (sep >= 0) {
		while (sep > 1 && buf[sep - 1] == '/')
			sep--;
		if (sep == 0)
			sep++;
		buf[sep] = '\0';
	}
	return (0);
}
