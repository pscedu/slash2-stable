/* $Id$ */
/*
 * %ISC_START_LICENSE%
 * ---------------------------------------------------------------------
 * Copyright 2016, Google, Inc.
 * Copyright 2018, Pittsburgh Supercomputing Center
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

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "pfl/log.h"
#include "pfl/bsearch.h"
#include "pfl/cdefs.h"
#include "pfl/pfl.h"

__dead void
usage(void)
{
	extern const char *__progname;

	fprintf(stderr, "usage: %s\n", __progname);
	exit(1);
}

int
cmp(const void *key, const void *hay)
{
	int keyval = (uintptr_t)key;
	const int *hayval = hay;

	return (CMP(keyval, *hayval));
}

int
main(int argc, char *argv[])
{
	int *p, pos, a[] = {
		0,
		1024,
		4096,
		8192,
	};

	pfl_init();
	if (getopt(argc, argv, "") != -1)
		usage();
	argc -= optind;
	if (argc)
		usage();

	p = (void *)(uintptr_t)0;
	pos = bsearch_floor(p, a, nitems(a), sizeof(a[0]), cmp);
	psc_assert(a[pos] == 0);

	p = (void *)(uintptr_t)1;
	pos = bsearch_floor(p, a, nitems(a), sizeof(a[0]), cmp);
	psc_assert(a[pos] == 0);

	p = (void *)(uintptr_t)1023;
	pos = bsearch_floor(p, a, nitems(a), sizeof(a[0]), cmp);
	psc_assert(a[pos] == 0);

	p = (void *)(uintptr_t)1024;
	pos = bsearch_floor(p, a, nitems(a), sizeof(a[0]), cmp);
	psc_assert(a[pos] == 1024);

	p = (void *)(uintptr_t)1025;
	pos = bsearch_floor(p, a, nitems(a), sizeof(a[0]), cmp);
	psc_assert(a[pos] == 1024);

	p = (void *)(uintptr_t)8191;
	pos = bsearch_floor(p, a, nitems(a), sizeof(a[0]), cmp);
	psc_assert(a[pos] == 4096);

	p = (void *)(uintptr_t)8192;
	pos = bsearch_floor(p, a, nitems(a), sizeof(a[0]), cmp);
	psc_assert(a[pos] == 8192);

	p = (void *)(uintptr_t)99999999999;
	pos = bsearch_floor(p, a, nitems(a), sizeof(a[0]), cmp);
	psc_assert(a[pos] == 8192);

	exit(0);
}
