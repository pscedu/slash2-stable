/* $Id$ */

/* XXX integrate this directly into pools */

#ifndef _PFL_REFMGR_H_
#define _PFL_REFMGR_H_

struct psc_refmgr {
	struct psc_poolmaster		 prm_pms;

	/* object access mechanisms */
	struct psc_listcache		 prm_list;
	struct psc_listcache		 prm_lru;
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
#define prm_pool_offset prm_list_offset
};

#define PRMF_LIST			(1 << 0)
#define PRMF_LRU			(1 << 1)
#define PRMF_HASH			(1 << 2)
#define PRMF_TREE			(1 << 3)
#define PRMF_MULTIWAIT			(1 << 4)
#define PRMF_AUTOSIZE			(1 << 5)	/* pool is dynamically sized */
#define PRMF_NOMEMPIN			(1 << 6)	/* do not pin in mem with mlock */
#define PRMF_HASHSTR			(1 << 7)	/* use strings as hash table keys */

/* additional lookup preference flags */
#define PRMF_ANY			(PRMF_LIST | PRMF_LRU | PRMF_HASH | PRMF_TREE)

struct psc_objref {
	int				 pobj_refcnt;
	union {
		psc_spinlock_t		 pobj_locku_lock;
		pthread_mutex_t		 pobj_locku_mutex;
	}				 pobj_locku;
	union {
		struct psc_waitq	 pobj_waitu_waitq;
		struct psc_multiwaitcond pobj_waitu_mwcond;
	}				 pobj_waitu;

	struct psclist_head		 pobj_list_entry;
	struct psclist_head		 pobj_lru_entry;
	struct psc_hashent		 pobj_hash_entry;
	struct psclist_head		 pobj_tree_entry;

#define pobj_lock	pobj_locku.pobj_locku_lock
#define pobj_mutex	pobj_locku.pobj_locku_mutex
#define pobj_waitq	pobj_waitu.pobj_waitu_waitq
#define pobj_mwcond	pobj_waitu.pobj_waitu_mwcond
};

#define POBJF_DYING			(1 << 0)
#define POBJF_BUSY			(1 << 1)
#define POBJF_MULTIWAIT			(1 << 2)

#define PSC_GETOBJREF(p)		((struct psc_obj *)(p))
#define PSC_GETOBJLOCK(p)		(&PSC_GETOBJREF(p)->pobj_lock)
#define PSC_OBJREF_LOCK(p)		spinlock(PSC_GETOBJLOCK(p))
#define PSC_OBJREF_RLOCK(p)		reqlock(PSC_GETOBJLOCK(p))
#define PSC_OBJREF_ULOCK(p)		freelock(PSC_GETOBJLOCK(p))
#define PSC_OBJREF_URLOCK(p, lk)	ureqlock(PSC_GETOBJLOCK(p), (lk))

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
psc_objref_lock(void *p)
{
	struct psc_objref *pobj = p;

	if (pobj->pobj_flags & POBJF_MULTIWAIT)
		psc_pthread_mutex_lock(&pobj->pobj_mutex);
	else
		spinlock(&pobj->pobj_lock);
}

static __inline void
psc_objref_unlock(void *p)
{
	struct psc_objref *pobj = p;

	if (pobj->pobj_flags & POBJF_MULTIWAIT)
		psc_pthread_mutex_unlock(&pobj->pobj_mutex);
	else
		freelock(&pobj->pobj_lock);
}

static __inline int
psc_objref_reqlock(void *p)
{
	struct psc_objref *pobj = p;

	if (pobj->pobj_flags & POBJF_MULTIWAIT)
		return (psc_pthread_mutex_reqlock(&pobj->pobj_mutex));
	return (reqlock(&pobj->pobj_lock));
}

static __inline void
psc_objref_ureqlock(void *p, int locked)
{
	struct psc_objref *pobj = p;

	if (pobj->pobj_flags & POBJF_MULTIWAIT)
		psc_pthread_mutex_ureqlock(&pobj->pobj_mutex, locked);
	else
		ureqlock(&pobj->pobj_lock, locked);
}

#endif /* _PFL_REFMGR_H_ */
