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
 * Memory pool routines and definitions.
 */

#ifndef _PFL_POOL_H_
#define _PFL_POOL_H_

#include <sys/types.h>

#include <stdarg.h>

#include "pfl/dynarray.h"
#include "pfl/explist.h"
#include "pfl/listcache.h"
#include "pfl/lock.h"
#include "pfl/lockedlist.h"
#include "pfl/log.h"
#include "pfl/memnode.h"
#include "pfl/mlist.h"

#if PFL_DEBUG > 1
# include "pfl/str.h"
#endif

struct psc_poolmgr;

/*
 * Poolsets contain a group of poolmgrs which can reap memory from each
 * other.
 *
 * Dynamic arrays are used here so pools can be members of multiple sets
 * without having to worry about allocating/managing multiple
 * psclist_head members.
 */
struct psc_poolset {
	psc_spinlock_t		  pps_lock;
	struct psc_dynarray	  pps_pools;		/* poolmasters in set */
};

/*
 * Pool masters are containers for pool managers.  Pool managers
 * manage buffers local to a memory node.  On non-NUMA architectures,
 * they manage buffers globally.
 */
struct psc_poolmaster {
	psc_spinlock_t		  pms_lock;
	struct psc_dynarray	  pms_poolmgrs;		/* NUMA pools */
	struct psc_dynarray	  pms_sets;		/* poolset memberships */

	/* for initializing memnode poolmgrs */
	char			  pms_name[PEXL_NAME_MAX];
	ptrdiff_t		  pms_offset;		/* item offset to psc_listentry */
	int			  pms_entsize;		/* size of an entry item */
	int			  pms_total;		/* #items to populate */
	int			  pms_min;		/* min bound of #items */
	int			  pms_max;		/* max bound of #items */
	int			  pms_flags;		/* flags */
	int			  pms_thres;		/* autoresize threshold */
	void			 *pms_mwcarg;		/* mlist cond arg */

	int			(*pms_reclaimcb)(struct psc_poolmgr *);
};

/*
 * Pool managers are like free lists containing items which can be
 * retrieved from the pool and put into circulation across other list
 * caches or in any way seen fit.
 */
struct psc_poolmgr {
	union {
		struct psc_listcache	ppmu_lc;	/* free pool entries */
		struct pfl_mlist	ppmu_ml;
		struct psc_explist	ppmu_explist;
	} ppm_u;
	struct psc_poolmaster	 *ppm_master;

	int			  ppm_total;		/* #items in circulation */
	int			  ppm_min;		/* min bound of #items */
	int			  ppm_max;		/* max bound of #items */
	int			  ppm_thres;		/* autoresize threshold */
	int			  ppm_flags;		/* flags */
	int			  ppm_entsize;		/* entry size */
	psc_atomic32_t		  ppm_nwaiters;		/* #thrs waiting for item */
	struct pfl_mutex	  ppm_reclaim_mutex;	/* exclusive reclamation */

	struct pfl_opstat	 *ppm_opst_grows;
	struct pfl_opstat	 *ppm_opst_shrinks;
	struct pfl_opstat	 *ppm_opst_returns;
	struct pfl_opstat	 *ppm_opst_preaps;
	struct pfl_opstat	 *ppm_opst_fails;

	int			(*ppm_reclaimcb)(struct psc_poolmgr *);

#define ppm_explist	ppm_u.ppmu_explist
#define ppm_lc		ppm_u.ppmu_lc
#define ppm_ml		ppm_u.ppmu_ml

#define ppm_lentry	ppm_explist.pexl_lentry
#define ppm_nfree	ppm_explist.pexl_nitems
#define ppm_name	ppm_explist.pexl_name
#define ppm_nseen	ppm_explist.pexl_nseen
#define ppm_offset	ppm_explist.pexl_offset
#define ppm_pll		ppm_explist.pexl_pll
};

/* Pool manager flags */
#define PPMF_NONE		0		/* no pool manager flags specified */
#define PPMF_AUTO		(1 << 0)	/* pool automatically resizes */
#define PPMF_PIN		(1 << 1)	/* mlock(2) items */
#define PPMF_MLIST		(1 << 2)	/* backend storage is mgt'd via mlist */
#define PPMF_DESPERATE		(1 << 3)	/* initial reaping attempt failed */
#define PPMF_ZERO		(1 << 4)	/* zero objects before passing them out */
#define PPMF_ALIGN		(1 << 5)	/* align to system page boundaries */
#define PPMF_NOPREEMPT		(1 << 6)	/* do reactive reaping */
#define PPMF_PREEMPTQ		(1 << 7)	/* queued for preemptive reaping */
#define PPMF_IDLEREAP		(1 << 8)	/* idle reaping */

#define POOL_LOCK(m)		PLL_LOCK(&(m)->ppm_pll)
#define POOL_LOCK_ENSURE(m)	PLL_LOCK_ENSURE(&(m)->ppm_pll)
#define POOL_TRYLOCK(m)		PLL_TRYLOCK(&(m)->ppm_pll)
#define POOL_TRYRLOCK(m, lkd)	PLL_TRYRLOCK(&(m)->ppm_pll, (lkd))
#define POOL_ULOCK(m)		PLL_ULOCK(&(m)->ppm_pll)
#define POOL_RLOCK(m)		PLL_RLOCK(&(m)->ppm_pll)
#define POOL_URLOCK(m, lk)	PLL_URLOCK(&(m)->ppm_pll, (lk))

