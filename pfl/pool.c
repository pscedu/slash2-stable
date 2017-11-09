/* $Id$ */
/*
 * %ISC_START_LICENSE%
 * ---------------------------------------------------------------------
 * Copyright 2015-2016, Google, Inc.
 * Copyright (c) 2006-2015, Pittsburgh Supercomputing Center (PSC).
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

/*
 * Memory pool routines.
 *
 * XXX: watch out, during communal reaping, for two pools
 * to continually reap other back and forth.
 */

#include <sys/param.h>

#include <errno.h>

#include "pfl/alloc.h"
#include "pfl/cdefs.h"
#include "pfl/dynarray.h"
#include "pfl/opstats.h"
#include "pfl/listcache.h"
#include "pfl/lock.h"
#include "pfl/lockedlist.h"
#include "pfl/log.h"
#include "pfl/mem.h"
#include "pfl/pool.h"
#include "pfl/pthrutil.h"
#include "pfl/str.h"
#include "pfl/waitq.h"
#include "pfl/workthr.h"

/* See psc_ctlmsg_pool_prdat() on how to print out pool statistics */

#if PFL_DEBUG > 1
#  define _POOL_SETPROT(p, m, prot)					\
	psc_mprotect((void *)(((uintptr_t)(p)) &			\
	    ~(psc_pagesize - 1)), (m)->ppm_entsize, (prot))

#  define _POOL_PROTNONE(p, m)	_POOL_SETPROT((p), (m), PROT_NONE)
#  define _POOL_PROTRDWR(p, m)	_POOL_SETPROT((p), (m), PROT_READ | PROT_WRITE)

#  define POOL_ADD_ITEM(m, p)						\
	do {								\
		int _locked;						\
		void *_p;						\
									\
		_locked = POOL_RLOCK(m);				\
		_p = pll_peektail(&(m)->ppm_pll);			\
		if (_p)							\
			_POOL_PROTRDWR(_p, (m));			\
									\
		if (POOL_IS_MLIST(m))					\
			pfl_mlist_add(&(m)->ppm_ml, (p));		\
		else							\
			lc_addtail_ifalive(&(m)->ppm_lc, (p));		\
									\
		if (_p)							\
			_POOL_PROTNONE(_p, (m));			\
		_POOL_PROTNONE((p), (m));				\
		POOL_URLOCK((m), _locked);				\
	} while (0)

#  define POOL_TRYGETOBJ(m)						\
	_PFL_RVSTART {							\
		void *_p, *_next;					\
									\
		POOL_LOCK_ENSURE(m);					\
		_p = pll_peekhead(&(m)->ppm_pll);			\
		if (_p) {						\
			_POOL_PROTRDWR(_p, (m));			\
			_next = psclist_next_obj2(			\
			    &(m)->ppm_pll.pll_listhd, _p,		\
			    (m)->ppm_offset);				\
			if (_next)					\
				_POOL_PROTRDWR(_next, (m));		\
			pll_remove(&(m)->ppm_pll, _p);			\
			if (_next)					\
				_POOL_PROTNONE(_next, (m));		\
		}							\
		_p;							\
	} _PFL_RVEND
#else
#  define POOL_ADD_ITEM(m, p)						\
	do {								\
		if (POOL_IS_MLIST(m))					\
			pfl_mlist_add(&(m)->ppm_ml, (p));		\
		else							\
			lc_addtail_ifalive(&(m)->ppm_lc, (p));		\
	} while (0)

#  define POOL_TRYGETOBJ(m)						\
	_PFL_RVSTART {							\
		void *_p;						\
		POOL_LOCK_ENSURE(m);					\
		_p = (POOL_IS_MLIST(m) ? 				\
			pfl_mlist_tryget(&(m)->ppm_ml) :		\
			lc_getnb(&(m)->ppm_lc));			\
		_p;							\
	} _PFL_RVEND
#endif

__static struct psc_poolset psc_poolset_main = PSC_POOLSET_INIT;
struct psc_lockedlist psc_pools =
    PLL_INIT(&psc_pools, struct psc_poolmgr, ppm_lentry);

struct pfl_wkdata_poolreap {
	struct psc_poolmgr *poolmgr;
};

