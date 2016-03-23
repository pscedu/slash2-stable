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

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "pfl/alloc.h"
#include "pfl/cdefs.h"
#include "pfl/hashtbl.h"
#include "pfl/pfl.h"

struct item {
	struct pfl_hashentry	hentry;
	uint64_t		id;
};

__dead void
usage(void)
{
	extern const char *__progname;

	fprintf(stderr, "usage: %s\n", __progname);
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
	    id, hentry, 97, NULL, "t");

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
	i = psc_hashtbl_search(&t, &key);
	printf("%"PRId64"\n", i->id);

	psc_hashtbl_resize(&t, 191);

	key = 3;
	i = psc_hashtbl_search(&t, &key);
	printf("%"PRId64"\n", i->id);

	exit(0);
}
