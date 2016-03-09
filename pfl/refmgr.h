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
 * A generic object reference count manager, complete with caching and
 * a variety of lookup methods.
 */

/* XXX integrate this directly into pools */

#ifndef _PFL_REFMGR_H_
#define _PFL_REFMGR_H_

#include "pfl/hashtbl.h"
#include "pfl/lockedlist.h"
#include "pfl/pool.h"
#include "pfl/pthrutil.h"
#include "pfl/tree.h"

SPLAY_HEAD(pfl_objref_tree, pfl_objref);

struct psc_refmgr {
	struct psc_poolmaster		 prm_pms;
	int				 prm_flags;

	/* object access mechanisms */
	struct psc_lockedlist		 prm_list;
	struct psc_lockedlist		 prm_lru;
	struct psc_hashtbl		 prm_hashtbl;
	struct pfl_refmgr_tree		*prm_tree;
	psc_spinlock_t			 prm_trlock;

	/* offsets into various object properties */
	int				 prm_lock_offset;
	int				 prm_wait_offset;
	int				 prm_list_offset;
	int				 prm_lru_offset;
	int				 prm_hash_offset;
	int				 prm_tree_offset;
	int				 prm_refmgrtree_offset;
	int				 prm_private_offset;
};

/* prm_flags */
#define PRMF_LIST			(1 << 0)
#define PRMF_LRU			(1 << 1)
#define PRMF_HASH			(1 << 2)
#define PRMF_TREE			(1 << 3)
#define PRMF_MULTIWAIT			(1 << 4)
#define PRMF_AUTOSIZE			(1 << 5)	/* pool is dynamically sized */
#define PRMF_PIN			(1 << 6)	/* mlock(2) items */
#define PRMF_HASHSTR			(1 << 7)	/* use strings as hash table keys */
#define PRMF_LINGER			(1 << 8)	/* don't destroy unref'd objs until out of space */
#define PRMF_MLIST			(1 << 9)	/* use mlist for pool backend */

/* additional lookup preference flags */
#define PRMF_ANY			(PRMF_LIST | PRMF_LRU | PRMF_HASH | PRMF_TREE)

struct pfl_objref {
#if PFL_DEBUG
	uint64_t			 pobj_magic;
#endif
	int				 pobj_refcnt;
	int				 pobj_flags;
	struct psclist_head		 pobj_pool_lentry;

	/* this is a ghost field; it will only be there if TREE is set in refmgr */
	SPLAY_ENTRY(pfl_objref)		 pobj_tree_entry;
};

#if PFL_DEBUG
#  define PSC_OBJ_MAGIC			UINT64_C(0x3452341223456345)
#  define PSC_OBJ_CHECK(pr)						\
	do {								\
		psc_assert((pr)->pobj_magic == PSC_OBJ_MAGIC);		\
	} while (0)
#else
#  define PSC_OBJ_CHECK(pr)
#endif

/* pobj_flags */
#define POBJF_DYING			(1 << 0)
#define POBJF_BUSY			(1 << 1)

#define _PSC_OBJCAST(prm, pr, field, type)				\
	((type *)((prm)->field + (char *)(pr)))

#define PSC_OBJREF_GETLOCK(m, r)	_PSC_OBJCAST(m, r, prm_lock_offset, psc_spinlock_t)
#define PSC_OBJREF_GETMUTEX(m, r)	_PSC_OBJCAST(m, r, prm_lock_offset, pthread_mutex_t)
#define PSC_OBJREF_GETWAITQ(m, r)	_PSC_OBJCAST(m, r, prm_wait_offset, struct psc_waitq)
#define PSC_OBJREF_GETMWCOND(m, r)	_PSC_OBJCAST(m, r, prm_wait_offset, struct pfl_multiwaitcond)
#define PSC_OBJREF_GETLISTENTRY(m, r)	_PSC_OBJCAST(m, r, prm_list_offset, struct psclist_head)
#define PSC_OBJREF_GETLRUENTRY(m, r)	_PSC_OBJCAST(m, r, prm_lru_offset, struct psclist_head)
#define PSC_OBJREF_GETHASHENTRY(m, r)	_PSC_OBJCAST(m, r, prm_hash_offset, struct psclist_head)
#define PSC_OBJREF_GETREFMGRTREE(m, r)	_PSC_OBJCAST(m, r, prm_refmgrtree_offset, struct psc_refmgr)
#define PSC_OBJREF_GETPRIVATE(m, r)	_PSC_OBJCAST(m, r, prm_private_offset, void)

