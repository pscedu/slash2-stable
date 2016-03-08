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

#include <ctype.h>
#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "pfl/alloc.h"
#include "pfl/dynarray.h"
#include "pfl/str.h"

int
pfl_vasprintf(char **p, const char *fmt, va_list ap)
{
	va_list apd;
	int sz;

	va_copy(apd, ap);
	sz = vsnprintf(NULL, 0, fmt, ap);
	psc_assert(sz != -1);

	sz++;
	*p = PSCALLOC(sz);

	vsnprintf(*p, sz, fmt, apd);
	va_end(apd);

	return (sz);
}

int
pfl_asprintf(char **p, const char *fmt, ...)
{
	va_list ap;
	int rc;

	va_start(ap, fmt);
	rc = pfl_vasprintf(p, fmt, ap);
	va_end(ap);

	return (rc);
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

char **
pfl_str_split(char *s)
{
	struct psc_dynarray a = DYNARRAY_INIT;
	char **v, *p, *beg;
	int delim, esc;
	size_t len;

	for (p = beg = s; *p; ) {
		if (isspace(*p)) {
			*p++ = '\0';
			while (isspace(*p))
				p++;
			beg = p;
			continue;
		}
		if (*p == '\'' ||
		    *p == '"') {
			delim = *p;
			esc = 0;

			for (; *p; p++) {
				if (esc) {
					esc = 0;
					continue;
				}
				if (*p == delim)
					break;
				else if (*p == '\\')
					esc = 1;
			}
		}
		if (beg == p)
			psc_dynarray_add(&a, beg);
		p++;
	}

	psc_dynarray_add(&a, NULL);

	len = sizeof(*v) * psc_dynarray_len(&a);
	v = PSCALLOC(len + sizeof(*v));
	memcpy(v, psc_dynarray_get(&a), len);
	psc_dynarray_free(&a);
	return (v);
}

char *
pfl_strrastr(const char *s, char c, size_t adj)
{
	size_t pos;
	char *p;

	pos = strlen(s) - adj - 1;
	for (p = (char *)s + pos; p > s && *p != c; p--)
		;
	return (p);
}

size_t
pfl_string_eqlen(const char *a, const char *b)
{
	size_t n;

	for (n = 0; *a && *b && *a == *b; n++, a++, b++) 
		;
	return (n); 
}
