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

#include <getopt.h>
#include <pthread.h>
#include <stdio.h>
#include <strings.h>
#include <time.h>
#include <unistd.h>

#include "pfl/alloc.h"
#include "pfl/cdefs.h"
#include "pfl/fmtstr.h"
#include "pfl/pfl.h"
#include "pfl/lock.h"
#include "pfl/log.h"
#include "pfl/thread.h"
#include "pfl/waitq.h"

int nthreads = 4;
int iterations = 4;
struct psc_waitq waitq;

__dead void
usage(void)
{
	extern const char *__progname;

	fprintf(stderr, "usage: %s [-n nthr] [-i iterations]\n",
	    __progname);
	exit(1);
}

void
child_main(__unusedx struct psc_thread *thr)
{
	char buf[40];
	int i;

	/*
	 * Wait here until the parent signals us to start
	 */
	psc_waitq_wait(&waitq, NULL);

	psclog_debug("after pseudo barrier");

	/* Connect to control socket. */
	(void)FMTSTR(buf, sizeof(buf), "foobar%h",
		FMTSTRCASE('h', "s", "test")
	);

	for (i = 0; i < iterations; i++) {
		psc_waitq_wait(&waitq, NULL);
		psclog_debug("i=%d awake", i);
	}
}

int
main(int argc, char *argv[])
{
	int c, i, rc = 0;

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

	psclog_debug("nthreads = %d", nthreads);

	psc_waitq_init(&waitq, "test");

	for (i = 0; i < nthreads; i++)
		pscthr_init(0, child_main, 0, "thr%d", i);

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
