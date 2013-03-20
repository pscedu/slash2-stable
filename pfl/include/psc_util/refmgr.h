/* $Id$ */
/*
 * %PSC_START_COPYRIGHT%
 * -----------------------------------------------------------------------------
 * Copyright (c) 2006-2013, Pittsburgh Supercomputing Center (PSC).
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

/* XXX integrate this directly into pools */

#ifndef _PFL_REFMGR_H_
#define _PFL_REFMGR_H_

#include "pfl/hashtbl.h"
#include "psc_ds/lockedlist.h"
#include "psc_ds/tree.h"
#include "psc_util/pool.h"
#include "psc_util/pthrutil.h"

SPLAY_HEAD(psc_objref_tree, psc_objref);

struct psc_refmgr {
	struct psc_poolmaster		 prm_pms;
	int				 prm_flags;

	/* object access mechanisms */
	struct psc_lockedlist		 prm_list;
	struct psc_lockedlist		 prm_lru;
	struct psc_hashtbl		 prm_hashtbl;
	struct psc_refmgr_tree		*prm_tree;
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

struct psc_objref {
#if PFL_DEBUG
	uint64_t			 pobj_magic;
#endif
	int				 pobj_refcnt;
	int				 pobj_flags;
	struct psclist_head		 pobj_pool_lentry;

	/* this is a ghost field; it will only be there if TREE is set in refmgr */
	SPLAY_ENTRY(psc_objref)		 pobj_tree_entry;
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
#define PSC_OBJREF_GETMWCOND(m, r)	_PSC_OBJCAST(m, r, prm_wait_offset, struct psc_multiwaitcond)
#define PSC_OBJREF_GETLISTENTRY(m, r)	_PSC_OBJCAST(m, r, prm_list_offset, struct psclist_head)
#define PSC_OBJREF_GETLRUENTRY(m, r)	_PSC_OBJCAST(m, r, prm_lru_offset, struct psclist_head)
#define PSC_OBJREF_GETHASHENTRY(m, r)	_PSC_OBJCAST(m, r, prm_hash_offset, struct psclist_head)
#define PSC_OBJREF_GETREFMGRTREE(m, r)	_PSC_OBJCAST(m, r, prm_refmgrtree_offset, struct psc_refmgr)
#define PSC_OBJREF_GETPRIVATE(m, r)	_PSC_OBJCAST(m, r, prm_private_offset, void)

void	 psc_refmgr_init(struct psc_refmgr *, int, int, int, int, int,
	    int (*)(struct psc_poolmgr *, void *), void (*)(void *),
	    const char *, ...);

void	*_psc_refmgr_findobj(struct psc_refmgr *, int, void *, int);
void	*psc_refmgr_getobj(struct psc_refmgr *, int, void *);

#define psc_refmgr_findremoveobj(prm, pref, key)			\
	psc_refmgr_findobj((prm), (pref), (key), 1)

#define psc_refmgr_findobj(prm, pref, key)				\
	psc_refmgr_findobj((prm), (pref), (key), 0)

int	 psc_objref_cmp(const void *, const void *);

void	 psc_obj_share(struct psc_refmgr *prm, void *);
void	 psc_obj_incref(struct psc_refmgr *prm, void *);
void	 psc_obj_decref(struct psc_refmgr *prm, void *);

void	 psc_obj_dump(const struct psc_refmgr *prm, const void *);

static __inline struct psc_objref *
psc_obj_getref(const struct psc_refmgr *prm, void *p)
{
	struct psc_objref *pobj;

	psc_assert(p);
	pobj = (void *)((char *)p - prm->prm_private_offset);
	PSC_OBJ_CHECK(pobj);
	return (pobj);
}

static __inline void
psc_objref_lock(const struct psc_refmgr *prm, struct psc_objref *pobj)
{
	if (prm->prm_flags & PRMF_MULTIWAIT)
		psc_pthread_mutex_lock(PSC_OBJREF_GETMUTEX(prm, pobj));
	else
		spinlock(PSC_OBJREF_GETLOCK(prm, pobj));
}

static __inline void
psc_objref_unlock(const struct psc_refmgr *prm, struct psc_objref *pobj)
{
	if (prm->prm_flags & PRMF_MULTIWAIT)
		psc_pthread_mutex_unlock(PSC_OBJREF_GETMUTEX(prm, pobj));
	else
		freelock(PSC_OBJREF_GETLOCK(prm, pobj));
}

static __inline int
psc_objref_reqlock(const struct psc_refmgr *prm, struct psc_objref *pobj)
{
	if (prm->prm_flags & PRMF_MULTIWAIT)
		return (psc_pthread_mutex_reqlock(
		    PSC_OBJREF_GETMUTEX(prm, pobj)));
	return (reqlock(PSC_OBJREF_GETLOCK(prm, pobj)));
}

static __inline void
psc_objref_ureqlock(const struct psc_refmgr *prm,
    struct psc_objref *pobj, int locked)
{
	if (prm->prm_flags & PRMF_MULTIWAIT)
		psc_pthread_mutex_ureqlock(
		    PSC_OBJREF_GETMUTEX(prm, pobj), locked);
	else
		ureqlock(PSC_OBJREF_GETLOCK(prm, pobj), locked);
}

static __inline void
psc_objref_wait(const struct psc_refmgr *prm, struct psc_objref *pobj)
{
	int waslocked;

	waslocked = psc_objref_reqlock(prm, pobj);
	if (prm->prm_flags & PRMF_MULTIWAIT)
		psc_multiwaitcond_wait(PSC_OBJREF_GETMWCOND(prm, pobj),
		    PSC_OBJREF_GETMUTEX(prm, pobj));
	else
		psc_waitq_wait(PSC_OBJREF_GETWAITQ(prm, pobj),
		    PSC_OBJREF_GETLOCK(prm, pobj));
	if (waslocked)
		psc_objref_lock(prm, pobj);
}

static __inline void
psc_objref_wake(const struct psc_refmgr *prm, struct psc_objref *pobj)
{
	int locked;

	locked = psc_objref_reqlock(prm, pobj);
	if (prm->prm_flags & PRMF_MULTIWAIT)
		psc_multiwaitcond_wakeup(PSC_OBJREF_GETMWCOND(prm, pobj));
	else
		psc_waitq_wakeall(PSC_OBJREF_GETWAITQ(prm, pobj));
	psc_objref_ureqlock(prm, pobj, locked);
}

SPLAY_PROTOTYPE(psc_objref_tree, psc_objref, pobj_tree_entry, psc_objref_cmp);

#endif /* _PFL_REFMGR_H_ */