/* Sanity check */
#define POOL_CHECK(m)							\
	do {								\
		psc_assert((m)->ppm_min >= 0);				\
		psc_assert((m)->ppm_max >= 0);				\
		psc_assert((m)->ppm_total >= 0);			\
	} while (0)

#define POOL_IS_MLIST(m)	((m)->ppm_flags & PPMF_MLIST)

#define PSC_POOLSET_INIT	{ SPINLOCK_INIT, DYNARRAY_INIT }

/* default value of pool fill before freeing items directly on pool_return */
#define POOL_AUTOSIZE_THRESH	80

/*
 * Initialize a pool resource.
 * @p: pool master.
 * @type: managed structure type.
 * @member: name of psclist_head structure used to interlink managed structs.
 * @total: # of initial entries to allocate.
 * @min: for autosizing pools, smallest number of pool entries to shrink to.
 * @max: for autosizing pools, highest number of pool entries to grow to.
 * @initf: managed structure initialization constructor.
 * @destroyf: managed structure destructor.
 * @reclaimcb: for accessing stale items outside the pool during low memory
 *	conditions.
 * @namefmt: printf(3)-like name of pool for external access.
 */
#define psc_poolmaster_init(p, type, member, flags, total, min,	max,	\
	    reclaimcb, namefmt, ...)					\
	_psc_poolmaster_init((p), sizeof(type), offsetof(type, member),	\
	    (flags), (total), (min), (max),				\
	    (reclaimcb), NULL, (namefmt), ## __VA_ARGS__)

#define psc_poolmaster_getmgr(p)	_psc_poolmaster_getmgr((p), psc_memnode_getid())


/*
 * Increase #items in a pool.
 * @m: the pool manager.
 * @i: #items to add to pool.
 */
int psc_pool_try_grow(struct psc_poolmgr *, int);
/*
 * Decrease #items in a pool.
 * @m: the pool manager.
 * @i: #items to remove from pool.
 */
int psc_pool_try_shrink(struct psc_poolmgr *, int);

#define PPGF_NONBLOCK			(1 << 0)
#define PPGF_SHALLOW			(1 << 1)

#if PFL_DEBUG >= 2
# define _PSC_POOL_CHECK_OBJ(m, p)	psc_assert(pfl_memchk((p), 0xa5, (m)->ppm_entsize));
# define _PSC_POOL_CLEAR_OBJ(m, p)	memset((p), 0xa5, (m)->ppm_entsize)
#else
# define _PSC_POOL_CHECK_OBJ(m, p)	do { } while (0)
# define _PSC_POOL_CLEAR_OBJ(m, p)	do { } while (0)
#endif

#define _PSC_POOL_GET(m, fl)						\
	{								\
		void *_ptr;						\
									\
		_ptr = _psc_pool_get((m), (fl));			\
		psclog_diag("got item %p from pool %s", _ptr,		\
		    (m)->ppm_name);					\
		_PSC_POOL_CHECK_OBJ((m), _ptr);				\
		_ptr;							\
	}

#define psc_pool_get(m)			(_PSC_POOL_GET((m), 0))
#define psc_pool_tryget(m)		(_PSC_POOL_GET((m), PPGF_NONBLOCK))
#define psc_pool_shallowget(m)		(_PSC_POOL_GET((m), PPGF_SHALLOW))

#define psc_pool_return(m, p)						\
	do {								\
		_psc_pool_return((m), (p));				\
		psclog_diag("returned item %p to pool %s", (p),		\
		    (m)->ppm_name);					\
		(p) = NULL;						\
	} while (0)

struct psc_poolmgr *
	_psc_poolmaster_getmgr(struct psc_poolmaster *, int);
void	 pfl_poolmaster_destroy(struct psc_poolmaster *);
void	_psc_poolmaster_init(struct psc_poolmaster *, size_t, ptrdiff_t,
	    int, int, int, int, int (*)(struct psc_poolmgr *),
	    void *, const char *, ...);
void	_psc_poolmaster_initv(struct psc_poolmaster *, size_t, ptrdiff_t,
	    int, int, int, int, int (*)(struct psc_poolmgr *),
	    void *, const char *, va_list);

struct psc_poolmgr *
	  psc_pool_lookup(const char *);
void	*_psc_pool_get(struct psc_poolmgr *, int);
int	  psc_pool_gettotal(struct psc_poolmgr *);
int	  psc_pool_inuse(struct psc_poolmgr *);
int	  psc_pool_nfree(struct psc_poolmgr *);
int	  psc_pool_reap(struct psc_poolmgr *, int);
void	  psc_pool_reapmem(size_t);
void	  psc_pool_resize(struct psc_poolmgr *);
void	 _psc_pool_return(struct psc_poolmgr *, void *);
int	  psc_pool_settotal(struct psc_poolmgr *, int);
void	  psc_pool_share(struct psc_poolmaster *);
void	  psc_pool_unshare(struct psc_poolmaster *);

void	  psc_poolset_disbar(struct psc_poolset *, struct psc_poolmaster *);
void	  psc_poolset_enlist(struct psc_poolset *, struct psc_poolmaster *);
void	  psc_poolset_init(struct psc_poolset *);

extern struct psc_lockedlist	psc_pools;

#endif /* _PFL_POOL_H_ */
