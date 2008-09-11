/* $Id$ */

#include "psc_ds/lockedlist.h"
#include "psc_ds/pool.h"

psc_spinlock_t psc_pools_lock = LOCK_INITIALIZER;
struct psc_lockedlist psc_pools = PLL_INITIALIZER(&psc_pools,
    struct psc_poolmgr, ppm_lentry, &psc_pools_lock);

/*
 * psc_pool_grow - increase #items in a pool.
 * @m: the pool manager.
 * @n: #items to add to pool.
 */
int
psc_pool_grow(struct psc_poolmgr *m, int n)
{
	int i, flags;
	void *p;

	psc_assert(n > 0);

	if (m->ppm_max) {
		POOL_LOCK(m);
		POOL_CHECK(m);
		if (m->ppm_total == m->ppm_max) {
			POOL_ULOCK(m);
			return (0);
		}
		/* Bound number to add to ppm_max. */
		n = MIN(n, m->ppm_max - m->ppm_total);
		POOL_ULOCK(m);
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
				free(p);
			return (i);
		}
		POOL_LOCK(m);
		if (m->ppm_total < m->ppm_max ||
		    m->ppm_max == 0) {
			m->ppm_total++;
			lc_add(&m->ppm_lc, p);
			p = NULL;
		}
		POOL_ULOCK(m);

		/*
		 * If we are prematurely exiting,
		 * we didn't use this item.
		 */
		if (p) {
			if (m->ppm_destroyf)
				m->ppm_destroyf(p);
			free(p);
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
psc_pool_shrink(struct psc_poolmgr *m, int n)
{
	void *p;
	int i;

	psc_assert(n > 0);

	if (m->ppm_min) {
		POOL_LOCK(m);
		POOL_CHECK(m);
		if (m->ppm_total == m->ppm_min) {
			POOL_ULOCK(m);
			return (0);
		}
		/* Bound number to add to ppm_min. */
		n = MAX(n, m->ppm_total - m->ppm_min);
		POOL_ULOCK(m);
	}

	for (i = 0; i < n; i++) {
		POOL_LOCK(m);
		if (m->ppm_total > m->ppm_min) {
			p = lc_getnb(&m->ppm_lc);
			psc_assert(p);
			m->ppm_total--;
		} else
			p = NULL;
		POOL_ULOCK(m);
		if (p == NULL)
			break;
		if (p && m->ppm_destroyf)
			m->ppm_destroyf(p);
		if (m->ppm_flags & PPMF_NOLOCK)
			free(p);
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
	int adj;

	adj = 0;
	POOL_LOCK(m);
	if (m->ppm_max && total > m->ppm_max)
		total = m->ppm_max;
	else if (total < m->ppm_min)
		total = m->ppm_min;
	adj = total - m->ppm_total;
	POOL_ULOCK(m);

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
	int adj;

	adj = 0;
	POOL_LOCK(m);
	if (m->ppm_max && m->ppm_total > m->ppm_max)
		adj = m->ppm_max - m->ppm_total;
	else if (m->ppm_total < m->ppm_min)
		adj = m->ppm_min - m->ppm_total;
	POOL_ULOCK(m);

	if (adj < 0)
		psc_pool_shrink(m, -adj);
	else if (adj)
		psc_pool_grow(m, adj);
}

void
_psc_pool_reap(void)
{
	struct psc_poolmgr *m, *culprit;
	int mx, culpritmx;

	culpritmx = mx = 0;
	PLL_LOCK(&psc_pools);
	psclist_for_each_entry(m, &psc_pools.pll_listhd, ppm_lentry) {
		POOL_LOCK(m);
		if (m->ppm_flags & PPMF_REAP)
			mx = m->ppm_lc.lc_entsize * m->ppm_lc.lc_size;
		POOL_ULOCK(m);

		if (mx > culpritmx) {
			culprit = m;
			culpritmx = mx;
		}
	}
	PLL_ULOCK(&psc_pools);

	if (culprit)
		psc_pool_shrink(culprit, 5);
}

/*
 * psc_pool_get - grab an item from a pool.
 * @m: the pool manager.
 */
void *
psc_pool_get(struct psc_poolmgr *m)
{
	void *p;
	int n;

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
	 * If this pool user provided a reaper routine e.g.
	 * for reclaiming buffers on a clean list, try that.
	 */
	if (m->ppm_reapcb) {
		do {
			n = m->ppm_reapcb(&m->ppm_lc, atomic_read(
			    &m->ppm_lc.lc_wq_empty.wq_nwaitors));
			p = lc_getnb(&m->ppm_lc);
			if (p)
				return (p);
		} while (n);
	}

	/* If communal, try reaping another pool. */
	if ((m->ppm_flags & PPMF_REAP)) {
		_psc_pool_reap();
		psc_pool_grow(m, 5);
	}

	/* when all else fails, wait for it */
	psc_dbg("waiting on %s pool",
		  m->ppm_lc.lc_name);
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
	POOL_LOCK(m);
	lc_add(&m->ppm_lc, p);
	if (m->ppm_max)
		psc_assert(m->ppm_lc.lc_size <= m->ppm_max);
	POOL_ULOCK(m);

	/* XXX if above high watermark, free some entries */
	if (m->ppm_flags & PPMF_AUTO && lc_sz(&m->ppm_lc) > 5)
		psc_pool_shrink(m, 5);
}

int
_psc_pool_init(struct psc_poolmgr *m, ptrdiff_t offset, size_t entsize,
    int flags, int total, int (*initf)(void *), const char *namefmt, ...)
{
	va_list ap;

	memset(m, 0, sizeof(&m));
	pll_add(&psc_pools, m);
	m->ppm_flags = flags;
	m->ppm_initf = initf;

	va_start(ap, namefmt);
	_lc_reginit(&m->ppm_lc, offset, entsize, namefmt, ap);
	va_end(ap);

	if (total)
		return (psc_pool_grow(m, total));
	return (0);
}

struct psc_poolmgr *
psc_pool_lookup(const char *name)
{
	struct psc_poolmgr *m;

	PLL_LOCK(&psc_pools);
	psclist_for_each_entry(m, &psc_pools.pll_listhd, ppm_lentry)
		if (strcmp(name, m->ppm_lc.lc_name) == 0)
			break;
	PLL_ULOCK(&psc_pools);
	return (m);
}
