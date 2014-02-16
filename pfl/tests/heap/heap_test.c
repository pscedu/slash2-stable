/* $Id$ */
/*
 * %PSC_START_COPYRIGHT%
 * -----------------------------------------------------------------------------
 * Copyright (c) 2014, Pittsburgh Supercomputing Center (PSC).
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

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "pfl/alloc.h"
#include "pfl/heap.h"
#include "pfl/pfl.h"

const char *progname;

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
main(__unusedx int argc, char *argv[])
{
	struct a *p;
	int i, j;

	progname = argv[0];
	pfl_init();

	psc_assert(sizeof(struct a) == 8);

	for (i = 0; i < 10; i++) {
		add(27);	// 1
		add(4);         // 2
		add(97);        // 3
		add(8);         // 4
		add(14);        // 5
		add(3);         // 8
		add(193);       // 10
		add(10);        // 14
		add(1024);      // 27
		add(1);         // 29
		add(2);         // 97
		add(5);         // 193
		add(29);        // 1024

		while ((p = pfl_heap_shift(&hp))) {
			printf("%d\n", p->val);
			PSCFREE(p);
		}
	}

	exit(0);
}
