/* $Id$ */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "pfl/cdefs.h"
#include "pfl/pfl.h"
#include "psc_ds/listcache.h"

const char *progname;
struct psclist_head hd = PSCLIST_HEAD_INIT(hd);
int nitems;

__dead void
usage(void)
{
	fprintf(stderr, "usage: %s\n", progname);
	exit(1);
}

struct m {
	int v;
	struct psclist_head lentry;
};

void
addelem(int v)
{
	struct m *m;

	m = PSCALLOC(sizeof(*m));
	m->v = v;
	psclist_xadd(&m->lentry, &hd);
	nitems++;
}

int
shift(void)
{
	struct m *m;

	m = psclist_first_entry(&hd, struct m, lentry);
	if (m == NULL)
		return (0);
	psclist_del(&m->lentry);
	printf("v: %d\n", m->v);
	PSCFREE(m);
	return (1);
}

int
m_cmp(const void *a, const void *b)
{
	struct m * const *ma = a, *x = *ma, * const *mb = b, *y = *mb;

	return (CMP(x->v, y->v));
}

int
main(int argc, char *argv[])
{
	void *p;
	int i;

	pfl_init();
	progname = argv[0];
	if (getopt(argc, argv, "") != -1)
		usage();
	argc -= optind;
	if (argc)
		usage();

	addelem(4);
	addelem(2);
	addelem(9);
	addelem(23);
	addelem(47);
	addelem(5);
	addelem(8);

	for (i = 0; i < 100; i++) {
		p = PSCALLOC(sizeof(void *) * nitems);
		psclist_sort(p, &hd, nitems, offsetof(struct m, lentry),
		    qsort, m_cmp);
		PSCFREE(p);
	}

	while (shift())
		;
	exit(0);
}
