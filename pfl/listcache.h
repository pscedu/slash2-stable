/* $Id$ */
/*
 * %PSC_START_COPYRIGHT%
 * -----------------------------------------------------------------------------
 * Copyright (c) 2006-2015, Pittsburgh Supercomputing Center (PSC).
 *
 * Permission to use, copy, modify, and distribute this software
 * for any purpose with or without fee is hereby granted, provided
 * that the above copyright notice and this permission notice
 * appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL
 * WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS.  IN NO EVENT SHALL
 * THE AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
 * LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT,
 * NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * Pittsburgh Supercomputing Center	phone: 412.268.4960  fax: 412.268.5832
 * 300 S. Craig Street			e-mail: remarks@psc.edu
 * Pittsburgh, PA 15213			web: http://www.psc.edu/
 * -----------------------------------------------------------------------------
 * %PSC_END_COPYRIGHT%
 */

/*
 * List caches are thread-safe lists which allow threads to wait for
 * items to become available.
 */

#ifndef _PFL_LISTCACHE_H_
#define _PFL_LISTCACHE_H_

#include <sys/types.h>

#include <errno.h>
#include <stdarg.h>
#include <stddef.h>
#include <string.h>
#include <time.h>

#include "pfl/explist.h"
#include "pfl/list.h"
#include "pfl/lockedlist.h"
#include "pfl/alloc.h"
#include "pfl/lock.h"
#include "pfl/log.h"
#include "pfl/waitq.h"

struct psc_listcache {
	struct psc_explist		plc_explist;
	struct psc_waitq		plc_wq_want;	/* when someone wants an item */
	struct psc_waitq		plc_wq_empty;	/* when we're empty */
#define plc_flags	plc_explist.pexl_flags
#define plc_lentry	plc_explist.pexl_lentry
#define plc_listhd	plc_explist.pexl_listhd
#define plc_lock	plc_explist.pexl_lock
#define plc_lockp	plc_explist.pexl_lockp
#define plc_name	plc_explist.pexl_name
#define plc_nitems	plc_explist.pexl_nitems
#define plc_nseen	plc_explist.pexl_nseen
#define plc_offset	plc_explist.pexl_offset
#define plc_pll		plc_explist.pexl_pll
};

/* lc_flags */
#define PLCF_DYING			(_PLLF_FLSHFT << 0)	/* listcache is about to go away */

#define LIST_CACHE_FOREACH(p, plc)					\
	PLL_FOREACH((p), &(plc)->plc_pll)

#define LIST_CACHE_FOREACH_BACKWARDS(p, plc)				\
	PLL_FOREACH_BACKWARDS((p), &(plc)->plc_pll)

#define LIST_CACHE_FOREACH_SAFE(p, t, plc)				\
	PLL_FOREACH_SAFE((p), (t), &(plc)->plc_pll)

#define LIST_CACHE_FOREACH_BACKWARDS_SAFE(p, t, plc)		\
	PLL_FOREACH_BACKWARDS_SAFE((p), (t), &(plc)->plc_pll)

#define LIST_CACHE_GETLOCK(plc)		_PLL_GETLOCK(&(plc)->plc_pll)

#define LIST_CACHE_LOCK(plc)		PLL_LOCK(&(plc)->plc_pll)
#define LIST_CACHE_ULOCK(plc)		PLL_ULOCK(&(plc)->plc_pll)
#define LIST_CACHE_RLOCK(plc)		PLL_RLOCK(&(plc)->plc_pll)
#define LIST_CACHE_URLOCK(plc, lk)	PLL_URLOCK(&(plc)->plc_pll, (lk))
#define LIST_CACHE_TRYLOCK(plc)		PLL_TRYLOCK(&(plc)->plc_pll)
#define LIST_CACHE_LOCK_ENSURE(plc)	PLL_LOCK_ENSURE(&(plc)->plc_pll)

/**
 * lc_empty - Determine if a list cache has no elements.
 * @plc: list cache to check.
 */
#define lc_empty(plc)			(lc_nitems(plc) == 0)

/**
 * lc_remove - Remove an item from a list cache.
 * @plc: the list cache.
 * @p: the item.
 */
#define lc_remove(plc, p)		pll_remove(&(plc)->plc_pll, (p))

/* list cache behavior flags */
#define PLCBF_HEAD	0
#define PLCBF_TAIL	(1 << 0)
#define PLCBF_NOBLOCK	(1 << 1)	/* return NULL if unavail */
#define PLCBF_PEEK	(1 << 2)	/* don't remove item */
#define PLCBF_DYINGOK	(1 << 3)	/* list can die */
#define PLCBF_REVERSE	(1 << 4)	/* order is reversed */

