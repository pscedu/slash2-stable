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

#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "pfl/cdefs.h"
#include "pfl/pfl.h"
#include "psc_ds/vbitmap.h"
#include "psc_util/alloc.h"
#include "psc_util/log.h"

const char *progname;

#define NELEM 524288	/* # of 2MB blocks in 1TG. */

__dead void
usage(void)
{
	fprintf(stderr, "usage: %s\n", progname);
	exit(1);
}

int
main(int argc, char *argv[])
{
	struct psc_vbitmap *vb, vba = VBITMAP_INIT_AUTO;
	int i, c, u, t;
	size_t elem, j;

	pfl_init();
	progname = argv[0];
	while ((c = getopt(argc, argv, "")) != -1)
		switch (c) {
		default:
			usage();
		}

	argc -= optind;
	if (argc)
		usage();

	for (i = 0; i < 79; i++)
		if (psc_vbitmap_next(&vba, &j) != 1)
			psc_fatalx("psc_vbitmap_next failed with auto");
		else if (j != (size_t)i)
			psc_fatalx("elem %d is not supposed to be %zu", i, j);

	if ((vb = psc_vbitmap_new(213)) == NULL)
		psc_fatal("psc_vbitmap_new");

	psc_vbitmap_setrange(vb, 13, 9);
	psc_vbitmap_printbin1(vb);
	for (i = 0; i < 13; i++)
		psc_assert(psc_vbitmap_get(vb, i) == 0);
	for (j = 0; j < 9; j++, i++)
	    psc_assert(psc_vbitmap_get(vb, i) == 1);
	for (j = 0; j < 25; j++, i++)
	    psc_assert(psc_vbitmap_get(vb, i) == 0);

	psc_vbitmap_clearall(vb);
	for (i = 0; i < 213; i++)
	    psc_assert(psc_vbitmap_get(vb, i) == 0);

	psc_vbitmap_setrange(vb, 25, 3);
	for (i = 0; i < 25; i++)
		psc_assert(psc_vbitmap_get(vb, i) == 0);
	for (j = 0; j < 3; j++, i++)
	    psc_assert(psc_vbitmap_get(vb, i) == 1);
	for (j = 0; j < 25; j++, i++)
	    psc_assert(psc_vbitmap_get(vb, i) == 0);

	psc_vbitmap_clearall(vb);
	for (i = 0; i < 213; i++)
	    psc_assert(psc_vbitmap_get(vb, i) == 0);

	for (i = 0; i < 213; i++)
		if (!psc_vbitmap_next(vb, &elem))
			psc_fatalx("out of elements at pos %d", i);

	if (psc_vbitmap_next(vb, &elem))
		psc_fatalx("an unexpected extra unused elem was found; pos=%zu", elem);

	psc_vbitmap_getstats(vb, &u, &t);
	if (u != 213 || t != 213)
		psc_fatalx("wrong size, got %d,%d want %d", u, t, 213);

	psc_vbitmap_unsetrange(vb, 13, 2);
	for (i = 0; i < 13; i++)
		psc_assert(psc_vbitmap_get(vb, i) == 1);
	for (j = 0; j < 2; j++, i++)
	    psc_assert(psc_vbitmap_get(vb, i) == 0);
	for (j = 0; j < 25; j++, i++)
	    psc_assert(psc_vbitmap_get(vb, i) == 1);

	if (psc_vbitmap_resize(vb, NELEM) == -1)
		psc_fatal("psc_vbitmap_new");

	psc_assert(psc_vbitmap_getsize(vb) == NELEM);

	/* fill up bitmap */
	for (i = 0; i < NELEM - 211; i++)
		if (!psc_vbitmap_next(vb, &elem))
			psc_fatalx("out of elements at iter %d", i);

	/* try one past end of filled bitmap */
	if (psc_vbitmap_next(vb, &elem))
		psc_fatalx("an unexpected extra unused elem was found; pos=%zu", elem);

	/* free some slots */
	for (i = 0, elem = 0; elem < NELEM; i++, elem += NELEM / 10)
		psc_vbitmap_unset(vb, elem);

	t = psc_vbitmap_nfree(vb);
	if (t != i)
		psc_fatalx("wrong number of free elements; has=%d want=%d", t, i);
	psc_vbitmap_invert(vb);
	t = psc_vbitmap_nfree(vb);
	if (t != NELEM - i)
		psc_fatalx("wrong number of inverted elements; has=%d want=%d",
		    t, NELEM - i);
	psc_vbitmap_invert(vb);
	t = psc_vbitmap_nfree(vb);
	if (t != i)
		psc_fatalx("wrong number of original elements; has=%d want=%d", t, i);

	/* try to re-grab the freed slots */
	for (i = 0; i <= 10; i++)
		if (!psc_vbitmap_next(vb, &elem))
			psc_fatalx("out of elements, request %d/%d", i, 10);

	/* try one past end of filled bitmap */
	if (psc_vbitmap_next(vb, &elem))
		psc_fatalx("an unexpected extra unused elem was found; pos=%zu", elem);

	psc_vbitmap_free(vb);
	exit(0);
}
