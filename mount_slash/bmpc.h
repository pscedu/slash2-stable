/* $Id$ */
/*
 * %PSC_START_COPYRIGHT%
 * -----------------------------------------------------------------------------
 * Copyright (c) 2006-2010, Pittsburgh Supercomputing Center (PSC).
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

#ifndef _SL_BMPC_H_
#define _SL_BMPC_H_

#include <time.h>

#include "psc_ds/list.h"
#include "psc_ds/listcache.h"
#include "psc_ds/lockedlist.h"
#include "psc_ds/tree.h"
#include "psc_ds/vbitmap.h"
#include "psc_util/atomic.h"
#include "psc_util/lock.h"
#include "psc_util/pool.h"
#include "psc_util/time.h"
#include "psc_util/waitq.h"

#include "cache_params.h"
#include "bmap.h"
#include "buffer.h"

extern struct psc_poolmgr	*bmpcePoolMgr;
extern struct bmpc_mem_slbs	 bmpcSlabs;
extern struct psc_listcache	 bmpcLru;

#define BMPC_BUFSZ		SLASH_BMAP_BLKSZ
#define BMPC_BLKSZ		BMPC_BUFSZ
#define BMPC_SLB_NBLKS		256  /* 8MB slab */
#define BMPC_DEFSLBS		8
#define BMPC_MAXSLBS		32
#define BMPC_BUFMASK		(BMPC_BLKSZ - 1)
#define BMPC_SLBBUFMASK		((BMPC_BLKSZ * BMPC_SLB_NBLKS) - 1)
#define BMPC_IOMAXBLKS		64
#define BMPC_MAXBUFSRPC		(1048576 / BMPC_BUFSZ)

#define BMPC_DEF_MINAGE		{ 0, 600000000 } /* seconds, nanoseconds */
#define BMPC_INTERVAL		{ 0, 200000000 }

struct bmpc_mem_slbs {
	atomic_t		bmms_waiters;
	uint16_t		bmms_reap;
	struct timespec		bmms_minage;
	psc_spinlock_t		bmms_lock;
	struct psc_lockedlist	bmms_slbs;
	struct psc_waitq	bmms_waitq;
};

#define lockBmpcSlabs()		spinlock(&bmpcSlabs.bmms_lock)
#define ulockBmpcSlabs()	freelock(&bmpcSlabs.bmms_lock)

struct bmap_pagecache_entry {
	psc_atomic16_t		 bmpce_wrref;	/* pending write ops        */
	psc_atomic16_t		 bmpce_rdref;	/* pending read ops         */
	psc_atomic16_t		 bmpce_infref;	/* inflight ref             */
	uint32_t		 bmpce_flags;	/* state bits               */
	uint32_t		 bmpce_off;	/* filewise, bmap relative  */
	psc_spinlock_t		 bmpce_lock;	/* serialize                */
	void			*bmpce_base;	/* base pointer from slb    */
	struct psc_waitq	*bmpce_waitq;	/* others block here on I/O */
	struct psclist_head	 bmpce_lentry;	/* chain on bmap lru        */
	struct timespec		 bmpce_laccess;	/* last page access         */
	SPLAY_ENTRY(bmap_pagecache_entry) bmpce_tentry;
};

#define BMPCE_LOCK(b)  spinlock(&(b)->bmpce_lock)
#define BMPCE_ULOCK(b) freelock(&(b)->bmpce_lock)

enum {
	BMPCE_NEW       = (1 << 0),
	BMPCE_GETBUF    = (1 << 1),
	BMPCE_DATARDY   = (1 << 2),
	BMPCE_DIRTY2LRU = (1 << 3),
	BMPCE_LRU       = (1 << 4),
	BMPCE_FREE      = (1 << 5),
	BMPCE_FREEING   = (1 << 6),
	BMPCE_INIT      = (1 << 7),
	BMPCE_READPNDG  = (1 << 8),
	BMPCE_IOSCHED   = (1 << 9),
	BMPCE_RBWPAGE   = (1 << 10)
};

