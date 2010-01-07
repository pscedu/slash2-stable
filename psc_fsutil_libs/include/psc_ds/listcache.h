/* $Id$ */
/*
 * %PSC_START_COPYRIGHT%
 * -----------------------------------------------------------------------------
 * Copyright (c) 2006-2009, Pittsburgh Supercomputing Center (PSC).
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
 * List caches are thread-safe lists which allow threads to wait
 * for entries to become available.
 */

#ifndef _PFL_LISTCACHE_H_
#define _PFL_LISTCACHE_H_

#include <sys/types.h>

#include <errno.h>
#include <stdarg.h>
#include <stddef.h>
#include <string.h>
#include <time.h>

#include "psc_ds/list.h"
#include "psc_ds/listguts.h"
#include "psc_ds/lockedlist.h"
#include "psc_util/alloc.h"
#include "psc_util/lock.h"
#include "psc_util/log.h"
#include "psc_util/waitq.h"

extern struct psc_lockedlist	pscListCaches;

struct psc_listcache {
	struct psc_listguts	lc_guts;
	int			lc_flags;

	struct psc_waitq	lc_wq_want;	/* when someone wants an ent */
	struct psc_waitq	lc_wq_empty;	/* when we're empty */
#define lc_index_lentry		lc_guts.plg_index_lentry
#define lc_lock			lc_guts.plg_lock
#define lc_name			lc_guts.plg_name
#define lc_listhd		lc_guts.plg_listhd
#define lc_size			lc_guts.plg_size
#define lc_nseen		lc_guts.plg_nseen
#define lc_entsize		lc_guts.plg_entsize
#define lc_offset		lc_guts.plg_offset
};
typedef struct psc_listcache list_cache_t;

#define PLCF_DYING	(1 << 0)	/* listcache is about to go away */

#define LIST_CACHE_FOREACH(p, plc)				\
	psclist_for_each_entry2((p), &(plc)->lc_listhd,		\
	    (plc)->lc_offset)

#define LIST_CACHE_LOCK(lc)	spinlock(&(lc)->lc_lock)
#define LIST_CACHE_ULOCK(lc)	freelock(&(lc)->lc_lock)
#define LIST_CACHE_TRYLOCK(lc)	trylock(&(lc)->lc_lock)

/**
 * lc_sz - how many items are in here.
 */
static __inline ssize_t
lc_sz(struct psc_listcache *lc)
{
	int locked;
	ssize_t sz;

	locked = reqlock(&lc->lc_lock);
	sz = lc->lc_size;
	ureqlock(&lc->lc_lock, locked);
	return (sz);
}

static __inline void
lc_del(struct psclist_head *e, struct psc_listcache *lc)
{
	int locked;

	locked = reqlock(&lc->lc_lock);
	psc_assert(lc->lc_size > 0);
	psclist_del(e);
	lc->lc_size--;
	ureqlock(&lc->lc_lock, locked);
}

/**
 * lc_remove - remove an item from a list cache.
 * @lc: the list cache.
 * @p: the item containing a psclist_head member.
 */
static __inline void
lc_remove(struct psc_listcache *lc, void *p)
{
	int locked;
	void *e;

	psc_assert(p);
	e = (char *)p + lc->lc_offset;
	locked = reqlock(&lc->lc_lock);
	psc_assert(lc->lc_size > 0);
	psclist_del(e);
	lc->lc_size--;
	ureqlock(&lc->lc_lock, locked);
}

enum psclc_pos {
	PLCP_TAIL,
	PLCP_HEAD
};

/* lc_get() position to grab from */
#define PLCP_STACK	PLCP_HEAD
#define PLCP_QUEUE	PLCP_TAIL

/* lc_get() flags */
#define PLCGF_NOBLOCK	(1 << 0)	/* return NULL if unavail */
#define PLCGF_WARN	(1 << 1)	/* emit messages */
#define PLCGF_PEEK	(1 << 2)	/* don't remove item */

static __inline void *
_lc_get(struct psc_listcache *lc, struct timespec *abstime,
    enum psclc_pos pos, int flags)
{
	struct psclist_head *e;
	int locked, rc;