void	 pfl_refmgr_init(struct psc_refmgr *, int, int, int, int, int,
	    int (*)(struct psc_poolmgr *, void *), void (*)(void *),
	    const char *, ...);

void	*_pfl_refmgr_findobj(struct psc_refmgr *, int, void *, int);
void	*pfl_refmgr_getobj(struct psc_refmgr *, int, void *);

#define pfl_refmgr_findremoveobj(prm, pref, key)			\
	pfl_refmgr_findobj((prm), (pref), (key), 1)

#define pfl_refmgr_findobj(prm, pref, key)				\
	pfl_refmgr_findobj((prm), (pref), (key), 0)

int	 pfl_objref_cmp(const void *, const void *);

void	 pfl_obj_share(struct psc_refmgr *prm, void *);
void	 pfl_obj_incref(struct psc_refmgr *prm, void *);
void	 pfl_obj_decref(struct psc_refmgr *prm, void *);

void	 pfl_obj_dump(const struct psc_refmgr *prm, const void *);

static __inline struct pfl_objref *
pfl_obj_getref(const struct psc_refmgr *prm, void *p)
{
	struct pfl_objref *pobj;

	psc_assert(p);
	pobj = (void *)((char *)p - prm->prm_private_offset);
	PSC_OBJ_CHECK(pobj);
	return (pobj);
}

static __inline void
pfl_objref_lock(const struct psc_refmgr *prm, struct pfl_objref *pobj)
{
	if (prm->prm_flags & PRMF_MULTIWAIT)
		psc_pthread_mutex_lock(PSC_OBJREF_GETMUTEX(prm, pobj));
	else
		spinlock(PSC_OBJREF_GETLOCK(prm, pobj));
}

static __inline void
pfl_objref_unlock(const struct psc_refmgr *prm, struct pfl_objref *pobj)
{
	if (prm->prm_flags & PRMF_MULTIWAIT)
		psc_pthread_mutex_unlock(PSC_OBJREF_GETMUTEX(prm, pobj));
	else
		freelock(PSC_OBJREF_GETLOCK(prm, pobj));
}

static __inline int
pfl_objref_reqlock(const struct psc_refmgr *prm, struct pfl_objref *pobj)
{
	if (prm->prm_flags & PRMF_MULTIWAIT)
		return (psc_pthread_mutex_reqlock(
		    PSC_OBJREF_GETMUTEX(prm, pobj)));
	return (reqlock(PSC_OBJREF_GETLOCK(prm, pobj)));
}

static __inline void
pfl_objref_ureqlock(const struct psc_refmgr *prm,
    struct pfl_objref *pobj, int locked)
{
	if (prm->prm_flags & PRMF_MULTIWAIT)
		psc_pthread_mutex_ureqlock(
		    PSC_OBJREF_GETMUTEX(prm, pobj), locked);
	else
		ureqlock(PSC_OBJREF_GETLOCK(prm, pobj), locked);
}

static __inline void
pfl_objref_wait(const struct psc_refmgr *prm, struct pfl_objref *pobj)
{
	int waslocked;

	waslocked = pfl_objref_reqlock(prm, pobj);
	if (prm->prm_flags & PRMF_MULTIWAIT)
		pfl_multiwaitcond_wait(PSC_OBJREF_GETMWCOND(prm, pobj),
		    PSC_OBJREF_GETMUTEX(prm, pobj));
	else
		psc_waitq_wait(PSC_OBJREF_GETWAITQ(prm, pobj),
		    PSC_OBJREF_GETLOCK(prm, pobj));
	if (waslocked)
		pfl_objref_lock(prm, pobj);
}

static __inline void
pfl_objref_wake(const struct psc_refmgr *prm, struct pfl_objref *pobj)
{
	int locked;

	locked = pfl_objref_reqlock(prm, pobj);
	if (prm->prm_flags & PRMF_MULTIWAIT)
		pfl_multiwaitcond_wakeup(PSC_OBJREF_GETMWCOND(prm, pobj));
	else
		psc_waitq_wakeall(PSC_OBJREF_GETWAITQ(prm, pobj));
	pfl_objref_ureqlock(prm, pobj, locked);
}

SPLAY_PROTOTYPE(pfl_objref_tree, pfl_objref, pobj_tree_entry, pfl_objref_cmp);

#endif /* _PFL_REFMGR_H_ */
