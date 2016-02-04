/* $Id$ */
/*
 * %ISC_START_LICENSE%
 * ---------------------------------------------------------------------
 * Copyright 2016, Google, Inc.
 * Copyright 2013, Pittsburgh Supercomputing Center
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

#include <stdio.h>
#include <string.h>

#include "pfl/str.h"

char *
pfl_getprocenv(int pid, const char *key)
{
	char fn[PATH_MAX], buf[BUFSIZ], *t, *p;
	FILE *fp;
	int c;

	if (snprintf(fn, sizeof(fn), "/proc/%d/environ", pid) == -1)
		return (NULL);

	fp = fopen(fn, "r");
	if (fp == NULL)
		return (NULL);

	t = buf;
	p = NULL;
	while ((c = fgetc(fp)) != EOF) {
		if (t < buf + sizeof(buf)) {
			*t++ = c;
			if (c == '\0' && t > buf + 1) {
				if ((p = strchr(buf, '=')) != NULL) {
					*p++ = '\0';
					if (strcmp(buf, key) == 0)
						break;
					p = NULL;
				}
			}
		}
		if (c == '\0')
			t = buf;
	}
	fclose(fp);
	return (p ? pfl_strdup(p) : NULL);
}
