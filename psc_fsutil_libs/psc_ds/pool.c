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
 * Memory pool routines.
 *
 * XXX: watch out, during communal reaping, for two pools
 * to continually reap other back and forth.
 */

#include <sys/param.h>

#include <errno.h>

#include "pfl/cdefs.h"
#include "psc_ds/dynarray.h"
#include "psc_ds/listcache.h"
#include "psc_ds/lockedlist.h"
#include "psc_ds/pool.h"
#include "psc_util/alloc.h"
#include "psc_util/lock.h"
#include "psc_util/log.h"
#include "psc_util/pthrutil.h"
#include "psc_util/waitq.h"

__static struct psc_poolset psc_poolset_main = PSC_POOLSET_INIT;
struct psc_lockedlist psc_pools =
    PLL_INITIALIZER(&psc_pools, struct psc_poolmgr, ppm_all_lentry);

__weak void
_psc_mlist_init(__unusedx struct psc_mlist *m, __unusedx int flags,
    __unusedx void *arg, __unusedx size_t entsize,
    __unusedx ptrdiff_t offset, __unusedx const char *fmt, ...)
{
	psc_fatalx("mlist support not compiled in");
}

__weak void
psc_mlist_add(__unusedx struct psc_mlist *pml, __unusedx void *p)
{
	psc_fatalx("mlist support not compiled in");
}

__weak void *
psc_mlist_tryget(__unusedx struct psc_mlist *pml)
{
	psc_fatalx("mlist support not compiled in");
}

void
_psc_poolmaster_initv(struct psc_poolmaster *p, size_t entsize,
    ptrdiff_t offset, int flags, int total, int min, int max,
    int (*initf)(struct psc_poolmgr *, void *), void (*destroyf)(void *),
    int (*reclaimcb)(struct psc_poolmgr *), void *mwcarg,
    const char *namefmt, va_list ap)
{
	va_list ap;

	memset(p, 0, sizeof(*p));
	LOCK_INIT(&p->pms_lock);
	psc_dynarray_init(&p->pms_poolmgrs);
	psc_dynarray_init(&p->pms_sets);
	p->pms_reclaimcb = reclaimcb;
	p->pms_destroyf = destroyf;
	p->pms_entsize = entsize;
	p->pms_offset = offset;
	p->pms_flags = flags;
	p->pms_initf = initf;
	p->pms_total = total;
	p->pms_min = min;
	p->pms_max = max;
	p->pms_thres = POOL_AUTOSIZE_THRESH;
	p->pms_mwcarg = mwcarg;

	vsnprintf(p->pms_name, sizeof(p->pms_name), namefmt, ap);
}

void
_psc_poolmaster_init(struct psc_poolmaster *p, size_t entsize,
    ptrdiff_t offset, int flags, int total, int min, int max,
    int (*initf)(struct psc_poolmgr *, void *), void (*destroyf)(void *),
    int (*reclaimcb)(struct psc_poolmgr *), void *mwcarg,
    const char *namefmt, ...)
{
	va_list ap;

	va_start(ap, namefmt);
	_psc_poolmaster_initv(p, entsize, offset, flags, total, min, max,
	    initf, destroyf, reclaimcb, mwcarg, namefmt, ap);
	va_end(ap);
}

int
_psc_poolmaster_initmgr(struct psc_poolmaster *p, struct psc_poolmgr *m)
{
	int n, locked;

	memset(m, 0, sizeof(*m));
	psc_pthread_mutex_init(&m->ppm_reclaim_mutex);
	locked = reqlock(&p->pms_lock);
	m->ppm_reclaimcb = p->pms_reclaimcb;
	m->ppm_destroyf = p->pms_destroyf;
	m->ppm_thres = p->pms_thres;
	m->ppm_flags = p->pms_flags;
	m->ppm_initf = p->pms_initf;
	m->ppm_min = p->pms_min;
	m->ppm_max = p->pms_max;
	m->ppm_master = p;

	if (POOL_IS_MLIST(m)) {
#ifdef HAVE_NUMA
		_psc_mlist_init(&m->ppm_ml, PMWCF_WAKEALL, p->pms_mwcarg,
		    p->pms_entsize, p->pms_offset, "%s:%d", p->pms_name,
		    psc_memnode_getid());
#else
		_psc_mlist_init(&m->ppm_ml, PMWCF_WAKEALL, p->pms_mwcarg,
		    p->pms_entsize, p->pms_offset, "%s", p->pms_name);
#endif
	} else {
		_lc_init(&m->ppm_lc, p->pms_offset, p->pms_entsize);

#ifdef HAVE_NUMA
		n = snprintf(m->ppm_lg.plg_name, sizeof(m->ppm_lg.plg_name),
		    "%s:%d", p->pms_name, psc_memnode_getid());
#else
		n = snprintf(m->ppm_lg.plg_name, sizeof(m->ppm_lg.plg_name),
		    "%s", p->pms_name);
#endif
		if (n == -1)
			psc_fatal("snprintf %s", p->pms_name);
		if (n >= (int)sizeof(m->ppm_lg.plg_name))
			psc_fatalx("%s: name too long", p->pms_name);
	}

	n = p->pms_total;
	ureqlock(&p->pms_lock, locked);

	pll_add(&psc_pools, m);

	if (n)
		n = psc_pool_grow(m, n);
	return (n);
}

