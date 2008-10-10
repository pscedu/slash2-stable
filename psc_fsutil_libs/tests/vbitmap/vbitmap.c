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

	if ((vb = vbitmap_new(NELEM)) == NULL)
		err(1, "vbitmap_new");

	/* fill up bitmap */
	for (i = 0; i < NELEM; i++)
		if (vbitmap_next(vb, &elem)) {
			if ((i % 1000) == 0)
				printf("got unused elem: %zu\n", elem);
		} else
			errx(1, "out of elements!");

	/* try one past end of filled bitmap */
	printf("expected to have filled up bitmap\n\n");
	if (vbitmap_next(vb, &elem))
		printf("got another expected unused elem! %zu\n", elem);
	else
		printf("rightfully out of elements\n");

	/* free some slots */
	printf("\nfreeing some slots\n");
	for (elem = 0; elem < NELEM; elem += NELEM / 10) {
		printf("putting back elem: %zu\n", elem);
		vbitmap_unset(vb, elem);
	}

	printf("\n");
	//vbitmap_printhex(vb);

	/* try to re-grab the freed slots */
	printf("\nregrabbing %d freed slots\n", vbitmap_nfree(vb));
	for (i = 0; i <= 10; i++)
		if (vbitmap_next(vb, &elem))
			printf("got unused elem: %zu\n", elem);
		else
			errx(1, "out of elements (%d/%d)!", i, 10);

	/* try one past end of filled bitmap */
	printf("expected to have filled up bitmap\n\n");
	if (vbitmap_next(vb, &elem))
		printf("got another expected unused elem! %zu\n", elem);
	else
		printf("rightfully out of elements\n");

	vbitmap_free(vb);
	exit(0);
}
