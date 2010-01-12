/* $Id$ */

/* XXX integrate this directly into pools */

#ifndef _PFL_REFMGR_H_
#define _PFL_REFMGR_H_

#include "psc_ds/hash2.h"
#include "psc_ds/lockedlist.h"
#include "psc_ds/tree.h"
#include "psc_util/pool.h"

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
	int				 prm_list_offset;
	int				 prm_lru_offset;
	int				 prm_hash_offset;
	int				 prm_tree_offset;
	int				 prm_refmgr_offset;
	int				 prm_private_offset;
};

/* prm_flags */
#define PRMF_LIST			(1 << 0)
#define PRMF_LRU			(1 << 1)
#define PRMF_HASH			(1 << 2)
#define PRMF_TREE			(1 << 3)
#define PRMF_MULTIWAIT			(1 << 4)
#define PRMF_AUTOSIZE			(1 << 5)	/* pool is dynamically sized */
#define PRMF_NOMEMPIN			(1 << 6)	/* do not pin in mem with mlock */
#define PRMF_HASHSTR			(1 << 7)	/* use strings as hash table keys */
#define PRMF_LINGER			(1 << 8)	/* don't destroy unref'd objs until out of space */
#define PRMF_MLIST			(1 << 9)	/* use mlist for pool backend */

/* additional lookup preference flags */
#define PRMF_ANY			(PRMF_LIST | PRMF_LRU | PRMF_HASH | PRMF_TREE)

struct psc_objref {
	int				 pobj_refcnt;
	struct psclist_head		 pobj_pool_lentry;
	union {
		psc_spinlock_t		 pobj_locku_lock;
		pthread_mutex_t		 pobj_locku_mutex;
	}				 pobj_locku;
	union {
		struct psc_waitq	 pobj_waitu_waitq;
		struct psc_multiwaitcond pobj_waitu_mwcond;
	}				 pobj_waitu;

#define pobj_lock	pobj_locku.pobj_locku_lock
#define pobj_mutex	pobj_locku.pobj_locku_mutex
#define pobj_waitq	pobj_waitu.pobj_waitu_waitq
#define pobj_mwcond	pobj_waitu.pobj_waitu_mwcond
};

/* pobj_flags */
#define POBJF_DYING			(1 << 0)
#define POBJF_BUSY			(1 << 1)
#define POBJF_MULTIWAIT			(1 << 2)

#define PSC_GETOBJREF(p)		((struct psc_obj *)(p))
#define PSC_GETOBJLOCK(p)		(&PSC_GETOBJREF(p)->pobj_lock)
#define PSC_OBJREF_LOCK(p)		spinlock(PSC_GETOBJLOCK(p))
#define PSC_OBJREF_RLOCK(p)		reqlock(PSC_GETOBJLOCK(p))
#define PSC_OBJREF_ULOCK(p)		freelock(PSC_GETOBJLOCK(p))
#define PSC_OBJREF_URLOCK(p, lk)	ureqlock(PSC_GETOBJLOCK(p), (lk))

#define PSC_OBJ_LOCK(p)			psc_objref_lock(psc_obj_getref(p))
#define PSC_OBJ_ULOCK(p)		psc_objref_unlock(psc_obj_getref(p))
#define PSC_OBJ_RLOCK(p)		psc_objref_reqlock(psc_obj_getref(p))
#define PSC_OBJ_URLOCK(p, lk)		psc_objref_ureqlock(psc_obj_getref(p), (lk))

void	 psc_refmgr_init(struct psc_refmgr *, int, int, int, int, int,
	    int (*)(struct psc_poolmgr *, void *), void (*)(void *),
	    const char *, ...);

void	*psc_refmgr_findobj(struct psc_refmgr *, int, void *);
void	*psc_refmgr_getobj(struct psc_refmgr *, int, void *);

int	 psc_objref_cmp(const void *, const void *);

void	 psc_obj_share(struct psc_refmgr *prm, void *);
void	 psc_obj_incref(struct psc_refmgr *prm, void *);
void	 psc_obj_decref(struct psc_refmgr *prm, void *);

static __inline struct psc_objref *
psc_obj_getref(const struct psc_refmgr *prm, void *p)
{
	psc_assert(p);
	return ((char *)p - prm->prm_private_offset);
}

static __inline void
psc_objref_wait(struct psc_objref *pobj)
{
	psc_objref_rlock(pobj);
	if (pobj->pobj_flags & POBJF_MULTIWAIT)
		psc_multiwaitcond_wait(&pobj->pobj_mwcond, &pobj->pobj_mutex);
	else
		psc_waitq_wait(&pobj->pobj_waitq, &pobj->pobj_lock);
}

static __inline void
psc_objref_wake(struct psc_objref *pobj)
{
	if (pobj->pobj_flags & POBJF_MULTIWAIT)
		psc_multiwaitcond_wakeup(&pobj->pobj_mwcond);
	else
		psc_waitq_wakeall(&pobj->pobj_waitq);
}

static __inline void
psc_objref_lock(struct psc_objref *pobj)
{
	if (pobj->pobj_flags & POBJF_MULTIWAIT)
		psc_pthread_mutex_lock(&pobj->pobj_mutex);
	else
		spinlock(&pobj->pobj_lock);
}

static __inline void
psc_objref_unlock(struct psc_objref *pboj)
{
	if (pobj->pobj_flags & POBJF_MULTIWAIT)
		psc_pthread_mutex_unlock(&pobj->pobj_mutex);
	else
		freelock(&pobj->pobj_lock);
}

static __inline int
psc_objref_reqlock(struct psc_objref *pobj)
{
	if (pobj->pobj_flags & POBJF_MULTIWAIT)
		return (psc_pthread_mutex_reqlock(&pobj->pobj_mutex));
	return (reqlock(&pobj->pobj_lock));
}

static __inline void
psc_objref_ureqlock(struct psc_objref *pobj, int locked)
{
	if (pobj->pobj_flags & POBJF_MULTIWAIT)
		psc_pthread_mutex_ureqlock(&pobj->pobj_mutex, locked);
	else
		ureqlock(&pobj->pobj_lock, locked);
}

SPLAY_GENERATE(psc_objref_tree, psc_objref, pobjf_tree_tentry, psc_objref_cmp);

#endif /* _PFL_REFMGR_H_ */
