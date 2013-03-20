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

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "pfl/cdefs.h"
#include "pfl/hashtbl.h"
#include "pfl/pfl.h"
#include "psc_util/alloc.h"

struct item {
	struct psc_hashent	hentry;
	uint64_t		id;
};

const char *progname;

__dead void
usage(void)
{
	fprintf(stderr, "usage: %s\n", progname);
	exit(1);
}

int
main(int argc, char *argv[])
{
	struct psc_hashtbl t;
	struct item *i;
	uint64_t key;
	int c;

	pfl_init();
	if ((c = getopt(argc, argv, "")) != -1)
		usage();
	argc -= optind;
	if (argc)
		usage();

	psc_hashtbl_init(&t, 0, struct item,
	    id, hentry, 127, NULL, "t");

	i = PSCALLOC(sizeof(*i));
	psc_hashent_init(&t, i);
	i->id = 1;
	psc_hashtbl_add_item(&t, i);

	i = PSCALLOC(sizeof(*i));
	psc_hashent_init(&t, i);
	i->id = 2;
	psc_hashtbl_add_item(&t, i);

	i = PSCALLOC(sizeof(*i));
	psc_hashent_init(&t, i);
	i->id = 3;
	psc_hashtbl_add_item(&t, i);

	i = PSCALLOC(sizeof(*i));
	psc_hashent_init(&t, i);
	i->id = 4;
	psc_hashtbl_add_item(&t, i);

	key = 3;
	i = psc_hashtbl_search(&t, NULL, NULL, &key);
	printf("%"PRId64"\n", i->id);

	exit(0);
}
