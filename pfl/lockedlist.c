/* $Id$ */
/*
 * %PSC_START_COPYRIGHT%
 * -----------------------------------------------------------------------------
 * Copyright (c) 2008-2013, Pittsburgh Supercomputing Center (PSC).
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

#include <string.h>

#include "psc_ds/list.h"
#include "psc_ds/lockedlist.h"
#include "psc_util/alloc.h"
#include "psc_util/lock.h"
#include "psc_util/log.h"

void
_pll_initf(struct psc_lockedlist *pll, int offset, psc_spinlock_t *lkp,
    int flags)
{
	memset(pll, 0, sizeof(*pll));
	INIT_PSCLIST_HEAD(&pll->pll_listhd);
	if (lkp) {
		pll->pll_flags |= PLLF_EXTLOCK;
		pll->pll_lockp = lkp;
	} else {
		if (flags & PLLF_LOGTMP)
			INIT_SPINLOCK_LOGTMP(&pll->pll_lock);
		else
			INIT_SPINLOCK(&pll->pll_lock);
	}
	pll->pll_offset = offset;
}

void
_pll_remove(const struct pfl_callerinfo *pci,
    struct psc_lockedlist *pll, void *p)
{
	struct psc_listentry *e;
	int locked;

	e = _pll_obj2entry(pll, p);
	locked = PLL_RLOCK(pll);
	psclist_del(e, &pll->pll_listhd);
	psc_assert(pll->pll_nitems > 0);
	pll->pll_nitems--;
	PLL_URLOCK(pll, locked);
	_psclog_pci(pci, PLL_DEBUG, 0, "lockedlist %p remove item %p",
	    pll, p);
}

void
_pll_add(const struct pfl_callerinfo *pci,
    struct psc_lockedlist *pll, void *p, int flags)
{
	struct psc_listentry *e;
	int locked;

	e = _pll_obj2entry(pll, p);
	locked = PLL_RLOCK(pll);
	if (flags & PLLBF_TAIL)
		psclist_add_tail(e, &pll->pll_listhd);
	else
		psclist_add_head(e, &pll->pll_listhd);
	pll->pll_nitems++;
	_psclog_pci(pci, PLL_DEBUG, 0, "lockedlist %p add item %p",
	    pll, p);
	PLL_URLOCK(pll, locked);
}

void *
_pll_get(const struct pfl_callerinfo *pci,
    struct psc_lockedlist *pll, int flags)
{
	int locked;
	void *p;

	locked = PLL_RLOCK(pll);
	if (pll_empty(pll)) {
		PLL_URLOCK(pll, locked);
		return (NULL);
	}
	if (flags & PLLBF_TAIL)
		p = psc_listhd_last_obj2(&pll->pll_listhd, void *,
		    pll->pll_offset);
	else
		p = psc_listhd_first_obj2(&pll->pll_listhd, void *,
		    pll->pll_offset);
	if ((flags & PLLBF_PEEK) == 0)
		_pll_remove(pci, pll, p);
	PLL_URLOCK(pll, locked);
	return (p);
}

int
pll_conjoint(struct psc_lockedlist *pll, void *p)
{
	struct psclist_head *e;
	int locked, conjoint;

	e = _pll_obj2entry(pll, p);
	locked = PLL_RLOCK(pll);
	conjoint = psclist_conjoint(e, &pll->pll_listhd);
	PLL_URLOCK(pll, locked);
	return (conjoint);
}

/**
 * pll_add_sorted - Add an item to a list in its sorted position.
 * @pll: list to add to.
 * @p: item to add.
 * @cmpf: item comparison routine.
 */
void
pll_add_sorted(struct psc_lockedlist *pll, void *p,
    int (*cmpf)(const void *, const void *))
{
	struct psc_listentry *e;
	int locked;

	e = _pll_obj2entry(pll, p);
	locked = PLL_RLOCK(pll);
	if (pll_empty(pll))
		psclist_add_head(e, &pll->pll_listhd);
	else
		psclist_add_sorted(&pll->pll_listhd, e, cmpf,
		    pll->pll_offset);
	psclog(PLL_DEBUG, "lockedlist %p add item %p",
	    pll, p);
	pll->pll_nitems++;
	PLL_URLOCK(pll, locked);
}

/**
 * pll_sort - Sort items in a list.
 * @pll: list to sort.
 * @sortf: sort routine, such as qsort(3) or mergesort(3).
 * @cmpf: comparison routine passed as argument to sortf() which
 *	operates on two pointer-to-a-pointer of type.
 */
void
pll_sort(struct psc_lockedlist *pll, void (*sortf)(void *, size_t,
    size_t, int (*)(const void *, const void *)),
    int (*cmpf)(const void *, const void *))
{
	int locked;
	void *p;

	locked = PLL_RLOCK(pll);
	if (pll->pll_nitems > 1) {
		p = PSCALLOC(pll->pll_nitems * sizeof(p));
		psclist_sort(p, &pll->pll_listhd, pll->pll_nitems,
		    pll->pll_offset, sortf, cmpf);
		PSCFREE(p);
	}
	PLL_URLOCK(pll, locked);
}
