/* $Id$ */

/*
 * Pool manager routines and definitions.
 *
 * Pools are like free lists but also track allocations for items
 * in circulation among multiple lists.
 *
 * Pools share allocations for items among themselves, so when
 * one greedy pool gets too large, others trim his size down.
 */

#ifndef __PFL_POOL_H__
#define __PFL_POOL_H__

#include <sys/param.h>

#include <stdarg.h>

#include "psc_ds/dynarray.h"
#include "psc_ds/listcache.h"
#include "psc_ds/lockedlist.h"
#include "psc_util/alloc.h"
#include "psc_util/assert.h"
#include "psc_util/cdefs.h"
#include "psc_util/lock.h"

struct psc_poolset {
	struct dynarray		  pps_pools;		/* pointers to pools */
};

#define PSC_POOLSET_INIT	{ DYNARRAY_INIT }

struct psc_poolmgr {
	struct psclist_head	  ppm_all_lentry;	/* global linker */
	struct dynarray		  ppm_sets;		/* pointers to sets */
	struct psc_listcache	  ppm_lc;		/* free pool entries */
	int			  ppm_flags;		/* flags */
	int			  ppm_min;		/* min bound of #items */
	int			  ppm_max;		/* max bound of #items */
	int			  ppm_total;		/* #items in circulation */
	int			(*ppm_initf)(void *);	/* entry initializer */
	void			(*ppm_destroyf)(void *);/* entry deinitializer */
	int			(*ppm_reclaimcb)(struct psc_listcache *, int);
};

/* Pool manager flags. */
#define PPMF_AUTO	(1 << 0)	/* pool automatically resizes */
#define PPMF_NOLOCK	(1 << 1)	/* pool ents shouldn't be alloc'd unswappably */

#define POOL_RLOCK(m)		reqlock(&(m)->ppm_lc.lc_lock)
#define POOL_ULOCK(m, l)	ureqlock(&(m)->ppm_lc.lc_lock, (l))

#define POOL_CHECK(m)								\
	do {									\
		psc_assert((m)->ppm_min >= 0);					\
		psc_assert((m)->ppm_max >= 0);					\
		psc_assert((m)->ppm_total >= 0);				\
	} while (0)

#define psc_pool_init(m, type, member, flags, total, max, initf, destroyf,	\
	    reclaimcb, namefmt, ...)						\
	_psc_pool_init((m), offsetof(type, member), sizeof(type), (flags), 	\
	    (total), (max), (initf), (destroyf), (reclaimcb), (namefmt),	\
	    ## __VA_ARGS__)

#define psc_pool_joindefset(m)	psc_pool_joinset((m), &psc_pooldefset)

struct psc_poolmgr *
	 psc_pool_lookup(const char *);
int	 psc_pool_grow(struct psc_poolmgr *, int);
int	 psc_pool_shrink(struct psc_poolmgr *, int);
int	 psc_pool_settotal(struct psc_poolmgr *, int);
void	 psc_pool_resize(struct psc_poolmgr *);
void	 psc_pool_joinset(struct psc_poolmgr *, struct psc_poolset *);
void	 psc_pool_leaveset(struct psc_poolmgr *, struct psc_poolset *);
void	*psc_pool_get(struct psc_poolmgr *);
void	 psc_pool_return(struct psc_poolmgr *, void *);
void	 psc_pool_destroy(struct psc_poolmgr *);
int	 _psc_pool_init(struct psc_poolmgr *, ptrdiff_t, size_t,
		int, int, int, int (*)(void *), void (*)(void *),
		int (*)(struct psc_listcache *, int), const char *, ...);

void	 psc_poolset_init(struct psc_poolset *);
void	 psc_poolset_destroy(struct psc_poolset *);

extern struct psc_lockedlist	psc_pools;
extern struct psc_poolset	psc_pooldefset;

#endif /* __PFL_POOL_H__ */
