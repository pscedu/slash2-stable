/* $Id$ */
/*
 * %PSC_START_COPYRIGHT%
 * -----------------------------------------------------------------------------
 * Copyright (c) 2006-2009, Pittsburgh Supercomputing Center (PSC).
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

#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "pfl/cdefs.h"
#include "pfl/pfl.h"
#include "psc_util/pthrutil.h"
#include "psc_util/completion.h"

const char *progname;
pthread_mutex_t mut;
struct psc_completion compl = PSC_COMPLETION_INIT;
struct psc_waitq wq = PSC_WAITQ_INIT;

void *
spawn(__unusedx void *arg)
{
	psc_pthread_mutex_lock(&mut);
	psc_completion_done(&compl, 0);
	psc_waitq_wait(&wq, NULL);
	psc_pthread_mutex_unlock(&mut);
	return (NULL);
}

__dead void
usage(void)
{
	fprintf(stderr, "usage: %s\n", progname);
	exit(1);
}

int
main(int argc, char *argv[])
{
	pthread_t pt;
	int rc;

	pfl_init();
	progname = argv[0];
	if (getopt(argc, argv, "") != -1)
		usage();
	argc -= optind;
	if (argc)
		usage();

	psc_pthread_mutex_init(&mut);

	rc = pthread_create(&pt, NULL, spawn, NULL);
	psc_completion_wait(&compl);

	rc = pthread_mutex_trylock(&mut);
	printf("master thread trylock rc=%d %s\n", rc, strerror(rc));

	psc_waitq_wakeall(&wq);

	printf("locking...\n");
	psc_pthread_mutex_lock(&mut);
	rc = pthread_mutex_lock(&mut);
	psc_assert(rc == EDEADLK);
	psc_pthread_mutex_ensure_locked(&mut);
	exit(0);
}
