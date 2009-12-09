/* $Id$ */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>

#include "pfl/cdefs.h"
#include "psc_util/pthrutil.h"

const char *progname;

__dead void
usage(void)
{
	fprintf(stderr, "usage: %s\n", progname);
	exit(1);
}

int
main(int argc, char *argv[])
{
	pthread_mutex_t m;
	int lk;

	progname = argv[0];
	if (getopt(argc, argv, "") != -1)
		usage();
	argc -= optind;
	if (argc)
		usage();

	psc_pthread_mutex_init(&m);
	psc_pthread_mutex_lock(&m);
	lk = psc_pthread_mutex_reqlock(&m);
	psc_pthread_mutex_ureqlock(&m, lk);
	psc_pthread_mutex_unlock(&m);
	exit(0);
}
