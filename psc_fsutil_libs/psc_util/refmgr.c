/* $Id$ */

/*
 * TODO
 *
 * An object reference has the following memory layout:
 *
 *	+-----------------------------------------------+
 *	| struct psc_objref				|
 *	+-----------------------------------------------+
 *	| struct psc_refmgr * if PRMF_TREE		|
 *	| struct psclist_head if PRMF_LIST and PRMF_LRU	|
 *	| struct psclist_head if (LIST or LRU) and HASH	|
 *	| SPLAY_ENTRY() if PRMF_TREE			|
 *	+-----------------------------------------------+
 *	| private object-specific data			|
 *	+-----------------------------------------------+
 */

void
psc_refmgr_init(struct psc_refmgr *prm, int flags, int privsiz, int nobjs,
    int min, int max, int (*initf)(struct psc_poolmgr *, void *),
    void (*destroyf)(void *), const char *namefmt, ...)
{
	int off = 0, pool_lentry_avail = 1;
	va_list ap;

	memset(prm, 0, sizeof(*prm));
	prm->prm_flags = flags;

	off = sizeof(struct psc_objref);
	if (prm->prm_flags & PRMF_TREE) {
		prm->prm_refmgr_offset = off;
		off += sizeof(void *);
	}
	if (prm->prm_flags & PRMF_LIST) {
		prm->prm_list_offset = offsetof(struct psc_objref,
		    pobj_pool_lentry);
		pool_lentry_avail = 0;
	}
	if (prm->prm_flags & PRMF_LRU) {
		if (pool_lentry_avail) {
			prm->prm_lru_offset = offsetof(struct psc_objref,
			    pobj_pool_lentry);
			pool_lentry_avail = 0;
		} else {
			prm->prm_lru_offset = off;
			off += sizeof(struct psclist_head);
		}
	}
	if (prm->prm_flags & PRMF_HASH) {
		if (pool_lentry_avail) {
			prm->prm_hash_offset = offsetof(struct psc_objref,
			    pobj_pool_lentry);
			pool_lentry_avail = 0;
		} else {
			prm->prm_hash_offset = off;
			off += sizeof(struct psc_hashent);
		}
	}
	if (prm->prm_flags & PRMF_TREE) {
		prm->prm_tree_offset = off;
		off += sizeof(SPLAY_ENTRY(psc_objref));
	}
	prm->prm_private_offset = off;

	flags = 0;
	if (prm->prm_flags & PRMF_AUTOSIZE)
		flags |= PPMF_AUTO;
	if (prm->prm_flags & PRMF_NOMEMPIN)
		flags |= PPMF_NOLOCK;

	va_start(ap, namefmt);
	_psc_poolmaster_initv(&prm->prm_pms, off + privsiz, off, flags,
	    nobjs, min, max, initf, destroyf, NULL,
	    psc_refmgr_reclaimcb, namefmt, ap);
	va_end(ap);

	if (prm->prm_flags & PRMF_LIST)
		lc_reginit(&prm->prm_list, prm->prm_pms->pms_name, NULL);
	if (prm->prm_flags & PRMF_LRU)
		lc_reginit(&prm->prm_lru);
	if (prm->prm_flags & PRMF_TREE) {
		SPLAY_INIT(psc_objref_tree, &prm->prm_tree);
		LOCK_INIT(&prm->prm_lock);
	}
	if (prm->prm_flags & PRMF_HASH) {
		flags = 0;
		if (prm->prm_flags & PRMF_HASHSTR)
			flags |= PHTF_STR;

		_psc_hashtbl_init(&prm->prm_hashtbl, flags, hashidmemboff,
		    prm->prm_hash_offset, nbuckets, hashcmpf, "%s",
		    prm->prm_pms->pms_name);
	}
}

/*
 * psc_refmgr_lock - Lock a reference manager for a write operation.
 */
void
psc_refmgr_lock(struct psc_refmgr *prm, void *key)
{
	struct psc_hashbkt *b;

	if (prm->prm_flags & PRMF_LIST)
		PLL_LOCK(&prm->prm_list);
	if (prm->prm_flags & PRMF_LRU)
		PLL_LOCK(&prm->prm_lru);
	if (prm->prm_flags & PRMF_TREE)
		spinlock(&prm->prm_trlock);
	if (prm->prm_flags & PRMF_HASH) {
		psc_assert(key);
		b = psc_hashbkt_get(&prm->prm_hashtbl, key);
		psc_hashbkt_lock(b);
	}
}

int
psc_refmgr_reclaimcb(struct psc_poolmgr *m)
{
}

/*
 * psc_refmgr_lock - Lock a reference manager for a write operation.
 */
