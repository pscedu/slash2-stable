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

#ifndef _PFL_STR_H_
#define _PFL_STR_H_

#include <sys/types.h>

#include <stdarg.h>
#include <stdint.h>
#include <string.h>

#ifdef HAVE_STRVIS
# include <vis.h>
#else
# include "compat/vis.h"
#endif

#ifndef HAVE_STRNVIS
# include "compat/strnvis.h"
#endif

struct pfl_strbuf {
	int	 psb_pos;
	int	 psb_max;
	char	*psb_buf;
};

#ifndef HAVE_STRLCPY
size_t	strlcpy(char *, const char *, size_t);
size_t	strlcat(char *, const char *, size_t);
#endif

#ifndef HAVE_STRNLEN
size_t	strnlen(const char *, size_t);
#endif

#define psc_str_hashify(c)	_psc_str_hashify((c), -1)
#define psc_strn_hashify(c, n)	_psc_str_hashify((c), (n))

int	 pfl_asprintf(char **, const char *, ...);
int	 pfl_vasprintf(char **, const char *, va_list);
int	 pfl_dirname(const char *, char *);
char	*pfl_strdup(const char *);
char	*pfl_strndup(const char *, size_t);
char   **pfl_str_split(char *);
char	*pfl_strrastr(const char *, char, size_t);

size_t	 pfl_string_eqlen(const char *, const char *);

int	 pfl_memchk(const void *, int, size_t);

uint64_t _psc_str_hashify(const void *, size_t);

int	xsnprintf(char *, size_t, const char *, ...);

#define pfl_strbuf_nul(buf) pfl_strbuf_append((buf), '\0')

void	 pfl_strbuf_append(struct pfl_strbuf *, int);
void	 pfl_strbuf_appendfv(struct pfl_strbuf *, const char *, ...);
void	 pfl_strbuf_appendv(struct pfl_strbuf *, const char *);
void	 pfl_strbuf_chop(struct pfl_strbuf *);
void	 pfl_strbuf_free(struct pfl_strbuf *);
char	*pfl_strbuf_get(const struct pfl_strbuf *);
void	 pfl_strbuf_init(struct pfl_strbuf *);
int	 pfl_strbuf_len(const struct pfl_strbuf *);
void	 pfl_strbuf_realloc(struct pfl_strbuf *);
void	 pfl_strbuf_reset(struct pfl_strbuf *);

static __inline const char *
pfl_basename(const char *s)
{
	const char *bn, *p;

	// XXX strrchr()
	for (bn = p = s; *p; p++)
		if (*p == '/')
			bn = p + 1;
	return (bn);
}

#endif /* _PFL_STR_H_ */
