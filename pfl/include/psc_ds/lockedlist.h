/* $Id$ */
/*
 * %PSC_START_COPYRIGHT%
 * -----------------------------------------------------------------------------
 * Copyright (c) 2006-2012, Pittsburgh Supercomputing Center (PSC).
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

#include "psc_ds/list.h"
#include "psc_util/lock.h"
#include "psc_util/log.h"

struct psc_lockedlist {
	struct psclist_head	 pll_listhd;		/* this must be first */
	int			 pll_nitems;		/* # items on list */
	int			 pll_flags;		/* see PLLF_* below */
	ptrdiff_t		 pll_offset;		/* offset into structure for linkage */
	union {
		psc_spinlock_t	 pllu_lock;
		psc_spinlock_t	*pllu_lockp;
	} pll_u;
#define pll_lockp	pll_u.pllu_lockp
#define pll_lock	pll_u.pllu_lock
};

#define PLLF_EXTLOCK		(1 << 0)		/* lock is external */
#define _PLLF_FLSHFT		(1 << 2)		/* last flag; for extending */

#define _PLL_GETLOCK(pll)	((pll)->pll_flags & PLLF_EXTLOCK ?	\
				 (pll)->pll_lockp : &(pll)->pll_lock)

#define PLL_LOCK(pll)		spinlock(_PLL_GETLOCK(pll))
#define PLL_ULOCK(pll)		freelock(_PLL_GETLOCK(pll))
#define PLL_TRYLOCK(pll)	trylock(_PLL_GETLOCK(pll))
#define PLL_RLOCK(pll)		reqlock(_PLL_GETLOCK(pll))
#define PLL_TRYRLOCK(pll, lkd)	tryreqlock(_PLL_GETLOCK(pll), (lkd))
#define PLL_URLOCK(pll, lkd)	ureqlock(_PLL_GETLOCK(pll), (lkd))
#define PLL_ENSURE_LOCKED(pll)	LOCK_ENSURE(_PLL_GETLOCK(pll))
#define PLL_HASLOCK(pll)	psc_spin_haslock(_PLL_GETLOCK(pll))

#define PLL_FOREACH(p, pll)						\
	psclist_for_each_entry2((p), &(pll)->pll_listhd, (pll)->pll_offset)

#define PLL_FOREACH_SAFE(p, t, pll)					\
	psclist_for_each_entry2_safe((p), (t), &(pll)->pll_listhd,	\
	    (pll)->pll_offset)

#define PLL_FOREACH_BACKWARDS(p, pll)					\
	psclist_for_each_entry2_backwards((p), &(pll)->pll_listhd,	\
	    (pll)->pll_offset)

#define PLL_FOREACH_BACKWARDS_SAFE(p, t, pll)				\
	psclist_for_each_entry2_backwards_safe((p), (t), &(pll)->pll_listhd, \
	    (pll)->pll_offset)

#define PLL_FOREACH_CONT(p, pll)					\
	psclist_for_each_entry2_cont((p), &(pll)->pll_listhd, (pll)->pll_offset)

#define PLL_INIT(pll, type, member)					\
	{ PSCLIST_HEAD_INIT((pll)->pll_listhd), 0, 0,			\
	  offsetof(type, member), { SPINLOCK_INIT } }

#define PLL_INIT_NOLOG(pll, type, member)				\
	{ PSCLIST_HEAD_INIT((pll)->pll_listhd), 0, 0,			\
	  offsetof(type, member), { SPINLOCK_INIT_NOLOG } }

#define PLL_INIT_LOGTMP(pll, type, member)				\
	{ PSCLIST_HEAD_INIT((pll)->pll_listhd), 0, 0,			\
	  offsetof(type, member), { SPINLOCK_INIT_LOGTMP } }

#define pll_next_item(pll, p)						\
	psclist_next_obj2(&(pll)->pll_listhd, (p), (pll)->pll_offset)

#define pll_last_item(pll, type)					\
	psc_listhd_last_obj2(&(pll)->pll_listhd, type, (pll)->pll_offset)

#define pll_init(pll, type, member, lock)				\
	_pll_initf((pll), offsetof(type, member), (lock), 0)

#define pll_initf(pll, type, member, lock, flags)			\
	_pll_initf((pll), offsetof(type, member), (lock), (flags))

/* list initialization flags */
#define PLLIF_LOGTMP		(1 << 0)

#define pll_empty(pll)		(pll_nitems(pll) == 0)

/* list  flags */
#define PLLBF_HEAD		0
#define PLLBF_TAIL		(1 << 1)
#define PLLBF_PEEK		(1 << 2)

#define pll_addstack(pll, p)	_pll_add(PFL_CALLERINFO(), (pll), (p), PLLBF_HEAD)
#define pll_addqueue(pll, p)	_pll_add(PFL_CALLERINFO(), (pll), (p), PLLBF_TAIL)
#define pll_addhead(pll, p)	_pll_add(PFL_CALLERINFO(), (pll), (p), PLLBF_HEAD)
#define pll_addtail(pll, p)	_pll_add(PFL_CALLERINFO(), (pll), (p), PLLBF_TAIL)
#define pll_add(pll, p)		_pll_add(PFL_CALLERINFO(), (pll), (p), PLLBF_TAIL)

#define pll_gethead(pll)	_pll_get((pll), PLLBF_HEAD)
#define pll_gettail(pll)	_pll_get((pll), PLLBF_TAIL)
#define pll_getstack(pll)	_pll_get((pll), PLLBF_HEAD)
#define pll_getqueue(pll)	_pll_get((pll), PLLBF_HEAD)
#define pll_get(pll)		_pll_get((pll), PLLBF_HEAD)
#define pll_peekhead(pll)	_pll_get((pll), PLLBF_HEAD | PLLBF_PEEK)
#define pll_peektail(pll)	_pll_get((pll), PLLBF_TAIL | PLLBF_PEEK)

void  _pll_add(const struct pfl_callerinfo *, struct psc_lockedlist *, void *, int);
void   pll_add_sorted(struct psc_lockedlist *, void *, int (*)(const
	    void *, const void *));
int    pll_conjoint(struct psc_lockedlist *, void *);
void *_pll_get(struct psc_lockedlist *, int);
void  _pll_initf(struct psc_lockedlist *, int, psc_spinlock_t *, int);
void   pll_remove(struct psc_lockedlist *, void *);
void   pll_sort(struct psc_lockedlist *, void (*)(void *, size_t,
	    size_t, int (*)(const void *, const void *)), int (*)(const void *,
	    const void *));

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

static __inline int
pll_nitems(struct psc_lockedlist *pll)
{
	int n, locked;

	locked = PLL_RLOCK(pll);
	n = pll->pll_nitems;
	PLL_URLOCK(pll, locked);
	return (n);
}

static __inline int
psc_listhd_empty_locked(psc_spinlock_t *lk, struct psclist_head *hd)
{
	int locked, empty;

	locked = reqlock(lk);
	empty = psc_listhd_empty(hd);
	ureqlock(lk, locked);
	return (empty);
}

#endif /* _PFL_LOCKEDLIST_H_ */