void
_psc_poolmaster_initv(struct psc_poolmaster *p, size_t entsize,
    ptrdiff_t offset, int flags, int total, int min, int max,
    int (*reclaimcb)(struct psc_poolmgr *), void *mwcarg,
    const char *namefmt, va_list ap)
{
	memset(p, 0, sizeof(*p));
	INIT_SPINLOCK(&p->pms_lock);
	psc_dynarray_init(&p->pms_poolmgrs);
	psc_dynarray_init(&p->pms_sets);
	p->pms_reclaimcb = reclaimcb;
	p->pms_entsize = entsize;
	p->pms_offset = offset;
	p->pms_flags = flags;
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
    int (*reclaimcb)(struct psc_poolmgr *), void *mwcarg,
    const char *namefmt, ...)
{
	va_list ap;

	va_start(ap, namefmt);
	_psc_poolmaster_initv(p, entsize, offset, flags, total, min,
	    max, reclaimcb, mwcarg, namefmt, ap);
	va_end(ap);
}

int
psc_pool_cmp(const void *a, const void *b)
{
	const struct psc_poolmgr *ma = a, *mb = b;

	return (strcmp(ma->ppm_name, mb->ppm_name));
}

int
_psc_poolmaster_initmgr(struct psc_poolmaster *p, struct psc_poolmgr *m)
{
	int n, locked;
	char name[PEXL_NAME_MAX];

	memset(m, 0, sizeof(*m));
	psc_mutex_init(&m->ppm_reclaim_mutex);

	if (p->pms_flags & PPMF_MLIST) {
#ifdef HAVE_NUMA
		_pfl_mlist_init(&m->ppm_ml, PMWCF_WAKEALL,
		    p->pms_mwcarg, p->pms_offset, "%s:%d", p->pms_name,
		    psc_memnode_getid());
#else
		_pfl_mlist_init(&m->ppm_ml, PMWCF_WAKEALL,
		    p->pms_mwcarg, p->pms_offset, "%s", p->pms_name);
#endif
	} else {

#ifdef HAVE_NUMA
		n = snprintf(name, sizeof(m->ppm_name),
		    "%s:%d", p->pms_name, psc_memnode_getid());
#else
		n = snprintf(name, sizeof(m->ppm_name),
		    "%s", p->pms_name);
#endif
		_lc_init(&m->ppm_lc, (const char *)name, p->pms_offset);
		strlcpy(m->ppm_name, name, PEXL_NAME_MAX);

		if (n == -1)
			psc_fatal("snprintf %s", p->pms_name);
		if (n >= (int)sizeof(m->ppm_name))
			psc_fatalx("%s: name too long", p->pms_name);
	}

	if (m->ppm_name[0] == '\0')
		psc_assert(0);

	INIT_PSC_LISTENTRY(&m->ppm_lentry);

	locked = reqlock(&p->pms_lock);
	m->ppm_reclaimcb = p->pms_reclaimcb;

	m->ppm_thres = p->pms_thres;
	m->ppm_flags = p->pms_flags;
	m->ppm_entsize = p->pms_entsize;
	m->ppm_min = p->pms_min;
	m->ppm_max = p->pms_max;
	m->ppm_master = p;

	m->ppm_nseen = pfl_opstat_initf(OPSTF_BASE10,
	    "pool.%s.seen", m->ppm_name);
	m->ppm_opst_grows = pfl_opstat_initf(OPSTF_BASE10,
	    "pool.%s.grows", m->ppm_name);
	m->ppm_opst_shrinks = pfl_opstat_initf(OPSTF_BASE10,
	    "pool.%s.shrinks", m->ppm_name);
	m->ppm_opst_returns = pfl_opstat_initf(OPSTF_BASE10,
	    "pool.%s.returns", m->ppm_name);
	m->ppm_opst_preaps = pfl_opstat_initf(OPSTF_BASE10,
	    "pool.%s.preaps", m->ppm_name);
	m->ppm_opst_fails = pfl_opstat_initf(OPSTF_BASE10,
	    "pool.%s.fails", m->ppm_name);

	n = p->pms_total;
	ureqlock(&p->pms_lock, locked);

	pll_add_sorted(&psc_pools, m, psc_pool_cmp);

	if (n)
		n = psc_pool_try_grow(m, n);
	return (n);
}

