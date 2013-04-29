/* $Id$ */
/*
 * %PSC_START_COPYRIGHT%
 * -----------------------------------------------------------------------------
 * Copyright (c) 2007-2013, Pittsburgh Supercomputing Center (PSC).
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

#include <sys/types.h>

#include <stdarg.h>

#include "psc_ds/listcache.h"
#include "psc_ds/lockedlist.h"
#include "psc_util/log.h"

struct psc_lockedlist	psc_listcaches =
    PLL_INIT(&psc_listcaches, struct psc_listcache, plc_lentry);

/**
 * lc_nitems - Grab the number of items present in a list cache.
 * @plc: list cache to inspect.
 */
int
lc_nitems(struct psc_listcache *plc)
{
	int locked;
	ssize_t sz;

	locked = LIST_CACHE_RLOCK(plc);
	sz = plc->plc_nitems;
	LIST_CACHE_URLOCK(plc, locked);
	return (sz);
}

void *
_lc_get(struct psc_listcache *plc, const struct timespec *abstime,
    int flags)
{
	int locked, rc;
	void *p;

	locked = LIST_CACHE_RLOCK(plc);
//	if (plc->plc_flags & PLCF_DYING)
//		psc_assert(flags & PLCF_DYINGOK)
	while (lc_empty(plc)) {
		if ((plc->plc_flags & PLCF_DYING) ||
		    (flags & PLCBF_NOBLOCK)) {
			LIST_CACHE_URLOCK(plc, locked);
			return (NULL);
		}

		/* Alert listeners who want to know about exhaustion. */
		psc_waitq_wakeall(&plc->plc_wq_want);
		if (abstime)
			psclog_debug("lc@%p <%s> timed wait "PSCPRI_TIMESPEC,
			    plc, plc->plc_name, PSCPRI_TIMESPEC_ARGS(abstime));
		else
			psclog_debug("lc@%p <%s> blocking wait", plc,
			    plc->plc_name);
		if (abstime) {
			rc = psc_waitq_waitabs(&plc->plc_wq_empty,
			    &plc->plc_lock, abstime);
			if (rc) {
				psc_assert(rc == ETIMEDOUT);
				errno = rc;
				return (NULL);
			}
		} else
			psc_waitq_wait(&plc->plc_wq_empty, &plc->plc_lock);
		LIST_CACHE_LOCK(plc);
	}
	if (flags & PLCBF_TAIL)
		p = pll_peektail(&plc->plc_pll);
	else
		p = pll_peekhead(&plc->plc_pll);
	if ((flags & PLCBF_PEEK) == 0)
		pll_remove(&plc->plc_pll, p);
	LIST_CACHE_URLOCK(plc, locked);
	return (p);
}

/**
 * lc_kill - List wants to go away; notify waiters.
 * @plc: list cache to kill.
 */
void
lc_kill(struct psc_listcache *plc)
{
	int locked;

	locked = LIST_CACHE_RLOCK(plc);
	plc->plc_flags |= PLCF_DYING;
	psc_waitq_wakeall(&plc->plc_wq_empty);
	LIST_CACHE_URLOCK(plc, locked);
}

/**
 * lc_add - Add an item entry to a list cache.
 * @plc: the list cache to add to.
 * @p: item to add.
 * @flags: PLCBF_* operational behavior flags.
 */
int
_lc_add(struct psc_listcache *plc, void *p, int flags)
{
	int locked;

	locked = LIST_CACHE_RLOCK(plc);

	if (plc->plc_flags & PLCF_DYING) {
		psc_assert(flags & PLCBF_DYINGOK);
		LIST_CACHE_URLOCK(plc, locked);
		return (0);
	}

	if (flags & PLCBF_TAIL)
		pll_addtail(&plc->plc_pll, p);
	else
		pll_addhead(&plc->plc_pll, p);

	plc->plc_nseen++;

	/*
	 * There is now an item available; wake up waiters
	 * who think the list is empty.
	 *
	 * XXX wakeone ??
	 */
	psc_waitq_wakeall(&plc->plc_wq_empty);
	LIST_CACHE_URLOCK(plc, locked);
	return (1);
}