	psc_assert(pos == PLCP_HEAD || pos ==  PLCP_TAIL);

	locked = reqlock(&lc->lc_lock);
	while (psclist_empty(&lc->lc_listhd)) {
		if ((lc->lc_flags & PLCF_DYING) ||
		    (flags & PLCGF_NOBLOCK)) {
			ureqlock(&lc->lc_lock, locked);
			return (NULL);
		}

		/* Alert listeners who want to know about exhaustion. */
		psc_waitq_wakeall(&lc->lc_wq_want);
		if (abstime)
			psc_logx(flags & PLCGF_WARN ? PLL_WARN : PLL_TRACE,
			    "lc_get(%s:%p): waiting %p", lc->lc_name, lc, abstime);
		else
			psc_logx(flags & PLCGF_WARN ? PLL_WARN : PLL_TRACE,
			    "lc_get(%s:%p): blocking wait", lc->lc_name, lc);
		if (abstime) {
			rc = psc_waitq_waitabs(&lc->lc_wq_empty,
			    &lc->lc_lock, abstime);
			if (rc) {
				psc_assert(rc == ETIMEDOUT);
				errno = rc;
				return (NULL);
			}
		} else
			psc_waitq_wait(&lc->lc_wq_empty, &lc->lc_lock);
		spinlock(&lc->lc_lock);
	}
	e = pos & PLCP_HEAD ?
	    psclist_first(&lc->lc_listhd) :
	    psclist_last(&lc->lc_listhd);
	psc_assert(lc->lc_size > 0);
	if ((flags & PLCGF_PEEK) == 0) {
		psclist_del(e);
		lc->lc_size--;
	}
	ureqlock(&lc->lc_lock, locked);
	if (e)
		return ((char *)e - lc->lc_offset);
	return (NULL);
}

/**
 * lc_gettimed - try to grab an item from the head of a listcache.
 * @lc: the list cache to access.
 * @abstime: timer which tells how long to wait.
 */
#define lc_gettimed(lc, tm)	_lc_get((lc), (tm), PLCP_HEAD, 0)

/**
 * lc_getwait - grab an item from a listcache,
 *	blocking if necessary until an item becomes available.
 * @lc: the list cache to access.
 */
#define lc_getwait(lc)		_lc_get((lc), NULL, PLCP_HEAD, 0)

/**
 * lc_getnb - grab an item from a listcache or NULL if none are available.
 * @lc: the list cache to access.
 */
#define lc_getnb(lc)		_lc_get((lc), NULL, PLCP_HEAD, PLCGF_NOBLOCK)

/**
 * lc_peekhead - peek at head item or NULL if unavailable.
 * @lc: the list cache to access.
 */
#define lc_peekheadwait(lc)	_lc_get((lc), NULL, PLCP_HEAD, PLCGF_PEEK)

/**
 * lc_peektail - peek at tail item or NULL if unavailable.
 * @lc: the list cache to access.
 */
#define lc_peektail(lc)		_lc_get((lc), NULL, PLCP_TAIL, PLCGF_NOBLOCK | PLCGF_PEEK)

/*
 * lc_kill - list wants to go away, notify waiters.
 * @lc: list cache to kill.
 */
static __inline void
lc_kill(struct psc_listcache *lc)
{
	spinlock(&lc->lc_lock);
	lc->lc_flags |= PLCF_DYING;
	psc_waitq_wakeall(&lc->lc_wq_empty);
	freelock(&lc->lc_lock);
}

/* lc_add() flags */
#define PLCAF_DYINGOK	(1 << 0)	/* list can die */

/**
 * lc_put - add an item entry to a list cache.
 * @lc: the list cache to add to.
 * @e: new list item to add.
 * @pos: where to add the item, head or tail of list.
 * @flags: operational behavior.
 */
static __inline int
_lc_put(struct psc_listcache *lc, struct psclist_head *e,
    enum psclc_pos pos, int flags)
{
	int locked;

