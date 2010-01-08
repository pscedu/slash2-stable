/* $Id$ */

/*
 * TODO
 */

void
psc_refmgr_init(struct psc_refmgr *prm, int flags, int objsiz, int nobjs,
    int min, int max, int (*initf)(struct psc_poolmgr *, void *),
    void (*destroyf)(void *), const char *namefmt, ...)
{
	int off = 0, pool_lentry_avail = 1;
	va_list ap;

	memset(prm, 0, sizeof(*prm));
	prm->prm_flags = flags;

	if (prm->prm_flags & PRMF_LIST) {
		if (pool_lentry_avail)
			pool_lentry_avail = 0;
		prm->prm_list_offset = off;
		off += sizeof(struct psclist_head);
	}
	if (prm->prm_flags & PRMF_LRU) {
		if (pool_lentry_avail)
			pool_lentry_avail = 0;
		prm->prm_lru_offset = off;
		off += sizeof(struct psclist_head);
	}
	if (prm->prm_flags & PRMF_HASH) {
		if (pool_lentry_avail)
			pool_lentry_avail = 0;
		prm->prm_hash_offset = off;
		off += sizeof(struct psc_hashent);
	}
	/* The pool list_head was not reused, force space for it */
	if (pool_lentry_avail)
		off += sizeof(struct psclist_head);
	if (prm->prm_flags & PRMF_TREE) {
		prm->prm_tree_offset = off;
		off += sizeof(SPLAY_ENTRY(psc_objref));

		prm->prm_refmgr_offset = off;
		off += sizeof(void *);
	}
	prm->prm_private_offset = off;

	flags = 0;
	if (prm->prm_flags & PRMF_AUTOSIZE)
		flags |= PPMF_AUTO;
	if (prm->prm_flags & PRMF_NOMEMPIN)
		flags |= PPMF_NOLOCK;

	va_start(ap, namefmt);
	_psc_poolmaster_initv(&prm->prm_pms, objsiz, off, flags, nobjs,
	    min, max, initf, destroyf, NULL, psc_refmgr_reclaimcb,
	    namefmt, ap);
	va_end(ap);

	if (prm->prm_flags & PRMF_LIST)
		lc_reginit(&prm->prm_list, prm->prm_pms->pms_name, NULL);
	if (prm->prm_flags & PRMF_LRU)
		lc_reginit(&prm->prm_lru);
	if (prm->prm_flags & PRMF_TREE) {
		SPLAY_INIT(&prm->prm_tree);
		LOCK_INIT(&prm->prm_lock);
	}
	if (prm->prm_flags & PRMF_HASH) {
		flags = 0;
		if (prm->prm_flags & PRMF_HASHSTR)
			flags |= PHTF_STR;

		_psc_hashtbl_init(&prm->prm_hashtbl, flags, idmemboff,
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
