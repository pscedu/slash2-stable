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

/*
 * Variable-sized bitmaps.  Internally, bitmaps are arrays
 * of chars that are realloc(3)'d to different lengths.
 *
 * This API is not thread-safe!
 */

#ifndef _PFL_VBITMAP_H_
#define _PFL_VBITMAP_H_

#include <sys/types.h>

#include <limits.h>

struct psc_vbitmap {
	unsigned char		*vb_start;
	unsigned char		*vb_end;
	unsigned char		*vb_pos;	/* ptr to current slot for speed */
	int			 vb_lastsize;	/* #ents in last byte, for sizes not multiple of NBBY */
	int			 vb_flags;	/* see PVBF_* flags below */
};

/* vb_flags */
#define PVBF_AUTO		(1 << 0)	/* auto grow bitmap as necessary */
#define PVBF_STATIC		(1 << 1)	/* vbitmap is statically allocated */
#define PVBF_EXTALLOC		(1 << 2)	/* bitmap mem is externally alloc'd */

#define VBITMAP_INIT_AUTO	{ NULL, NULL, NULL, 0, PVBF_AUTO | PVBF_STATIC }

#define psc_vbitmap_new(siz)	psc_vbitmap_newf((siz), 0)

/*
 * Reclaim memory from a variable-sized bitmap.
 * @vb: variable bitmap.
 */
#define psc_vbitmap_free(vb)						\
	do {								\
		_psc_vbitmap_free(vb);					\
		(vb) = NULL;						\
	} while (0)

/*
 * Get the number of elements a bitmap represents.
 * @vb: variable bitmap.
 */
#define psc_vbitmap_getsize(vb)						\
	((size_t)(((vb)->vb_end - (vb)->vb_start) * NBBY + (vb)->vb_lastsize))

#define psc_vbitmap_getnextpos(vb)		(((vb)->vb_pos - (vb)->vb_start) * NBBY)

#define psc_vbitmap_set(vb, pos)		((void)psc_vbitmap_setval((vb), (pos), 1))
#define psc_vbitmap_xset(vb, pos)		(psc_vbitmap_setval((vb), (pos), 1) == 0)
#define psc_vbitmap_xsetval(vb, pos, v)		(psc_vbitmap_setval((vb), (pos), (v)) != (v))
#define psc_vbitmap_unset(vb, pos)		((void)psc_vbitmap_setval((vb), (pos), 0))

#define psc_vbitmap_setrange(vb, pos, siz)	psc_vbitmap_setval_range((vb), (pos), (siz), 1)
#define psc_vbitmap_unsetrange(vb, pos, siz)	psc_vbitmap_setval_range((vb), (pos), (siz), 0)

#define psc_vbitmap_nset(vb)			(psc_vbitmap_getsize(vb) - psc_vbitmap_nfree(vb))

#define psc_vbitmap_isfull(vb)			pfl_vbitmap_israngeset((vb), 1,	\
						    0, psc_vbitmap_getsize(vb))
#define pfl_vbitmap_isempty(vb)			pfl_vbitmap_israngeset((vb), 0,	\
						    0, psc_vbitmap_getsize(vb))

#define psc_vbitmap_printbin1(vb)						\
	do {									\
		const unsigned char *_p;					\
		char *_s, _buf[LINE_MAX];					\
		int _i;								\
										\
		for (_p = (vb)->vb_start, _s = _buf;				\
		    _p <= (vb)->vb_end && _s + 10 < _buf + sizeof(_buf);	\
		    _p++, *_s++ = ' ')						\
			for (_i = 0; _i < 8; _i++, _s++)			\
				if ((*_p >> _i) & 1)				\
					*_s = '1';				\
				else						\
					*_s = '0';				\
		*_s = '\0';							\
		psclog_debug("vbitmap=%p contents=%s", (vb), _buf);		\
	} while (0)

struct psc_vbitmap *
	 psc_vbitmap_newf(size_t, int);

void	_psc_vbitmap_free(struct psc_vbitmap *);
void	 psc_vbitmap_clearall(struct psc_vbitmap *);
int	 psc_vbitmap_get(const struct psc_vbitmap *, size_t);
int	 psc_vbitmap_getncontig(struct psc_vbitmap *, int *);
void	 psc_vbitmap_invert(struct psc_vbitmap *);
int	 pfl_vbitmap_israngeset(struct psc_vbitmap *, int, size_t, size_t);
int	 psc_vbitmap_lcr(struct psc_vbitmap *);
int	 psc_vbitmap_next(struct psc_vbitmap *, size_t *);
int	 psc_vbitmap_nfree(const struct psc_vbitmap *);
int	 psc_vbitmap_resize(struct psc_vbitmap *, size_t);
void	 psc_vbitmap_setall(struct psc_vbitmap *);
void	 psc_vbitmap_setnextpos(struct psc_vbitmap *, size_t);
int	 psc_vbitmap_setval(struct psc_vbitmap *, size_t, int);
int	 psc_vbitmap_setval_range(struct psc_vbitmap *, size_t, size_t, int);

char	*pfl_vbitmap_getabbrbinstring(const struct psc_vbitmap *);
char	*pfl_vbitmap_getbinstring(const struct psc_vbitmap *);
void	 psc_vbitmap_printbin(const struct psc_vbitmap *);
void	 psc_vbitmap_printhex(const struct psc_vbitmap *);
void	 psc_vbitmap_printstats(struct psc_vbitmap *);
void	 psc_vbitmap_getstats(struct psc_vbitmap *, int *, int *);

#endif /* _PFL_VBITMAP_H_ */
