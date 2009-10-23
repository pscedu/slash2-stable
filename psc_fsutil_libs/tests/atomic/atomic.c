/* $Id$ */

#include <inttypes.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "pfl/pfl.h"
#include "psc_util/alloc.h"
#include "psc_util/atomic.h"
#include "psc_util/log.h"

struct thr {
	int pos;
};

pthread_barrier_t barrier;
const char *progname;

psc_atomic64_t v64 = PSC_ATOMIC64_INIT(100000000000ULL);
psc_atomic32_t v32 = PSC_ATOMIC32_INIT(0);
psc_atomic16_t v16 = PSC_ATOMIC16_INIT(0);
atomic_t v = ATOMIC_INIT(10);

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

void *
startf(void *arg)
{
	struct thr *thr = arg;
	int32_t ov, mask;
	int i;

	pthread_barrier_wait(&barrier);
	for (i = 0; i < 1024 * 1024 * 10; i++) {
		mask = 1 << (i % 4 + thr->pos * 4);
		ov = psc_atomic32_setmask_retold(&v32, mask);
		psc_assert((ov & mask) == 0);

		ov = psc_atomic32_clearmask_retold(&v32, mask);
		psc_assert(ov & mask);
	}
	return (NULL);
}

void
usage(void)
{
	fprintf(stderr, "usage: %s\n", progname);
	exit(1);
}

int
main(int argc, char *argv[])
{
	struct thr *thr;
	pthread_t pthrs[4];
	int rc, i;

	pfl_init();
	progname = argv[0];
	if (getopt(argc, argv, "") != -1)
		usage();
	argc -= optind;
	if (argc)
		usage();

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
	TEST(psc_atomic64, add, &v64, &v64, 15, 2000000000015ULL);
	TEST(psc_atomic64, sub, &v64, &v64, 9, 2000000000006ULL);
	TEST1(psc_atomic64, inc, &v64, 2000000000007ULL);
	TEST1(psc_atomic64, dec, &v64, 2000000000006ULL);

	TEST1(psc_atomic16, inc, &v16, 1);
	TEST1V(psc_atomic16, dec_test_zero, &v16, 0, 1);

	rc = pthread_barrier_init(&barrier, NULL, 4);
	if (rc)
		psc_fatalx("pthread_barrier_init: %s", strerror(rc));
	for (i = 0; i < 4; i++) {
		thr = PSCALLOC(sizeof(*thr));
		thr->pos = i;
		rc = pthread_create(&pthrs[i], NULL, startf, thr);
		if (rc)
			psc_fatalx("pthread_create: %s", strerror(rc));
	}
	for (i = 0; i < 4; i++) {
		rc = pthread_join(pthrs[i], NULL);
		if (rc)
			psc_fatalx("pthread_join: %s", strerror(rc));
	}
	exit(0);
}
