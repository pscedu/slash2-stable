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

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "pfl/cdefs.h"
#include "pfl/pfl.h"
#include "pfl/list.h"
#include "pfl/listcache.h"

struct psclist_head hd = PSCLIST_HEAD_INIT(hd);
int nitems;

__dead void
usage(void)
{
	extern const char *__progname;

	fprintf(stderr, "usage: %s\n", __progname);
	exit(1);
}

struct m {
	int garbage;
	struct psclist_head lentry;
	int v;
};

void
addelem(int v)
{
	struct m *m;

	m = PSCALLOC(sizeof(*m));
	INIT_PSC_LISTENTRY(&m->lentry);
	m->v = v;
	psclist_add(&m->lentry, &hd);
	nitems++;
}

int
shift(void)
{
	struct m *m;

	m = psc_listhd_first_obj(&hd, struct m, lentry);
	if (m == NULL)
		return (0);
	psclist_del(&m->lentry, &hd);
	printf("v: %d\n", m->v);
	PSCFREE(m);
	return (1);
}

int
m_cmp1(const void *a, const void *b)
{
	struct m * const *ma = a, *x = *ma, * const *mb = b, *y = *mb;

	return (CMP(x->v, y->v));
}

int
m_cmp2(const void *a, const void *b)
{
	struct m const *x = a, *y = b;

	return (CMP(x->v, y->v));
}

struct i {
	struct psc_listentry i_lentry;
	int i_v;
};

int
it_cmp(const void *a, const void *b)
{
	const struct i *x = a, *y = b;

	return (CMP(x->i_v, y->i_v));
}

void
dump(struct psc_lockedlist *pll)
{
	struct i *it;

	PLL_FOREACH(it, pll)
		printf("%d ", it->i_v);
	printf("\n");
}

void
pll_sort_backwards(void)
{
	struct i *it;
	struct psc_lockedlist pll = PLL_INIT(&pll, struct i, i_lentry);

	int i;
	int sorted[] = {    0,  1,   2, 3, 4, 5, 9, 18, 27, 100, 156 };
	int unsorted[] = { 156, 5, 100, 3, 9, 2, 0, 27,  4,   1,  18 };

	for (i = 0; i < (int) (sizeof(sorted) / sizeof (int)); i++) {
		PFL_ALLOC_OBJ(it);
		INIT_PSC_LISTENTRY(&it->i_lentry);
		it->i_v = unsorted[i];

		/* cscope: psclist_add_sorted_backwards() */
		pll_add_sorted_backwards(&pll, it, it_cmp);
		/* dump(&pll); */
	}


	for (i = 0; i < (int) (sizeof(sorted) / sizeof (int)); i++) {
		it = pll_get(&pll);
		psc_assert(it->i_v == sorted[i]);
		PSCFREE(it);
	}
	printf("Locked list sort backwards seems to be working.\n");
}

void
lc_sort_test(void)
{
	int i;
	int sorted[] = {    0,  1,   2, 3, 4, 5, 9, 18,  27, 100, 156, 400 };
	int unsorted[] = { 156, 5, 100, 3, 9, 2, 0, 400, 27,   4,   1,  18 };

	int sorted2[] = { -44, -9, -1, 0, 7, 8, 9, 21, 72, 128, 999, 1234 };
	int unsorted2[] = { -1, 128, 9, 72, -44, 999, 1234, 0, 7, 8, 21, -9 };

	struct m *m;
	struct psc_listcache lc;

	lc_init(&lc, "test 1", struct m, lentry);

	for (i = 0; i < (int) (sizeof(sorted) / sizeof (int)); i++) {
		m = PSCALLOC(sizeof(*m));
		INIT_PSC_LISTENTRY(&m->lentry);
		m->v = unsorted[i];
		lc_add_sorted_backwards(&lc, m, m_cmp2);
	}
	for (i = 0; i < (int) (sizeof(sorted) / sizeof (int)); i++) {
		m = lc_getwait(&lc);
		psc_assert(m->v == sorted[i]);
		PSCFREE(m);
	}

	lc_init(&lc, "test 2", struct m, lentry);

	for (i = 0; i < (int) (sizeof(sorted) / sizeof (int)); i++) {
		m = PSCALLOC(sizeof(*m));
		INIT_PSC_LISTENTRY(&m->lentry);
		m->v = unsorted[i];
		lc_add_sorted(&lc, m, m_cmp2);
	}
	for (i = 0; i < (int) (sizeof(sorted) / sizeof (int)); i++) {
		m = lc_getwait(&lc);
		psc_assert(m->v == sorted[i]);
		PSCFREE(m);
	}

	lc_init(&lc, "test 3", struct m, lentry);

	for (i = 0; i < (int) (sizeof(sorted2) / sizeof (int)); i++) {
		m = PSCALLOC(sizeof(*m));
		INIT_PSC_LISTENTRY(&m->lentry);
		m->v = unsorted2[i];
		lc_add_sorted(&lc, m, m_cmp2);
	}
	for (i = 0; i < (int) (sizeof(sorted2) / sizeof (int)); i++) {
		m = lc_getwait(&lc);
		psc_assert(m->v == sorted2[i]);
		PSCFREE(m);
	}

	printf("List cache sort seems to be working.\n");
}


