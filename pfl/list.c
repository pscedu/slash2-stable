/* $Id$ */
/*
 * %PSC_START_COPYRIGHT%
 * -----------------------------------------------------------------------------
 * Copyright (c) 2006-2013, Pittsburgh Supercomputing Center (PSC).
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

#include "psc_ds/listcache.h"
#include "psc_ds/lockedlist.h"

void
psclist_sort(void **p, struct psclist_head *hd, int n, ptrdiff_t off,
    void (*sortf)(void *, size_t, size_t, int (*)(const void *, const void *)),
    int (*cmpf)(const void *, const void *))
{
	struct psc_listentry *e;
	void *next, *prev;
	int j = 0;

	psc_assert(n >= 0);
	if (n < 2)
		return;

	psclist_for_each(e, hd)
		p[j++] = ((char *)e) - off;
	sortf(p, n, sizeof(*p), cmpf);
	prev = hd;
	for (j = 0; j < n; j++, prev = e) {
		e = (void *)((char *)p[j] + off);
		if (j + 1 == n)
			next = hd;
		else
			next = (char *)p[j + 1] + off;
		psc_lentry_prev(e) = prev;
		psc_lentry_next(e) = next;
	}
	psc_listhd_first(hd) = (void *)((char *)p[0] + off);
	psc_listhd_last(hd) = prev;
}
