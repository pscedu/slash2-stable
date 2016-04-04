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

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "pfl/cdefs.h"
#include "pfl/pfl.h"
#include "pfl/dynarray.h"
#include "pfl/log.h"

__dead void
usage(void)
{
	extern const char *__progname;

	fprintf(stderr, "usage: %s\n", __progname);
	exit(1);
}

#define PTR_0	((void *)0x00)
#define PTR_1	((void *)0x01)
#define PTR_2	((void *)0x02)
#define PTR_3	((void *)0x03)
#define PTR_4	((void *)0x04)

#define PTR_a	((void *)0x0a)
#define PTR_b	((void *)0x0b)
#define PTR_c	((void *)0x0c)

void
display(struct psc_dynarray *da)
{
	void *p;
	int j;

	DYNARRAY_FOREACH(p, j, da)
		printf("%p ", p);
	printf("\n");
}

#define CHECK(d, ...)	_check(PFL_CALLERINFO(), (d), ##__VA_ARGS__)

void
_check(const struct pfl_callerinfo *pci, struct psc_dynarray *da, ...)
{
	void *p, *t, *checkp;
	va_list ap, c;
	int j;

	va_start(ap, da);
	va_copy(c, ap);
	DYNARRAY_FOREACH(p, j, da) {
		checkp = va_arg(ap, void *);
		if (p != checkp) {
			psclog_max("error");
			printf("is:\t");
			display(da);
			printf("want:\t");
			do {
				t = va_arg(c, void *);
				printf("%p ", t);
			} while (t);
			printf("\n");
			psc_fatalx("%p != %p", p, checkp);
		}
	}
	va_end(c);
	va_end(ap);
	psc_assert(va_arg(ap, void *) == NULL);
}

int
cmp(const void *a, const void *b)
{
	return (CMP(a, b));
}

int
pcmp(const void *a, const void *b)
{
	const void * const *x = a;
	const void * const *y = b;

	return (CMP(*x, *y));
}