/*
 * psc_poolmaster_getmgr - obtain a pool manager for this NUMA from the
 *	master.
 * @p: pool master.
 */
struct psc_poolmgr *
_psc_poolmaster_getmgr(struct psc_poolmaster *p, int memnid)
{
	struct psc_poolmgr *m, **mv;

	spinlock(&p->pms_lock);
	if (psc_dynarray_ensurelen(&p->pms_poolmgrs, memnid + 1) == -1)
		psc_fatal("unable to resize poolmgrs");
	mv = psc_dynarray_get(&p->pms_poolmgrs);
	m = mv[memnid];
	if (m == NULL) {
		mv[memnid] = m = PSCALLOC(sizeof(*m));
		_psc_poolmaster_initmgr(p, m);
	}
	freelock(&p->pms_lock);
	return (m);
}

/*
 * _psc_pool_destroy_obj - Release an object allocated by pool.
 * @m: pool manager.
 * @p: item to free.
 */
void
_psc_pool_destroy_obj(struct psc_poolmgr *m, void *p)
{
	if (p && m->ppm_destroyf)
		m->ppm_destroyf(p);
	if (m->ppm_flags & PPMF_NOLOCK)
		PSCFREE(p);
	else
		psc_freel(p, m->ppm_lg.plg_entsize);
}

/*
 * psc_pool_grow - increase #items in a pool.
 * @m: the pool manager.
 * @n: #items to add to pool.
 */
int
psc_pool_grow(struct psc_poolmgr *m, int n)
{
	int i, flags, locked;
	void *p;

	psc_assert(n > 0);

	if (m->ppm_max) {
		locked = POOL_RLOCK(m);
		POOL_CHECK(m);
		if (m->ppm_total == m->ppm_max) {
			POOL_URLOCK(m, locked);
			return (0);
		}
		/* Bound number to add to ppm_max. */
		n = MIN(n, m->ppm_max - m->ppm_total);
		POOL_URLOCK(m, locked);
	}

	flags = PAF_CANFAIL;
	if ((m->ppm_flags & PPMF_NOLOCK) == 0)
		flags |= PAF_LOCK;
	for (i = 0; i < n; i++) {
		p = psc_alloc(m->ppm_lg.plg_entsize, flags);
		if (p == NULL) {
			errno = ENOMEM;
			return (i);
		}
		if (m->ppm_initf && m->ppm_initf(m, p)) {
			if (flags & PAF_LOCK)
				psc_freel(p, m->ppm_lg.plg_entsize);
			else
				PSCFREE(p);
			return (i);
		}
		locked = POOL_RLOCK(m);
		if (m->ppm_total < m->ppm_max ||
		    m->ppm_max == 0) {
			m->ppm_total++;
			m->ppm_ngrow++;
			if (POOL_IS_MLIST(m))
				psc_mlist_add(&m->ppm_ml, p);
			else
				lc_add(&m->ppm_lc, p);
			p = NULL;
		}
		POOL_URLOCK(m, locked);

		/*
		 * If we are prematurely exiting,
		 * we didn't use this item.
		 */
		if (p) {
			_psc_pool_destroy_obj(m, p);
			break;
		}
	}
	return (i);
}

