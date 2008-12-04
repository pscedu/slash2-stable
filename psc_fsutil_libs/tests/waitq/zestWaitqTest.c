/* $Id$ */

#include <sys/select.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/wait.h>

#include <getopt.h>
#include <pthread.h>
#include <stdio.h>
#include <strings.h>
#include <time.h>
#include <unistd.h>

#include "psc_util/assert.h"
#include "psc_util/lock.h"
#include "psc_util/waitq.h"
#include "psc_util/cdefs.h"

#include "zestion.h"

psc_waitq_t waitq;

int nthreads;
int iterations;

void printHelp(void)
{
	fprintf(stderr, "usage: zestWaitqTest [-n NumThreads] [-i iterations]\n");
	exit(1);
}

void getOptions(int argc, char *argv[]) {
#define ARGS "n:i:"
	int c, err = 0;
	optarg = NULL;

	while ( !err && ((c = getopt(argc, argv, ARGS)) != -1))
		switch (c) {

		case 'n':
			nthreads = atoi(optarg);
			break;

		case 'i':
			iterations = atoi(optarg);
			break;

		default :
			fprintf(stderr, "what?\n");
			err++;
		}

	if (err)
		printHelp();
	return;
}

void * child_main(__unusedx void *arg)
{
	struct psc_thread *t;
	int i;

	/*
	 * Wait here until the parent signals us to start
	 */
	psc_waitq_wait(&waitq, NULL);

	t = pscthr_get();

	psc_dbg("t%d: after pseudo barrier", t->pscthr_id);

	for (i=0; i < iterations; i++) {
		psc_waitq_wait(&waitq, NULL);
		psc_dbg("tid=%d, pid=0x%lx i=%d awake",
			t->pscthr_id, t->pscthr_pthread, i);
	}

	return NULL;
}


int main(int argc, char *argv[])
{
	int rc = 0;
	int i;
	struct psc_thread *zthr, *p;

	nthreads = 1;
	iterations = 1;

	getOptions(argc, argv);

	psc_dbg("nthreads = %d", nthreads);

	psc_waitq_init(&waitq);

	p = zthr = PSCALLOC(nthreads * sizeof(struct psc_thread));
	psc_assert(zthr != NULL);

	bzero(zthr, sizeof(struct psc_thread) * nthreads);

	for (i=0; i < nthreads; i++, zthr++)
		pscthr_init(zthr, ZTHRT_IO, child_main, i);
	sleep(1);
	psc_waitq_wakeall(&waitq);
	sleep(2);

	i = (nthreads * iterations);
	while(i--) {
		psc_waitq_wakeup(&waitq);
		usleep(30);
	}

	/*
	 * Finalize
	 */
	psc_dbg("Finalize");
	for (i=0, zthr=p; i < nthreads; i++, zthr++) {
		rc = pthread_join(zthr->pscthr_pthread, NULL);
		psc_assert(rc == 0);
	}

	return rc;
}