void
_lc_add_sorted(struct psc_listcache *plc, void *p, 
    int (*cmpf)(const void *, const void *))
{
	int locked;

	locked = LIST_CACHE_RLOCK(plc);

	pll_add_sorted(&plc->plc_pll, p, cmpf);
	plc->plc_nseen++;

	psc_waitq_wakeall(&plc->plc_wq_empty);
	LIST_CACHE_URLOCK(plc, locked);
}

void
_lc_add_sorted_backwards(struct psc_listcache *plc, void *p, 
    int (*cmpf)(const void *, const void *))
{
	int locked;

	locked = LIST_CACHE_RLOCK(plc);

	pll_add_sorted_backwards(&plc->plc_pll, p, cmpf);
	plc->plc_nseen++;

	psc_waitq_wakeall(&plc->plc_wq_empty);
	LIST_CACHE_URLOCK(plc, locked);
}

void
_lc_move(struct psc_listcache *plc, void *p, int flags)
{
	int locked;

	locked = LIST_CACHE_RLOCK(plc);
	lc_remove(plc, p);
	if (flags & PLCBF_TAIL)
		pll_addtail(&plc->plc_pll, p);
	else
		pll_addhead(&plc->plc_pll, p);
	LIST_CACHE_URLOCK(plc, locked);
}

void
_lc_init(struct psc_listcache *plc, ptrdiff_t offset)
{
	memset(plc, 0, sizeof(*plc));
	INIT_PSC_LISTENTRY(&plc->plc_lentry);
	_pll_initf(&plc->plc_pll, offset, NULL, 0);
	psc_waitq_init(&plc->plc_wq_empty);
	psc_waitq_init(&plc->plc_wq_want);
}

/**
 * lc_vregister - Register a list cache for external access.
 * @plc: the list cache to register.
 * @name: printf(3) format of name for list.
 * @ap: variable argument list for printf(3) name argument.
 */
void
lc_vregister(struct psc_listcache *plc, const char *name, va_list ap)
{
	int rc;

	PLL_LOCK(&psc_listcaches);
	LIST_CACHE_LOCK(plc);

	rc = vsnprintf(plc->plc_name, sizeof(plc->plc_name), name, ap);
	if (rc == -1)
		psc_fatal("vsnprintf");
	if (rc > (int)sizeof(plc->plc_name))
		psc_fatalx("plc_name is too large (%s)", name);

	pll_addtail(&psc_listcaches, plc);

	LIST_CACHE_ULOCK(plc);
	PLL_ULOCK(&psc_listcaches);
}

/**
 * lc_register - Register a list cache for external access.
 * @plc: the list cache to register.
 * @name: printf(3) format of name for list.
 */
void
lc_register(struct psc_listcache *plc, const char *name, ...)
{
	va_list ap;

	va_start(ap, name);
	lc_vregister(plc, name, ap);
	va_end(ap);
}

void
_lc_reginit(struct psc_listcache *plc, ptrdiff_t offset,
    const char *name, ...)
{
	va_list ap;

	_lc_init(plc, offset);

	va_start(ap, name);
	lc_vregister(plc, name, ap);
	va_end(ap);
}

/**
 * lc_unregister - Remove list cache external access registration.
 * @plc: the list cache to unregister, must be UNLOCKED.
 */
void
lc_unregister(struct psc_listcache *plc)
{
	PLL_LOCK(&psc_listcaches);
	LIST_CACHE_LOCK(plc);
	pll_remove(&psc_listcaches, plc);
	LIST_CACHE_ULOCK(plc);
	PLL_ULOCK(&psc_listcaches);
}

/**
 * lc_lookup - Find a list cache by its registration name.
 * @name: name of list cache.
 * Notes: returns the list cache locked if found.
 */
struct psc_listcache *
lc_lookup(const char *name)
{
	struct psc_listcache *plc;

	PLL_LOCK(&psc_listcaches);
	PLL_FOREACH(plc, &psc_listcaches)
		if (strcmp(name, plc->plc_name) == 0) {
			LIST_CACHE_LOCK(plc);
			break;
		}
	PLL_ULOCK(&psc_listcaches);
	return (plc);
}
