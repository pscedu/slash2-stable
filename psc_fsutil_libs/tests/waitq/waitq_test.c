/* $Id$ */
/*
 * %PSC_START_COPYRIGHT%
 * -----------------------------------------------------------------------------
 * Copyright (c) 2006-2012, Pittsburgh Supercomputing Center (PSC).
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

#include <getopt.h>
#include <pthread.h>
#include <stdio.h>
#include <strings.h>
#include <time.h>
#include <unistd.h>

#include "pfl/cdefs.h"
#include "pfl/pfl.h"
#include "psc_util/alloc.h"
#include "psc_util/fmtstr.h"
#include "psc_util/lock.h"
#include "psc_util/log.h"
#include "psc_util/thread.h"
#include "psc_util/waitq.h"

int nthreads = 4;
int iterations = 4;
const char *progname;
struct psc_waitq waitq;

__dead void
usage(void)
{
	fprintf(stderr, "usage: %s [-n nthr] [-i iterations]\n", progname);
	exit(1);
}

void
child_main(struct psc_thread *thr)
{
	char buf[40];
	int i;

	/*
	 * Wait here until the parent signals us to start
	 */
	psc_waitq_wait(&waitq, NULL);

	psclog_dbg("after pseudo barrier");

	/* Connect to control socket. */
	(void)FMTSTR(buf, sizeof(buf), "foobar%h",
		FMTSTRCASE('h', buf, sizeof(buf), "s", "test")
	);


	for (i = 0; i < iterations; i++) {
		psc_waitq_wait(&waitq, NULL);
		psclog_dbg("tid=%d, i=%d awake",
		    thr->pscthr_thrid, i);
	}
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

	psclog_dbg("nthreads = %d", nthreads);

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
