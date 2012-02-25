/* $Id$ */
/*
 * %PSC_START_COPYRIGHT%
 * -----------------------------------------------------------------------------
 * Copyright (c) 2006-2012, Pittsburgh Supercomputing Center (PSC).
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

#ifndef _PFL_STR_H_
#define _PFL_STR_H_

#include <sys/types.h>

#include <string.h>

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
int	 pfl_dirname(const char *, char *);
char	*pfl_strdup(const char *);

int	_psc_str_hashify(const char *, int);

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

static __inline int
pfl_memchk(const void *buf, int val, size_t len)
{
	const char *p;

	for (p = buf; p < (const char *)buf + len; p++)
		if (*p != val)
			return (0);
	return (1);
}

static __inline const char *
pfl_basename(const char *s)
{
	const char *bn, *p;

	for (bn = p = s; *p; p++)
		if (*p == '/')
			bn = p + 1;
	return (bn);
}

#endif /* _PFL_STR_H_ */
