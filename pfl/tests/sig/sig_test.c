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

#include <err.h>
#include <errno.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "pfl/cdefs.h"
#include "pfl/types.h"

pthread_t thr1;
pthread_t thr2;
pthread_t thr3;

char *signames[] = {
	"<zero>",
	"SIGHUP",
	"SIGINT",
	"SIGQUIT",
	"SIGILL",
	"SIGTRAP",
	"SIGABRT",
	"SIGBUS",
	"SIGFPE",
	"SIGKILL",
	"SIGUSR1",
	"SIGSEGV",
	"SIGUSR2",
	"SIGPIPE",
	"SIGALRM",
	"SIGTERM",
	"SIGSTKFLT",
	"SIGCHLD",
	"SIGCONT",
	"SIGSTOP",
	"SIGTSTP",
	"SIGTTIN",
	"SIGTTOU",
	"SIGURG",
	"SIGXCPU",
	"SIGXFSZ",
	"SIGVTALRM",
	"SIGPROF",
	"SIGWINCH",
	"SIGIO",
	"SIGPWR",
	"SIGSYS"
};

__dead void
usage(void)
{
	extern const char *__progname;

	fprintf(stderr, "usage: %s\n", __progname);
	exit(1);
}

void
report_sigs(void)
{
	struct sigaction sa;
	const char *act;
	int i;

	for (i = 1; i < nitems(signames); i++) {
		if (i == SIGKILL || i == SIGSTOP)
			continue;
		if (sigaction(i, NULL, &sa) == -1)
			err(1, "sigaction");
		if (sa.sa_handler == SIG_IGN)
			act = "ign";
		else if (sa.sa_handler == SIG_DFL)
			act = "dfl";
		else
			act = "?";
		printf("thr %"PSCPRI_PTHRT": sig %02d [%s]: %s\n",
		    pthread_self(), i, signames[i], act);
	}
}

void *
thr1_main(__unusedx void *arg)
{
	report_sigs();
	return (NULL);
}

void *
thr2_main(__unusedx void *arg)
{
	report_sigs();
	return (NULL);
}

void *
thr3_main(__unusedx void *arg)
{
	report_sigs();
	return (NULL);
}

int
main(int argc, char *argv[])
{
	int rc, c;

	while ((c = getopt(argc, argv, "")) != -1)
		switch (c) {
		default:
			usage();
		}

	if ((rc = pthread_create(&thr1, NULL, thr1_main, NULL)) != 0)
		errx(1, "pthread_join: %s", strerror(rc));
	if ((rc = pthread_create(&thr2, NULL, thr2_main, NULL)) != 0)
		errx(1, "pthread_join: %s", strerror(rc));
	if ((rc = pthread_create(&thr3, NULL, thr3_main, NULL)) != 0)
		errx(1, "pthread_join: %s", strerror(rc));

	if ((rc = pthread_join(thr1, NULL)) != 0)
		errx(1, "pthread_join: %s", strerror(rc));
	if ((rc = pthread_join(thr2, NULL)) != 0)
		errx(1, "pthread_join: %s", strerror(rc));
	if ((rc = pthread_join(thr3, NULL)) != 0)
		errx(1, "pthread_join: %s", strerror(rc));
	exit(0);
}
