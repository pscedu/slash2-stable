/* $Id$ */

/*
 * Dynamically-sized array API.
 * This code is *not* thread-safe!
 */

#include <err.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "psc_ds/dynarray.h"

void
dynarray_init(struct dynarray *da)
{
	da->da_pos = 0;
	da->da_nalloc = 0;
	da->da_items = NULL;
}

int
dynarray_hintlen(struct dynarray *da, int n)
{
	void *p;
	int i;

	if (n > da->da_nalloc) {
		p = realloc(da->da_items, n * sizeof(*da->da_items));
		if (p == NULL && n)
			return (-1);
		da->da_items = p;
		for (i = da->da_nalloc; i < n; i++)
			da->da_items[i] = NULL;
		da->da_nalloc = n;
	}
	return (0);
}

int
dynarray_add(struct dynarray *da, const void *item)
{
	if (dynarray_hintlen(da, da->da_pos + 1) == -1)
		return (-1);
	da->da_items[da->da_pos++] = item;
	return (0);
}

void *
dynarray_get(const struct dynarray *da)
{
	return (da->da_items);
}

void
dynarray_free(struct dynarray *da)
{
	free(da->da_items);
}

int
dynarray_len(const struct dynarray *da)
{
	return (da->da_pos);
}

void
dynarray_reset(struct dynarray *da)
{
	da->da_pos = 0;
}

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
			return;
		}
	abort();
}

int
dynarray_freeslack(struct dynarray *da)
{
	void *p;
	int n;

	if (da->da_pos < da->da_nalloc) {
		n = da->da_pos;
		p = realloc(da->da_items, n * sizeof(*da->da_items));
		if (p == NULL && n)
			return (-1);
		da->da_items = p;
		da->da_nalloc = n;
	}
	return (0);
}
