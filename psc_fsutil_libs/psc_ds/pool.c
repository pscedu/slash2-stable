/* $Id$ */

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

struct psc_poolset psc_poolset_main = PSC_POOLSET_INIT;
__static psc_spinlock_t psc_pools_lock = LOCK_INITIALIZER;
struct psc_lockedlist psc_pools = PLL_INITIALIZER(&psc_pools,
    struct psc_poolmgr, ppm_all_lentry, &psc_pools_lock);

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
		if (m->ppm_initf && m->ppm_initf(p)) {
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
 */
void
_psc_pool_reap(struct psc_poolset *s, struct psc_poolmgr *initiator)
{
	struct psc_poolmgr *m, *culprit;
	int i, len, mx, culpritmx;
	void **pools;

	culpritmx = mx = 0;
	len = dynarray_len(&s->pps_pools);
	pools = dynarray_get(&s->pps_pools);
	for (i = 0; i < len; i++) {
		m = pools[i];
		if (m == initiator)
			continue;

		if (!trylock(&m->ppm_lc.lc_lock))
			continue;
		mx = m->ppm_lc.lc_entsize * m->ppm_lc.lc_size;
		freelock(&m->ppm_lc.lc_lock);

		if (mx > culpritmx) {
			culprit = m;
			culpritmx = mx;
		}
	}

	if (culprit)
		psc_pool_shrink(culprit, 5);
}

/*
 * _psc_pool_reap - reap some memory from a global (or local on NUMAs)
 *	memory pool.
 * @size: desired amount of memory to reclaim.
 */
__weak void
_psc_pool_reapsome(__unusedx size_t size)
{
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

	/* If autoresizable, try to grow the pool. */
	if ((m->ppm_flags & PPMF_AUTO) &&
	    psc_pool_grow(m, 5)) {
		p = lc_getnb(&m->ppm_lc);
		if (p)
			return (p);
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
	locked = POOL_RLOCK(m);
	for (n = 0; n < dynarray_len(&m->ppm_sets); n++)
		_psc_pool_reap(dynarray_getpos(&m->ppm_sets, n), m);
	POOL_ULOCK(m, locked);
	if (n)
		psc_pool_grow(m, 5);
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

	locked = POOL_RLOCK(m);
	lc_add(&m->ppm_lc, p);
	if (m->ppm_max)
		psc_assert(m->ppm_lc.lc_size <= m->ppm_max);
	POOL_ULOCK(m, locked);

	/* XXX if above high watermark, free some entries */
	if (m->ppm_flags & PPMF_AUTO && lc_sz(&m->ppm_lc) > 5)
		psc_pool_shrink(m, 5);
}

int
_psc_pool_init(struct psc_poolmaster *pms, struct psc_poolmgr *m)
{
	memset(m, 0, sizeof(*m));
	dynarray_init(&m->ppm_sets);
	m->ppm_reclaimcb = pms->pms_reclaimcb;
	m->ppm_destroyf = pms->pms_destroyf;
	m->ppm_flags = pms->pms_flags;
	m->ppm_initf = pms->pms_initf;
	m->ppm_max = pms->pms_max;
	m->ppm_min = pms->pms_min;

	_lc_reginit(&m->ppm_lc, pms->pms_offset, pms->pms_entsize,
#ifdef HAVE_CPUSET
	    "%s:%d", pms->pms_name, numa_preferred()
#else
	    "%s", pms->pms_name
#endif
	);

	pll_add(&psc_pools, m);
	if (pms->pms_total)
		return (psc_pool_grow(m, pms->pms_total));
	return (0);
}

void
_psc_poolmaster_init(struct psc_poolmaster *pms, ptrdiff_t offset,
    size_t entsize, int flags, int total, int max, int (*initf)(void *),
    void (*destroyf)(void *), int (*reclaimcb)(struct psc_listcache *, int),
    const char *namefmt, ...)
{
	va_list ap;

	memset(pms, 0, sizeof(*pms));
	LOCK_INIT(&pms->pms_lock);
	dynarray_init(&pms->pms_poolmgrs);
	pms->pms_reclaimcb = reclaimcb;
	pms->pms_destroyf = destroyf;
	pms->pms_entsize = entsize;
	pms->pms_offset = offset;
	pms->pms_flags = flags;
	pms->pms_initf = initf;
	pms->pms_total = total;
	pms->pms_max = max;
	pms->pms_min = min;

	va_start(ap, namefmt);
	lc_vregister(&pms->ppm_lc, namefmt, ap);
	va_end(ap);
}

void
_psc_pool_destroy(struct psc_poolmgr *m)
{
	int i, len, tot, locked;
	void **sets;

	pll_remove(&psc_pools, m);
	locked = POOL_RLOCK(m);
	if (m->ppm_lc.lc_size != m->ppm_total)
		psc_fatalx("psc_pool_destroy: items in use");
	m->ppm_min = 0;
	m->ppm_max = 1;
	m->ppm_flags &= ~PPMF_AUTO;
	tot = m->ppm_total;
	if (tot && psc_pool_shrink(m, tot) != tot)
		psc_fatalx("psc_pool_destroy: unable to drain");
	sets = dynarray_get(&m->ppm_sets);
	len = dynarray_len(&m->ppm_sets);
	for (i = 0; i < len; i++)
		psc_pool_leaveset(m, sets[i]);
	dynarray_free(&m->ppm_sets);
	POOL_ULOCK(m, locked);
	lc_unregister(&m->ppm_lc);
}

#define ppm_lock ppm_lc.lc_lock

/*
 * psc_poolmaster_destroy - destroy a poolmaster.
 * @pms: poolmaster to destroy.
 */
void
psc_poolmaster_destroy(struct psc_poolmaster *pms)
{
	struct psc_poolmgr *mv, *setv;
	int i, nm, nset;

	/* Remove membership from all sets first due to deadlock ordering. */
	spinlock(&pms->pms_lock);
	nset = dynarray_len(&pms->pms_sets);
	setv = dynarray_get(&pms->pms_sets);
	for (i = 0; i < nset; i++) {
		if (!trylock(&setv[i].pps_lock)) {
			freelock(&pms->pms_lock);
			return;
		}
		psc_poolset_disbar(setv[i], pms);
		freelock(&setv[i].pps_lock);
	}

	/* Now acquire locks on every pool before destroying anything. */
	PLL_LOCK(&psc_pools);
	mv = dynarray_len(&pms->pms_poolmgrs);
	nm = dynarray_get(&pms->pms_poolmgrs);
	for (i = 0; i < nm; i++) {
		m = mv[i];
		if (!trylock(&m->ppm_lock)) {
			i--;
			goto bail;
		}
		if (m->ppm_refcnt)
			goto bail;
	}

	/* We acquired locks on every pool, time to let go. */
	for (i = 0; i < nm; i++)
		_psc_pool_destroy(mv[i]);
	dynarray_free(&pms->pms_poolmgrs);
	dynarray_free(&pms->pms_sets);
	return;

 bail:
	/*
	 * Unable to destroy, there are still active references.
	 * Someone else will try again once the refcnt on everyone
	 * reaches zero again.
	 */
	for (; i > 0; i--)
		freelock(&pv[i]->ppm_lock);
	PLL_ULOCK(&psc_pools);
	freelock(&pms->pms_lock);
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
		if (strcmp(name, m->ppm_lc.lc_name) == 0) {
			_psc_pool_incref(m);
			break;
		}
	PLL_ULOCK(&psc_pools);
	return (m);
}

void
_psc_pool_incref(struct psc_poolmgr *m)
{
	reqlock(&m->ppm_lock);
	m->ppm_refcnt++;
	ureqlock(&m->ppm_lock);
}

void
_psc_poolset_incref(struct psc_poolset *s)
{
	reqlock(&s->pps_lock);
	s->pps_refcnt++;
	ureqlock(&s->pps_lock);
}

/*
 * psc_pool_decref - drop a reference to a pool manager.  If this was
 *	the last reference, free the pool master.
 * @m: pool to release.
 */
void
psc_pool_decref(struct psc_poolmgr *m)
{
	int n;

	reqlock(&m->ppm_lock);
	POOL_ENSUREREF(m);
	n = --m->ppm_refcnt;
	freelock(&m->ppm_lock);
	if (n == 0)
		psc_poolmaster_destroy(m->ppm_master);
}

/*
 * psc_poolset_decref - drop a reference to a pool set.  If this was
 *	the last reference, free it.
 * @s: poolset to release.
 */
void
psc_poolset_decref(struct psc_poolset *s)
{
	int n;

	reqlock(&s->pps_lock);
	POOLSET_ENSUREREF(s);
	n = --s->pps_refcnt;
	freelock(&s->ppm_lock);
	if (n == 0)
		psc_poolset_destroy(s);
}

/*
 * POOL_ENSUREREF - ensure there is at least one reference to a pool.
 * @m: pool manager to check.
 */
#define POOL_ENSUREREF(m)				\
	do {						\
		LOCK_ENSURE(&(m)->ppm_lock);		\
		psc_assert((m)->ppm_refcnt > 0);	\
	} while (0)

/*
 * POOLSET_ENSUREREF - ensure there is at least one reference to a poolset.
 * @m: poolset to check.
 */
#define POOLSET_ENSUREREF(m)				\
	do {						\
		LOCK_ENSURE(&(s)->pps_lock);		\
		psc_assert((s)->pps_refcnt > 0);	\
	} while (0)

/*
 * psc_poolset_enlist - add a pool to a pool set.
 * @s: set to which @pms should be added.
 * @pms: pool master to add.
 */
void
psc_poolset_enlist(struct psc_poolset *s, struct psc_poolmaster *pms)
{
	int locked;

	spinlock(&s->pps_lock);
	spinlock(&pms->pms_lock);
	if (dynarray_exists(&s->pps_pools, pms))
		psc_fatalx("pool already exists in set");
	if (dynarray_exists(&pms->pms_sets, s))
		psc_fatalx("pool already member of set");
	if (dynarray_add(&s->pps_pools, pms) == -1)
		psc_fatalx("dynarray_add");
	if (dynarray_add(&pms->pms_sets, s) == -1)
		psc_fatalx("dynarray_add");
	freelock(&pms->pms_lock);
	freelock(&s->pps_lock);
}

/*
 * psc_pool_share - allow pool to share its resources with everyone.
 * @pms: pool master to share.
 */
void
psc_pool_share(struct psc_poolmaster *pms)
{
	psc_poolset_enlist(&psc_poolset_main, pms);
}

/*
 * psc_pool_leaveset - remove a pool's membership from a pool set.
 * @s: set from which pool should be removed.
 * @m: pool to remove from set.
 */
void
psc_poolset_disbar(struct psc_poolset *s, struct psc_poolmaster *m)
{
	int locked, locked2;

	locked = reqlock(&s->pps_lock);
	locked2 = reqlock(&pms->pms_lock);
	if (locked == 0 && locked2)
		psc_fatalx("deadlock ordering");
	dynarray_remove(&s->pps_pools, pms);
	dynarray_remove(&pms->pms_sets, s);
	ureqlock(&pms->pms_lock, locked2);
	ureqlock(&s->pps_lock, locked);
}

/*
 * psc_pool_unshare - disallow pool from sharing its resources with everyone.
 * @pms: pool master to share.
 */
void
psc_pool_unshare(struct psc_poolmaster *pms)
{
	psc_poolset_disbar(&psc_poolset_main, pms);
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
 * psc_poolset_destroy - destroy a pool set.
 * @s: set to destroy.
 */
void
psc_poolset_destroy(struct psc_poolset *s)
{
	reqlock(&s->pps_lock);
	if (s->pps_refcnt) {
		freelock(&s->pps_lock);
		return;
	}
	psc_assert(dynarray_len(&s->pps_pools) == 0);
	dynarray_free(&s->pps_pools);
	memset(s, 0, sizeof(*s));
}

/*
 * psc_poolmaster_getmgr - obtain a pool manager for this NUMA from the
 *	master.
 * @pms: pool master.
 */
struct psc_poolmgr *
psc_poolmaster_getmgr(struct psc_poolmaster *pms)
{
	struct psc_poolmgr *m;
	struct psc_thread *thr;
	int memnid;

	memnid = psc_get_memnid();
	spinlock(&pms->pms_lock);
	if (dynarray_hintlen(&pms->pms_poolmgrs, memnid) == -1)
		psc_fatal("unable to resize poolmgrs");
	m = pms->pms_poolmgrs[memnid];
	if (m == NULL) {
		pms->pms_poolmgrs[memnid] = m = PSCALLOC(sizeof(*m));
		_psc_pool_init(pms, m);
	}
	_psc_pool_incref(m);
	freelock(&pms->pms_lock);
	return (m);
}
