/* $Id$ */
/*
 * %PSC_START_COPYRIGHT%
 * -----------------------------------------------------------------------------
 * Copyright (c) 2006-2010, Pittsburgh Supercomputing Center (PSC).
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

/*
 * Locked lists are thread-safe linked lists.
 */

#ifndef _PFL_LOCKEDLIST_H_
#define _PFL_LOCKEDLIST_H_

#include <sys/types.h>

#include <stddef.h>
#include <string.h>

#include "psc_ds/list.h"
#include "psc_util/alloc.h"
#include "psc_util/lock.h"
#include "psc_util/log.h"

struct psc_lockedlist {
	struct psclist_head	 pll_listhd;			/* this must be first */
	int			 pll_nitems;			/* # items on list */
	int			 pll_flags;			/* see PLLF_* below */
	ptrdiff_t		 pll_offset;			/* offset of the sub-structure linkage */
	union {
		psc_spinlock_t	 pllu_lock;
		psc_spinlock_t	*pllu_lockp;
	} pll_u;
#define pll_lockp	pll_u.pllu_lockp
#define pll_lock	pll_u.pllu_lock
};

#define PLLF_EXTLOCK		(1 << 0)			/* lock is external */
#define _PLLF_FLSHFT		(1 << 1)

#define PLL_GETLOCK(pll)	((pll)->pll_flags & PLLF_EXTLOCK ?	\
				 (pll)->pll_lockp : &(pll)->pll_lock)

#define PLL_LOCK(pll)		spinlock(PLL_GETLOCK(pll))
#define PLL_ULOCK(pll)		freelock(PLL_GETLOCK(pll))
#define PLL_TRYLOCK(pll)	trylock(PLL_GETLOCK(pll))
#define PLL_RLOCK(pll)		reqlock(PLL_GETLOCK(pll))
#define PLL_URLOCK(pll, lk)	ureqlock(PLL_GETLOCK(pll), (lk))
#define PLL_ENSURE_LOCKED(pll)	LOCK_ENSURE(PLL_GETLOCK(pll))

#define PLL_FOREACH(p, pll)						\
	psclist_for_each_entry2((p), &(pll)->pll_listhd, (pll)->pll_offset)

#define PLL_FOREACH_SAFE(p, t, pll)					\
	psclist_for_each_entry2_safe((p), (t), &(pll)->pll_listhd,	\
	    (pll)->pll_offset)

#define PLL_FOREACH_BACKWARDS(p, pll)					\
	psclist_for_each_entry2_backwards((p), &(pll)->pll_listhd,	\
	    (pll)->pll_offset)

#define PLL_INIT(pll, type, member)					\
	{ PSCLIST_HEAD_INIT((pll)->pll_listhd), 0, 0,			\
	  offsetof(type, member), { LOCK_INITIALIZER } }

static __inline struct psc_listentry *
_pll_obj2entry(struct psc_lockedlist *pll, void *p)
{
	psc_assert(p);
	return ((void *)((char *)p + pll->pll_offset));
}

static __inline void *
_pll_entry2obj(struct psc_lockedlist *pll, struct psc_listentry *e)
{
	psc_assert(e);
	return ((char *)e - pll->pll_offset);
}

static __inline void
_pll_init(struct psc_lockedlist *pll, int offset, psc_spinlock_t *lkp)
{
	memset(pll, 0, sizeof(*pll));
	INIT_PSCLIST_HEAD(&pll->pll_listhd);
	if (lkp) {
		pll->pll_flags |= PLLF_EXTLOCK;
		pll->pll_lockp = lkp;
	} else
		INIT_SPINLOCK(&pll->pll_lock);
	pll->pll_offset = offset;
}

#define pll_init(pll, type, member, lock)				\
	_pll_init((pll), offsetof(type, member), (lock))

static __inline int
pll_nitems(struct psc_lockedlist *pll)
{
	int n, locked;

	locked = PLL_RLOCK(pll);
	n = pll->pll_nitems;
	PLL_URLOCK(pll, locked);
	return (n);
}

#define pll_empty(pll)			(pll_nitems(pll) == 0)

static __inline void
pll_remove(struct psc_lockedlist *pll, void *p)
{
	struct psc_listentry *e;
	int locked;

	e = _pll_obj2entry(pll, p);
	locked = PLL_RLOCK(pll);
	psclist_del(e, &pll->pll_listhd);
	psc_assert(pll->pll_nitems > 0);
	pll->pll_nitems--;
	PLL_URLOCK(pll, locked);
}

#define PLLBF_HEAD	0
#define PLLBF_TAIL	(1 << 1)
#define PLLBF_PEEK	(1 << 2)

static __inline void
_pll_add(struct psc_lockedlist *pll, void *p, int flags)
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
	PLL_URLOCK(pll, locked);
}

#define pll_addstack(pll, p)	_pll_add((pll), (p), PLLBF_HEAD)
#define pll_addqueue(pll, p)	_pll_add((pll), (p), PLLBF_TAIL)
#define pll_addhead(pll, p)	_pll_add((pll), (p), PLLBF_HEAD)
#define pll_addtail(pll, p)	_pll_add((pll), (p), PLLBF_TAIL)
#define pll_add(pll, p)		_pll_add((pll), (p), PLLBF_TAIL)

static __inline void *
_pll_get(struct psc_lockedlist *pll, int flags)
{
	int locked;
	void *p;

	locked = PLL_RLOCK(pll);
	if (pll_empty(pll)) {
		PLL_URLOCK(pll, locked);
		return (NULL);
	}
	if (flags & PLLBF_TAIL)
		p = psc_listhd_last_obj2(&pll->pll_listhd, void *, pll->pll_offset);
	else
		p = psc_listhd_first_obj2(&pll->pll_listhd, void *, pll->pll_offset);
	if ((flags & PLLBF_PEEK) == 0)
		pll_remove(pll, p);
	PLL_URLOCK(pll, locked);
	return (p);
}

#define pll_gethead(pll)	_pll_get((pll), PLLBF_HEAD)
#define pll_gettail(pll)	_pll_get((pll), PLLBF_TAIL)
#define pll_getstack(pll)	_pll_get((pll), PLLBF_HEAD)
#define pll_getqueue(pll)	_pll_get((pll), PLLBF_HEAD)
#define pll_get(pll)		_pll_get((pll), PLLBF_HEAD)
#define pll_peekhead(pll)	_pll_get((pll), PLLBF_HEAD | PLLBF_PEEK)
#define pll_peektail(pll)	_pll_get((pll), PLLBF_TAIL | PLLBF_PEEK)

static __inline int
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
static __inline void
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
static __inline void
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

#endif /* _PFL_LOCKEDLIST_H_ */
