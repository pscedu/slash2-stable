/* $Id$ */
/*
 * %ISC_START_LICENSE%
 * ---------------------------------------------------------------------
 * Copyright 2015, Google, Inc.
 * Copyright (c) 2007-2015, Pittsburgh Supercomputing Center (PSC).
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

#include <sys/types.h>

#include <stdarg.h>

#include "pfl/opstats.h"
#include "pfl/listcache.h"
#include "pfl/lockedlist.h"
#include "pfl/log.h"

struct psc_lockedlist	psc_listcaches =
    PLL_INIT(&psc_listcaches, struct psc_listcache, plc_lentry);

/*
 * Grab the number of items present in a list cache.
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
			    LIST_CACHE_GETLOCK(plc), abstime);
			if (rc) {
				psc_assert(rc == ETIMEDOUT);
				errno = rc;
				return (NULL);
			}
		} else
			psc_waitq_wait(&plc->plc_wq_empty,
			    LIST_CACHE_GETLOCK(plc));
		LIST_CACHE_LOCK(plc);
	}
	if (flags & PLCBF_TAIL)
		p = pll_peektail(&plc->plc_pll);
	else
		p = pll_peekhead(&plc->plc_pll);
	if ((flags & PLCBF_PEEK) == 0)
		lc_remove(plc, p);
	LIST_CACHE_URLOCK(plc, locked);
	return (p);
}

/*
 * List wants to go away; notify waiters.
 * @plc: list cache to kill.
 */
void
lc_kill(struct psc_listcache *plc)
{
	int locked;

	locked = LIST_CACHE_RLOCK(plc);
	plc->plc_flags |= PLCF_DYING;
	psc_waitq_wakeall(&plc->plc_wq_empty);
	psc_waitq_wakeall(&plc->plc_wq_want);
	LIST_CACHE_URLOCK(plc, locked);
}

/*
 * Add an item entry to a list cache.
 * @plc: the list cache to add to.
 * @p: item to add.
 * @flags: PLCBF_* operational behavior flags.
 */
int
_lc_add(struct psc_listcache *plc, void *p, int flags, void *cmpf)
{
	int locked;

	locked = LIST_CACHE_RLOCK(plc);

	if (plc->plc_flags & PLCF_DYING) {
		psc_assert(flags & PLCBF_DYINGOK);
		LIST_CACHE_URLOCK(plc, locked);
		return (0);
	}

	if (cmpf && (flags & PLCBF_REVERSE))
		pll_add_sorted_backwards(&plc->plc_pll, p, cmpf);
	else if (cmpf)
		pll_add_sorted(&plc->plc_pll, p, cmpf);
	else if (flags & PLCBF_TAIL)
		pll_addtail(&plc->plc_pll, p);
	else
		pll_addhead(&plc->plc_pll, p);

	if (plc->plc_nseen)
		pfl_opstat_incr(plc->plc_nseen);

	/*
	 * There is now an item available; wake up waiters who think the
	 * list is empty.
	 */
	if (flags & PLCBF_WAKEALL)
		psc_waitq_wakeall(&plc->plc_wq_empty);
	else
		psc_waitq_wakeone(&plc->plc_wq_empty);
	LIST_CACHE_URLOCK(plc, locked);
	return (1);
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
_lc_init(struct psc_listcache *plc, const char *name, ptrdiff_t offset)
{
	char tmpname[MAX_WQ_NAME];

	memset(plc, 0, sizeof(*plc));
	INIT_PSC_LISTENTRY(&plc->plc_lentry);
	_pll_initf(&plc->plc_pll, offset, NULL, 0);
	snprintf(tmpname, MAX_WQ_NAME, "%s-empty", name);
	psc_waitq_init(&plc->plc_wq_empty, tmpname);
	snprintf(tmpname, MAX_WQ_NAME, "%s-want", name);
	psc_waitq_init(&plc->plc_wq_want, tmpname);
}

void
pfl_listcache_destroy(struct psc_listcache *plc)
{
	psc_waitq_destroy(&plc->plc_wq_empty);
	psc_waitq_destroy(&plc->plc_wq_want);
}

void
pfl_listcache_destroy_registered(struct psc_listcache *plc)
{
	lc_unregister(plc);
	pfl_listcache_destroy(plc);
}

int
lc_cmp(const void *a, const void *b)
{
	const struct psc_listcache *ma = a, *mb = b;

	return (strcmp(ma->plc_name, mb->plc_name));
}

/*
 * Register a list cache for external access.
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

	plc->plc_nseen = pfl_opstat_initf(OPSTF_BASE10,
	    "listcache.%s.adds", plc->plc_name);
	plc->plc_st_removes = pfl_opstat_initf(OPSTF_BASE10,
	    "listcache.%s.removes", plc->plc_name);
	pll_add_sorted(&psc_listcaches, plc, lc_cmp);

	LIST_CACHE_ULOCK(plc);
	PLL_ULOCK(&psc_listcaches);
}

/*
 * Register a list cache for external access.
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

	_lc_init(plc, name, offset);

	va_start(ap, name);
	lc_vregister(plc, name, ap);
	va_end(ap);
}

/*
 * Remove list cache external access registration.
 * @plc: the list cache to unregister, must be UNLOCKED.
 */
void
lc_unregister(struct psc_listcache *plc)
{
	pfl_opstat_destroy(plc->plc_nseen);
	pfl_opstat_destroy(plc->plc_st_removes);

	PLL_LOCK(&psc_listcaches);
	LIST_CACHE_LOCK(plc);
	pll_remove(&psc_listcaches, plc);
	LIST_CACHE_ULOCK(plc);
	PLL_ULOCK(&psc_listcaches);
}

/*
 * Find a list cache by its registration name.
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

void
pfl_listcache_wait_empty(struct psc_listcache *plc)
{
	LIST_CACHE_LOCK(plc);
	while (lc_nitems(plc)) {
		psc_waitq_wait(&plc->plc_wq_empty,
		    LIST_CACHE_GETLOCK(plc));
		LIST_CACHE_LOCK(plc);
	}
	LIST_CACHE_ULOCK(plc);
}