int
main(int argc, char *argv[])
{
	struct psc_lockedlist pll = PLL_INIT(&pll, struct i, i_lentry);
	struct psc_listcache lc;
	struct timespec ts;
	struct m *m, *next;
	struct i *it;
	void *p;
	int i;

	pfl_init();
	if (getopt(argc, argv, "") != -1)
		usage();
	argc -= optind;
	if (argc)
		usage();

	lc_sort_test();
	pll_sort_backwards();

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
		    qsort, m_cmp1);
		PSCFREE(p);
	}

	psclist_for_each_entry_safe(m, next, &hd, lentry)
		printf("v: %d\n", m->v);

	while (shift())
		;

	lc_init(&lc, "test 3", struct m, lentry);

	m = PSCALLOC(sizeof(*m));
	INIT_PSC_LISTENTRY(&m->lentry);
	m->v = 5;
	lc_addqueue(&lc, m);

	m = PSCALLOC(sizeof(*m));
	INIT_PSC_LISTENTRY(&m->lentry);
	m->v = 8;
	lc_addqueue(&lc, m);

	m = PSCALLOC(sizeof(*m));
	INIT_PSC_LISTENTRY(&m->lentry);
	m->v = 13;
	lc_addqueue(&lc, m);

	memset(&ts, 0, sizeof(ts));
	ts.tv_sec = time(NULL) + 1;

	m = lc_gettimed(&lc, &ts);
	psc_assert(m->v == 5);
	PSCFREE(m);

	m = lc_gettimed(&lc, &ts);
	psc_assert(m->v == 8);
	PSCFREE(m);

	m = lc_gettimed(&lc, &ts);
	psc_assert(m->v == 13);
	PSCFREE(m);

	PFL_ALLOC_OBJ(it);
	INIT_PSC_LISTENTRY(&it->i_lentry);
	it->i_v = 5;
	pll_add_sorted(&pll, it, it_cmp);

	PFL_ALLOC_OBJ(it);
	INIT_PSC_LISTENTRY(&it->i_lentry);
	it->i_v = 3;
	pll_add_sorted(&pll, it, it_cmp);

	PFL_ALLOC_OBJ(it);
	INIT_PSC_LISTENTRY(&it->i_lentry);
	it->i_v = 9;
	pll_add_sorted(&pll, it, it_cmp);

	PFL_ALLOC_OBJ(it);
	INIT_PSC_LISTENTRY(&it->i_lentry);
	it->i_v = 2;
	pll_add_sorted(&pll, it, it_cmp);

	PFL_ALLOC_OBJ(it);
	INIT_PSC_LISTENTRY(&it->i_lentry);
	it->i_v = 27;
	pll_add_sorted(&pll, it, it_cmp);

	PFL_ALLOC_OBJ(it);
	INIT_PSC_LISTENTRY(&it->i_lentry);
	it->i_v = 4;
	pll_add_sorted(&pll, it, it_cmp);

	it = pll_get(&pll);
	psc_assert(psclist_disjoint(&it->i_lentry));

	psc_assert(it->i_v == 2);
	PSCFREE(it);

	exit(0);
}
