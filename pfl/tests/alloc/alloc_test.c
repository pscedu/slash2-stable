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

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "pfl/alloc.h"
#include "pfl/cdefs.h"
#include "pfl/log.h"

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
	size_t sz;
	void *p;

	pfl_init();
	if (getopt(argc, argv, "") != -1)
		usage();
	argc -= optind;
	if (argc)
		usage();

	p = PSCALLOC(213);
	p = psc_realloc(p, 65536, 0);
	p = psc_realloc(p, 0, 0);
	p = psc_realloc(p, 128, 0);
	p = psc_realloc(p, 0, 0);
	PSCFREE(p);

	p = PSCALLOC(128);
	PSCFREE(p);

	p = psc_alloc(24, PAF_PAGEALIGN);
	psc_free(p, PAF_PAGEALIGN);

	p = PSCALLOC(24);
	p = psc_realloc(p, 128, 0);
	PSCFREE(p);

	p = psc_alloc(8, PAF_LOCK);
	*(uint64_t *)p = 0;
	psc_free(p, PAF_LOCK, (size_t)8);

	sz = 1024;
	p = psc_alloc(sz, PAF_LOCK | PAF_PAGEALIGN);
	memset(p, 0, sz);
	psc_free(p, PAF_LOCK | PAF_PAGEALIGN, sz);

	exit(0);
}
