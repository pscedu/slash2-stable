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

/*
 * TODO: test reset().
 */

#include <err.h>
#include <limits.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "pfl/cdefs.h"
#include "pfl/pfl.h"
#include "pfl/log.h"
#include "pfl/multiwait.h"

struct thr {
	pthread_t			t_pthread;
	struct pfl_multiwait		t_ml;
	struct pfl_multiwaitcond	t_mlc;
};

struct pfl_multiwaitcond mastermlc;
int nthreads = 32;
int iterations = 1000;

void *
thr_main(void *arg)
{
	struct thr *t = arg;
	void *p;
	int i;

	for (i = 0; i < iterations; i++) {
		pfl_multiwait(&t->t_ml, &p);
		usleep(200);
	}
	return (NULL);
}

__dead void
usage(void)
{
	extern const char *__progname;

	fprintf(stderr, "%s [-i iterations] [-n nthreads]\n", __progname);
	exit(1);
}

int
main(int argc, char *argv[])
{
	struct thr *t, *threads;
	int rc, c, j;
	long l;

	pfl_init();
	while ((c = getopt(argc, argv, "i:n:")) != -1)
		switch (c) {
		case 'i':
			l = strtol(optarg, NULL, 10);
			if (l < 0 || l > INT_MAX)
				errx(1, "invalid iterations: %s", optarg);
			iterations = (int)l;
			break;
		case 'n':
			l = strtol(optarg, NULL, 10);
			if (l < 0 || l > INT_MAX)
				errx(1, "invalid nthreads: %s", optarg);
			nthreads = (int)l;
			break;
		default:
			usage();
		}
	argc -= optind;
	if (argc)
		usage();

	if ((threads = calloc(nthreads, sizeof(*threads))) == NULL)
		err(1, "calloc");

	pfl_multiwaitcond_init(&mastermlc, NULL, 0, "master");

	for (j = 0, t = threads; j < nthreads; j++, t++) {
		pfl_multiwaitcond_init(&t->t_mlc, NULL, 0, "cond%d", j);

		pfl_multiwait_init(&t->t_ml, "ml%d", j);
		rc = pfl_multiwait_addcond(&t->t_ml, &mastermlc);
		if (rc)
			psc_fatal("addcond");
		rc = pfl_multiwait_addcond(&t->t_ml, &t->t_mlc);
		if (rc)
			psc_fatal("addcond");

		rc = pthread_create(&t->t_pthread, NULL, thr_main, t);
		if (rc)
			errx(1, "pthread_create: %s", strerror(rc));
		sched_yield();
	}

	for (j = 0; j < iterations; j++) {
		pfl_multiwaitcond_wakeup(&mastermlc);
		usleep(100);
	}
	exit(0);
}
