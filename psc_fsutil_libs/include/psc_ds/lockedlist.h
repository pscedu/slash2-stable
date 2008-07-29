/* $Id$ */

/*
 * Locked lists are lists for use in a multithreaded environment.
 */

#ifndef __PFL_LOCKEDLIST_H__
#define __PFL_LOCKEDLIST_H__

#include <stddef.h>

#include "psc_ds/list.h"
#include "psc_util/assert.h"
#include "psc_util/atomic.h"
#include "psc_util/lock.h"

struct psc_lockedlist {
	struct psclist_head	pll_listhd;
	psc_spinlock_t		pll_lock;
	atomic_t		pll_nitems;
	int			pll_offset;
};

#define PLL_INITIALIZER(pll, type, member) \
	{ PSCLIST_HEAD_INIT((pll)->pll_listhd), LOCK_INITIALIZER, \
	  ATOMIC_INIT(0), offsetof(type, member) }

#define pll_init(pll, type, member) \
	_pll_init((pll), offsetof(type, member))

static inline void
_pll_init(struct psc_lockedlist *pll, int offset)
{
	INIT_PSCLIST_HEAD(&pll->pll_listhd);
	LOCK_INIT(&pll->pll_lock);
	pll->pll_offset = offset;
	atomic_set(&pll->pll_nitems, 0);
}

#define pll_nitems(pll)		atomic_read(&(pll)->pll_nitems)

#define pll_add(pll, p)		_pll_add((pll), (p), 0)
#define pll_addstack(pll, p)	_pll_add((pll), (p), 0)
#define pll_addqueue(pll, p)	_pll_add((pll), (p), 1)
#define pll_addhead(pll, p)	_pll_add((pll), (p), 0)
#define pll_addtail(pll, p)	_pll_add((pll), (p), 1)

static inline void
_pll_add(struct psc_lockedlist *pll, void *p, int tail)
{
	int locked;
	void *e;

	psc_assert(p);
	e = (char *)p + pll->pll_offset;
	locked = reqlock(&pll->pll_lock);
	if (tail)
		psclist_xadd_tail(e, &pll->pll_listhd);
	else
		psclist_xadd_head(e, &pll->pll_listhd);
	atomic_inc(&pll->pll_nitems);
	ureqlock(&pll->pll_lock, locked);
}

static inline void *
pll_get(struct psc_lockedlist *pll, ptrdiff_t tail)
{
	struct psclist_head *e;
	int locked;

	locked = reqlock(&pll->pll_lock);
	if (psclist_empty(&pll->pll_listhd)) {
		ureqlock(&pll->pll_lock, locked);
		return (NULL);
	}
	if (tail)
		e = psclist_last(&pll->pll_listhd);
	else
		e = psclist_first(&pll->pll_listhd);
	psclist_del(e);
	atomic_dec(&pll->pll_nitems);
	ureqlock(&pll->pll_lock, locked);
	return ((char *)e - pll->pll_offset);
}

static inline void
pll_remove(struct psc_lockedlist *pll, void *p)
{
	int locked;
	void *e;

	psc_assert(p);
	e = (char *)p + pll->pll_offset;
	locked = reqlock(&pll->pll_lock);
	psclist_del(e);
	ureqlock(&pll->pll_lock, locked);
}

static inline int
pll_empty(struct psc_lockedlist *pll)
{
	int locked, empty;

	locked = reqlock(&pll->pll_lock);
	empty = psclist_empty(&pll->pll_listhd);
	ureqlock(&pll->pll_lock, locked);
	return (empty);
}

#define PLL_LOCK(pll)	spinlock(&(pll)->pll_lock)
#define PLL_ULOCK(pll)	freelock(&(pll)->pll_lock)

#endif /* __PFL_LOCKEDLIST_H__ */
