/* $Id$ */
/*
 * %PSC_START_COPYRIGHT%
 * -----------------------------------------------------------------------------
 * Copyright (c) 2014, Pittsburgh Supercomputing Center (PSC).
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

#include "pfl/alloc.h"
#include "pfl/cdefs.h"
#include "pfl/heap.h"
#include "pfl/log.h"

void
_pfl_heap_swap(struct pfl_heap *ph, struct pfl_heap_entry *phe,
    struct pfl_heap_entry *che)
{
	int tidx;
	void *t;

	SWAP(ph->ph_base[phe->phe_idx], ph->ph_base[che->phe_idx], t);
	SWAP(phe->phe_idx, che->phe_idx, tidx);
}

void
pfl_heap_add(struct pfl_heap *ph, void *c)
{
	struct pfl_heap_entry *che, *phe;
	size_t nalloc;
	void *p;

	psc_assert(c);
	che = PSC_AGP(c, ph->ph_entoff);
	if (ph->ph_nitems == ph->ph_nalloc) {
		nalloc = MAX(8, 2 * ph->ph_nalloc);
		ph->ph_base = psc_realloc(ph->ph_base,
		    nalloc * sizeof(void *), 0);
		ph->ph_nalloc = nalloc;
	}
	ph->ph_base[che->phe_idx = ph->ph_nitems++] = c;
	/* bubble up */
	while (che->phe_idx > 0) {
		p = ph->ph_base[(che->phe_idx - 1) / 2];
		if (ph->ph_cmpf(p, c) == -1)
			break;
		phe = PSC_AGP(p, ph->ph_entoff);
		_pfl_heap_swap(ph, phe, che);
	}
}

void
pfl_heap_remove(struct pfl_heap *ph, void *p)
{
	struct pfl_heap_entry *phe, *che;
	void *c, *minc;
	int idx, i;

	psc_assert(ph->ph_nitems > 0);

	psc_assert(p);
	phe = PSC_AGP(p, ph->ph_entoff);
	p = ph->ph_base[phe->phe_idx] = ph->ph_base[--ph->ph_nitems];
	phe = PSC_AGP(p, ph->ph_entoff);
	phe->phe_idx = 0;
	/* bubble down */
	for (;;) {
		for (minc = p, idx = phe->phe_idx * 2 + 1, i = 0;
		    i < 2 && idx < ph->ph_nitems; idx++, i++) {
			c = ph->ph_base[idx];
			if (ph->ph_cmpf(c, minc) == -1)
				minc = c;
		}
		if (minc == p)
			break;
		che = PSC_AGP(minc, ph->ph_entoff);
		_pfl_heap_swap(ph, phe, che);
	}
}

void *
pfl_heap_shift(struct pfl_heap *ph)
{
	void *p;

	if (ph->ph_nitems == 0)
		return (NULL);

	p = ph->ph_base[0];
struct pfl_heap_entry *phe;
phe = PSC_AGP(p, ph->ph_entoff);
psc_assert(phe->phe_idx == 0);
	pfl_heap_remove(ph, p);
	return (p);
}

void
pfl_heap_reseat(struct pfl_heap *ph, void *p)
{
	pfl_heap_remove(ph, p);
	pfl_heap_add(ph, p);
}
