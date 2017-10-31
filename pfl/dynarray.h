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
 * Routines for managing dynamically-sized arrays.
 */

#ifndef _PFL_DYNARRAY_H_
#define _PFL_DYNARRAY_H_

struct psc_dynarray {
	int			  pda_flags;
	int			  pda_pos;
	int			  pda_nalloc;
	void			**pda_items;
};

#define PDAF_NOLOG		(1 << 0)	/* do not log allocations */

#define DYNARRAY_INIT		{ 0, 0, 0, NULL }
#define DYNARRAY_INIT_NOLOG	{ PDAF_NOLOG, 0, 0, NULL }

#define _DYNARRAY_FOREACH(initcode, p, n, pda)				\
	for (initcode; ((n) < psc_dynarray_len(pda) || ((p) = NULL)) &&	\
	    (((p) = psc_dynarray_getpos((pda), (n))) || 1); (n)++)

#define DYNARRAY_FOREACH(p, n, pda)					\
	_DYNARRAY_FOREACH((n) = 0, (p), (n), (pda))

#define DYNARRAY_FOREACH_CONT(p, n, pda)				\
	_DYNARRAY_FOREACH(, (p), (n), (pda))

/**
 * DYNARRAY_FOREACH - Iterate across items of a dynarray.
 * @p: item iterator.
 * @n: integer iterator variable.
 * @pda: dynamic array.
 * Notes: do not invoke psc_dynarray_add/remove/splice/etc in the body
 *	of this loop.
 */
#define DYNARRAY_FOREACH_REVERSE(p, n, pda)				\
	for ((n) = psc_dynarray_len(pda) - 1;				\
	    ((n) >= 0 || ((p) = NULL)) &&				\
	    (((p) = psc_dynarray_getpos((pda), (n))) || 1); (n)--)

/**
 * Obtain the number of elements stored in a dynamic array.
 * @pda: dynamic array to access.
 */
#define psc_dynarray_len(pda)	(pda)->pda_pos

/**
 * psc_dynarray_get - Access a dynamic array's item list.
 * Note: this should only ever be used for read access and never modify
 * access, otherwise the internal structure of the dynarray may become
 * inconsistent!
 *
 * @pda: dynamic array to access.
 */
#define psc_dynarray_get(pda)	((const void *)(pda)->pda_items)

#define psc_dynarray_reverse(pda)					\
	psc_dynarray_reverse_subsequence((pda), 0, psc_dynarray_len(pda))

#define psc_dynarray_get_mutable(pda)					\
	((void *)(pda)->pda_items)

/**
 * psc_dynarray_sort - Reorder elements of a dynarray.
 * @pda: dynamic array whose elements to sort.
 * @sortf: which sorting algorithm to use.
 * @cmpf: criteria for reordering items.
 * Returns -1 on failure or zero on success.
 */
#define psc_dynarray_sort(pda, sortf, cmpf)				\
	do {								\
		if (psc_dynarray_len(pda) > 1)				\
			(sortf)((pda)->pda_items, (pda)->pda_pos,	\
			    sizeof(*(pda)->pda_items), (cmpf));		\
	} while (0)

#define psc_dynarray_init(da)		psc_dynarray_initf((da), 0)

#define psc_dynarray_remove(da, p)	psc_dynarray_removeitem((da), (p))

#define	psc_dynarray_exists(da, p)	(psc_dynarray_finditem((da), (p)) != -1)

#define pfl_dynarray_xadd(da, p)	psc_assert(!psc_dynarray_add_ifdne((da), (p)))

#define pfl_dynarray_truncate(da, n)					\
	do {								\
		(da)->pda_pos = (n);					\
	} while (0)

int	 psc_dynarray_add(struct psc_dynarray *, void *);
int	 psc_dynarray_add_ifdne(struct psc_dynarray *, void *);
int	 psc_dynarray_bsearch(const struct psc_dynarray *, const void *,
	    int (*)(const void *, const void *));
int	 psc_dynarray_concat(struct psc_dynarray *, const struct psc_dynarray *);
int	 psc_dynarray_copy(const struct psc_dynarray *, struct psc_dynarray *);
int	 psc_dynarray_ensurelen(struct psc_dynarray *, int);
int	 psc_dynarray_finditem(struct psc_dynarray *, const void *);
void	 psc_dynarray_free(struct psc_dynarray *);
void	*psc_dynarray_getpos(const struct psc_dynarray *, int);
void	 psc_dynarray_initf(struct psc_dynarray *, int);
int	 psc_dynarray_removeitem(struct psc_dynarray *, const void *);
void	 psc_dynarray_removepos(struct psc_dynarray *, int);
void	 psc_dynarray_reset(struct psc_dynarray *);
void	 psc_dynarray_reverse_subsequence(struct psc_dynarray *, int, int);
void	 psc_dynarray_setpos(struct psc_dynarray *, int, void *);
int	 psc_dynarray_splice(struct psc_dynarray *, int, int, const void *, int);
void	 psc_dynarray_swap(struct psc_dynarray *, int, int);

int	 psc_dynarray_pos(const struct psc_dynarray *);

#endif /* _PFL_DYNARRAY_H_ */