int
_psc_pool_shrink(struct psc_poolmgr *m, int n, int failok)
{
	int i, locked;
	void *p;

	psc_assert(n > 0);

	if (m->ppm_min) {
		locked = POOL_RLOCK(m);
		POOL_CHECK(m);
		if (m->ppm_total == m->ppm_min) {
			POOL_URLOCK(m, locked);
			return (0);
		}
		/* Bound number to shrink to ppm_min. */
		n = MAX(n, m->ppm_total - m->ppm_min);
		POOL_URLOCK(m, locked);
	}

	for (i = 0; i < n; i++) {
		locked = POOL_RLOCK(m);
		if (m->ppm_total > m->ppm_min) {
			if (POOL_IS_MLIST(m))
				p = psc_mlist_tryget(&m->ppm_ml);
			else
				p = lc_getnb(&m->ppm_lc);
			if (p == NULL && !failok)
				psc_fatalx("psc_pool_shrink: no free "
				    "items available to remove");
			m->ppm_total--;
			m->ppm_nshrink++;
		} else
			p = NULL;
		POOL_URLOCK(m, locked);
		if (p == NULL)
			break;
		_psc_pool_destroy_obj(m, p);
	}
	return (i);
}

/*
 * psc_pool_settotal - set #items in a pool.
 * @m: the pool manager.
 * @total: #items the pool should contain.
 */
int
psc_pool_settotal(struct psc_poolmgr *m, int total)
{
	int adj, locked;

	adj = 0;
	locked = POOL_RLOCK(m);
	if (m->ppm_max && total > m->ppm_max)
		total = m->ppm_max;
	else if (total < m->ppm_min)
		total = m->ppm_min;
	adj = total - m->ppm_total;
	POOL_URLOCK(m, locked);

	if (adj < 0)
		adj = psc_pool_shrink(m, -adj);
	else if (adj)
		adj = psc_pool_grow(m, adj);
	return (adj);
}

/*
 * psc_pool_resize - resize a pool so the current total is between
 *	ppm_min and ppm_max.  Note the pool size may not go into
 *	effect immediately if enough entries are not on the free list.
 * @m: the pool manager.
 */
void
psc_pool_resize(struct psc_poolmgr *m)
{
	int adj, locked;

	adj = 0;
	locked = POOL_RLOCK(m);
	if (m->ppm_max && m->ppm_total > m->ppm_max)
		adj = m->ppm_max - m->ppm_total;
	else if (m->ppm_total < m->ppm_min)
		adj = m->ppm_min - m->ppm_total;
	POOL_URLOCK(m, locked);

	if (adj < 0)
		psc_pool_shrink(m, -adj);
	else if (adj)
		psc_pool_grow(m, adj);
}

/*
 * _psc_pool_reap - reap other pools in attempt to reclaim memory in the
 *	dire situation of pool exhaustion.
 * @s: set to reap from.
 * @initiator: the requestor pool, which should be a member of @s, but
 *	should not itself be considered a candidate for reaping.
 * @size: amount of memory needed to be released, normally
 *	@initiator->ppm_entsize but not necessarily always.
 *
 * XXX: if invoking this routine does not reap enough available memory
 *	as adequate, it may be because there are no free buffers
 *	available in any pools and the caller should then resort to
 *	forcefully reclaiming pool buffers from the set.
 *
 * XXX: pass back some kind of return value determining whether or not
 *	any memory could be reaped.
 */
void
_psc_pool_reap(struct psc_poolset *s, struct psc_poolmaster *initiator,
    size_t size)
{
	struct psc_poolmgr *m, *culprit;
	struct psc_poolmaster *p;
	size_t tmx, culpritmx;
	int i, np, nobj;
	void **pv;

	nobj = 0;
	culprit = NULL;
	culpritmx = tmx = 0;
	spinlock(&s->pps_lock);
	np = psc_dynarray_len(&s->pps_pools);
	pv = psc_dynarray_get(&s->pps_pools);
	for (i = 0; i < np; i++) {
		p = pv[i];
		if (p == initiator)
			continue;
		m = psc_poolmaster_getmgr(p);

		if (!POOL_TRYLOCK(m))
			continue;
		tmx = m->ppm_lg.plg_entsize * m->ppm_lg.plg_size;
		nobj = MAX(size / m->ppm_lg.plg_entsize, 1);
		POOL_UNLOCK(m);

		if (tmx > culpritmx) {
			culprit = m;
			culpritmx = tmx;
		}
	}
	freelock(&s->pps_lock);

	if (culprit && nobj)
		psc_pool_tryshrink(culprit, nobj);
}

