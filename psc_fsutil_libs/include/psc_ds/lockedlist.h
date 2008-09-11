/* $Id$ */

/*
 * Locked lists are lists for use in a multithreaded environment.
 */

#ifndef __PFL_LOCKEDLIST_H__
#define __PFL_LOCKEDLIST_H__

#include <stddef.h>

#include "psc_ds/list.h"
#include "psc_util/alloc.h"
#include "psc_util/assert.h"
#include "psc_util/atomic.h"
#include "psc_util/lock.h"

struct psc_lockedlist {
	struct psclist_head	 pll_listhd;
	psc_spinlock_t		*pll_lockp;
	atomic_t		 pll_nitems;
	int			 pll_offset;
	int			 pll_flags;
};

#define PLLF_ALLOCLOCK	(1 << 0)	/* list alloc'd its own lock */

#define PLL_INITIALIZER(pll, type, member, lock)		\
	{ PSCLIST_HEAD_INIT((pll)->pll_listhd), (lock),		\
	  ATOMIC_INIT(0), offsetof(type, member), 0 }

#define pll_init(pll, type, member, lock)			\
	_pll_init((pll), offsetof(type, member), (lock))

static __inline void
_pll_init(struct psc_lockedlist *pll, int offset, psc_spinlock_t *lkp)
{
	INIT_PSCLIST_HEAD(&pll->pll_listhd);
	pll->pll_flags = 0;
	if (lkp)
		pll->pll_lockp = lkp;
	else {
		pll->pll_lockp = PSCALLOC(sizeof(*pll->pll_lockp));
		LOCK_INIT(pll->pll_lockp);
		pll->pll_flags |= PLLF_ALLOCLOCK;
	}
	pll->pll_offset = offset;
	atomic_set(&pll->pll_nitems, 0);
}

static __inline void
pll_destroy(struct psc_lockedlist *pll)
{
	if (pll->pll_flags & PLLF_ALLOCLOCK)
		free(pll->pll_lockp);
}

#define pll_nitems(pll)		atomic_read(&(pll)->pll_nitems)

#define pll_add(pll, p)		_pll_add((pll), (p), 0)
#define pll_addstack(pll, p)	_pll_add((pll), (p), 0)
#define pll_addqueue(pll, p)	_pll_add((pll), (p), 1)
#define pll_addhead(pll, p)	_pll_add((pll), (p), 0)
#define pll_addtail(pll, p)	_pll_add((pll), (p), 1)

static __inline void
_pll_add(struct psc_lockedlist *pll, void *p, int tail)
{
	int locked;
	void *e;

	psc_assert(p);
	e = (char *)p + pll->pll_offset;
	locked = reqlock(pll->pll_lockp);
	if (tail)
		psclist_xadd_tail(e, &pll->pll_listhd);
	else
		psclist_xadd_head(e, &pll->pll_listhd);
	atomic_inc(&pll->pll_nitems);
	ureqlock(pll->pll_lockp, locked);
}

static __inline void *
pll_get(struct psc_lockedlist *pll, ptrdiff_t tail)
{
	struct psclist_head *e;
	int locked;

	locked = reqlock(pll->pll_lockp);
	if (psclist_empty(&pll->pll_listhd)) {
		ureqlock(pll->pll_lockp, locked);
		return (NULL);
	}
	if (tail)
		e = psclist_last(&pll->pll_listhd);
	else
		e = psclist_first(&pll->pll_listhd);
	psclist_del(e);
	atomic_dec(&pll->pll_nitems);
	ureqlock(pll->pll_lockp, locked);
	return ((char *)e - pll->pll_offset);
}

static __inline void
pll_remove(struct psc_lockedlist *pll, void *p)
{
	int locked;
	void *e;

	psc_assert(p);
	e = (char *)p + pll->pll_offset;
	locked = reqlock(pll->pll_lockp);
	psclist_del(e);
	atomic_dec(&pll->pll_nitems);
	ureqlock(pll->pll_lockp, locked);
}

static __inline int
pll_empty(struct psc_lockedlist *pll)
{
	int locked, empty;

	locked = reqlock(pll->pll_lockp);
	empty = psclist_empty(&pll->pll_listhd);
	ureqlock(pll->pll_lockp, locked);
	return (empty);
}

#define PLL_LOCK(pll)	spinlock((pll)->pll_lockp)
#define PLL_ULOCK(pll)	freelock((pll)->pll_lockp)

#endif /* __PFL_LOCKEDLIST_H__ */
