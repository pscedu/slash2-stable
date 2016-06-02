/* $Id$ */
/* %ISC_LICENSE% */

#include <err.h>
#include <limits.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "pfl/cdefs.h"
#include "pfl/pfl.h"
#include "pfl/log.h"
#include "pfl/listcache.h"

struct thr {
	pthread_t		t_pthread;
	int			t_id;
};

struct item {
	struct psc_listentry	lentry;
	int			val;
};

struct psc_listcache lc;
int nthreads = 32;
int iterations = 1000;

void *
thr_main(__unusedx void *arg)
{
	struct thr *t = arg;
	struct item *it;
	int i;

	for (i = 0; i < iterations; i++) {
		it = lc_peekheadwait(&lc);
		printf("%d got item %d\n", t->t_id, it->val);
	}
	it = lc_getwait(&lc);
	lc_add(&lc, it);
	return (NULL);
}

__dead void
usage(void)
{
	extern const char *__progname;

	fprintf(stderr, "%s\n", __progname);
	exit(1);
}

int
main(int argc, char *argv[])
{
	struct thr *t, *threads;
	struct item it;
	int rc, c, j;

	pfl_init();
	while ((c = getopt(argc, argv, "")) != -1)
		switch (c) {
		default:
			usage();
		}
	argc -= optind;
	if (argc)
		usage();

	if ((threads = calloc(nthreads, sizeof(*threads))) == NULL)
		err(1, "calloc");

	lc_init(&lc, "test", struct item, lentry);

	for (j = 0, t = threads; j < nthreads; j++, t++) {
		t->t_id = j;
		rc = pthread_create(&t->t_pthread, NULL, thr_main, t);
		if (rc)
			errx(1, "pthread_create: %s", strerror(rc));
	}
	INIT_LISTENTRY(&it.lentry);
	it.val = -1;
	lc_add(&lc, &it);
	for (j = 0, t = threads; j < nthreads; j++, t++)
		pthread_join(t->t_pthread, NULL);
	exit(0);
}
