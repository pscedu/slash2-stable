/* $Id$ */

/*
 *
 */

void
psc_refmgr_init(struct psc_refmgr *prm, int flags)
{
	int off;

	off = 0;
	memset(prm, 0, sizeof(*prm));
	prm->prm_flags = flags;

	if (prm->prm_flags & PRMF_LIST) {
		prm->prm_list_offset = off;
		off += sizeof(struct psclist_head);
	}
	if (prm->prm_flags & PRMF_LRU) {
		prm->prm_lru_offset = off;
		off += sizeof(struct psclist_head);
	}
	if (prm->prm_flags & PRMF_TREE) {
		prm->prm_tree_offset = off;
		off += sizeof(SPLAY_ENTRY(psc_objref));

		prm->prm_refmgr_offset = off;
		off += sizeof(void *);
	}
	if (prm->prm_flags & PRMF_HASH) {
		prm->prm_hash_offset = off;
		off += sizeof(struct psc_hashent);
	}
	prm->prm_private_offset = off;

	psc_poolmaster_init(prm->prm_pms);

	if (prm->prm_flags & PRMF_LIST)
		pll_init(&prm->prm_list);
	if (prm->prm_flags & PRMF_LRU)
		lc_init(&prm->prm_list);
	if (prm->prm_flags & PRMF_TREE)
		SPLAY_INIT(&prm->prm_tree);
	if (prm->prm_flags & PRMF_HASH)
		psc_hashtbl_init(&prm->prm_hashtbl);
}

/*
 * psc_refmgr_lock - Lock a reference manager for a write operation.
 */
void
psc_refmgr_lock(struct psc_refmgr *prm, void *key)
{
	struct psc_hashbkt *b;

	if (prm->prm_flags & PRMF_LRU)
		LIST_CACHE_LOCK(&prm->prm_lru);
	if (prm->prm_flags & PRMF_LIST)
		PLL_LOCK(&prm->prm_list);
	if (prm->prm_flags & PRMF_TREE)
		spinlock(&prm->prm_trlock);
	if (prm->prm_flags & PRMF_HASH) {
		psc_assert(key);
		b = psc_hashbkt_get(&prm->prm_hashtbl, key);
		psc_hashbkt_lock(b);
	}
}

psc_refmgr_reap()
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
		LIST_CACHE_LOCK(&prm->prm_lru);
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
		locked = LIST_CACHE_RLOCK(&prm->prm_lru);
		LIST_CACHE_URLOCK(&prm->prm_lru, locked);
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
	return (pobj);
}

void *
psc_refmgr_getobj(struct psc_refmgr *prm, int pref, void *key)
{
	void *pobj, *newp;

	pobj = psc_refmgr_findobj(prm, pref, key);
	if (pobj)
		return (pobj);

	m = psc_poolmaster_getmgr(prm->prm_pms);
	newp = psc_pool_get(m);

	/* couldn't find it; lock so we can create it */
	psc_refmgr_lock(prm);
	pobj = psc_refmgr_findobj(prm, pref, key);
	if (pobj) {
		psc_refmgr_unlock(prm);
		psc_pool_return(prm->prm_pms, newp);
		return (pobj)
	}

	/* OK, it's ours */
	pobj = newp;
	if (prm->prm_flags & PRMF_LIST)
		pll_add(&prm->prm_list, );
	if (prm->prm_flags & PRMF_LRU) {
	}
	if (prm->prm_flags & PRMF_TREE)
		PSC_SPLAY_XINSERT();
	if (prm->prm_flags & PRMF_HASH) {
	}
	psc_refmgr_unlock(prm);
	return (pobj);
}

void
psc_objref_share(struct psc_refmgr *prm, void *obj)
{
	reqlock();
	BUSY
	    freelock();
}

void
psc_objref_incref(struct psc_refmgr *prm, void *obj)
{
}

void
psc_objref_decref(struct psc_refmgr *prm, void *obj)
{
	reqlock();
	psc_atomic32_dec_and_test0();
	freelock();
}