/*
 * Obtain a pool manager for this NUMA from the master.
 * @p: pool master.
 * @memnid: memory node ID.
 */
struct psc_poolmgr *
_psc_poolmaster_getmgr(struct psc_poolmaster *p, int memnid)
{
	struct psc_poolmgr *m, **mv;

	spinlock(&p->pms_lock);
	if (psc_dynarray_ensurelen(&p->pms_poolmgrs, memnid + 1) == -1)
		psc_fatal("unable to resize poolmgrs");
	mv = psc_dynarray_get_mutable(&p->pms_poolmgrs);
	m = mv[memnid];
	if (m == NULL) {
		m = PSCALLOC(sizeof(*m));
		_psc_poolmaster_initmgr(p, m);
		psc_dynarray_setpos(&p->pms_poolmgrs, memnid, m);
	}
	freelock(&p->pms_lock);
	return (m);
}

void
pfl_poolmaster_destroy(struct psc_poolmaster *pms)
{
	struct psc_poolmgr *m;
	int i;

	spinlock(&pms->pms_lock);
	DYNARRAY_FOREACH(m, i, &pms->pms_poolmgrs) {
		pll_remove(&psc_pools, m);

		psc_mutex_destroy(&m->ppm_reclaim_mutex);

		if (pms->pms_flags & PPMF_MLIST)
			pfl_mlist_destroy(&m->ppm_ml);
		else
			pfl_listcache_destroy(&m->ppm_lc);

		pfl_opstat_destroy(m->ppm_nseen);
		pfl_opstat_destroy(m->ppm_opst_grows);
		pfl_opstat_destroy(m->ppm_opst_shrinks);
		pfl_opstat_destroy(m->ppm_opst_returns);
		pfl_opstat_destroy(m->ppm_opst_preaps);
		pfl_opstat_destroy(m->ppm_opst_fails);
		PSCFREE(m);
	}
	freelock(&pms->pms_lock);
	psc_dynarray_free(&pms->pms_poolmgrs);
	psc_dynarray_free(&pms->pms_sets);
}

/*
 * Release an object allocated by pool.
 * @m: pool manager.
 * @p: item to free.
 */
void
_psc_pool_destroy_obj(struct psc_poolmgr *m, void *p)
{
	int flags;

	flags = 0;
	if (m->ppm_flags & PPMF_PIN)
		flags |= PAF_LOCK;
	_PSC_POOL_CLEAR_OBJ(m, p);
	psc_free(p, flags, m->ppm_entsize);
}

/*
 * Increase #items in a pool.
 * @m: the pool manager.
 * @n: #items to add to pool.
 */
int
psc_pool_try_grow(struct psc_poolmgr *m, int n)
{
	void *p;
	int i, flags;

	psc_assert(n > 0);

	POOL_LOCK(m);
	POOL_CHECK(m);
	if (m->ppm_max) {
		if (m->ppm_total == m->ppm_max) {
			POOL_ULOCK(m);
			return (0);
		}
		/* Bound number to add to ppm_max. */
		n = MIN(n, m->ppm_max - m->ppm_total);
	}
	POOL_ULOCK(m);

	flags = PAF_CANFAIL;
	if (m->ppm_flags & PPMF_PIN)
		flags |= PAF_LOCK;
	if (m->ppm_flags & PPMF_ALIGN)
		flags |= PAF_PAGEALIGN;
	for (i = 0; i < n; i++) {
		/*
 		 * Do not hold any locks because we might call psc_pool_reapmem()
 		 * when memory is low.
 		 */
		p = psc_alloc(m->ppm_entsize, flags);
		if (p == NULL) {
			fprintf(stderr, "ENOMEM: m = %p, name = %s, n = %d\n", 
				p, m->ppm_master->pms_name, n); 
			errno = ENOMEM;
			return (i);
		}
		INIT_PSC_LISTENTRY(psclist_entry2(p,
		    m->ppm_explist.pexl_offset));

#if 0
		fprintf(stderr, "m = %p, p = %p, size = %d, name = %s\n", 
		    m, p, m->ppm_entsize, m->ppm_master->pms_name);
#endif
			
		/*
 		 * Add unconditionally, we might go overboard occasionally.
 		 */
		POOL_LOCK(m);
		m->ppm_total++;
		pfl_opstat_incr(m->ppm_opst_grows);
		POOL_ADD_ITEM(m, p);
		POOL_ULOCK(m);
	}
	return (i);
}

