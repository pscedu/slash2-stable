/* $Id$ */

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "psc_util/assert.h"
#include "psc_util/atomic.h"

const char *progname;

#define CHECKV(prefix, op, v, newval)					\
	if (prefix ## _read(v) != (newval))				\
		psc_fatalx(#prefix "_" #op ": got %"PRId64", "		\
		    "wanted %"PRId64,					\
		    (int64_t)prefix ## _read(v),			\
		    (int64_t)(newval))					\

#define TEST(prefix, op, v, arg1, arg2, newval)				\
	do {								\
		prefix ## _ ## op((arg1), (arg2));			\
		CHECKV(prefix, op, v, newval);				\
	} while (0)

#define TEST1V(prefix, op, v, newval, rv)				\
	do {								\
		if (prefix ## _ ## op(v) != (rv))			\
			psc_fatalx(#prefix "_" #op ": want rv %d",	\
			    (rv));					\
		CHECKV(prefix, op, v, newval);				\
	} while (0)

#define TEST1(prefix, op, v, newval)					\
	do {								\
		prefix ## _ ## op(v);					\
		CHECKV(prefix, op, v, newval);				\
	} while (0)

void
usage(void)
{
	fprintf(stderr, "usage: %s\n", progname);
	exit(1);
}

int
main(int argc, char *argv[])
{
	progname = argv[0];
	if (getopt(argc, argv, "") != -1)
		usage();
	argc -= optind;
	if (argc)
		usage();

	psc_atomic64_t v64 = PSC_ATOMIC64_INIT(100000000000ULL);
	atomic_t v = ATOMIC_INIT(10);

	psc_assert(atomic_read(&v) == 10);
	TEST(atomic, set, &v, &v, 200, 200);
	TEST(atomic, add, &v, 15, &v, 215);
	TEST(atomic, sub, &v, 9, &v, 206);
	TEST1(atomic, inc, &v, 207);
	TEST1(atomic, dec, &v, 206);
	TEST1V(atomic, dec_and_test, &v, 205, 0);
	TEST(atomic, set, &v, &v, 1, 1);
	TEST1V(atomic, dec_and_test, &v, 0, 1);
	TEST(atomic, set_mask, &v, 0x75, &v, 0x75);
	TEST(atomic, clear_mask, &v, 0x41, &v, 0x34);

	psc_assert(psc_atomic64_read(&v64) == 100000000000ULL);
	TEST(psc_atomic64, set, &v64, &v64, 2000000000000ULL, 2000000000000ULL);
	TEST(psc_atomic64, add, &v64, 15, &v64, 2000000000015ULL);
	TEST(psc_atomic64, sub, &v64, 9, &v64, 2000000000006ULL);
	TEST1(psc_atomic64, inc, &v64, 2000000000007ULL);
	TEST1(psc_atomic64, dec, &v64, 2000000000006ULL);

	exit(0);
}
