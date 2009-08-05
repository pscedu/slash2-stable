/* $Id$ */

#include <sys/types.h>
#include <sys/time.h>

#include <getopt.h>
#include <pthread.h>
#include <stdio.h>
#include <strings.h>
#include <time.h>
#include <unistd.h>

#include "pfl.h"
#include "psc_util/alloc.h"
#include "psc_util/assert.h"
#include "psc_util/cdefs.h"
#include "psc_util/lock.h"
#include "psc_util/thread.h"
#include "psc_util/waitq.h"

int nthreads = 1;
int iterations = 1;
const char *progname;
psc_waitq_t waitq;

__dead void
usage(void)
{
	fprintf(stderr, "usage: %s [-n nthr] [-i iterations]\n", progname);
	exit(1);
}

void *
child_main(__unusedx void *arg)
{
	struct psc_thread *t;
	int i;

	/*
	 * Wait here until the parent signals us to start
	 */
	psc_waitq_wait(&waitq, NULL);

	t = pscthr_get();

	psc_dbg("t%d: after pseudo barrier", t->pscthr_thrid);

	for (i=0; i < iterations; i++) {
		psc_waitq_wait(&waitq, NULL);
		psc_dbg("tid=%d, i=%d awake",
			t->pscthr_thrid, i);
	}

	return NULL;
}

int
main(int argc, char *argv[])
{
	int c, i, rc = 0;

	progname = argv[0];
	pfl_init();
	while ((c = getopt(argc, argv, "n:i:")) != -1)
		switch (c) {
		case 'n':
			nthreads = atoi(optarg);
			break;
		case 'i':
			iterations = atoi(optarg);
			break;
		default:
			usage();
		}
	argc -= optind;
	if (argc)
		usage();

	psc_dbg("nthreads = %d", nthreads);

	psc_waitq_init(&waitq);

	for (i = 0; i < nthreads; i++)
		pscthr_init(0, 0, child_main, NULL, 0, "thr%d", i);

	sleep(1);
	psc_waitq_wakeall(&waitq);
	sleep(2);

	i = nthreads * iterations;
	while (i--) {
		psc_waitq_wakeone(&waitq);
		usleep(30);
	}
	return rc;
}
