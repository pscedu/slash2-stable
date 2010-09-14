/* $Id$ */
/*
 * %PSC_START_COPYRIGHT%
 * -----------------------------------------------------------------------------
 * Copyright (c) 2006-2010, Pittsburgh Supercomputing Center (PSC).
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

#define PSC_SUBSYS PSS_MEM

/*
 * Dynamically resizeable arrays.
 * This API is not thread-safe!
 */

#include <err.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "psc_ds/dynarray.h"
#include "psc_util/alloc.h"
#include "psc_util/log.h"

/**
 * psc_dynarray_initf - Initialize a dynamically resizable array.
 * @pda: dynamic array to initialize.
 * @flags: behavioral flags.
 */
void
psc_dynarray_initf(struct psc_dynarray *pda, int flags)
{
	pda->pda_pos = 0;
	pda->pda_nalloc = 0;
	pda->pda_flags = flags;
	pda->pda_items = NULL;
}

/**
 * _psc_dynarray_resize - Resize a dynamic array.
 * @pda: dynamic array to resize.
 * @n: size.
 * Returns -1 on failure and zero on success.
 */
int
_psc_dynarray_resize(struct psc_dynarray *pda, int n)
{
	int i, flags;
	void *p;

	flags = 0;
	if (pda->pda_flags & PDAF_NOLOG)
		flags |= PAF_NOLOG;
	p = psc_realloc(pda->pda_items,
	    n * sizeof(*pda->pda_items), flags);
	pda->pda_items = p;
	/* Initialize any new slots to zero. */
	for (i = pda->pda_nalloc; i < n; i++)
		pda->pda_items[i] = NULL;
	pda->pda_nalloc = n;
	return (0);
}

/**
 * psc_dynarray_ensurelen - If necessary, enlarge the allocation for
 *	a dynamic array to fit the given number of elements.
 * @pda: dynamic array to ensure.
 * @n: size.
 * Returns -1 on failure and zero on success.
 */
int
psc_dynarray_ensurelen(struct psc_dynarray *pda, int n)
{
	int rc;

	rc = 0;
	if (n > pda->pda_nalloc)
		rc = _psc_dynarray_resize(pda, n);
	return (rc);
}

/**
 * psc_dynarray_exists - Check for membership existence of an item in a
 *	dynamic array.
 * @pda: dynamic array to inspect.
 * @item: element to check existence of.
 * Returns Boolean true on existence and false on nonexistence.
 */
int
psc_dynarray_exists(const struct psc_dynarray *pda, const void *item)
{
	int j, len;
	void **p;

	p = psc_dynarray_get(pda);
	len = psc_dynarray_len(pda);
	for (j = 0; j < len; j++)
		if (p[j] == item)
			return (1);
	return (0);
}

/**
 * psc_dynarray_add - Add a new item to a dynamic array, resizing if necessary.
 * @pda: dynamic array to add to.
 * @item: element to add.
 * Returns -1 on failure or zero on success.
 */
int
psc_dynarray_add(struct psc_dynarray *pda, void *item)
{
	if (psc_dynarray_ensurelen(pda, pda->pda_pos + 1) == -1)
		return (-1);
	pda->pda_items[pda->pda_pos++] = item;
	return (0);
}

/**
 * psc_dynarray_add_ifdne - Add an item to a dynamic array unless it
 *	already exists in the array.
 * @pda: dynamic array to add to.
 * @item: element to add.
 * Returns 1 if already existent, -1 on failure, or zero on nonexistence.
 */
int
psc_dynarray_add_ifdne(struct psc_dynarray *pda, void *item)
{
	int j;

	for (j = 0; j < psc_dynarray_len(pda); j++)
		if (item == psc_dynarray_getpos(pda, j))
			return (1);
	return (psc_dynarray_add(pda, item));
}

/**
 * psc_dynarray_getpos - Access an item in dynamic array.
 * @pda: dynamic array to access.
 * @pos: item index.
 */
