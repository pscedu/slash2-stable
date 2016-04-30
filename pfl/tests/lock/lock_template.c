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
#include "pfl/alloc.h"
#include "pfl/atomic.h"
#include "pfl/thread.h"
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

__dead void
usage(void)
{
	extern const char *__progname;

	fprintf(stderr, "usage: %s [-n nruns] [-t nthr]\n", __progname);
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
		pscthr_init(0, thr_main, 0, "thr%d", i);

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