#define BMPCE_2_BIORQ(b) ((b)->bmpce_waitq == NULL) ? NULL :		\
	(((char *)((b)->bmpce_waitq)) -					\
	 offsetof(struct bmpc_ioreq, biorq_waitq))


#define BMPCE_FLAGS_FORMAT "%s%s%s%s%s%s%s%s%s%s%s"
#define DEBUG_BMPCE_FLAGS(b)						\
	(b)->bmpce_flags & BMPCE_NEW			? "n" : "",	\
	(b)->bmpce_flags & BMPCE_GETBUF			? "g" : "",	\
	(b)->bmpce_flags & BMPCE_DATARDY		? "d" : "",	\
	(b)->bmpce_flags & BMPCE_DIRTY2LRU		? "D" : "",	\
	(b)->bmpce_flags & BMPCE_LRU			? "l" : "",	\
	(b)->bmpce_flags & BMPCE_FREE			? "f" : "",	\
	(b)->bmpce_flags & BMPCE_FREEING		? "F" : "",	\
	(b)->bmpce_flags & BMPCE_INIT			? "i" : "",	\
	(b)->bmpce_flags & BMPCE_READPNDG		? "r" : "",	\
	(b)->bmpce_flags & BMPCE_IOSCHED		? "I" : "",	\
	(b)->bmpce_flags & BMPCE_RBWPAGE		? "B" : ""