void *
psc_dynarray_getpos(const struct psc_dynarray *pda, int pos)
{
	psc_assert(pos >= 0);
	if (pos >= psc_dynarray_len(pda))
		psc_fatalx("out of bounds array access");
	return (pda->pda_items[pos]);
}

/**
 * psc_dynarray_free - Release memory associated with a dynamic array.
 * @pda: dynamic array to access.
 * @pos: item index.
 */
void
psc_dynarray_free(struct psc_dynarray *pda)
{
	int flags = 0;

	if (pda->pda_flags & PDAF_NOLOG)
		flags |= PAF_NOLOG;
	psc_free(pda->pda_items, flags);
	psc_dynarray_init(pda);
}

/**
 * psc_dynarray_reset - Clear all items from a dynamic array.
 * @pda: dynamic array to reset.
 */
void
psc_dynarray_reset(struct psc_dynarray *pda)
{
	pda->pda_pos = 0;
}

/**
 * psc_dynarray_remove - Remove an item from a dynamic array.
 * @pda: dynamic array to remove from.
 * @item: item to remove.
 * @fs: whether to resize the array to save space.
 * Returns the position index the item had.
 * Notes: this routine swaps the last element in the dynarray array
 *	into the slot opened up by the removal.
 */
int
_psc_dynarray_remove(struct psc_dynarray *pda, const void *item, int fs)
{
	int j, len;
	void **p;

	p = psc_dynarray_get(pda);
	len = psc_dynarray_len(pda);
	for (j = 0; j < len; j++)
		if (p[j] == item) {
			p[j] = p[len - 1];
			pda->pda_pos--;
			if (fs)
				_psc_dynarray_resize(pda, pda->pda_pos);
			return (j);
		}
	psc_fatalx("element not found");
}

/**
 * psc_dynarray_splice - Cut and replace a section of a dynarray.
 * @pda: dynamic array to splice.
 * @startpos: offset into array to begin splice.
 * @len: length from offset to remove.
 * @base: start array to splice into
 * @nitems: number of new items to splice into the array.
 */
int
psc_dynarray_splice(struct psc_dynarray *pda, int startpos, int len,
    const void *base, int nitems)
{
	int rc, rem;

	rem = psc_dynarray_len(pda) - startpos - len;
	psc_assert(nitems >= 0);
	psc_assert(len >= 0);
	psc_assert(len <= psc_dynarray_len(pda));
	rc = psc_dynarray_ensurelen(pda, psc_dynarray_len(pda) - len + nitems);
	if (rc)
		return (rc);

	if (nitems != len)
		memmove(pda->pda_items + startpos + nitems - len,
		    pda->pda_items + startpos, rem * sizeof(void *));
	memcpy(pda->pda_items + startpos, base, nitems * sizeof(void *));
	pda->pda_pos += nitems - len;
	return (0);
}

/**
 * psc_dynarray_bsearch - Find the position of an item in a sorted
 *	dynarray.
 * @pda: sorted dynamic array to search.
 * @item: item contained within whose array index is desired.
 * @cmpf: comparison routine.
 * Returns the item's index into the array.  If the item is not in the
 * dynarray, the index value returned is the position the element should
 * take on to maintain sort order.
 */
int
psc_dynarray_bsearch(const struct psc_dynarray *pda, const void *item,
    int (*cmpf)(const void *, const void *))
{
	int rc, min, max, mid;
	void *p;

	min = mid = 0;
	max = psc_dynarray_len(pda) - 1;
	while (min <= max) {
		mid = min + (max - min) / 2;
		p = psc_dynarray_getpos(pda, mid);
		rc = cmpf(item, p);
		if (rc < 0)
			max = mid - 1;
		else if (rc > 0) {
			min = mid + 1;

			/*
			 * If the item doesn't exist, inform caller that
			 * the position the item should take on is after
			 * this mid index.
			 */
			mid++;
		} else
			break;
	}
	return (mid);
}
