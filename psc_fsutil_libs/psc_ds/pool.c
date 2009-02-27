/* $Id$ */

/*
 * Memory pool routines.
 *
 * XXX: watch out, during communal reaping, for two pools
 * to continually reap other back and forth.
 */

#include <sys/param.h>

#include <errno.h>

#include "psc_ds/dynarray.h"
#include "psc_ds/listcache.h"
#include "psc_ds/lockedlist.h"
#include "psc_ds/pool.h"
#include "psc_util/alloc.h"
#include "psc_util/cdefs.h"
#include "psc_util/lock.h"
#include "psc_util/log.h"
#include "psc_util/waitq.h"

#define ppm_lock ppm_lc.lc_lock

__static struct psc_poolset psc_poolset_main = PSC_POOLSET_INIT;
struct psc_lockedlist psc_pools =
    PLL_INITIALIZER(&psc_pools, struct psc_poolmgr, ppm_all_lentry);

void
_psc_poolmaster_init(struct psc_poolmaster *p, ptrdiff_t offset,
    size_t entsize, int flags, int total, int min, int max,
    int (*initf)(struct psc_poolmgr *, void *), void (*destroyf)(void *),
    int (*reclaimcb)(struct psc_listcache *, int), const char *namefmt, ...)
{
	va_list ap;

	memset(p, 0, sizeof(*p));
	LOCK_INIT(&p->pms_lock);
	dynarray_init(&p->pms_poolmgrs);
	dynarray_init(&p->pms_sets);
	p->pms_reclaimcb = reclaimcb;
	p->pms_destroyf = destroyf;
	p->pms_entsize = entsize;
	p->pms_offset = offset;
	p->pms_flags = flags;
	p->pms_initf = initf;
	p->pms_total = total;
	p->pms_min = min;
	p->pms_max = max;
	p->pms_thres = 80;

	va_start(ap, namefmt);
	vsnprintf(p->pms_name, sizeof(p->pms_name), namefmt, ap);
	va_end(ap);
}

int
_psc_poolmaster_initmgr(struct psc_poolmaster *p, struct psc_poolmgr *m)
{
	int n, locked;

	locked = reqlock(&p->pms_lock);
	memset(m, 0, sizeof(*m));
	m->ppm_reclaimcb = p->pms_reclaimcb;
	m->ppm_destroyf = p->pms_destroyf;
	m->ppm_thres = p->pms_thres;
	m->ppm_flags = p->pms_flags;
	m->ppm_initf = p->pms_initf;
	m->ppm_min = p->pms_min;
	m->ppm_max = p->pms_max;
	m->ppm_master = p;

#ifdef HAVE_CPUSET
	_lc_reginit(&m->ppm_lc, p->pms_offset, p->pms_entsize,
	    "%s:%d", p->pms_name, psc_memnode_getid());
#else
	_lc_reginit(&m->ppm_lc, p->pms_offset, p->pms_entsize,
	    "%s", p->pms_name);
#endif

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
	if (dynarray_hintlen(&p->pms_poolmgrs, memnid + 1) == -1)
		psc_fatal("unable to resize poolmgrs");
	mv = dynarray_get(&p->pms_poolmgrs);
	m = mv[memnid];
	if (m == NULL) {
		mv[memnid] = m = PSCALLOC(sizeof(*m));
		_psc_poolmaster_initmgr(p, m);
	}
	freelock(&p->pms_lock);
	return (m);
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
			POOL_ULOCK(m, locked);
			return (0);
		}
		/* Bound number to add to ppm_max. */
		n = MIN(n, m->ppm_max - m->ppm_total);
		POOL_ULOCK(m, locked);
	}

	flags = PAF_CANFAIL;
	if ((m->ppm_flags & PPMF_NOLOCK) == 0)
		flags |= PAF_LOCK;
	for (i = 0; i < n; i++) {
		p = psc_alloc(m->ppm_lc.lc_entsize, flags);
		if (p == NULL) {
			errno = ENOMEM;
			return (i);
		}
		if (m->ppm_initf && m->ppm_initf(m, p)) {
			if (flags & PAF_LOCK)
				psc_freel(p, m->ppm_lc.lc_entsize);
			else
				PSCFREE(p);
			return (i);
		}
		locked = POOL_RLOCK(m);
		if (m->ppm_total < m->ppm_max ||
		    m->ppm_max == 0) {
			m->ppm_total++;
			lc_add(&m->ppm_lc, p);
			p = NULL;
		}
		POOL_ULOCK(m, locked);

		/*
		 * If we are prematurely exiting,
		 * we didn't use this item.
		 */
		if (p) {
			if (m->ppm_destroyf)
				m->ppm_destroyf(p);
			PSCFREE(p);
			break;
		}
	}
	return (i);
}

