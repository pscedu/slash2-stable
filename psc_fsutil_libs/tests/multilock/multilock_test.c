/* $Id$ */

#include <err.h>
#include <limits.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "psc_util/cdefs.h"
#include "psc_util/log.h"
#include "psc_util/multilock.h"

struct thr {
	pthread_t		t_pthread;
	struct multilock	t_ml;
	struct multilock_cond	t_mlc;
};

struct multilock_cond mastermlc;
int nthreads = 32;
int iterations = 1000;
const char *progname;

void *
thr_main(void *arg)
{
	struct thr *t = arg;
	void *p;
	int i;

	for (i = 0; i < iterations; i++) {
		multilock_wait(&t->t_ml, &p, 0);
		usleep(200);
	}
	return (NULL);
}

__dead void
usage(void)
{
	fprintf(stderr, "%s [-i iterations] [-n nthreads]\n", progname);
	exit(1);
}

int
main(int argc, char *argv[])
{
	struct thr *t, *threads;
	int error, c, j;
	long l;

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

	if ((threads = calloc(nthreads, sizeof(*threads))) == NULL)
		err(1, "calloc");

	multilock_cond_init(&mastermlc, NULL, 0, "master");

	for (j = 0, t = threads; j < nthreads; j++, t++) {
		multilock_cond_init(&t->t_mlc, NULL, 0, "cond%d", j);
		multilock_init(&t->t_ml, "ml%d", j);
		multilock_addcond(&t->t_ml, &mastermlc, 1);
		multilock_addcond(&t->t_ml, &t->t_mlc, 1);

		if ((error = pthread_create(&t->t_pthread, NULL,
		    thr_main, t)) != 0)
			errx(1, "pthread_create: %s", strerror(error));
		sched_yield();
	}

	for (j = 0; j < iterations; j++) {
		multilock_cond_wakeup(&mastermlc);
		usleep(100);
	}
	exit(0);
}
