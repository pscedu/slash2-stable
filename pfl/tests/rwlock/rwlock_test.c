/* $Id$ */
/*
 * %ISC_START_LICENSE%
 * ---------------------------------------------------------------------
 * Copyright 2015-2016, Google, Inc.
 * Copyright (c) 2009-2015, Pittsburgh Supercomputing Center (PSC).
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

#include <sys/types.h>
#include <sys/time.h>

#include <err.h>
#include <errno.h>
#include <pthread.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "pfl/cdefs.h"
#include "pfl/pfl.h"
#include "pfl/list.h"
#include "pfl/alloc.h"
#include "pfl/atomic.h"
#include "pfl/log.h"
#include "pfl/pthrutil.h"
#include "pfl/random.h"

#define NTHRS_MAX	32
#define NLOCKS_MAX	8192
#define SLEEP_US	10

struct thr {
	struct psclist_head lentry;
	char name[40];
	int st;
};

int nlocks = 2000;
int nrd = 8;
int nwr = 3;

struct pfl_rwlock rw = pfl_rwlock_INIT;
struct psclist_head thrs = PSCLIST_HEAD_INIT(thrs);

__dead void
usage(void)
{
	extern const char *__progname;

	fprintf(stderr, "usage: %s [-n locks] [-r readers] [-w writers]\n", 
	    __progname);
	exit(1);
}

void *
rd_main(void *arg)
{
	struct thr *thr = arg;

	for (; thr->st < nlocks; thr->st++) {
//		do {
//			rc = pthread_rwlock_tryrdlock(&rw);
//			if (rc)
//				usleep(1);
//		} while (rc);
		pfl_rwlock_rdlock(&rw);
//		rc = pthread_rwlock_rdlock(&rw);
//		if (rc != EBUSY)
//			errx(1, "rdlock: %s", strerror(rc));

		usleep(SLEEP_US);
		pfl_rwlock_unlock(&rw);
		sched_yield();
	}
	return (NULL);
}

void *
wr_main(void *arg)
{
	struct thr *thr = arg;
	struct timespec ts;
	int rc;

	for (; thr->st < nlocks; thr->st++) {
		if (psc_random32u(10) == 3) {
			pfl_rwlock_rdlock(&rw);
			usleep(SLEEP_US);
			pfl_rwlock_unlock(&rw);
		}
		pfl_rwlock_wrlock(&rw);

		rc = pthread_rwlock_tryrdlock(&rw.pr_rwlock);
		if (rc != EBUSY)
			psc_fatalx("rdlock: %s", strerror(rc));

		rc = pthread_rwlock_rdlock(&rw.pr_rwlock);
		if (rc != EDEADLK)
			psc_fatalx("rdlock: %s", strerror(rc));

		rc = pthread_rwlock_wrlock(&rw.pr_rwlock);
		if (rc != EDEADLK)
			psc_fatalx("wrlock: %s", strerror(rc));

#ifdef HAVE_PTHREAD_RWLOCK_TIMEDRDLOCK
		memset(&ts, 0, sizeof(ts));
		rc = pthread_rwlock_timedwrlock(&rw.pr_rwlock, &ts);
		if (rc != EDEADLK)
			psc_fatalx("wrlock: %s", strerror(rc));
#else
		(void)ts;
		rc = pthread_rwlock_trywrlock(&rw.pr_rwlock);
		if (rc != EBUSY)
			psc_fatalx("wrlock: %s", strerror(rc));
#endif

		usleep(SLEEP_US);
		pfl_rwlock_unlock(&rw);
		sched_yield();
		usleep(20 * SLEEP_US);
	}
	return (NULL);
}

void
spawn(void *(*f)(void *), const char *namefmt, ...)
{
	struct thr *thr;
	pthread_t pthr;
	va_list ap;
	int rc;

	thr = PSCALLOC(sizeof(*thr));
	INIT_PSC_LISTENTRY(&thr->lentry);

	psclist_add_tail(&thr->lentry, &thrs);

	va_start(ap, namefmt);
	vsnprintf(thr->name, sizeof(thr->name), namefmt, ap);
	va_end(ap);

	rc = pthread_create(&pthr, NULL, f, thr);
	if (rc)
		errx(1, "pthread_create: %s", strerror(rc));
}

int
main(int argc, char *argv[])
{
	struct thr *thr;
	int run, i, c;
	char *endp;
	long l;

	pfl_init();
	while ((c = getopt(argc, argv, "n:r:w:")) != -1)
		switch (c) {
		case 'n':
			l = strtol(optarg, &endp, 10);
			if (l < 1 || l > NLOCKS_MAX ||
			    *endp != '\0' || optarg == endp)
				errx(1, "invalid argument: %s", optarg);
			nlocks = (int)l;
			break;
		case 'r':
			l = strtol(optarg, &endp, 10);
			if (l < 0 || l > NTHRS_MAX ||
			    *endp != '\0' || optarg == endp)
				errx(1, "invalid argument: %s", optarg);
			nrd = (int)l;
			break;
		case 'w':
			l = strtol(optarg, &endp, 10);
			if (l < 0 || l > NTHRS_MAX ||
			    *endp != '\0' || optarg == endp)
				errx(1, "invalid argument: %s", optarg);
			nwr = (int)l;
			break;
		default:
			usage();
		}
	argc -= optind;
	if (argc)
		usage();

	for (i = 0; i < nrd; i++)
		spawn(rd_main, "rd%d", i);
	for (i = 0; i < nwr; i++)
		spawn(wr_main, "wr%d", i);
	printf("target");
	psclist_for_each_entry(thr, &thrs, lentry)
		printf(" %5s", thr->name);
	printf("\n");
	do {
		printf("\r%6d", nlocks);

		run = 0;
		psclist_for_each_entry(thr, &thrs, lentry) {
			if (thr->st != nlocks)
				run = 1;
			printf(" %5d", thr->st);
		}

		fflush(stdout);
		usleep(1000000 / 24);
	} while (run);
	printf("\n");
	exit(0);
}