	psc_assert(pos == PLCP_HEAD || pos == PLCP_TAIL);
	psc_assert(psclist_disjoint(e));

	locked = reqlock(&lc->lc_lock);

	if (lc->lc_flags & PLCF_DYING) {
		psc_assert(flags & PLCAF_DYINGOK);
		ureqlock(&lc->lc_lock, locked);
		return (0);
	}

	if (pos == PLCP_TAIL)
		psclist_xadd_tail(e, &lc->lc_listhd);
	else
		psclist_xadd(e, &lc->lc_listhd);

	lc->lc_size++;
	lc->lc_nseen++;

	ureqlock(&lc->lc_lock, locked);

	/*
	 * There is now an item available; wake up waiters
	 * who think the list is empty.
	 */
	psc_waitq_wakeall(&lc->lc_wq_empty);
	return (1);
}

static __inline int
_lc_add(struct psc_listcache *lc, void *p, enum psclc_pos pos, int flags)
{
	void *e;

	psc_assert(p);
	e = (char *)p + lc->lc_offset;
	if (_lc_put(lc, e, pos, flags))
		return (1);
	psc_assert(flags & PLCAF_DYINGOK);
	return (0);
}

#define lc_queue(lc, e)		((void)_lc_put((lc), (e), PLCP_TAIL, 0))
#define lc_stack(lc, e)		((void)_lc_put((lc), (e), PLCP_HEAD, 0))
#define lc_puthead(lc, e)	((void)_lc_put((lc), (e), PLCP_HEAD, 0))
#define lc_puttail(lc, e)	((void)_lc_put((lc), (e), PLCP_TAIL, 0))
#define lc_put(lc, e)		((void)_lc_put((lc), (e), PLCP_TAIL, 0))

#define lc_addstack(lc, p)	((void)_lc_add((lc), (p), PLCP_HEAD, 0))
#define lc_addqueue(lc, p)	((void)_lc_add((lc), (p), PLCP_TAIL, 0))
#define lc_addhead(lc, p)	((void)_lc_add((lc), (p), PLCP_HEAD, 0))
#define lc_addtail(lc, p)	((void)_lc_add((lc), (p), PLCP_TAIL, 0))
#define lc_add(lc, p)		((void)_lc_add((lc), (p), PLCP_TAIL, 0))

#define lc_add_ifalive(lc, p)	_lc_add((lc), (p), PLCP_TAIL, PLCAF_DYINGOK)

/**
 * lc_move_entry - move an item entry on a list cache to the start or end.
 * @lc: list cache to move on.
 * @e: item entry to move.
 * @pos: where to move the item entry; list head or tail.
 */
static __inline void
lc_move_entry(struct psc_listcache *lc, struct psclist_head *e,
    enum psclc_pos pos)
{
	int locked;

	locked = reqlock(&lc->lc_lock);
	psclist_del(e);
	if (pos == PLCP_TAIL)
		psclist_xadd_tail(e, &lc->lc_listhd);
	else
		psclist_xadd(e, &lc->lc_listhd);
	ureqlock(&lc->lc_lock, locked);
}

/**
 * lc_move - move an item on a list cache to the start or end.
 * @lc: list cache to move on.
 * @p: item to move.
 * @pos: where to move the item; list head or tail.
 */
static __inline void
lc_move(struct psc_listcache *lc, void *p, enum psclc_pos pos)
{
	void *e;

	psc_assert(pos == PLCP_HEAD || pos == PLCP_TAIL);
	psc_assert(p);
	e = (char *)p + lc->lc_offset;
	lc_move_entry(lc, e, pos);
}

#define lc_move2tail(lc, p)	lc_move((lc), (p), PLCP_TAIL)
#define lc_move2head(lc, p)	lc_move((lc), (p), PLCP_HEAD)

static __inline void
_lc_init(struct psc_listcache *lc, ptrdiff_t offset, size_t entsize)
{
	psclg_init(&lc->lc_guts, offset, entsize);
	psc_waitq_init(&lc->lc_wq_empty);
	psc_waitq_init(&lc->lc_wq_want);
}

