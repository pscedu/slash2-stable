/* $Id$ */
/*
 * %PSC_START_COPYRIGHT%
 * -----------------------------------------------------------------------------
 * Copyright (c) 2006-2009, Pittsburgh Supercomputing Center (PSC).
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

#include "pfl/cdefs.h"
#include "pfl/pfl.h"
#include "psc_ds/dynarray.h"

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

void
dump(struct psc_dynarray *da)
{
	void *p;
	int j;

	DYNARRAY_FOREACH(p, j, da)
		fprintf(stderr, "%p ", p);
	fprintf(stderr, "\n");
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

	p = PTR_4; psc_dynarray_splice(&da, 0, 0, &p, 1); dump(&da);
	p = PTR_3; psc_dynarray_splice(&da, 0, 0, &p, 1); dump(&da);
	p = PTR_2; psc_dynarray_splice(&da, 0, 0, &p, 1); dump(&da);
	p = PTR_1; psc_dynarray_splice(&da, 0, 0, &p, 1); dump(&da);

	exit(0);
}
