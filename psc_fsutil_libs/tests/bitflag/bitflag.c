/* $Id: random.c 4118 2008-09-09 17:59:03Z yanovich $ */

#include <sys/types.h>
#include <sys/wait.h>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "psc_util/bitflag.h"

const char *progname;

#define NENTS 5000
int buf[NENTS];

__dead void
usage(void)
{
	fprintf(stderr, "usage: %s\n", progname);
	exit(1);
}

#define CNT_ASSERT(code, xcode)				\
	switch (fork()) {				\
	case -1:					\
		psc_fatal("fork");			\
	case 0: /* child */				\
		code;					\
		fprintf(stdout, "code completed\n");	\
		exit(0);				\
	default: /* parent */				\
		if (wait(&st) == -1)			\
			psc_fatal("wait");		\
		if (!(xcode))				\
			psc_fatalx("want %s, got %d",	\
			    # xcode, st);		\
		break;					\
	}

#define CNT_ASSERT0(code)	CNT_ASSERT(code, st == 0)
#define CNT_ASSERTA(code)	CNT_ASSERT(code, WCOREDUMP(st))

#define B0	(1 << 0)
#define B1	(1 << 1)
#define B2	(1 << 2)
#define B3	(1 << 3)
#define B4	(1 << 4)
#define B5	(1 << 5)
#define B6	(1 << 6)
#define B7	(1 << 7)

int
main(int argc, char *argv[])
{
	int st, f, c;

	progname = argv[0];
	while ((c = getopt(argc, argv, "")) != -1)
		switch (c) {
		default:
			usage();
		}
	argc -= optind;
	if (argc)
		usage();

	f = 0;
	CNT_ASSERT0(f = B1; psc_assert(bitflag_sorc(&f, NULL, B1, 0, 0, 0, 0) == 0));
	CNT_ASSERT0(f = B1; psc_assert(bitflag_sorc(&f, NULL, 0, B1, 0, 0, 0) != 0));
	CNT_ASSERT0(f = B2; psc_assert(bitflag_sorc(&f, NULL, B2, B3, 0, 0, 0) == 0));

	CNT_ASSERT0(psc_assert(bitflag_sorc(&f, NULL, 0, 0, B1, 0, BIT_STRICT) == 0);
		    psc_assert(bitflag_sorc(&f, NULL, 0, 0, B1, 0, BIT_STRICT) != 0);
		    psc_assert(bitflag_sorc(&f, NULL, B1|B2, 0, 0, 0, BIT_STRICT) != 0);
		    psc_assert(bitflag_sorc(&f, NULL, 0, 0, B2, 0, BIT_STRICT) == 0);
		    psc_assert(bitflag_sorc(&f, NULL, B1|B2, 0, 0, 0, BIT_STRICT) == 0);
		    psc_assert(bitflag_sorc(&f, NULL, B1, 0, B3, 0, BIT_STRICT) == 0);
		    psc_assert(f==(B1|B2|B3));
		    psc_assert(bitflag_sorc(&f, NULL, B3, 0, 0, B2, BIT_STRICT) == 0);
		    psc_assert(f==(B1|B3));
		    );

	CNT_ASSERT0(psc_assert(bitflag_sorc(&f, NULL, 0, 0, B6, 0, 0) == 0); psc_assert(f == B6));

	exit(0);
}
