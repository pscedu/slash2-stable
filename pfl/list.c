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

#include "pfl/listcache.h"
#include "pfl/lockedlist.h"

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