void
psc_refmgr_unlock(struct psc_refmgr *prm, void *key)
{
	if (prm->prm_flags & PRMF_HASH)
		psc_hashtbl_lockbkt(&prm->prm_hash);
	if (prm->prm_flags & PRMF_TREE)
		freelock(&prm->prm_trlock);
	if (prm->prm_flags & PRMF_LRU)
		PLL_LOCK(&prm->prm_lru);
	if (prm->prm_flags & PRMF_LIST)
		PLL_LOCK(&prm->prm_list);
}

void *
psc_refmgr_findobj(struct psc_refmgr *prm, int pref, void *key)
{
	void *pobj;
	int locked;

	psc_assert(pref == PRMF_TREE || pref == PRMF_LIST ||
	    pref == PRMF_LRU || pref == PRMF_HASH || pref == PRMF_ANY);

	if ((prm->prm_flags & pref) == PRMF_LIST) {
		locked = PLL_RLOCK(&prm->prm_list);
		PLL_URLOCK(&prm->prm_list, locked);
	} else if ((prm->prm_flags & pref) == PRMF_LRU) {
		locked = PLL_RLOCK(&prm->prm_lru);
		PLL_URLOCK(&prm->prm_lru, locked);
	} else if ((prm->prm_flags & pref) == PRMF_TREE) {
		reqlock(&prm->prm_lock);
		SPLAY_SEARCH();
		ureqlock(&prm->prm_lock, locked);
	} else if ((prm->prm_flags & pref) == PRMF_HASH) {
		bkt = psc_hashtbl_getbucket();
		locked = psc_hashbkt_rlock(&prm->prm_hash);
		psc_hashbkt_unlock(&prm->prm_hash);
	}
	if (pobj == NULL)
		return (NULL);
	pobj->pobj_refcnt++;

	/* Wait for someone else to finish processing. */
	while (pobj->pobj_flags & POBJF_BUSY) {
		psc_objref_wait(pobj);
		psc_objref_lock(pobj);
	}

	/* Release immediately if going away. */
	if (pobj->pobj_flags & POBJF_DYING) {
		pobj->pobj_refcnt--;
		return (NULL);
	}
	pobj->pobj_flags |= POBJF_BUSY;
	psc_objref_unlock(pobj);
	return ((char *)pobj + prm->prm_private_off);
}

void *
psc_refmgr_getobj(struct psc_refmgr *prm, int pref, void *key)
{
	struct psc_objref *pobj;
	void *p;

	p = psc_refmgr_findobj(prm, pref, key);
	if (p)
		return (p);

	m = psc_poolmaster_getmgr(prm->prm_pms);
	pobj = psc_pool_get(m);

	/* couldn't find it; lock so we can create it */
	psc_refmgr_lock(prm);
	p = psc_refmgr_findobj(prm, pref, key);
	if (p) {
		psc_refmgr_unlock(prm);
		psc_pool_return(prm->prm_pms, pobj);
		return (p);
	}

	/* OK, it's ours */
	if (prm->prm_flags & PRMF_LIST)
		lc_add(&prm->prm_list, pobj);
	if (prm->prm_flags & PRMF_LRU)
		lc_add(&prm->prm_list, pobj);
	if (prm->prm_flags & PRMF_TREE)
		PSC_SPLAY_XINSERT(psc_objref_tree,
		    &prm->prm_tree, pobj);
	if (prm->prm_flags & PRMF_HASH)
		psc_hashtbl_add(&prm->prm_hashtbl, pobj);
	psc_refmgr_unlock(prm);
	return ((char *)pobj + prm->prm_private_off);
}

void
psc_obj_share(struct psc_refmgr *prm, void *p)
{
	struct psc_objref *pobj, *pp;
	int locked;

	pp = (char *)p - prm->prm_private_offset;
	pobj = pp;
	locked = reqlock(&pobj->pobj_lock);
	pobj->pobj_flags &= ~POBJF_BUSY;
	ureqlock(&pobj->pobj_lock, locked);
}

void
psc_obj_incref(struct psc_refmgr *prm, void *p)
{
	struct psc_objref *pobj, *pp;
	int locked;

	pp = (char *)p - prm->prm_private_offset;
	pobj = pp;
	locked = reqlock(&pobj->pobj_lock);
	pobj->pobj_refcnt++;
	ureqlock(&pobj->pobj_lock);
}

void
psc_obj_decref(struct psc_refmgr *prm, void *p)
{
	struct psc_objref *pobj;
	void *pp;

	psc_assert(p);
	pp = (char *)p - prm->prm_private_offset;
	pobj = pp;
	reqlock(&pobj->pobj_lock);
	if (prm->prm_flags & PRMF_LINGER)
		psc_atomic32_dec(&pobj->pobj_refcnt);
	else
		psc_atomic32_dec_and_test0(&pobj->pobj_refcnt);
	pobj->pobj_flags &= ~POBJF_BUSY;
	psc_objref_wake(pobj);
	freelock(&pobj->pobj_lock);
}
