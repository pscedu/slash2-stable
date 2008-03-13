/* $Id$ */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "psc_util/random.h"
#include "psc_util/cdefs.h"

const char *progname;

#define NENTS 5000
int buf[NENTS];

__dead void
usage(void)
{
	fprintf(stderr, "usage: %s\n", progname);
	exit(1);
}

int
main(int argc, char *argv[])
{
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

	printf("%u\n", psc_random32());
	printf("%016"_P_U64"x\n", psc_random64());

	for (i = 0; i < 500000; i++)
		buf[psc_random32u(NENTS)]++;
	for (i = 0; i < NENTS; i++)
		printf("%d\t%d\n", i+1, buf[i]);
	exit(0);
}