/**
 * lc_init - initialize a list cache.
 * @lc: the list cache to initialize.
 * @type: type of variable the list will contain.
 * @member: member name in type linking entries together.
 */
#define lc_init(lc, type, member) \
	_lc_init((lc), offsetof(type, member), sizeof(type))

/**
 * lc_vregister - register a list cache for external access.
 * @lc: the list cache to register.
 * @name: printf(3) format of name for list.
 * @ap: variable argument list for printf(3) name argument.
 */
static __inline void
lc_vregister(struct psc_listcache *lc, const char *name, va_list ap)
{
	int rc;

	PLL_LOCK(&pscListCaches);
	spinlock(&lc->lc_lock);

	rc = vsnprintf(lc->lc_name, sizeof(lc->lc_name), name, ap);
	if (rc == -1)
		psc_fatal("vsnprintf");
	else if (rc > (int)sizeof(lc->lc_name))
		psc_fatalx("lc_name is too large (%s)", name);

	pll_add(&pscListCaches, lc);

	freelock(&lc->lc_lock);
	PLL_ULOCK(&pscListCaches);
}

/**
 * lc_register - register a list cache for external access.
 * @lc: the list cache to register.
 * @name: printf(3) format of name for list.
 */
static __inline void
lc_register(struct psc_listcache *lc, const char *name, ...)
{
	va_list ap;

	va_start(ap, name);
	lc_vregister(lc, name, ap);
	va_end(ap);
}

static __inline void
_lc_reginit(struct psc_listcache *lc, ptrdiff_t offset, size_t entsize,
    const char *name, ...)
{
	va_list ap;

	_lc_init(lc, offset, entsize);

	va_start(ap, name);
	lc_vregister(lc, name, ap);
	va_end(ap);
}

/**
 * lc_reginit - initialize and register a list cache.
 * @lc: the list cache.
 * @type: type of variable the list will contain.
 * @member: member name in type linking entries together.
 * @fmt: printf(3) format of name for list.
 */
#define lc_reginit(lc, type, member, fmt, ...) \
	_lc_reginit((lc), offsetof(type, member), sizeof(type), (fmt), ## __VA_ARGS__)

/**
 * lc_unregister - remove list cache external access registration.
 * @lc: the list cache to unregister, must be UNLOCKED.
 */
static __inline void
lc_unregister(struct psc_listcache *lc)
{
	PLL_LOCK(&pscListCaches);
	spinlock(&lc->lc_lock);
	pll_remove(&pscListCaches, lc);
	freelock(&lc->lc_lock);
	PLL_ULOCK(&pscListCaches);
}

/**
 * lc_lookup - find a list cache by its registration name.
 * @name: name of list cache.
 * Notes: returns the list cache locked if found.
 */
static __inline struct psc_listcache *
lc_lookup(const char *name)
{
	struct psc_listcache *lc;

	PLL_LOCK(&pscListCaches);
	psclist_for_each_entry(lc,
	    &pscListCaches.pll_listhd, lc_index_lentry)
		if (strcmp(name, lc->lc_name) == 0) {
			LIST_CACHE_LOCK(lc);
			break;
		}
	PLL_ULOCK(&pscListCaches);
	return (lc);
}

/**
 * lc_empty - determine if the list cache has elements currently.
 * @lc: list cache to check.
 */
static __inline int
lc_empty(struct psc_listcache *lc)
{
	int rc, locked;

	locked = reqlock(&lc->lc_lock);
	rc = psclist_empty(&lc->lc_listhd);
	ureqlock(&lc->lc_lock, locked);
	return (rc);
}

/**
 * lc_sort - sort items in a list cache.
 * @lc: list cache to sort.
 * @sortf: sort routine, such as qsort(3) or mergesort(3).
 * @cmpf: comparision routine passed as argument to sortf().
 */
#define lc_sort(lc, sortf, cmpf)	psclg_sort(&(lc)->lc_guts, (sortf), (cmpf))

#endif /* _PFL_LISTCACHE_H_ */