int
psc_pool_try_shrink(struct psc_poolmgr *m, int n)
{
	int i;
	void *p;

	POOL_LOCK(m);
	for (i = 0; i < n; i++) {
		if (m->ppm_total > m->ppm_min) {
			p = POOL_TRYGETOBJ(m);
			if (!p) {
				/*
				 * 09/23/2016: hit this message with pool bmpce. Need
				 * to double check locking.
				 *
				 * 07/07/2017: When lock rework, the following message
				 * should never appear again.
				 *
				 * On a second thought, it is still possible because
				 * a different task might grab an item off the list.
				 */
				fprintf(stderr, "pool corrupt? m = %p, total = %d, "
				    "min = %d, name = %s.\n", 
				    m, m->ppm_total, m->ppm_min, 
				    m->ppm_master->pms_name);
				break;
			}
			_psc_pool_destroy_obj(m, p);
			m->ppm_total--;
			pfl_opstat_incr(m->ppm_opst_shrinks);
		} else {
			break;
		}
	}
	POOL_ULOCK(m);
	return (i);
}

/*
 * Set #items in a pool.
 * @m: the pool manager.
 * @total: #items the pool should contain.
 */
int
psc_pool_settotal(struct psc_poolmgr *m, int total)
{
	int adj;

	POOL_LOCK(m);
	if (m->ppm_max && total > m->ppm_max)
		total = m->ppm_max;
	else if (total < m->ppm_min)
		total = m->ppm_min;
	adj = total - m->ppm_total;
	POOL_ULOCK(m);

	if (adj < 0)
		adj = psc_pool_try_shrink(m, -adj);
	else if (adj)
		adj = psc_pool_try_grow(m, adj);
	return (adj);
}

/*
 * Resize a pool so the current total is between ppm_min and ppm_max.
 * Note the pool size may not go into effect immediately if enough
 * entries are not on the free list.
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
		psc_pool_try_shrink(m, -adj);
	else if (adj)
		psc_pool_try_grow(m, adj);
}

/*
 * Reap other pools in attempt to reclaim memory in the dire situation
 * of pool exhaustion.
 *
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
_psc_poolset_reap(struct psc_poolset *s,
    struct psc_poolmaster *initiator, size_t size)
{
	struct psc_poolmgr *m, *culprit;
	struct psc_poolmaster *p;
	size_t tmx, culpritmx;
	int i, nobj;

	nobj = 0;
	culprit = NULL;
	culpritmx = tmx = 0;
	spinlock(&s->pps_lock);
	DYNARRAY_FOREACH(p, i, &s->pps_pools) {
		if (p == initiator)
			continue;
		m = psc_poolmaster_getmgr(p);

		if (!POOL_TRYLOCK(m))
			continue;
		tmx = m->ppm_entsize * m->ppm_nfree;
		nobj = MAX(size / m->ppm_entsize, 1);
		POOL_ULOCK(m);

		if (tmx > culpritmx) {
			culprit = m;
			culpritmx = tmx;
		}
	}
	freelock(&s->pps_lock);

	if (culprit && nobj)
		psc_pool_try_shrink(culprit, nobj);
}

/*
 * Reap some memory from a global (or local on NUMAs)
 *	memory pool.
 * @size: desired amount of memory to reclaim.
 */
void
psc_pool_reapmem(size_t size)
{
	_psc_poolset_reap(&psc_poolset_main, NULL, size);
}

int
psc_pool_reap(struct psc_poolmgr *m, int desperate)
{
	int reaped;

	reaped = m->ppm_reclaimcb(m);
	return (reaped);
}

int
pfl_pool_reap_wkcb(void *arg)
{
	struct pfl_wkdata_poolreap *wk = arg;
	struct psc_poolmgr *m = wk->poolmgr;

	psc_pool_reap(m, 0);
	POOL_LOCK(m);
	m->ppm_flags &= ~PPMF_PREEMPTQ;
	POOL_ULOCK(m);
	return (0);
}

/*
 * Grab an item from a pool.
 * @m: the pool manager.
 */
