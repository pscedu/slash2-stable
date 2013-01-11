/* $Id$ */
/*
 * %PSC_START_COPYRIGHT%
 * -----------------------------------------------------------------------------
 * Copyright (c) 2005-2012, Pittsburgh Supercomputing Center (PSC).
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