/*
 * psc_pool_shrink - decrease #items in a pool.
 * @m: the pool manager.
 * @n: #items to remove from pool.
 */
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
			POOL_ULOCK(m, locked);
			return (0);
		}
		/* Bound number to shrink to ppm_min. */
		n = MAX(n, m->ppm_total - m->ppm_min);
		POOL_ULOCK(m, locked);
	}

	for (i = 0; i < n; i++) {
		locked = POOL_RLOCK(m);
		if (m->ppm_total > m->ppm_min) {
			p = lc_getnb(&m->ppm_lc);
			if (p == NULL && !failok)
				psc_fatalx("psc_pool_shrink: no free "
				    "items available to remove");
			m->ppm_total--;
		} else
			p = NULL;
		POOL_ULOCK(m, locked);
		if (p == NULL)
			break;
		if (p && m->ppm_destroyf)
			m->ppm_destroyf(p);
		if (m->ppm_flags & PPMF_NOLOCK)
			PSCFREE(p);
		else
			psc_freel(p, m->ppm_lc.lc_entsize);
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
	POOL_ULOCK(m, locked);

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
	POOL_ULOCK(m, locked);

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
 * @size: amount of memory needed to be released, should probably be
 *	@initiator->pms_entsize.
 */
void
_psc_pool_reap(struct psc_poolset *s, struct psc_poolmaster *initiator, size_t mem)
{
	struct psc_poolmgr *m, *culprit;
	struct psc_poolmaster *p;
	size_t mx, culpritmx;
	void **pv;
	int i, np;

	spinlock(&s->pps_lock);
	culpritmx = mx = 0;
	np = dynarray_len(&s->pps_pools);
	pv = dynarray_get(&s->pps_pools);
	for (i = 0; i < np; i++) {
		p = pv[i];
		if (p == initiator)
			continue;
		m = psc_poolmaster_getmgr(p);

		if (!trylock(&m->ppm_lock))
			continue;
		mx = m->ppm_lc.lc_entsize * m->ppm_lc.lc_size;
		freelock(&m->ppm_lock);

		if (mx > culpritmx) {
			culprit = m;
			culpritmx = mx;
		}
	}
	freelock(&s->pps_lock);

	if (culprit)
		psc_pool_tryshrink(culprit, 5);
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
	POOL_ULOCK(m, locked);
	return (rc);
}

/*
 * psc_pool_get - grab an item from a pool.
 * @m: the pool manager.
 */
void *
psc_pool_get(struct psc_poolmgr *m)
{
	int locked, n;
	void *p;

	p = lc_getnb(&m->ppm_lc);
	if (p)
		return (p);

	/* If total < min, try to grow the pool. */
	locked = POOL_RLOCK(m);
	n = m->ppm_min - m->ppm_total;
	POOL_ULOCK(m, locked);
	if (n > 0) {
		psc_pool_grow(m, n);
		p = lc_getnb(&m->ppm_lc);
		if (p)
			return (p);
	}

	/* If autoresizable, try to grow the pool. */
	if (_psc_pool_flagtest(m, PPMF_AUTO)) {
		do {
			n = psc_pool_grow(m, 2);
			p = lc_getnb(&m->ppm_lc);
			if (p)
				return (p);
			/* If we've grown to pool max, quit. */
		} while (n && _psc_pool_flagtest(m, PPMF_AUTO));
	}

	/*
	 * If this pool user provided a reclaimer routine e.g.
	 * for reclaiming buffers on a clean list, try that.
	 */
	if (m->ppm_reclaimcb)
		do {
			/* Add +1 here to count the invoker as a waitor. */
			n = m->ppm_reclaimcb(&m->ppm_lc, atomic_read(
			    &m->ppm_lc.lc_wq_empty.wq_nwaitors) + 1);
			p = lc_getnb(&m->ppm_lc);
			if (p)
				return (p);
		} while (n);

	/* If communal, try reaping other pools in sets. */
	locked = reqlock(&m->ppm_master->pms_lock);
	for (n = 0; n < dynarray_len(&m->ppm_master->pms_sets); n++)
		_psc_pool_reap(dynarray_getpos(&m->ppm_master->pms_sets, n),
		    m->ppm_master, m->ppm_lc.lc_entsize);
	ureqlock(&m->ppm_master->pms_lock, locked);
	if (n)
		psc_pool_grow(m, 2);

	/* Nothing else we can do, wait for an item to return. */
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
	 * if pool max < total or we reached the auto
	 * resize threshold, directly free this item.
	 */
	locked = POOL_RLOCK(m);
	if ((m->ppm_flags & PPMF_AUTO) &&
	    (m->ppm_max && m->ppm_max < m->ppm_total ||
	     lc_sz(&m->ppm_lc) * 100 <
	     m->ppm_total * m->ppm_thres)) {
		m->ppm_total--;
		POOL_ULOCK(m, locked);

		if (p && m->ppm_destroyf)
			m->ppm_destroyf(p);
		if (m->ppm_flags & PPMF_NOLOCK)
			PSCFREE(p);
		else
			psc_freel(p, m->ppm_lc.lc_entsize);
	} else {
		/* Pool should keep this item. */
		lc_addhead(&m->ppm_lc, p);
		POOL_ULOCK(m, locked);
	}
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
		if (strcmp(name, m->ppm_lc.lc_name) == 0)
			break;
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
	dynarray_init(&s->pps_pools);
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
	if (dynarray_exists(&s->pps_pools, p))
		psc_fatalx("pool already exists in set");
	if (dynarray_exists(&p->pms_sets, s))
		psc_fatalx("pool already member of set");
	if (dynarray_add(&s->pps_pools, p) == -1)
		psc_fatalx("dynarray_add");
	if (dynarray_add(&p->pms_sets, s) == -1)
		psc_fatalx("dynarray_add");
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
	dynarray_remove(&s->pps_pools, p);
	dynarray_remove(&p->pms_sets, s);
	ureqlock(&p->pms_lock, locked2);
	ureqlock(&s->pps_lock, locked);
}
