/* $Id$ */

#include <inttypes.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "pfl/pfl.h"
#include "pfl/cdefs.h"
#include "psc_util/alloc.h"
#include "psc_util/atomic.h"
#include "psc_util/log.h"

struct thr {
	int pos;
};

int nthr = 4;
int niter = 1024 * 1024 * 2;
pthread_barrier_t barrier;
const char *progname;

psc_atomic64_t v64 = PSC_ATOMIC_INIT(UINT64_C(100000000000));
psc_atomic32_t v32 = PSC_ATOMIC_INIT(0);
psc_atomic16_t v16 = PSC_ATOMIC_INIT(0);

#define CHECKV(op, v, newval)						\
	if (psc_atomic_read(v) != (newval))				\
		psc_fatalx("psc_atomic_" #op ": got %"PRId64", "	\
		    "wanted %"PRId64,					\
		    (int64_t)psc_atomic_read(v),			\
		    (int64_t)(newval))					\

#define TEST(op, v, arg1, arg2, newval)					\
	do {								\
		psc_atomic_ ## op((arg1), (arg2));			\
		CHECKV(op, v, newval);					\
	} while (0)

#define TEST1V(op, v, newval, rv)					\
	do {								\
		if (psc_atomic_ ## op(v) != (rv))			\
			psc_fatalx("psc_atomic_" #op ": want rv %d",	\
			    (rv));					\
		CHECKV(op, v, newval);					\
	} while (0)

#define TEST1(op, v, newval)						\
	do {								\
		psc_atomic_ ## op(v);					\
		CHECKV(op, v, newval);					\
	} while (0)

void *
startf(void *arg)
{
	struct thr *thr = arg;
	int32_t ov, mask;
	int i;

	pthread_barrier_wait(&barrier);
	mask = 1 << thr->pos;
	for (i = 0; i < niter; i++) {
		ov = psc_atomic_setmask_getold(&v32, mask);
		psc_assert((ov & mask) == 0);

		ov = psc_atomic_clearmask_getold(&v32, mask);
		psc_assert(ov & mask);
		sched_yield();
	}
	pthread_barrier_wait(&barrier);
	return (NULL);
}

__dead void
usage(void)
{
	fprintf(stderr, "usage: %s [-i niter] [-n nthr]\n", progname);
	exit(1);
}

int
main(int argc, char *argv[])
{
	struct thr *thr;
	pthread_t pthr;
	int c, rc, i;

	pfl_init();
	progname = argv[0];
	while ((c = getopt(argc, argv, "i:n:")) != -1)
		switch (c) {
		case 'i':
			niter = atoi(optarg);
			break;
		case 'n':
			nthr = atoi(optarg);
			break;
		default:
			usage();
		}
	argc -= optind;
	if (argc)
		usage();

	psc_assert(psc_atomic_read(&v32) == 10);
	TEST(set, &v32, &v32, 200, 200);
	TEST(add, &v32, &v32, 15, 215);
	TEST(sub, &v32, &v32, 9, 206);
	TEST1(inc, &v32, 207);
	TEST1(dec, &v32, 206);
	TEST1V(dec_and_test0, &v32, 205, 0);
	TEST(set, &v32, &v32, 1, 1);
	TEST1V(dec_and_test0, &v32, 0, 1);
	TEST(setmask, &v32, &v32, 0x75, 0x75);
	TEST(clearmask, &v32, &v32, 0x41, 0x34);

	psc_assert(psc_atomic_read(&v64) == UINT64_C(100000000000));
	TEST(set, &v64, &v64, UINT64_C(2000000000000), UINT64_C(2000000000000));
	TEST(add, &v64, &v64, 15, UINT64_C(2000000000015));
	TEST(sub, &v64, &v64, 9, UINT64_C(2000000000006));
	TEST1(inc, &v64, UINT64_C(2000000000007));
	TEST1(dec, &v64, UINT64_C(2000000000006));

	TEST1(inc, &v16, 1);
	TEST1V(dec_and_test0, &v16, 0, 1);

	rc = pthread_barrier_init(&barrier, NULL, nthr + 1);
	if (rc)
		psc_fatalx("pthread_barrier_init: %s", strerror(rc));
	for (i = 0; i < nthr; i++) {
		thr = PSCALLOC(sizeof(*thr));
		thr->pos = i;
		rc = pthread_create(&pthr, NULL, startf, thr);
		if (rc)
			psc_fatalx("pthread_create: %s", strerror(rc));
	}
	pthread_barrier_wait(&barrier);
	pthread_barrier_wait(&barrier);
	exit(0);
}