void *
_psc_pool_get(struct psc_poolmgr *m, int flags)
{
	int desperate = 0, reaped = 0, locked, n;
	void *p;

	POOL_LOCK(m);
	/*
	 * (gdb) p m.ppm_u.ppmu_explist.pexl_pll.
	 *
	 * Can we grow even if PPGF_NONBLOCK is set?
	 *
	 */
	p = POOL_TRYGETOBJ(m);
	if (p || (flags & PPGF_NONBLOCK))
		PFL_GOTOERR(gotitem, 0);

	n = 0;

	/* If total < min, try to grow the pool. */
	if (m->ppm_min > m->ppm_total) {
		n = m->ppm_min - m->ppm_total;
		goto grow;
	}
	if (m->ppm_max) {
		n = MIN(2, m->ppm_max - m->ppm_total);
		goto grow;
	}

 grow:
	if (n > 0) {
		POOL_ULOCK(m);
		psc_pool_try_grow(m, n);
		POOL_LOCK(m);

		p = POOL_TRYGETOBJ(m);
		if (p)
			PFL_GOTOERR(gotitem, 0);
	}

 reclaim:

	/* If this pool has a reclaimer routine, try that. */
	if (m->ppm_reclaimcb) {

		POOL_ULOCK(m);
		reaped = psc_pool_reap(m, desperate);
		POOL_LOCK(m);

		p = POOL_TRYGETOBJ(m);
		if (p)
			PFL_GOTOERR(gotitem, 0);
	}


	/* If autoresizable, try to grow the pool. */
	while (m->ppm_flags & PPMF_AUTO) {
		/*
		 * XXX we should increase by some % to amortize
		 * allocation costs.
		 *
		 * adj = MAX(1, m->ppm_total / 20)
		 * adj = MIN(64, adj)
		 * adj = PSC_ALIGN(adj, 8)
		 *
		 * If there is a max setting, the following grow
		 * will fail if we have aleady reached our limit.
		 */
		POOL_ULOCK(m);
		n = psc_pool_try_grow(m, 2); // MAX(1, m->ppm_total / 20)
		POOL_LOCK(m);

		p = POOL_TRYGETOBJ(m);
		if (p)
			PFL_GOTOERR(gotitem, 0);

		/* If we were unable to grow, stop. */
		if (n == 0)
			break;
	}

	/* If communal, try reaping other pools in sets. */
	if (psc_dynarray_len(&m->ppm_master->pms_sets)) {
		POOL_ULOCK(m);

		locked = reqlock(&m->ppm_master->pms_lock);
		for (n = 0; n < psc_dynarray_len(
		    &m->ppm_master->pms_sets); n++)
			_psc_poolset_reap(psc_dynarray_getpos(
			    &m->ppm_master->pms_sets, n), m->ppm_master,
			    m->ppm_entsize);
		ureqlock(&m->ppm_master->pms_lock, locked);
		if (n) {
			psc_pool_try_grow(m, 2);

			POOL_LOCK(m);
			p = POOL_TRYGETOBJ(m);
			if (p)
				PFL_GOTOERR(gotitem, 0);
		} else
			POOL_LOCK(m);
	}

	/*
	 * Try once more for a multiwait-list; if nothing, the caller
	 * should add a condition should be added to the multiwait since
	 * there is nothing more we can do to get an object at the
	 * moment.  This invocation should have been done in a multiwait
	 * critical section to prevent dropping notifications.
	 */
	if (POOL_IS_MLIST(m)) {
		p = POOL_TRYGETOBJ(m);
		PFL_GOTOERR(gotitem, 0);
	}

	if (reaped && !m->ppm_nfree) {
		reaped = 0;
		goto reclaim;
	}

	/* Nothing else we can do; wait for an item to return. */
	psc_atomic32_inc(&m->ppm_nwaiters);
	/*
 	 * (gdb) p m->ppm_u.ppmu_lc.plc_explist.pexl_pll.pll_nitems
 	 * (gdb) p m->ppm_u.ppmu_lc.plc_explist.pexl_name
 	 */
	p = lc_getwait(&m->ppm_lc);
	psc_atomic32_dec(&m->ppm_nwaiters);

 gotitem:

	if (p && m->ppm_reclaimcb && !m->ppm_nfree &&
	    (m->ppm_flags & (PPMF_NOPREEMPT | PPMF_PREEMPTQ)) == 0 &&
	    m != pfl_workrq_pool && pfl_workrq_pool) {
		struct pfl_wkdata_poolreap *wk;

		wk = pfl_workq_getitem_nb(pfl_pool_reap_wkcb,
		    struct pfl_wkdata_poolreap);
		if (wk) {
			m->ppm_flags |= PPMF_PREEMPTQ;
			wk->poolmgr = m;
			pfl_workq_putitem(wk);
			pfl_opstat_incr(m->ppm_opst_preaps);
		}
	}
	POOL_ULOCK(m);
	if (p == NULL)
		pfl_opstat_incr(m->ppm_opst_fails);
	return (p);
}

