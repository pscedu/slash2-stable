/* $Id$ */
/*
 * %ISC_START_LICENSE%
 * ---------------------------------------------------------------------
 * Copyright 2015, Google, Inc.
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

#include "pfl/alloc.h"
#include "pfl/dynarray.h"
#include "pfl/log.h"

/*
 * Initialize a dynamically resizable array.
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

int
psc_dynarray_pos(const struct psc_dynarray *pda)
{
	return (pda->pda_pos);
}
/*
 * Resize a dynamic array.
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

/*
 * If necessary, enlarge the allocation for a dynamic array to fit the
 * given number of elements.
 *
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

/*
 * Reverse the order of array entries.
 * @pda: the dynamic array to reverse.
 */
void
psc_dynarray_reverse_subsequence(struct psc_dynarray *pda, int start,
    int len)
{
	void *tmp;
	int i;

	for (i = start; i < len / 2; i++)
		SWAP(pda->pda_items[i], pda->pda_items[
		    start + len - 1 - i], tmp);
}

/*
 * Add a new item to a dynamic sized array, resizing if necessary.
 * @pda: dynamic array to add to.
 * @item: element to add.
 * Returns -1 on failure or zero on success.
 */
int
psc_dynarray_add(struct psc_dynarray *pda, void *item)
{
	if (pda->pda_pos + 1 > pda->pda_nalloc &&
	    psc_dynarray_ensurelen(pda, 2 * (pda->pda_pos + 1)) == -1)
		return (-1);

	pda->pda_items[pda->pda_pos++] = item;
	return (0);
}

/*
 * Add an item to a dynamic array unless it already exists in the array.
 * @pda: dynamic array to add to.
 * @item: element to add.
 * Returns 1 if already existent, -1 on failure, or zero on
 * nonexistence.
 */
int
psc_dynarray_add_ifdne(struct psc_dynarray *pda, void *item)
{
	if (psc_dynarray_exists(pda, item))
		return (1);
	return (psc_dynarray_add(pda, item));
}

/*
 * Access an item in a dynamic array.
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

/*
 * Set the item for a position in a dynamic array.
 * @pda: dynamic array to access.
 * @pos: item index.
 * @p: item.
 */
void
psc_dynarray_setpos(struct psc_dynarray *pda, int pos, void *p)
{
	psc_assert(pos >= 0);
	if (pos >= pda->pda_nalloc)
		psc_fatalx("out of bounds array access");
	pda->pda_items[pos] = p;
	if (pos >= pda->pda_pos)
		pda->pda_pos = pos + 1;
}

/*
 * Release memory associated with a dynamic array.
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

/*
 * Clear all items from a dynamic array.
 * @pda: dynamic array to reset.
 */
void
psc_dynarray_reset(struct psc_dynarray *pda)
{
	pda->pda_pos = 0;
}

int
psc_dynarray_finditem(struct psc_dynarray *pda, const void *item)
{
	void *p;
	int j;

	DYNARRAY_FOREACH(p, j, pda)
		if (p == item)
			return (j);
	return (-1);
}

/*
 * Remove the given position from the dynarray.  This API assumes the
 * dynarray is unordered so it will reposition the final element in the
 * emptied slot.  Use a different API if this is undesirable.
 */
void
psc_dynarray_removepos(struct psc_dynarray *pda, int pos)
{
	void **p;

	p = psc_dynarray_get_mutable(pda);
	psc_assert(pos >= 0 && pos < psc_dynarray_len(pda));
	if (pos != psc_dynarray_len(pda) - 1)
		p[pos] = p[psc_dynarray_len(pda) - 1];
	pda->pda_pos--;
}

/*
 * Remove an item from a dynamic array.
 * @pda: dynamic array to remove from.
 * @item: item to remove.
 * Returns the position index the item had.
 * Notes: this routine swaps the last element in the dynarray into the
 *	slot opened up by the removal.
 */
int
psc_dynarray_removeitem(struct psc_dynarray *pda, const void *item)
{
	int idx;

	idx = psc_dynarray_finditem(pda, item);
	if (idx == -1)
		psc_fatalx("element not found");
	psc_dynarray_removepos(pda, idx);
	return (idx);
}

/*
 * Cut and replace a section of a dynarray.
 * @pda: dynamic array to splice.
 * @off: offset into array to begin splice.
 * @nrmv: number of items to remove.
 * @base: start array to splice from.
 * @nadd: number of new items to splice into the array.
 */
int
psc_dynarray_splice(struct psc_dynarray *pda, int off, int nrmv,
    const void *base, int nadd)
{
	int oldlen, rc, rem;
	void **p;

	oldlen = psc_dynarray_len(pda);
	psc_assert(nadd >= 0);
	psc_assert(nrmv >= 0);
	psc_assert(off + nrmv <= oldlen);
	rc = psc_dynarray_ensurelen(pda, oldlen + nadd - nrmv);
	if (rc)
		return (rc);

	p = pda->pda_items + off;
	if (nadd != nrmv) {
		rem = oldlen - off - nrmv;
		memmove(nadd > nrmv ? p + nadd - nrmv : p + nadd,
		    p + nrmv, rem * sizeof(void *));
		pda->pda_pos += nadd - nrmv;
	}
	memcpy(p, base, nadd * sizeof(void *));
	return (0);
}

/*
 * Find the position of an item in a sorted dynarray.
 * @pda: sorted dynamic array to search.
 * @item: item contained within whose array index is desired.
 * @cmpf: comparison routine.
 * Returns the item's index into the array.  If the item is not in the
 * dynarray, the index value returned is the position the element should
 * take on to maintain sort order.
 *
 * XXX this should be changed to use bsearch_ceil().
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
		mid = (max + min) / 2;
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

/*
 * Duplicate items in one dynarray to another.
 * @pda: dynamic array to copy to.
 * @src: dynamic array to copy from.
 */
int
psc_dynarray_concat(struct psc_dynarray *pda,
    const struct psc_dynarray *src)
{
	int rc, i;

	for (i = 0; i < psc_dynarray_len(src); i++) {
		rc = psc_dynarray_add(pda, psc_dynarray_getpos(src, i));
		if (rc)
			return (rc);
	}
	return (0);
}

void
psc_dynarray_swap(struct psc_dynarray *da, int a, int b)
{
	void *tmp;

	psc_assert(a >= 0);
	psc_assert(b >= 0);
	psc_assert(a < psc_dynarray_len(da));
	psc_assert(b < psc_dynarray_len(da));
	if (a != b)
		SWAP(da->pda_items[a], da->pda_items[b], tmp);
}

int
psc_dynarray_copy(const struct psc_dynarray *src,
    struct psc_dynarray *dst)
{
	psc_dynarray_ensurelen(dst, psc_dynarray_len(src));
	memcpy(dst->pda_items, src->pda_items, sizeof(void *) *
	    psc_dynarray_len(src));
	dst->pda_pos = psc_dynarray_len(src);
	return (0);
}
