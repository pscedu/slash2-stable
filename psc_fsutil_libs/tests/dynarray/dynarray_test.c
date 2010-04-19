/* $Id$ */
/*
 * %PSC_START_COPYRIGHT%
 * -----------------------------------------------------------------------------
 * Copyright (c) 2006-2010, Pittsburgh Supercomputing Center (PSC).
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

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "pfl/cdefs.h"
#include "pfl/pfl.h"
#include "psc_ds/dynarray.h"
#include "psc_util/log.h"

const char *progname;

__dead void
usage(void)
{
	fprintf(stderr, "usage: %s\n", progname);
	exit(1);
}

#define PTR_1	((void *)0x01)
#define PTR_2	((void *)0x02)
#define PTR_3	((void *)0x03)
#define PTR_4	((void *)0x04)

#define PTR_a	((void *)0x0a)
#define PTR_b	((void *)0x0b)
#define PTR_c	((void *)0x0c)

void
check(struct psc_dynarray *da, ...)
{
	void *p, *checkp;
	va_list ap;
	int j;

	va_start(ap, da);
	DYNARRAY_FOREACH(p, j, da) {
		checkp = va_arg(ap, void *);
		psc_assert(p == checkp);
//		printf("%p ", p);
}
	va_end(ap);
//	printf("\n");
}

int
main(int argc, char *argv[])
{
	struct psc_dynarray da = DYNARRAY_INIT;
	void *p;

	pfl_init();
	progname = argv[0];
	if (getopt(argc, argv, "") != -1)
		usage();
	argc -= optind;
	if (argc)
		usage();


	p = PTR_4; psc_dynarray_splice(&da, 0, 0, &p, 1); check(&da, PTR_4, NULL);
	p = PTR_3; psc_dynarray_splice(&da, 0, 0, &p, 1); check(&da, PTR_3, PTR_4, NULL);
	p = PTR_2; psc_dynarray_splice(&da, 0, 0, &p, 1); check(&da, PTR_2, PTR_3, PTR_4, NULL);
	p = PTR_1; psc_dynarray_splice(&da, 0, 0, &p, 1); check(&da, PTR_1, PTR_2, PTR_3, PTR_4, NULL);

	psc_dynarray_splice(&da, 0, 0, NULL, 0); check(&da, PTR_1, PTR_2, PTR_3, PTR_4, NULL);

	p = PTR_a; psc_dynarray_splice(&da, 2, 1, &p, 1); check(&da, PTR_1, PTR_2, PTR_a, PTR_4, NULL);
	p = PTR_b; psc_dynarray_splice(&da, 3, 0, &p, 1); check(&da, PTR_1, PTR_2, PTR_a, PTR_b, PTR_4, NULL);

	exit(0);
}
