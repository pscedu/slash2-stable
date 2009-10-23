/* $Id$ */

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "psc_ds/hash2.h"
#include "psc_util/alloc.h"
#include "pfl/cdefs.h"

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
	int c;

	if ((c = getopt(argc, argv, "")) != -1)
		usage();
	argc -= optind;
	if (argc)
		usage();

	psc_hashtbl_init(&t, 0, struct item,
	    id, hentry, 127, NULL, "t");

	i = PSCALLOC(sizeof(*i));
	i->id = 1;
	psc_hashtbl_add_item(&t, i);

	i = PSCALLOC(sizeof(*i));
	i->id = 2;
	psc_hashtbl_add_item(&t, i);

	i = PSCALLOC(sizeof(*i));
	i->id = 3;
	psc_hashtbl_add_item(&t, i);

	i = PSCALLOC(sizeof(*i));
	i->id = 4;
	psc_hashtbl_add_item(&t, i);

	i = psc_hashtbl_search(&t, NULL, NULL, 3);
	printf("%"PRId64"\n", i->id);

	exit(0);
}
