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

#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "pfl/cdefs.h"
#include "pfl/pfl.h"
#include "pfl/completion.h"
#include "pfl/pthrutil.h"

struct pfl_mutex	 m = PSC_MUTEX_INIT;
struct psc_compl	 compl = PSC_COMPL_INIT("test");
struct psc_waitq	 wq = PSC_WAITQ_INIT("test");
int			 var = 0;

void *
spawn(__unusedx void *arg)
{
	psc_mutex_lock(&m);
	psc_compl_ready(&compl, 1);
	psc_waitq_wait(&wq, NULL);
	var = 1;
	psc_mutex_unlock(&m);
	return (NULL);
}

__dead void
usage(void)
{
	extern const char *__progname;

	fprintf(stderr, "usage: %s\n", __progname);
	exit(1);
}

int
main(int argc, char *argv[])
{
	pthread_t pt;
	int rc, lk;

	pfl_init();
	if (getopt(argc, argv, "") != -1)
		usage();
	argc -= optind;
	if (argc)
		usage();

	//psc_mutex_init(&m);

	rc = pthread_create(&pt, NULL, spawn, NULL);
	psc_compl_wait(&compl);

	psc_assert(!psc_mutex_trylock(&m));

	while (var == 0) {
		psc_waitq_wakeall(&wq);
		sleep(1);
	}

	psc_mutex_lock(&m);
	rc = pthread_mutex_lock(&m.pm_mutex);
	psc_assert(rc == EDEADLK);

	lk = psc_mutex_reqlock(&m);

	psc_mutex_ensure_locked(&m);
	psc_mutex_ureqlock(&m, lk);

	psc_assert(psc_mutex_tryreqlock(&m, &lk));
	psc_mutex_ureqlock(&m, lk);

	psc_assert(psc_mutex_haslock(&m));

	psc_mutex_unlock(&m);

	psc_assert(psc_mutex_haslock(&m) == 0);

	psc_assert(psc_mutex_tryreqlock(&m, &lk));
	psc_mutex_ureqlock(&m, lk);
	exit(0);
}
