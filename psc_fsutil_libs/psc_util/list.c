/* $Id$ */

#include "psc_ds/listcache.h"
#include "psc_ds/lockedlist.h"

struct psc_lockedlist pscListCaches =
    PLL_INITIALIZER(&pscListCaches, struct psc_listcache, lc_index_lentry);

void
psclist_sort(void **p, struct psclist_head *hd, int n, ptrdiff_t off,
    void (*sortf)(void *, size_t, size_t, int (*)(const void *, const void *)),
    int (*cmpf)(const void *, const void *))
{
	struct psclist_head *e;
	void *next, *prev;
	int j = 0;

	psc_assert(n >= 0);
	if (n < 2)
		return;

	psclist_for_each(e, hd)
		p[j++] = ((char *)e) - off;
	sortf(p, n, sizeof(*p), cmpf);
	prev = hd;
	for (j = 0; j < n; j++, prev = e) {
		e = (void *)((char *)p[j] + off);
		if (j + 1 == n)
			next = hd;
		else
			next = (char *)p[j + 1] + off;
		e->zprev = prev;
		e->znext = next;
	}
	hd->znext = (void *)((char *)p[0] + off);
	hd->zprev = prev;
}
