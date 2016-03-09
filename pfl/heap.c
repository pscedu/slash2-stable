/* $Id$ */
/*
 * %ISC_START_LICENSE%
 * ---------------------------------------------------------------------
 * Copyright 2015-2016, Google, Inc.
 * Copyright (c) 2014-2015, Pittsburgh Supercomputing Center (PSC).
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
		if (ph->ph_cmpf(p, c) != 1)
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
	p = ph->ph_base[idx = phe->phe_idx] =
	    ph->ph_base[--ph->ph_nitems];
	phe = PSC_AGP(p, ph->ph_entoff);
	phe->phe_idx = idx;
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
pfl_heap_peekidx(struct pfl_heap *ph, int idx)
{
	struct pfl_heap_entry *phe;
	void *p;

	if (idx >= ph->ph_nitems)
		return (NULL);

	p = ph->ph_base[idx];
	phe = PSC_AGP(p, ph->ph_entoff);
	psc_assert(phe->phe_idx == idx);
	return (p);
}

void *
pfl_heap_peek(struct pfl_heap *ph)
{
	if (ph->ph_nitems == 0)
		return (NULL);
	return (pfl_heap_peekidx(ph, 0));
}

void *
pfl_heap_shift(struct pfl_heap *ph)
{
	void *p;

	p = pfl_heap_peek(ph);
	if (p)
		pfl_heap_remove(ph, p);
	return (p);
}

void
pfl_heap_reseat(struct pfl_heap *ph, void *p)
{
	pfl_heap_remove(ph, p);
	pfl_heap_add(ph, p);
}

int
pfl_heap_nitems(struct pfl_heap *ph)
{
	return (ph->ph_nitems);
}

void
_pfl_heap_init(struct pfl_heap *ph, int entoff,
    int (*cmpf)(const void *, const void *))
{
	memset(ph, 0, sizeof(*ph));
	ph->ph_cmpf = cmpf;
	ph->ph_entoff = entoff;
}