/*
 * Return an item to a pool.
 * @m: the pool manager.
 * @p: item to return.
 */
void
_psc_pool_return(struct psc_poolmgr *m, void *p)
{
	int locked;

	/*
	 * If pool max is less than total (i.e., when an administrator lowers 
	 * max below total) or free reaches the auto resize threshold, which
	 * can be adjusted, directly free this item.
	 *
	 * m->ppm_nfree: m->ppm_u.ppmu_explist.pexl_pll.pll_nitems
	 */
	locked = POOL_RLOCK(m);
	pfl_opstat_incr(m->ppm_opst_returns);
	if ((m->ppm_flags & PPMF_AUTO) && m->ppm_total > m->ppm_min &&
	    ((m->ppm_max && m->ppm_total > m->ppm_max) ||
	     m->ppm_nfree > m->ppm_total * m->ppm_thres / 100)) {
		/* Reached free threshold; completely deallocate obj. */
		_psc_pool_destroy_obj(m, p);
		m->ppm_total--;
		pfl_opstat_incr(m->ppm_opst_shrinks);
		POOL_URLOCK(m, locked);
	} else {
		/* Pool should keep this item. */
#if 0
		/*
		 * We need this if all the memory of the item will be used 
		 * including the link entry. Right now, we don't do this.
		 *
		 * 10/23/2017: Looks like we should not touch the linkage
		 * entry.  If we zero the whole item, we must re-init the
		 * list entry.
		 */

		INIT_PSC_LISTENTRY(psclist_entry2(p, m->ppm_explist.pexl_offset));
#endif
		POOL_ADD_ITEM(m, p);
		POOL_URLOCK(m, locked);
	}
}

/*
 * Obtain the number of objects in a pool circulation (note: this is not
 * the number of free objects available for use).
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

int
psc_pool_inuse(struct psc_poolmgr *m)
{
	int locked, rc;

	locked = POOL_RLOCK(m);
	rc = m->ppm_total != m->ppm_nfree;
	POOL_URLOCK(m, locked);
	return (rc);
}

/*
 * Retrieve the number of free/available items in a pool.
 * @m: pool.
 */
int
psc_pool_nfree(struct psc_poolmgr *m)
{
	int locked, nf;

	locked = POOL_RLOCK(m);
	nf = m->ppm_nfree;
	POOL_URLOCK(m, locked);
	return (nf);
}

/*
 * Find a pool by name.
 * @name: name of pool to find.
 */
struct psc_poolmgr *
psc_pool_lookup(const char *name)
{
	struct psc_poolmgr *m = NULL;

	PLL_LOCK(&psc_pools);
	PLL_FOREACH(m, &psc_pools)
		if (strcmp(name, m->ppm_name) == 0)
			break;
	PLL_ULOCK(&psc_pools);
	return (m);
}

/*
 * Allow a pool to share its resources with everyone.
 * @p: pool master to share.
 */
void
psc_pool_share(struct psc_poolmaster *p)
{
	psc_poolset_enlist(&psc_poolset_main, p);
}

/*
 * Disallow a pool from sharing its resources with everyone.
 * @p: pool master to unshare.
 */
void
psc_pool_unshare(struct psc_poolmaster *p)
{
	psc_poolset_disbar(&psc_poolset_main, p);
}

/*
 * Initialize a pool set.
 * @s: set to initialize.
 */
void
psc_poolset_init(struct psc_poolset *s)
{
	INIT_SPINLOCK(&s->pps_lock);
	psc_dynarray_init(&s->pps_pools);
}

/*
 * Add a pool master to a pool set.
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
 * Remove a pool master from from a pool set.
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
