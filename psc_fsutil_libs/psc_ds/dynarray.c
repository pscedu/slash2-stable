/* $Id$ */

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
 * dynarray_init - Initialize a dynamic array.
 * @da: dynamic array to initialize.
 */
void
dynarray_init(struct dynarray *da)
{
	da->da_pos = 0;
	da->da_nalloc = 0;
	da->da_items = NULL;
}

/**
 * _dynarray_resize - Resize a dynamic array.
 * @da: dynamic array to resize.
 * @n: size.
 * Returns -1 on failure and zero on success.
 */
int
_dynarray_resize(struct dynarray *da, int n)
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
 * dynarray_ensurelen - If necessary, enlarge the allocation for a dynamic
 *	array to fit the given number of elements.
 * @da: dynamic array to ensure.
 * @n: size.
 * Returns -1 on failure and zero on success.
 */
int
dynarray_ensurelen(struct dynarray *da, int n)
{
	int rc;

	rc = 0;
	if (n > da->da_nalloc)
		rc = _dynarray_resize(da, n);
	return (rc);
}

/**
 * dynarray_exists - Check for membership existence of an item in a
 *	dynamic array.
 * @da: dynamic array to inspect.
 * @item: element to check existence of.
 * Returns Boolean true on existence and false on nonexistence.
 */
int
dynarray_exists(const struct dynarray *da, const void *item)
{
	int j, len;
	void **p;

	p = dynarray_get(da);
	len = dynarray_len(da);
	for (j = 0; j < len; j++)
		if (p[j] == item)
			return (1);
	return (0);
}

/**
 * dynarray_add - Add a new item to a dynamic array, resizing if necessary.
 * @da: dynamic array to add to.
 * @item: element to add.
 * Returns -1 on failure or zero on success.
 */
int
dynarray_add(struct dynarray *da, void *item)
{
	if (dynarray_ensurelen(da, da->da_pos + 1) == -1)
		return (-1);
	da->da_items[da->da_pos++] = item;
	return (0);
}

/**
 * dynarray_add_ifdne - Add an item to a dynamic array unless it already
 *	exists in the array.
 * @da: dynamic array to add to.
 * @item: element to add.
 * Returns 1 if already existent, -1 on failure, or zero on nonexistence.
 */
int
dynarray_add_ifdne(struct dynarray *da, void *item)
{
	int j;

	for (j = 0; j < dynarray_len(da); j++)
		if (item == dynarray_getpos(da, j))
			return (1);
	return (dynarray_add(da, item));
}

/**
 * dynarray_getpos - Access an item in dynamic array.
 * @da: dynamic array to access.
 * @pos: item index.
 */
void *
dynarray_getpos(const struct dynarray *da, int pos)
{
	if (pos >= da->da_pos)
		psc_fatalx("out of bounds array access");
	return (da->da_items[pos]);
}

/**
 * dynarray_free - Release memory associated with a dynarray
 * @da: dynamic array to access.
 * @pos: item index.
 */
void
dynarray_free(struct dynarray *da)
{
	free(da->da_items);
	dynarray_init(da);
}

/**
 * dynarray_reset - Clear all items from a dynamic array.
 * @da: dynamic array to reset.
 */
void
dynarray_reset(struct dynarray *da)
{
	da->da_pos = 0;
	dynarray_freeslack(da);
}

/**
 * dynarray_remove - Remove an item from a dynamic array.
 * @da: dynamic array to remove from.
 * @item: item to remove.
 */
void
dynarray_remove(struct dynarray *da, const void *item)
{
	int j, len;
	void **p;

	p = dynarray_get(da);
	len = dynarray_len(da);
	for (j = 0; j < len; j++)
		if (p[j] == item) {
			p[j] = p[len - 1];
			da->da_pos--;

			dynarray_freeslack(da);
			return;
		}
	psc_fatalx("dynarray_remove: element not found");
}

/**
 * dynarray_freeslack - Release free space slots in an dynarray.
 * @da: dynamic array to trim.
 * Returns the size (# of item slots) the dynarray has decreased by.
 */
int
dynarray_freeslack(struct dynarray *da)
{
	int rc;

	rc = 0;
	if (da->da_pos < da->da_nalloc)
		rc = _dynarray_resize(da, da->da_pos);
	return (rc);
}
