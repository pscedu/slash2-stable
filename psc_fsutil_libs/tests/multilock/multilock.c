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

int nthreads;
int iterations;

void *
thr_main(void *arg)
{
	struct thr *t = arg;
	void *p;
	int i;

	printf("spawned thread 0x%lx\n", t->t_pthread);
	for (i = 0; i < iterations; i++) {
		multilock_wait(&t->t_ml, &p);
		printf("thr 0x%lx: obtained master lock\n",
		    t->t_pthread);
		sleep(2);
	}
	return (NULL);
}

const char *progname;

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

	nthreads = 2;
	iterations = 10;
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

	multilock_cond_init(&mastermlc, NULL);

	for (j = 0; j < nthreads; j++) {
		t = &threads[j];
		multilock_cond_init(&t->t_mlc, NULL);
		multilock_addcond(&t->t_ml, &mastermlc);
		multilock_addcond(&t->t_ml, &t->t_mlc);

		multilock_free(&t->t_ml);
		multilock_init(&t->t_ml);
		multilock_addcond(&t->t_ml, &mastermlc);
		multilock_addcond(&t->t_ml, &t->t_mlc);

		if ((error = pthread_create(&t->t_pthread, NULL,
		    thr_main, t)) != 0)
			errx(1, "pthread_create: %s", strerror(error));
		sched_yield();
	}
	printf("spawned %d threads, doing %d iterations\n", nthreads,
	    iterations);

	for (j = 0; j < iterations; j++) {
		printf("waking up master condition\n");
		multilock_cond_wakeup(&mastermlc);
		sleep(1);
	}
	exit(0);
}