/*
 * _psc_pool_reap - reap some memory from a global (or local on NUMAs)
 *	memory pool.
 * @size: desired amount of memory to reclaim.
 */
void
psc_pool_reapmem(size_t size)
{
	_psc_pool_reap(&psc_poolset_main, NULL, size);
}

/*
 * _psc_pool_flagtest - test pool state.
 * @m: pool to inspect.
 * @flags: flags to check.
 */
__static int
_psc_pool_flagtest(struct psc_poolmgr *m, int flags)
{
	int rc, locked;

	locked = POOL_RLOCK(m);
	rc = m->ppm_flags & flags;
	POOL_URLOCK(m, locked);
	return (rc);
}

#define POOL_GETOBJ(m) \
	(POOL_IS_MLIST(m) ? psc_mlist_tryget(&(m)->ppm_ml) : lc_getnb(&(m)->ppm_lc))

/*
 * psc_pool_get - grab an item from a pool.
 * @m: the pool manager.
 */
void *
psc_pool_get(struct psc_poolmgr *m)
{
	int locked, n;
	void *p;

	p = POOL_GETOBJ(m);
	if (p)
		return (p);

	/* If total < min, try to grow the pool. */
	locked = POOL_RLOCK(m);
	n = m->ppm_min - m->ppm_total;
	POOL_URLOCK(m, locked);
	if (n > 0) {
		psc_pool_grow(m, n);
		p = POOL_GETOBJ(m);
		if (p)
			return (p);
	}

	/* If autoresizable, try to grow the pool. */
	if (_psc_pool_flagtest(m, PPMF_AUTO)) {
		do {
			n = psc_pool_grow(m, 2);
			p = POOL_GETOBJ(m);
			if (p)
				return (p);
			/* If we've grown to pool max, quit. */
		} while (n && _psc_pool_flagtest(m, PPMF_AUTO));
	}

	/*
	 * If this pool user provided a reclaimer routine e.g.
	 * for reclaiming buffers on a clean list, try that.
	 */
 tryreclaim:
	if (m->ppm_reclaimcb)
		do {
			atomic_inc(&m->ppm_nwaiters);
			pthread_mutex_lock(&m->ppm_reclaim_mutex);
			n = m->ppm_reclaimcb(m);
			pthread_mutex_unlock(&m->ppm_reclaim_mutex);
			atomic_dec(&m->ppm_nwaiters);
			p = POOL_GETOBJ(m);
			if (p)
				return (p);
		} while (n);

	/* If communal, try reaping other pools in sets. */
	locked = reqlock(&m->ppm_master->pms_lock);
	for (n = 0; n < psc_dynarray_len(&m->ppm_master->pms_sets); n++)
		_psc_pool_reap(psc_dynarray_getpos(&m->ppm_master->pms_sets, n),
		    m->ppm_master, m->ppm_lg.plg_entsize);
	ureqlock(&m->ppm_master->pms_lock, locked);
	if (n)
		psc_pool_grow(m, 2);

	/*
	 * Try once more, if nothing, a condition should be added to the
	 * multiwait.  This invocation should have been done in a
	 * multiwait critical section to prevent dropping notifications.
	 */
	if (POOL_IS_MLIST(m))
		return (POOL_GETOBJ(m));

	/*
	 * If there is a reclaimer routine specified, invoke it
	 * periodically instead of blocking forever.
	 */
	if (m->ppm_reclaimcb) {
		POOL_LOCK(m);
		p = POOL_GETOBJ(m);
		if (p) {
			POOL_UNLOCK(m);
			return (p);
		}
		atomic_inc(&m->ppm_nwaiters);
		psc_waitq_waitrel_us(&m->ppm_lc.lc_wq_empty,
		    &m->ppm_lc.lc_lock, 100);
		atomic_dec(&m->ppm_nwaiters);

		p = POOL_GETOBJ(m);
		if (p)
			return (p);

		goto tryreclaim;
	}

	/* Nothing else we can do; wait for an item to return. */
	return (lc_getwait(&m->ppm_lc));
}

/*
 * psc_pool_return - return an item to a pool.
 * @m: the pool manager.
 * @p: item to return.
 */
