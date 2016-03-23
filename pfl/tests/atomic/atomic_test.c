/* $Id$ */
/*
 * %ISC_START_LICENSE%
 * ---------------------------------------------------------------------
 * Copyright 2015-2016, Google, Inc.
 * Copyright (c) 2006-2015, Pittsburgh Supercomputing Center (PSC).
 * All rights reserved.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the
 * above copyright notice and this permission notice appear in all
 * copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL
 * WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS.  IN NO EVENT SHALL THE
 * AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
 * PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 * --------------------------------------------------------------------
 * %END_LICENSE%
 */

#include <inttypes.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "pfl/alloc.h"
#include "pfl/atomic.h"
#include "pfl/cdefs.h"
#include "pfl/log.h"
#include "pfl/pfl.h"
#include "pfl/pthrutil.h"

struct thr {
	int pos;
};

int nthr = 4;
int niter = 1024 * 1024 * 2;
pthread_barrier_t barrier;

psc_atomic64_t v64 = PSC_ATOMIC64_INIT(UINT64_C(100000000000));
psc_atomic32_t v32 = PSC_ATOMIC32_INIT(0);
psc_atomic16_t v16 = PSC_ATOMIC16_INIT(0);

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
			psc_fatalx(#prefix "_" #op ": "			\
			    "want rv %d", (rv));			\
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
	mask = 1 << thr->pos;
	for (i = 0; i < niter; i++) {
		ov = psc_atomic32_setmask_getold(&v32, mask);
		psc_assert((ov & mask) == 0);

		ov = psc_atomic32_clearmask_getold(&v32, mask);
		psc_assert(ov & mask);
		sched_yield();
	}
	pthread_barrier_wait(&barrier);
	return (NULL);
}

__dead void
usage(void)
{
	extern const char *__progname;

	fprintf(stderr, "usage: %s [-i niter] [-n nthr]\n", __progname);
	exit(1);
}

int
main(int argc, char *argv[])
{
	struct thr *thr;
	pthread_t pthr;
	int c, rc, i;

	pfl_init();
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

	psc_assert(psc_atomic64_read(&v64) == UINT64_C(100000000000));
	TEST(psc_atomic64, set, &v64, &v64, UINT64_C(2000000000000), UINT64_C(2000000000000));
	TEST(psc_atomic64, add, &v64, &v64, 15, UINT64_C(2000000000015));
	TEST(psc_atomic64, sub, &v64, &v64, 9, UINT64_C(2000000000006));
	TEST1(psc_atomic64, inc, &v64, UINT64_C(2000000000007));
	TEST1(psc_atomic64, dec, &v64, UINT64_C(2000000000006));

	psc_atomic16_set(&v16, 2);
	TEST(psc_atomic16, set, &v16, &v16, 200, 200);
	TEST(psc_atomic16, add, &v16, &v16, 15, 215);
	TEST(psc_atomic16, sub, &v16, &v16, 9, 206);
	TEST1(psc_atomic16, inc, &v16, 207);
	TEST1(psc_atomic16, dec, &v16, 206);
	TEST1V(psc_atomic16, dec_and_test0, &v16, 205, 0);
	TEST(psc_atomic16, set, &v16, &v16, 1, 1);
	TEST1V(psc_atomic16, dec_and_test0, &v16, 0, 1);
	TEST(psc_atomic16, setmask, &v16, &v16, 0x75, 0x75);
	TEST(psc_atomic16, clearmask, &v16, &v16, 0x41, 0x34);
	TEST(psc_atomic16, set, &v16, &v16, 0, 0);

	psc_atomic32_set(&v32, 2);
	TEST(psc_atomic32, set, &v32, &v32, 200, 200);
	TEST(psc_atomic32, add, &v32, &v32, 15, 215);
	TEST(psc_atomic32, sub, &v32, &v32, 9, 206);
	TEST1(psc_atomic32, inc, &v32, 207);
	TEST1(psc_atomic32, dec, &v32, 206);
	TEST1V(psc_atomic32, dec_and_test0, &v32, 205, 0);
	TEST(psc_atomic32, set, &v32, &v32, 1, 1);
	TEST1V(psc_atomic32, dec_and_test0, &v32, 0, 1);
	TEST(psc_atomic32, setmask, &v32, &v32, 0x75, 0x75);
	TEST(psc_atomic32, clearmask, &v32, &v32, 0x41, 0x34);
	TEST(psc_atomic32, set, &v32, &v32, 0, 0);

	psc_atomic64_set(&v64, 2);
	TEST(psc_atomic64, set, &v64, &v64, 200, 200);
	TEST(psc_atomic64, add, &v64, &v64, 15, 215);
	TEST(psc_atomic64, sub, &v64, &v64, 9, 206);
	TEST1(psc_atomic64, inc, &v64, 207);
	TEST1(psc_atomic64, dec, &v64, 206);
	TEST1V(psc_atomic64, dec_and_test0, &v64, 205, 0);
	TEST(psc_atomic64, set, &v64, &v64, 1, 1);
	TEST1V(psc_atomic64, dec_and_test0, &v64, 0, 1);
	TEST(psc_atomic64, setmask, &v64, &v64, 0x75, 0x75);
	TEST(psc_atomic64, clearmask, &v64, &v64, 0x41, 0x34);
	TEST(psc_atomic64, set, &v64, &v64, 0, 0);

	TEST1(psc_atomic16, inc, &v16, 1);
	TEST1V(psc_atomic16, dec_and_test0, &v16, 0, 1);

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
