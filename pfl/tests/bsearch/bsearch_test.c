/* $Id$ */
/* %ISC_LICENSE% */

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
