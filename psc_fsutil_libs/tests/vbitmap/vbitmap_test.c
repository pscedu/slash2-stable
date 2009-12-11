/* $Id$ */

#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "pfl/cdefs.h"
#include "pfl/pfl.h"
#include "psc_ds/vbitmap.h"
#include "psc_util/alloc.h"

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
			errx(1, "psc_vbitmap_next failed with auto");
		else if (j != (size_t)i)
			errx(1, "elem %d is not supposed to be %zu", i, j);

	if ((vb = psc_vbitmap_new(213)) == NULL)
		err(1, "psc_vbitmap_new");
	for (i = 0; i < 213; i++)
		if (!psc_vbitmap_next(vb, &elem))
			errx(1, "out of elements at pos %d", i);

	if (psc_vbitmap_next(vb, &elem))
		errx(1, "got another expected unused elem! %zu\n", elem);

	psc_vbitmap_getstats(vb, &u, &t);
	if (u != 213 || t != 213)
		errx(1, "wrong size, got %d,%d want %d", u, t, 213);

	if (psc_vbitmap_resize(vb, NELEM) == -1)
		err(1, "psc_vbitmap_new");

	/* fill up bitmap */
	for (; i < NELEM; i++)
		if (!psc_vbitmap_next(vb, &elem))
			errx(1, "out of elements at pos %d", i);

	/* try one past end of filled bitmap */
	if (psc_vbitmap_next(vb, &elem))
		errx(1, "got another expected unused elem! %zu\n", elem);

	/* free some slots */
	for (i = 0, elem = 0; elem < NELEM; i++, elem += NELEM / 10)
		psc_vbitmap_unset(vb, elem);

	t = psc_vbitmap_nfree(vb);
	if (t != i)
		errx(1, "wrong number of free elements, has=%d, want=%d", t, i);
	psc_vbitmap_invert(vb);
	t = psc_vbitmap_nfree(vb);
	if (t != NELEM - i)
		errx(1, "wrong number of inverted elements, has=%d, want=%d",
		    t, NELEM - i);
	psc_vbitmap_invert(vb);
	t = psc_vbitmap_nfree(vb);
	if (t != i)
		errx(1, "wrong number of original elements, has=%d, want=%d", t, i);

	/* try to re-grab the freed slots */
	for (i = 0; i <= 10; i++)
		if (!psc_vbitmap_next(vb, &elem))
			errx(1, "out of elements, request %d/%d", i, 10);

	/* try one past end of filled bitmap */
	if (psc_vbitmap_next(vb, &elem))
		errx(1, "got another expected unused elem! %zu\n", elem);

	psc_vbitmap_free(vb);
	exit(0);
}