int
main(int argc, char *argv[])
{
	extern int _psc_dynarray_resize(struct psc_dynarray *, int);
	struct psc_dynarray da = DYNARRAY_INIT;
	int i, j;
	void *p;

	pfl_init();
	if (getopt(argc, argv, "") != -1)
		usage();
	argc -= optind;
	if (argc)
		usage();

	psc_dynarray_ensurelen(&da, 1);
	_psc_dynarray_resize(&da, 0);

	p = PTR_4; psc_dynarray_splice(&da, 0, 0, &p, 1); CHECK(&da, PTR_4, NULL);
	psc_dynarray_splice(&da, 0, 1, NULL, 0); CHECK(&da, NULL);

	p = PTR_4; psc_dynarray_splice(&da, 0, 0, &p, 1); CHECK(&da, PTR_4, NULL);
	p = PTR_3; psc_dynarray_splice(&da, 0, 0, &p, 1); CHECK(&da, PTR_3, PTR_4, NULL);
	p = PTR_2; psc_dynarray_splice(&da, 0, 0, &p, 1); CHECK(&da, PTR_2, PTR_3, PTR_4, NULL);
	p = PTR_1; psc_dynarray_splice(&da, 0, 0, &p, 1); CHECK(&da, PTR_1, PTR_2, PTR_3, PTR_4, NULL);

	psc_dynarray_splice(&da, 0, 0, NULL, 0); CHECK(&da, PTR_1, PTR_2, PTR_3, PTR_4, NULL);

	p = PTR_a; psc_dynarray_splice(&da, 2, 1, &p, 1); CHECK(&da, PTR_1, PTR_2, PTR_a, PTR_4, NULL);
	p = PTR_b; psc_dynarray_splice(&da, 3, 0, &p, 1); CHECK(&da, PTR_1, PTR_2, PTR_a, PTR_b, PTR_4, NULL);

	psc_dynarray_sort(&da, qsort, pcmp); CHECK(&da, PTR_1, PTR_2, PTR_4, PTR_a, PTR_b, NULL);

	psc_assert(psc_dynarray_bsearch(&da, PTR_0, cmp) == 0);
	psc_assert(psc_dynarray_bsearch(&da, PTR_1, cmp) == 0);
	psc_assert(psc_dynarray_bsearch(&da, PTR_2, cmp) == 1);
	psc_assert(psc_dynarray_bsearch(&da, PTR_3, cmp) == 2);
	psc_assert(psc_dynarray_bsearch(&da, PTR_4, cmp) == 2);
	psc_assert(psc_dynarray_bsearch(&da, PTR_a, cmp) == 3);
	psc_assert(psc_dynarray_bsearch(&da, PTR_b, cmp) == 4);
	psc_assert(psc_dynarray_bsearch(&da, PTR_c, cmp) == 5);

	psc_dynarray_splice(&da, 2, 1, NULL, 0); CHECK(&da, PTR_1, PTR_2, PTR_a, PTR_b, NULL);
	psc_dynarray_splice(&da, 0, 1, NULL, 0); CHECK(&da, PTR_2, PTR_a, PTR_b, NULL);

	psc_dynarray_reverse(&da); CHECK(&da, PTR_b, PTR_a, PTR_2, NULL);

	psc_dynarray_reset(&da); CHECK(&da, NULL);

	psc_dynarray_add(&da, PTR_3); CHECK(&da, PTR_3, NULL);

	psc_dynarray_free(&da);

	psc_dynarray_init(&da);
	psc_dynarray_add(&da, PTR_3); CHECK(&da, PTR_3, NULL);
	psc_dynarray_add(&da, PTR_2); CHECK(&da, PTR_3, PTR_2, NULL);
	psc_dynarray_add(&da, PTR_3); CHECK(&da, PTR_3, PTR_2, PTR_3, NULL);
	psc_dynarray_add(&da, PTR_2); CHECK(&da, PTR_3, PTR_2, PTR_3, PTR_2, NULL);

	psc_dynarray_removeitem(&da, PTR_3); CHECK(&da, PTR_2, PTR_2, PTR_3, NULL);

	while (psc_dynarray_len(&da))
		psc_dynarray_removepos(&da, 0);

	CHECK(&da, NULL);

	psc_dynarray_free(&da);

	psc_dynarray_init(&da);
	psc_dynarray_add(&da, "foobar-a");
	psc_dynarray_add(&da, "foobar-b");
	psc_dynarray_add(&da, "foobar-c");
	psc_dynarray_add(&da, "foobar-d");
	psc_dynarray_add(&da, "foobar-e");
	psc_dynarray_add(&da, "foobar-f");
	psc_assert(strcmp(psc_dynarray_getpos(&da, psc_dynarray_bsearch(&da, "foobar-a", (void *)strcmp)), "foobar-a") == 0);
	psc_assert(strcmp(psc_dynarray_getpos(&da, psc_dynarray_bsearch(&da, "foobar-b", (void *)strcmp)), "foobar-b") == 0);
	psc_assert(strcmp(psc_dynarray_getpos(&da, psc_dynarray_bsearch(&da, "foobar-c", (void *)strcmp)), "foobar-c") == 0);
	psc_assert(strcmp(psc_dynarray_getpos(&da, psc_dynarray_bsearch(&da, "foobar-d", (void *)strcmp)), "foobar-d") == 0);
	psc_assert(strcmp(psc_dynarray_getpos(&da, psc_dynarray_bsearch(&da, "foobar-e", (void *)strcmp)), "foobar-e") == 0);
	psc_assert(strcmp(psc_dynarray_getpos(&da, psc_dynarray_bsearch(&da, "foobar-f", (void *)strcmp)), "foobar-f") == 0);
	psc_dynarray_splice(&da, 0, 1, NULL, 0);
	psc_assert(strcmp(psc_dynarray_getpos(&da, psc_dynarray_bsearch(&da, "foobar-b", (void *)strcmp)), "foobar-b") == 0);
	psc_assert(strcmp(psc_dynarray_getpos(&da, psc_dynarray_bsearch(&da, "foobar-c", (void *)strcmp)), "foobar-c") == 0);
	psc_assert(strcmp(psc_dynarray_getpos(&da, psc_dynarray_bsearch(&da, "foobar-d", (void *)strcmp)), "foobar-d") == 0);
	psc_assert(strcmp(psc_dynarray_getpos(&da, psc_dynarray_bsearch(&da, "foobar-e", (void *)strcmp)), "foobar-e") == 0);
	psc_assert(strcmp(psc_dynarray_getpos(&da, psc_dynarray_bsearch(&da, "foobar-f", (void *)strcmp)), "foobar-f") == 0);
	psc_dynarray_splice(&da, 2, 1, NULL, 0);
	psc_assert(strcmp(psc_dynarray_getpos(&da, psc_dynarray_bsearch(&da, "foobar-b", (void *)strcmp)), "foobar-b") == 0);
	psc_assert(strcmp(psc_dynarray_getpos(&da, psc_dynarray_bsearch(&da, "foobar-c", (void *)strcmp)), "foobar-c") == 0);
	psc_assert(strcmp(psc_dynarray_getpos(&da, psc_dynarray_bsearch(&da, "foobar-e", (void *)strcmp)), "foobar-e") == 0);
	psc_assert(strcmp(psc_dynarray_getpos(&da, psc_dynarray_bsearch(&da, "foobar-f", (void *)strcmp)), "foobar-f") == 0);
	psc_dynarray_splice(&da, 3, 1, NULL, 0);
	psc_assert(strcmp(psc_dynarray_getpos(&da, psc_dynarray_bsearch(&da, "foobar-b", (void *)strcmp)), "foobar-b") == 0);
	psc_assert(strcmp(psc_dynarray_getpos(&da, psc_dynarray_bsearch(&da, "foobar-c", (void *)strcmp)), "foobar-c") == 0);
	psc_assert(strcmp(psc_dynarray_getpos(&da, psc_dynarray_bsearch(&da, "foobar-e", (void *)strcmp)), "foobar-e") == 0);
	psc_dynarray_splice(&da, 1, 1, NULL, 0);
	psc_assert(strcmp(psc_dynarray_getpos(&da, psc_dynarray_bsearch(&da, "foobar-b", (void *)strcmp)), "foobar-b") == 0);
	psc_assert(strcmp(psc_dynarray_getpos(&da, psc_dynarray_bsearch(&da, "foobar-e", (void *)strcmp)), "foobar-e") == 0);
	CHECK(&da, "foobar-b", "foobar-e", NULL);
	psc_dynarray_splice(&da, 0, 1, NULL, 0);
	psc_assert(strcmp(psc_dynarray_getpos(&da, psc_dynarray_bsearch(&da, "foobar-e", (void *)strcmp)), "foobar-e") == 0);
	CHECK(&da, "foobar-e", NULL);
	psc_dynarray_free(&da);

	psc_dynarray_init(&da);
	for (i = 0; i < 1025; i++) {
		uintptr_t foo = i;

		psc_dynarray_splice(&da, i, 0, &foo, 1);
		for (j = 0; j < i; j++) 
			psc_assert(psc_dynarray_getpos(&da, j) ==
			    (void *)(uintptr_t)j);
	}

	exit(0);
}