#define DEBUG_BMPCE(level, b, fmt, ...)					\
	psc_logs((level), PSS_GEN,					\
		 "bmpce@%p fl=%u o=%x b=%p ts=%ld:%ld wr=%hu rd=%hu "	\
		 "inf=%hu lru=%d biorq=%p "BMPCE_FLAGS_FORMAT" "fmt,	\
		 (b), (b)->bmpce_flags, (b)->bmpce_off, (b)->bmpce_base, \
		 (b)->bmpce_laccess.tv_sec, (b)->bmpce_laccess.tv_nsec,	\
		 psc_atomic16_read(&(b)->bmpce_wrref),			\
		 psc_atomic16_read(&(b)->bmpce_rdref),			\
		 psc_atomic16_read(&(b)->bmpce_infref),			\
		 psclist_conjoint(&(b)->bmpce_lentry),			\
		 BMPCE_2_BIORQ(b),					\
		 DEBUG_BMPCE_FLAGS(b), ## __VA_ARGS__)

static __inline int
bmpce_sort_cmp(const void *x, const void *y)
{
	const struct bmap_pagecache_entry * const *pa = x, *a = *pa;
	const struct bmap_pagecache_entry * const *pb = y, *b = *pb;

	return (CMP(a->bmpce_off, b->bmpce_off));
}

static __inline int
bmpce_lrusort_cmp(const void *x, const void *y)
{
	const struct bmap_pagecache_entry * const *pa = x, *a = *pa;
	const struct bmap_pagecache_entry * const *pb = y, *b = *pb;

	if (timespeccmp(&a->bmpce_laccess, &b->bmpce_laccess, <))
		return (-1);

	if (timespeccmp(&a->bmpce_laccess, &b->bmpce_laccess, >))
		return (1);

	return (0);
}

static __inline int
bmpce_lrusort_cmp1(const void *x, const void *y)
{
	const struct bmap_pagecache_entry *a = x;
	const struct bmap_pagecache_entry *b = y;

	if (timespeccmp(&a->bmpce_laccess, &b->bmpce_laccess, <))
		return (-1);

	if (timespeccmp(&a->bmpce_laccess, &b->bmpce_laccess, >))
		return (1);

	return (0);
}

static __inline int
bmpce_cmp(const void *x, const void *y)
{
	const struct bmap_pagecache_entry *a = x, *b = y;

	return (CMP(a->bmpce_off, b->bmpce_off));
}

SPLAY_HEAD(bmap_pagecachetree, bmap_pagecache_entry);
SPLAY_PROTOTYPE(bmap_pagecachetree, bmap_pagecache_entry, bmpce_tentry,
		bmpce_cmp);

struct bmap_pagecache {
	struct bmap_pagecachetree bmpc_tree;   /* tree of cbuf_handle        */
	struct timespec           bmpc_oldest; /* LRU's oldest item          */
	struct psc_lockedlist     bmpc_lru;    /* cleancnt can be kept here  */
	struct psc_lockedlist     bmpc_new_biorqs;
	struct psc_lockedlist     bmpc_pndg_biorqs; /* chain pending I/O requests */
	atomic_t                  bmpc_pndgwr; /* # pending wr req           */
	psc_spinlock_t            bmpc_lock;   /* serialize tree and pll     */
	struct psclist_head       bmpc_lentry; /* chain to global LRU lc     */
};

#define BMPC_LOCK(b)	spinlock(&(b)->bmpc_lock)
#define BMPC_ULOCK(b)	freelock(&(b)->bmpc_lock)

static __inline int
bmpc_queued_writes(struct bmap_pagecache *bmpc)
{
	int nwrites = atomic_read(&bmpc->bmpc_pndgwr);

	psc_assert(nwrites >= 0);

	if (nwrites > 0)
		psc_assert((pll_nitems(&bmpc->bmpc_pndg_biorqs) > 0) ||
			   (pll_nitems(&bmpc->bmpc_new_biorqs) > 0));

	return (nwrites);
}

struct bmpc_ioreq {
	uint32_t                   biorq_off;   /* filewise, bmap relative   */
	uint32_t                   biorq_len;   /* non-aligned, real length  */
	uint32_t                   biorq_flags; /* state and op type bits    */
	psc_spinlock_t             biorq_lock;
	struct timespec            biorq_start; /* issue time                */
	struct psc_dynarray        biorq_pages; /* array of bmpce            */
	struct psclist_head        biorq_lentry;/* chain on bmpc_pndg_biorqs        */
	struct bmapc_memb         *biorq_bmap;  /* backpointer to our bmap   */
	struct pscrpc_request_set *biorq_rqset;
	struct psc_waitq           biorq_waitq;
};

enum {
	BIORQ_READ         = (1<<0),
	BIORQ_WRITE        = (1<<1),
	BIORQ_RBWFP        = (1<<2),
	BIORQ_RBWLP        = (1<<3),
	BIORQ_SCHED        = (1<<4),
	BIORQ_INFL         = (1<<5),
	BIORQ_DIO          = (1<<6),
	BIORQ_FORCE_EXPIRE = (1<<7),
	BIORQ_DESTROY      = (1<<8),
	BIORQ_FLUSHRDY     = (1<<9)
};

#define BIORQ_FLAGS_FORMAT "%s%s%s%s%s%s%s%s%s%s"
#define DEBUG_BIORQ_FLAGS(b)						\
	(b)->biorq_flags & BIORQ_READ		? "r" : "",		\
	(b)->biorq_flags & BIORQ_WRITE		? "w" : "",		\
	(b)->biorq_flags & BIORQ_RBWFP		? "f" : "",		\
	(b)->biorq_flags & BIORQ_RBWLP		? "l" : "",		\
	(b)->biorq_flags & BIORQ_SCHED		? "s" : "",		\
	(b)->biorq_flags & BIORQ_INFL		? "i" : "",		\
	(b)->biorq_flags & BIORQ_DIO		? "d" : "",		\
	(b)->biorq_flags & BIORQ_FORCE_EXPIRE	? "x" : "",		\
	(b)->biorq_flags & BIORQ_DESTROY	? "D" : "",		\
	(b)->biorq_flags & BIORQ_FLUSHRDY	? "R" : ""

#define DEBUG_BIORQ(level, b, fmt, ...)					\
	psc_logs((level), PSS_GEN,					\
		 "biorq@%p fl=%x o=%x l=%d np=%d b=%p ts=%ld:%ld "	\
		 BIORQ_FLAGS_FORMAT" "fmt,				\
		 (b), (b)->biorq_flags, (b)->biorq_off, (b)->biorq_len,	\
		 psc_dynarray_len(&(b)->biorq_pages), (b)->biorq_bmap,	\
		 (b)->biorq_start.tv_sec, (b)->biorq_start.tv_nsec,	\
		 DEBUG_BIORQ_FLAGS(b), ## __VA_ARGS__)


static __inline void
bmpce_freeprep(struct bmap_pagecache_entry *bmpce)
{
	LOCK_ENSURE(&bmpce->bmpce_lock);

	psc_assert(!(bmpce->bmpce_flags &
		     (BMPCE_FREEING|BMPCE_GETBUF|BMPCE_NEW)));
	psc_assert(bmpce->bmpce_flags & BMPCE_DATARDY);

	psc_assert(!psc_atomic16_read(&bmpce->bmpce_rdref));
	psc_assert(!psc_atomic16_read(&bmpce->bmpce_wrref));
	psc_assert(!psc_atomic16_read(&bmpce->bmpce_infref));

	bmpce->bmpce_flags = BMPCE_FREEING;
}

static __inline void
bmpce_useprep(struct bmap_pagecache_entry *bmpce, struct bmpc_ioreq *biorq)
{
	psc_assert(!bmpce->bmpce_base);
	psc_assert(!bmpce->bmpce_waitq);
	psc_assert(psclist_disjoint(&bmpce->bmpce_lentry));
	psc_assert(bmpce->bmpce_flags == BMPCE_NEW);
	psc_assert(!psc_atomic16_read(&bmpce->bmpce_wrref));
	psc_assert(!psc_atomic16_read(&bmpce->bmpce_rdref));

	bmpce->bmpce_flags = (BMPCE_GETBUF | BMPCE_INIT);
	bmpce->bmpce_waitq = &biorq->biorq_waitq;
}

static __inline void
bmpce_usecheck(struct bmap_pagecache_entry *bmpce, int op, uint32_t off)
{
	int locked;

	locked = reqlock(&bmpce->bmpce_lock);

	DEBUG_BMPCE(PLL_NOTIFY, bmpce, "op=%d off=%u", op, off);

	while (bmpce->bmpce_flags & BMPCE_GETBUF) {
		psc_assert(!bmpce->bmpce_base);
		psc_assert(bmpce->bmpce_waitq);
		psc_waitq_wait(bmpce->bmpce_waitq, &bmpce->bmpce_lock);
		spinlock(&bmpce->bmpce_lock);
	}

	psc_assert(bmpce->bmpce_base);
	psc_assert(!(bmpce->bmpce_flags & BMPCE_GETBUF));

	if (op == BIORQ_READ)
		psc_assert(psc_atomic16_read(&bmpce->bmpce_rdref) > 0);
	else
		psc_assert(psc_atomic16_read(&bmpce->bmpce_wrref) > 0);

	psc_assert(bmpce->bmpce_off == off);
	ureqlock(&bmpce->bmpce_lock, locked);
}


static __inline void
bmpce_inflight_dec_locked(struct bmap_pagecache_entry *bmpce)
{
       if (!(bmpce->bmpce_flags & BMPCE_IOSCHED))
		   return;

	psc_atomic16_dec(&bmpce->bmpce_infref);
	psc_assert(psc_atomic16_read(&bmpce->bmpce_infref) >= 0);
	if (!psc_atomic16_read(&bmpce->bmpce_infref))
		bmpce->bmpce_flags &= ~BMPCE_IOSCHED;
}

static __inline void
bmpce_inflight_inc_locked(struct bmap_pagecache_entry *bmpce)
{
	psc_atomic16_inc(&bmpce->bmpce_infref);
	if (psc_atomic16_read(&bmpce->bmpce_infref) == 1) {
		psc_assert(!(bmpce->bmpce_flags & BMPCE_IOSCHED));
		bmpce->bmpce_flags |= BMPCE_IOSCHED;
	}
	DEBUG_BMPCE(PLL_INFO, bmpce, "set inflight");
}

/* biorq_is_my_bmpce - informs the caller that biorq, r, owns the
 *    the page cache entry, b.  This state implies that the thread
 *    processing 'r' is responsible for allocating a memory page
 *    and possible faulting in that page from the ION.
 */
#define biorq_is_my_bmpce(r, b)	(&(r)->biorq_waitq == (b)->bmpce_waitq)

#define biorq_getaligned_off(r, nbmpce) (((r)->biorq_off & ~BMPC_BUFMASK) + \
					 ((nbmpce) * BMPC_BUFSZ))

#define biorq_voff_get(r) ((r)->biorq_off + (r)->biorq_len)

#define bmpce_is_rbw_page(r, b, pos)					\
	(biorq_is_my_bmpce((r), (b)) &&					\
	 ((!(pos) && ((r)->biorq_flags & BIORQ_RBWFP)) ||		\
	  (((pos) == (psc_dynarray_len(&(r)->biorq_pages)-1) &&		\
	    ((r)->biorq_flags & BIORQ_RBWLP)))))

static __inline void
bmpc_lru_del(struct bmap_pagecache *bmpc)
{
	lc_remove(&bmpcLru, bmpc);
}

static __inline void
bmpc_init(struct bmap_pagecache *bmpc)
{
	memset(bmpc, 0, sizeof(*bmpc));

	LOCK_INIT(&bmpc->bmpc_lock);

	pll_init(&bmpc->bmpc_lru, struct bmap_pagecache_entry,
		 bmpce_lentry, &bmpc->bmpc_lock);

	pll_init(&bmpc->bmpc_pndg_biorqs, struct bmpc_ioreq,
		 biorq_lentry, &bmpc->bmpc_lock);

	pll_init(&bmpc->bmpc_new_biorqs, struct bmpc_ioreq,
		 biorq_lentry, &bmpc->bmpc_lock);

	/* Add the bmpc to the LRU where it will stay until it's freed.
	 */
	lc_add(&bmpcLru, bmpc);
}

static __inline void
bmpc_ioreq_init(struct bmpc_ioreq *ioreq, uint32_t off, uint32_t len, int op,
		struct bmapc_memb *bmap)
{
	memset(ioreq, 0, sizeof(*ioreq));
	psc_waitq_init(&ioreq->biorq_waitq);
	LOCK_INIT(&ioreq->biorq_lock);
	clock_gettime(CLOCK_REALTIME, &ioreq->biorq_start);

	ioreq->biorq_off  = off;
	ioreq->biorq_len  = len;
	ioreq->biorq_bmap = bmap;
	ioreq->biorq_flags = op;
	if (bmap->bcm_mode & BMAP_DIO)
		ioreq->biorq_flags |= BIORQ_DIO;
}

static __inline int
bmpc_lru_cmp(const void *x, const void *y)
{
	const struct bmap_pagecache * const *pa = x, *a = *pa;
	const struct bmap_pagecache * const *pb = y, *b = *pb;

	if (timespeccmp(&a->bmpc_oldest, &b->bmpc_oldest, <))
		return (-1);

	if (timespeccmp(&a->bmpc_oldest, &b->bmpc_oldest, >))
		return (1);

	return (0);
}

static __inline void
bmpc_decrease_minage(void)
{
	struct timespec ts = BMPC_INTERVAL;

	lockBmpcSlabs();

	timespecsub(&bmpcSlabs.bmms_minage, &ts,
		    &bmpcSlabs.bmms_minage);

	if (bmpcSlabs.bmms_minage.tv_sec < 0)
		timespecclear(&bmpcSlabs.bmms_minage);

	ulockBmpcSlabs();
}

static __inline void
bmpc_increase_minage(void)
{
	struct timespec ts = BMPC_INTERVAL;

	lockBmpcSlabs();

	timespecadd(&bmpcSlabs.bmms_minage, &ts,
		    &bmpcSlabs.bmms_minage);

	ulockBmpcSlabs();
}

int   bmpce_init(struct psc_poolmgr *, void *);
void  bmpc_init(struct bmap_pagecache *);
void  bmpc_global_init(void);
int   bmpc_grow(int);
void *bmpc_alloc(void);
void  bmpc_free(void *);
void  bmpc_freeall_locked(struct bmap_pagecache *);
void  bmpce_handle_lru_locked(struct bmap_pagecache_entry *,
			      struct bmap_pagecache *, int, int);
#endif /* _SL_BMPC_H_ */
