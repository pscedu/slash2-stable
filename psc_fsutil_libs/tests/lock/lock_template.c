/* $Id$ */
/*
 * %PSC_START_COPYRIGHT%
 * -----------------------------------------------------------------------------
 * Copyright (c) 2006-2010, Pittsburgh Supercomputing Center (PSC).
 *
 * Permission to use, copy, and modify this software and its documentation
 * without fee for personal use or non-commercial use within your organization
 * is hereby granted, provided that the above copyright notice is preserved in
 * all copies and that the copyright and this permission notice appear in
 * supporting documentation.  Permission to redistribute this software to other
 * organizations or individuals is not permitted without the written permission
 * of the Pittsburgh Supercomputing Center.  PSC makes no representations about
 * the suitability of this software for any purpose.  It is provided "as is"
 * without express or implied warranty.
 * -----------------------------------------------------------------------------
 * %PSC_END_COPYRIGHT%
 */

#include <sys/types.h>
#include <sys/time.h>

#include <err.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "pfl/cdefs.h"
#include "pfl/pfl.h"
#include "psc_util/alloc.h"
#include "psc_util/atomic.h"
#include "psc_util/thread.h"
#include "pfl/time.h"

#include TEST_LOCK_INCLUDE

#define STARTWATCH(t) PFL_GETTIMEVAL(&(t)[0])
#define STOPWATCH(t)  PFL_GETTIMEVAL(&(t)[1])

TEST_LOCK_TYPE	 lock = TEST_LOCK_INITIALIZER;
atomic_t	 idx = ATOMIC_INIT(0);
atomic_t	 nworkers = ATOMIC_INIT(0);
int		 nthrs = 32;
int		 nruns = 4000;
int		*buf;
const char	*progname;

__dead void
usage(void)
{
	fprintf(stderr, "usage: %s [-n nruns] [-t nthr]\n", progname);
	exit(1);
}

void
thr_main(__unusedx struct psc_thread *thr)
{
	int *p, i;

	for (i = 0, p = buf; i < nruns; i++, p++) {
		TEST_LOCK_ACQUIRE(&lock);
		atomic_inc(&idx);
		*p = i;
		TEST_LOCK_RELEASE(&lock);
		usleep(1);
	}
	atomic_dec(&nworkers);
}

int
main(int argc, char *argv[])
{
	int slen, oldidx, c, i, *j;
	struct timeval tv[2], res;

	progname = argv[0];
	pfl_init();
	while (((c = getopt(argc, argv, "n:t:")) != -1))
		switch (c) {
		case 't':
			nthrs = atoi(optarg);
			break;
		case 'n':
			nruns = atoi(optarg);
			break;
		default:
			usage();
		}
	argc -= optind;
	if (argc)
		usage();

	buf = PSCALLOC(nruns * sizeof(*buf));

	atomic_set(&nworkers, nthrs);
	for (i = 0; i < nthrs; i++)
		pscthr_init(0, 0, thr_main, NULL, 0, "thr%d", i);

	slen = snprintf(NULL, 0, "%d", nruns * nthrs);

	oldidx = 0;
	while (atomic_read(&nworkers)) {
		STARTWATCH(tv);
		sleep(1);
		STOPWATCH(tv);
		timersub(&tv[1], &tv[0], &res);
		i = atomic_read(&idx);
		printf("%*d/%d current lock cnt; LPS %f\n",
		    slen, i, nruns * nthrs,
		    (i - oldidx) / (res.tv_sec + res.tv_usec * 1e-6));
		oldidx = i;
	}

	for (i = 0, j = buf; i < nruns; j++, i++)
		if (*j != i)
			psc_fatalx("position %d should be %d, not %d",
			    i, i, *j);
	return 0;
}