#define lc_addstack(plc, p)		((void)_lc_add((plc), (p), PLCBF_HEAD, NULL))
#define lc_addqueue(plc, p)		((void)_lc_add((plc), (p), PLCBF_TAIL, NULL))
#define lc_addhead(plc, p)		((void)_lc_add((plc), (p), PLCBF_HEAD, NULL))
#define lc_addtail(plc, p)		((void)_lc_add((plc), (p), PLCBF_TAIL, NULL))
#define lc_add(plc, p)			((void)_lc_add((plc), (p), PLCBF_TAIL, NULL))

#define lc_add_sorted(plc, p, f)	_lc_add((plc), (p), 0, (f))
#define lc_add_sorted_backwards(plc, p, f)				\
					_lc_add((plc), (p), PLCBF_REVERSE, (f))

#define lc_addtail_ifalive(plc, p)	_lc_add((plc), (p), PLCBF_TAIL | PLCBF_DYINGOK, NULL)

#define lc_gettimed(plc, tm)		_lc_get((plc), (tm), PLCBF_HEAD)
#define lc_getwait(plc)			_lc_get((plc), NULL, PLCBF_HEAD)
#define lc_getnb(plc)			_lc_get((plc), NULL, PLCBF_HEAD | PLCBF_NOBLOCK)
#define lc_peekheadtimed(plc, tm)	_lc_get((plc), (tm), PLCBF_HEAD | PLCBF_PEEK)
#define lc_peekheadwait(plc)		_lc_get((plc), NULL, PLCBF_HEAD | PLCBF_PEEK)
#define lc_peekhead(plc)		_lc_get((plc), NULL, PLCBF_HEAD | PLCBF_NOBLOCK | PLCBF_PEEK)
#define lc_peektail(plc)		_lc_get((plc), NULL, PLCBF_TAIL | PLCBF_NOBLOCK | PLCBF_PEEK)

/**
 * lc_move2head - Move an item on a list cache to the list head.
 * @plc: list cache to move on.
 * @p: item to move.
 */
#define lc_move2head(plc, p)		_lc_move((plc), (p), PLCBF_HEAD)
#define lc_move2tail(plc, p)		_lc_move((plc), (p), PLCBF_TAIL)

/**
 * lc_init - Initialize a list cache.
 * @plc: the list cache to initialize.
 * @type: type of variable the list will contain.
 * @member: member name in type linking entries together.
 */
#define lc_init(plc, type, member)					\
	_lc_init((plc), offsetof(type, member))

/**
 * lc_reginit - Initialize and register a list cache.
 * @plc: the list cache.
 * @type: type of variable the list will contain.
 * @member: structure member name in type linking entries together.
 * @fmt: printf(3) format of name for list.
 */
#define lc_reginit(plc, type, member, fmt, ...)				\
	_lc_reginit((plc), offsetof(type, member), (fmt), ## __VA_ARGS__)

/**
 * lc_sort - Sort items in a list cache.
 * @plc: list cache to sort.
 * @sortf: sort routine, such as qsort(3) or mergesort(3).
 * @cmpf: comparision routine passed as argument to sortf().
 */
#define lc_sort(plc, sortf, cmpf)	pll_sort(&(plc)->plc_pll, (sortf), (cmpf))

#define lc_conjoint(plc, p)		pll_conjoint(&(plc)->plc_pll, (p))

struct psc_listcache *
	  lc_lookup(const char *);
int	 _lc_add(struct psc_listcache *, void *, int, void *);
void	*_lc_get(struct psc_listcache *, const struct timespec *, int);
void	 _lc_init(struct psc_listcache *, ptrdiff_t);
void	  lc_kill(struct psc_listcache *);
void	 _lc_move(struct psc_listcache *, void *, int);
int	  lc_nitems(struct psc_listcache *);
void	 _lc_reginit(struct psc_listcache *, ptrdiff_t, const char *, ...);
void	  lc_register(struct psc_listcache *, const char *, ...);
void	  lc_unregister(struct psc_listcache *);
void	  lc_vregister(struct psc_listcache *, const char *, va_list);

void	 _lc_add_sorted(struct psc_listcache *, void *, int (*)(const void *, const void *));
void	 _lc_add_sorted_backwards(struct psc_listcache *, void *, int (*)(const void *, const void *));

extern struct psc_lockedlist	psc_listcaches;

#endif /* _PFL_LISTCACHE_H_ */
