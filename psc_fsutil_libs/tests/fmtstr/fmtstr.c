/* $Id$ */

#include <limits.h>
#include <stdio.h>
#include <unistd.h>

#include "fmtstr.h"
#include "cdefs.h"

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
	char b[LINE_MAX];
	int ch;

	progname = argv[0];
	while ((ch = getopt(argc, argv, "")) != -1)
		switch (ch) {
		default:
			usage();
		}

	FMTSTR(b, sizeof(b), "[<%a>:<%b>:<%c>]",
		FMTSTRCASE('a', b, sizeof(b), "d", 1)
		FMTSTRCASE('b', b, sizeof(b), "s", "foobar")
		FMTSTRCASE('c', b, sizeof(b), "lu", 0UL)
	);

	printf("%s\n", b);
	exit(0);
}
