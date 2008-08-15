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

#include "psc_ds/listcache.h"
#include "psc_ds/lockedlist.h"
#include "psc_util/alloc.h"
#include "psc_util/assert.h"
#include "psc_util/cdefs.h"
#include "psc_util/lock.h"

struct psc_poolmgr {
	struct psclist_head	  ppm_lentry;		/* pool linker */
	struct psc_listcache	  ppm_lc;		/* free pool entries */
	int			  ppm_flags;		/* flags */
	int			  ppm_min;		/* min bound of #items */
	int			  ppm_max;		/* max bound of #items */
	int			  ppm_total;		/* #items in circulation */
	int			(*ppm_initf)(void *);	/* entry initializer */
	void			(*ppm_destroyf)(void *);/* entry deinitializer */
	int			(*ppm_reapcb)(struct psc_listcache *, int);
};

/* Pool manager flags. */
#define PPMF_AUTO	(1 << 0)	/* this pool automatically resizes */
#define PPMF_REAP	(1 << 1)	/* this pool may reap pigs and be reaped */

#define POOL_LOCK(m)	spinlock(&(m)->ppm_lc.lc_lock)
#define POOL_ULOCK(m)	freelock(&(m)->ppm_lc.lc_lock)

#define POOL_CHECK(m)								\
	do {									\
		psc_assert((m)->ppm_min >= 0);					\
		psc_assert((m)->ppm_max >= 0);					\
		psc_assert((m)->ppm_total >= 0);				\
	} while (0)

#define psc_pool_init(m, type, member, flags, total, initf, namefmt, ...)	\
	_psc_pool_init((m), offsetof(type, member), sizeof(type), (flags),	\
	    (total), (initf), namefmt, ## __VA_ARGS__)

struct psc_poolmgr *
	 psc_pool_lookup(const char *);
int	 psc_pool_grow(struct psc_poolmgr *, int);
int	 psc_pool_shrink(struct psc_poolmgr *, int);
int	 psc_pool_settotal(struct psc_poolmgr *, int);
void	 psc_pool_resize(struct psc_poolmgr *);
void	*psc_pool_get(struct psc_poolmgr *);
void	 psc_pool_return(struct psc_poolmgr *, void *);
int	 _psc_pool_init(struct psc_poolmgr *, ptrdiff_t, size_t,
		int, int, int (*)(void *), const char *, ...);

extern struct psc_lockedlist psc_pools;

#endif /* __PFL_POOL_H__ */
