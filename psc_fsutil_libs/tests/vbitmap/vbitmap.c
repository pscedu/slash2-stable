/* $Id$ */

#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "psc_ds/vbitmap.h"
#include "psc_util/cdefs.h"
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
	struct vbitmap *vb;
	size_t elem;
	int i, c;

	progname = argv[0];
	while ((c = getopt(argc, argv, "")) != -1)
		switch (c) {
		default:
			usage();
		}

	argc -= optind;
	if (argc)
		usage();

	if ((vb = vbitmap_new(213)) == NULL)
		err(1, "vbitmap_new");
	for (i = 0; i < 213; i++)
		if (!vbitmap_next(vb, &elem))
			errx(1, "out of elements at pos %d", i);

	if (vbitmap_next(vb, &elem))
		errx(1, "got another expected unused elem! %zu\n", elem);

	if (vbitmap_resize(vb, NELEM) == -1)
		err(1, "vbitmap_new");

	/* fill up bitmap */
	for (; i < NELEM; i++)
		if (!vbitmap_next(vb, &elem))
			errx(1, "out of elements at pos %d", i);

	/* try one past end of filled bitmap */
	if (vbitmap_next(vb, &elem))
		errx(1, "got another expected unused elem! %zu\n", elem);

	/* free some slots */
	for (elem = 0; elem < NELEM; elem += NELEM / 10)
		vbitmap_unset(vb, elem);

	/* try to re-grab the freed slots */
	for (i = 0; i <= 10; i++)
		if (!vbitmap_next(vb, &elem))
			errx(1, "out of elements, request %d/%d", i, 10);

	/* try one past end of filled bitmap */
	if (vbitmap_next(vb, &elem))
		errx(1, "got another expected unused elem! %zu\n", elem);

	vbitmap_free(vb);
	exit(0);
}
