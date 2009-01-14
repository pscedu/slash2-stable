/* $Id: typedump.c 4897 2009-01-14 17:14:28Z yanovich $ */

#include <sys/param.h>

#include <err.h>
#include <stdio.h>
#include <unistd.h>

#include "psc_util/cdefs.h"
#include "psc_util/ctl.h"

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
	int c;

	progname = argv[0];
	while ((c = getopt(argc, argv, "")) != -1)
		switch (c) {
		default:
			usage();
		}
	argc -= optind;
	if (argc)
		usage();

#define PRTYPE(type) \
	printf("%-24s %zu\n", #type, sizeof(type))

#define PRVAL(val) \
	printf("%-24s %lu\n", #val, (unsigned long)(val))

	/* C types/values */
	PRTYPE(int);
	PRTYPE(unsigned char);
	PRTYPE(unsigned short);
	PRTYPE(unsigned char *);
	PRTYPE(uint8_t);
	PRTYPE(uint16_t);
	PRTYPE(uint32_t);
	PRTYPE(uint64_t);
	PRTYPE(INT_MAX);

	/* system types/values */
	PRVAL(PATH_MAX);

	/* PFL types/values */
	PRTYPE(struct psc_ctlmsghdr);

	exit(0);
}
