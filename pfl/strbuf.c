/* $Id$ */
/*
 * %ISC_START_LICENSE%
 * ---------------------------------------------------------------------
 * Copyright 2015-2016, Google, Inc.
 * Copyright (c) 2005-2015, Pittsburgh Supercomputing Center (PSC).
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

#include <err.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "pfl/str.h"

#define BUF_GROWAMT 30

__inline void
pfl_strbuf_init(struct pfl_strbuf *buf)
{
	buf->psb_pos = buf->psb_max = -1;
	buf->psb_buf = NULL;
}

void
pfl_strbuf_realloc(struct pfl_strbuf *buf)
{
	void *ptr;

	if ((ptr = realloc(buf->psb_buf,
	     buf->psb_max * sizeof(*buf->psb_buf))) == NULL)
		err(1, "realloc");
	buf->psb_buf = ptr;
}

void
pfl_strbuf_append(struct pfl_strbuf *buf, int ch)
{
	if (++buf->psb_pos >= buf->psb_max) {
		buf->psb_max += BUF_GROWAMT;
		pfl_strbuf_realloc(buf);
	}
	buf->psb_buf[buf->psb_pos] = ch;
}

__inline void
pfl_strbuf_appendv(struct pfl_strbuf *buf, const char *s)
{
	while (*s != '\0')
		pfl_strbuf_append(buf, *s++);
}

void
pfl_strbuf_appendfv(struct pfl_strbuf *buf, const char *fmt, ...)
{
	char *s, *t;
	va_list ap;

	va_start(ap, fmt);
	if (vasprintf(&t, fmt, ap) == -1)
		err(1, "vasprintf");
	va_end(ap);

	for (s = t; *s != '\0'; s++)
		pfl_strbuf_append(buf, *s);
	free(t);
}

__inline char *
pfl_strbuf_get(const struct pfl_strbuf *buf)
{
	return (buf->psb_buf);
}

__inline void
pfl_strbuf_free(struct pfl_strbuf *buf)
{
	free(buf->psb_buf);
}

__inline void
pfl_strbuf_reset(struct pfl_strbuf *buf)
{
	buf->psb_pos = -1;
}

__inline void
pfl_strbuf_chop(struct pfl_strbuf *buf)
{
	buf->psb_pos--;
}

__inline int
pfl_strbuf_len(const struct pfl_strbuf *buf)
{
	return (buf->psb_pos + 1);
}