void
psc_pool_return(struct psc_poolmgr *m, void *p)
{
	int locked;

	/*
	 * If pool max < total (e.g. when an administrator lowers max
	 * beyond current total) or we reached the auto resize
	 * threshold, directly free this item.
	 */
	locked = POOL_RLOCK(m);
	if ((m->ppm_flags & PPMF_AUTO) && m->ppm_total > m->ppm_min &&
	    ((m->ppm_max && m->ppm_max < m->ppm_total) ||
	     m->ppm_lg.plg_size * 100 > m->ppm_thres * m->ppm_total)) {
		m->ppm_total--;
		m->ppm_nshrink++;
		POOL_URLOCK(m, locked);
		_psc_pool_destroy_obj(m, p);
	} else {
		/* Pool should keep this item. */
		if (POOL_IS_MLIST(m))
			psc_mlist_add(&m->ppm_ml, p);
		else
			lc_addhead(&m->ppm_lc, p);
		POOL_URLOCK(m, locked);
	}
}

/*
 * psc_pool_gettotal - obtain the number of objects in a pool
 *	circulation (note: this is not the number of free objects
 *	available for use).
 * @m: pool to query.
 */
int
psc_pool_gettotal(struct psc_poolmgr *m)
{
	int locked, n;

	locked = POOL_RLOCK(m);
	n = m->ppm_total;
	POOL_URLOCK(m, locked);
	return (n);
}

/*
 * psc_pool_lookup - find a pool by name.
 * @name: name of pool to find.
 */
struct psc_poolmgr *
psc_pool_lookup(const char *name)
{
	struct psc_poolmgr *m;

	PLL_LOCK(&psc_pools);
	psclist_for_each_entry(m, &psc_pools.pll_listhd, ppm_all_lentry)
		if (strcmp(name, m->ppm_lg.plg_name) == 0) {
			POOL_LOCK(m);
			break;
		}
	PLL_ULOCK(&psc_pools);
	return (m);
}

/*
 * psc_pool_share - allow a pool to share its resources with everyone.
 * @p: pool master to share.
 */
void
psc_pool_share(struct psc_poolmaster *p)
{
	psc_poolset_enlist(&psc_poolset_main, p);
}

/*
 * psc_pool_unshare - disallow a pool from sharing its resources with everyone.
 * @p: pool master to unshare.
 */
void
psc_pool_unshare(struct psc_poolmaster *p)
{
	psc_poolset_disbar(&psc_poolset_main, p);
}

/*
 * psc_poolset_init - initialize a pool set.
 * @s: set to initialize.
 */
void
psc_poolset_init(struct psc_poolset *s)
{
	LOCK_INIT(&s->pps_lock);
	psc_dynarray_init(&s->pps_pools);
}

/*
 * psc_poolset_enlist - add a pool master to a pool set.
 * @s: set to which pool master should be added.
 * @p: pool master to add.
 */
void
psc_poolset_enlist(struct psc_poolset *s, struct psc_poolmaster *p)
{
	int locked, locked2;

	locked = reqlock(&s->pps_lock);
	locked2 = reqlock(&p->pms_lock);
	if (locked == 0 && locked2)
		psc_fatalx("deadlock ordering");
	if (psc_dynarray_exists(&s->pps_pools, p))
		psc_fatalx("pool already exists in set");
	if (psc_dynarray_exists(&p->pms_sets, s))
		psc_fatalx("pool already member of set");
	if (psc_dynarray_add(&s->pps_pools, p) == -1)
		psc_fatalx("psc_dynarray_add");
	if (psc_dynarray_add(&p->pms_sets, s) == -1)
		psc_fatalx("psc_dynarray_add");
	ureqlock(&p->pms_lock, locked2);
	ureqlock(&s->pps_lock, locked);
}

/*
 * psc_poolset_disbar - remove a pool master from from a pool set.
 * @s: set from which pool master should be removed.
 * @p: pool master to remove from set.
 */
void
psc_poolset_disbar(struct psc_poolset *s, struct psc_poolmaster *p)
{
	int locked, locked2;

	locked = reqlock(&s->pps_lock);
	locked2 = reqlock(&p->pms_lock);
	if (locked == 0 && locked2)
		psc_fatalx("deadlock ordering");
	psc_dynarray_remove(&s->pps_pools, p);
	psc_dynarray_remove(&p->pms_sets, s);
	ureqlock(&p->pms_lock, locked2);
	ureqlock(&s->pps_lock, locked);
}
