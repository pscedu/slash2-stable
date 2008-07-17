/* $Id$ */

/*
 * Locked lists are lists for use in a multithreaded environment.
 */

#include "psc_ds/list.h"
#include "psc_util/assert.h"
#include "psc_util/atomic.h"
#include "psc_util/lock.h"

struct psc_locked_list {
	struct psclist_head	pll_listhd;
	psc_spinlock_t		pll_lock;
	atomic_t		pll_nitems;
	int			pll_offset;
};

#define pll_init(pll, type, member) \
	_pll_init((pll), offsetof(type, member))

static inline void
_pll_init(struct psc_locked_list *pll, int offset)
{
	PSCLIST_INIT_HEAD(&pll->pll_listhd);
	LOCK_INIT(&pll->pll_lock);
	pll->pll_offset = offset;
	atomic_set(&pll->pll_nitems, 0);
}

#define pll_add(pll, p)		_pll_add((pll), (p), 0)
#define pll_addstack(pll, p)	_pll_add((pll), (p), 0)
#define pll_addqueue(pll, p)	_pll_add((pll), (p), 1)
#define pll_addhead(pll, p)	_pll_add((pll), (p), 0)
#define pll_addtail(pll, p)	_pll_add((pll), (p), 1)

static inline void
_pll_add(struct psc_locked_list *pll, void *p, int tail)
{
	struct psclist_head *e;
	int locked;

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
pll_get(struct psc_locked_list *pll, int tail)
{
	struct psclist_head *e;

	locked = reqlock(&pll->pll_lock);
	if (psclist_empty(&pll->pll)) {
		ureqlock(&pll->pll_lock, locked);
		return (NULL);
	}
	if (tail)
		e = psclist_last(&pll);
	else
		e = psclist_first(&pll);
	psclist_del(e);
	atomic_dec(&pll->pll_nitems);
	ureqlock(&pll->pll_lock, locked);
	return ((char *)e - pll->pll_offset);
}

static inline int
pll_empty(struct psc_locked_list *pll)
{
	int empty;

	locked = reqlock(&pll->pll_lock);
	empty = psclist_empty(&pll->pll_lock);
	ureqlock(&pll->pll_lock, locked);
	return (empty);
}

#define PLL_LOCK(pll)	spinlock((pll)->pll_lock)
#define PLL_ULOCK(pll)	freelock((pll)->pll_lock)
