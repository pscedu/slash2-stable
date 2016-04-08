/* $Id$ */
/*
 * %ISC_START_LICENSE%
 * ---------------------------------------------------------------------
 * Copyright 2015-2016, Google, Inc.
 * Copyright (c) 2014-2015, Pittsburgh Supercomputing Center (PSC).
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

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "pfl/alloc.h"
#include "pfl/heap.h"
#include "pfl/pfl.h"
#include "pfl/random.h"

struct a {
	int val;
	struct pfl_heap_entry entry;
};

int
cmp(const void *a, const void *b)
{
	const struct a *x = a, *y = b;

	return (CMP(x->val, y->val));
}

struct pfl_heap hp = HEAP_INIT(struct a, entry, cmp);

/*
 *                       _0:_1
 *           _1:_2                   _2:_3
 *     _3:_4       _4:_5       _5:_6       _6:_7
 *  _7:_8 _8:_9 _9:10 10:11 11:12 12:13 13:14 14:15
 */
void
prheap(void)
{
	int j, width = 1;
	struct a *p;

	for (j = hp.ph_nitems; j; j >>= 1)
		width *= 2;
	for (j = 0; j < hp.ph_nitems; j++) {
		if ((j & (j+1)) == 0) {
			if (j)
				printf("\n");
			width /= 2;
		}
		p = hp.ph_base[j];
		printf("%*d:%d ", 4*width-1, p->val, p->entry.phe_idx);
	}
	printf("\n");
}

void
add(int v)
{
	struct a *p;

	p = PSCALLOC(sizeof(*p));
	p->val = v;
	pfl_heap_add(&hp, p);
}

int
main(__unusedx int argc, __unusedx char *argv[])
{
	struct a *p;
	int i, last;

	pfl_init();

	psc_assert(sizeof(struct a) == 8);

	for (i = 0; i < 1000; i++) 
		add(psc_random32u(100));

	last = -1;
	while ((p = pfl_heap_shift(&hp))) {
		printf("%d\n", p->val);
		psc_assert(p->val >= last);
		last = p->val;
		PSCFREE(p);
	}

	exit(0);
}
