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

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "pfl/atomic.h"
#include "pfl/cdefs.h"
#include "pfl/refmgr.h"

struct psc_refmgr	 refmgr;
psc_atomic32_t		 c = PSC_ATOMIC32_INIT(0);

struct obj {
	int	obj_val;
};

__dead void
usage(void)
{
	extern const char *__progname;

	fprintf(stderr, "usage: %s\n", __progname);
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

	pfl_init();
	if (getopt(argc, argv, "") != -1)
		usage();
	argc -= optind;
	if (argc)
		usage();

	psc_refmgr_init(&refmgr, PRMF_LIST, sizeof(*o),
	    128, 64, 256, init_obj, destroy_obj, "test");
	exit(0);
}
