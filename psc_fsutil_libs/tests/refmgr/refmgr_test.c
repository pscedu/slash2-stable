/* $Id$ */
/*
 * %PSC_START_COPYRIGHT%
 * -----------------------------------------------------------------------------
 * Copyright (c) 2006-2011, Pittsburgh Supercomputing Center (PSC).
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
#include "psc_util/atomic.h"
#include "psc_util/refmgr.h"

const char		*progname;
struct psc_refmgr	 refmgr;
psc_atomic32_t		 c = PSC_ATOMIC32_INIT(0);

struct obj {
	int	obj_val;
};

__dead void
usage(void)
{
	fprintf(stderr, "usage: %s\n", progname);
	exit(1);
}

int
init_obj(__unusedx struct psc_poolmgr *m, void *p)
{
	struct obj *o = p;

	o->obj_val = psc_atomic32_inc_getnew(&c);
	printf("created obj %d\n", o->obj_val);
	return (0);
}

void
destroy_obj(void *p)
{
	struct obj *o = p;

	printf("destroy obj %d\n", o->obj_val);
}

int
main(int argc, char *argv[])
{
	struct obj *o;

	progname = argv[0];
	if (getopt(argc, argv, "") != -1)
		usage();
	argc -= optind;
	if (argc)
		usage();

	psc_refmgr_init(&refmgr, PRMF_LIST, sizeof(*o),
	    128, 64, 256, init_obj, destroy_obj, "test");
	exit(0);
}
