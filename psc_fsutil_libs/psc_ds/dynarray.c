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
 * psc_dynarray_init - Initialize a dynamic array.
 * @da: dynamic array to initialize.
 */
void
psc_dynarray_init(struct psc_dynarray *da)
{
	da->da_pos = 0;
	da->da_nalloc = 0;
	da->da_items = NULL;
}

/**
 * _psc_dynarray_resize - Resize a dynamic array.
 * @da: dynamic array to resize.
 * @n: size.
 * Returns -1 on failure and zero on success.
 */
int
_psc_dynarray_resize(struct psc_dynarray *da, int n)
{
	void *p;
	int i;

	p = psc_realloc(da->da_items, n * sizeof(*da->da_items),
	    PAF_NOLOG | PAF_CANFAIL | PAF_NOREAP);
	if (p == NULL && n)
		return (-1);
	da->da_items = p;
	/* Initialize new slots to zero. */
	for (i = da->da_nalloc; i < n; i++)
		da->da_items[i] = NULL;
	da->da_nalloc = n;
	return (0);
}

/**
 * psc_dynarray_ensurelen - If necessary, enlarge the allocation for
 *	a dynamic array to fit the given number of elements.
 * @da: dynamic array to ensure.
 * @n: size.
 * Returns -1 on failure and zero on success.
 */
int
psc_dynarray_ensurelen(struct psc_dynarray *da, int n)
{
	int rc;

	rc = 0;
	if (n > da->da_nalloc)
		rc = _psc_dynarray_resize(da, n);
	return (rc);
}

/**
 * psc_dynarray_exists - Check for membership existence of an item in a
 *	dynamic array.
 * @da: dynamic array to inspect.
 * @item: element to check existence of.
 * Returns Boolean true on existence and false on nonexistence.
 */
int
psc_dynarray_exists(const struct psc_dynarray *da, const void *item)
{
	int j, len;
	void **p;

	p = psc_dynarray_get(da);
	len = psc_dynarray_len(da);
	for (j = 0; j < len; j++)
		if (p[j] == item)
			return (1);
	return (0);
}

/**
 * psc_dynarray_add - Add a new item to a dynamic array, resizing if necessary.
 * @da: dynamic array to add to.
 * @item: element to add.
 * Returns -1 on failure or zero on success.
 */
int
psc_dynarray_add(struct psc_dynarray *da, void *item)
{
	if (psc_dynarray_ensurelen(da, da->da_pos + 1) == -1)
		return (-1);
	da->da_items[da->da_pos++] = item;
	return (0);
}

/**
 * psc_dynarray_add_ifdne - Add an item to a dynamic array unless it
 *	already exists in the array.
 * @da: dynamic array to add to.
 * @item: element to add.
 * Returns 1 if already existent, -1 on failure, or zero on nonexistence.
 */
int
psc_dynarray_add_ifdne(struct psc_dynarray *da, void *item)
{
	int j;

	for (j = 0; j < psc_dynarray_len(da); j++)
		if (item == psc_dynarray_getpos(da, j))
			return (1);
	return (psc_dynarray_add(da, item));
}

/**
 * psc_dynarray_getpos - Access an item in dynamic array.
 * @da: dynamic array to access.
 * @pos: item index.
 */
void *
psc_dynarray_getpos(const struct psc_dynarray *da, int pos)
{
	psc_assert(pos >= 0);
	if (pos >= psc_dynarray_len(da))
		psc_fatalx("out of bounds array access");
	return (da->da_items[pos]);
}

/**
 * psc_dynarray_free - Release memory associated with a dynamic array.
 * @da: dynamic array to access.
 * @pos: item index.
 */
void
psc_dynarray_free(struct psc_dynarray *da)
{
	free(da->da_items);
	psc_dynarray_init(da);
}

/**
 * psc_dynarray_reset - Clear all items from a dynamic array.
 * @da: dynamic array to reset.
 */
void
psc_dynarray_reset(struct psc_dynarray *da)
{
	da->da_pos = 0;
	psc_dynarray_freeslack(da);
}

/**
 * psc_dynarray_remove - Remove an item from a dynamic array.
 * @da: dynamic array to remove from.
 * @item: item to remove.
 * Returns the position index the item had.
 * Notes: this routine swaps the last element in the dynarray array
 *	into the slot opened up by the removal.
 */
int
psc_dynarray_remove(struct psc_dynarray *da, const void *item)
{
	int j, len;
	void **p;

	p = psc_dynarray_get(da);
	len = psc_dynarray_len(da);
	for (j = 0; j < len; j++)
		if (p[j] == item) {
			p[j] = p[len - 1];
			da->da_pos--;

			psc_dynarray_freeslack(da);
			return (j);
		}
	psc_fatalx("element not found");
}

/**
 * psc_dynarray_freeslack - Release free space from a dynamic array.
 * @da: dynamic array to trim.
 * Returns the size (# of item slots) the array has decreased by/freed.
 */
int
psc_dynarray_freeslack(struct psc_dynarray *da)
{
	int rc;

	rc = 0;
	if (da->da_pos < da->da_nalloc)
		rc = _psc_dynarray_resize(da, da->da_pos);
	return (rc);
}

/**
 * psc_dynarray_splice - Cut and replace a section of a dynarray.
 * @da: dynamic array to splice.
 * @startpos: offset into array to begin splice.
 * @len: length from offset to remove.
 * @base: start array to splice into
 * @nitems: number of new items to splice into the array.
 */
int
psc_dynarray_splice(struct psc_dynarray *da, int startpos, int len,
    const void *base, int nitems)
{
	int rc, rem;

	rem = psc_dynarray_len(da) - startpos - len;
	psc_assert(nitems >= 0);
	psc_assert(len >= 0);
	psc_assert(len <= psc_dynarray_len(da));
	rc = psc_dynarray_ensurelen(da, psc_dynarray_len(da) - len + nitems);
	if (rc)
		return (rc);

	if (nitems != len)
		memmove(da->da_items + startpos + nitems - len,
		    da->da_items + startpos, rem * sizeof(void *));
	memcpy(da->da_items + startpos, base, nitems * sizeof(void *));
	da->da_pos += nitems - len;
	return (0);
}

/**
 * psc_dynarray_bsearch_pos - Find the position of an item in a sorted
 *	dynarray.
 * @da: sorted dynamic array to search.
 * @item: item contained within whose array index is desired.
 * @cmpf: comparison routine.
 * Returns the item's index into the array.  If the item is not in the
 * dynarray, the index value returned is the position the element should
 * take on to maintain sort order.
 */
int
psc_dynarray_bsearch(const struct psc_dynarray *da, const void *item,
    int (*cmpf)(const void *, const void *))
{
	int rc, min, max, mid;
	void *p;

	min = mid = 0;
	max = psc_dynarray_len(da) - 1;
	while (min <= max) {
		mid = min + (max - min) / 2;
		p = psc_dynarray_getpos(da, mid);
		rc = cmpf(p, item);
		if (rc < 0) {
			max = mid - 1;

			/*
			 * If the item doesn't exist, inform caller that
			 * the position the item should take on is after
			 * this mid index.
			 */
			mid++;
		} else if (rc > 0)
			min = mid + 1;
		else
			break;
	}
	return (mid);
}
