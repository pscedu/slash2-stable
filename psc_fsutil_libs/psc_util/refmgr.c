/* $Id$ */

/*
 * An object reference has the following memory layout:
 *
 *	+-------------------------------------------------------+
 *	| struct psc_objref					|
 *	+-------------------------------------------------------+
 *	| SPLAY_ENTRY() if PRMF_TREE				|
 *	| struct psc_refmgr * if PRMF_TREE			|
 *	| struct psclist_head (for pool and (LIST|LRU|HASH))	|
 *	| struct psclist_head (if LIST and LRU)			|
 *	| struct psclist_head (if (LIST or LRU) and HASH)	|
 *	+-------------------------------------------------------+
 *	| private object-specific data				|
 *	+-------------------------------------------------------+
 *
 * XXX need a poolmgr pointer to avoid returning to different poolmgrs.
 */

#include <stdarg.h>
#include <string.h>

#include "psc_ds/dynarray.h"
#include "psc_ds/hash2.h"
#include "psc_ds/list.h"
#include "psc_ds/lockedlist.h"
#include "psc_ds/pool.h"
#include "psc_ds/tree.h"
#include "psc_util/lock.h"
#include "psc_util/log.h"
#include "psc_util/refmgr.h"

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
		prm->prm_tree_offset = off;
		off += sizeof(SPLAY_ENTRY(psc_objref));

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
	prm->prm_private_offset = off;

	flags = 0;
	if (prm->prm_flags & PRMF_AUTOSIZE)
		flags |= PPMF_AUTO;
	if (prm->prm_flags & PRMF_NOMEMPIN)
		flags |= PPMF_NOLOCK;
	if (prm->prm_flags & PRMF_MLIST)
		flags |= PPMF_MLIST;

	va_start(ap, namefmt);
	_psc_poolmaster_initv(&prm->prm_pms, off + privsiz, off, flags,
	    nobjs, min, max, initf, destroyf, NULL,
	    psc_refmgr_reclaimcb, namefmt, ap);
	va_end(ap);

	if (prm->prm_flags & PRMF_LIST)
		pll_init(&prm->prm_list, struct psc_objref,
		    prm->prm_list_offset);
	if (prm->prm_flags & PRMF_LRU)
		pll_init(&prm->prm_lru, struct psc_objref,
		    prm->prm_lru_offset);
	if (prm->prm_flags & PRMF_TREE) {
		SPLAY_INIT(psc_objref_tree, &prm->prm_tree);
		LOCK_INIT(&prm->prm_trlock);
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
psc_refmgr_lock_generic(struct psc_refmgr *prm)
{
	struct psc_hashbkt *b;

	if (prm->prm_flags & PRMF_LIST)
		PLL_LOCK(&prm->prm_list);
	if (prm->prm_flags & PRMF_LRU)
		PLL_LOCK(&prm->prm_lru);
	if (prm->prm_flags & PRMF_TREE)
		spinlock(&prm->prm_trlock);
}

void
psc_refmgr_lock4obj(struct psc_refmgr *prm, struct psc_objref *pobj)
{
	struct psc_hashbkt *b;

	psc_refmgr_lock_generic(prm);
	if (prm->prm_flags & PRMF_HASH) {
		psc_assert(key);
		b = psc_hashbkt_get(&prm->prm_hashtbl, (char *)pobj +
		    prm->prm_private_off + prm->prm_privhashidmemb_off);
		psc_hashbkt_lock(b);
	}
}

void
psc_refmgr_lock4priv(struct psc_refmgr *prm, void *key)
{
	struct psc_hashbkt *b;

	psc_refmgr_lock_generic(prm);
	if (prm->prm_flags & PRMF_HASH) {
		psc_assert(key);
		b = psc_hashbkt_get(&prm->prm_hashtbl,
		    (char *)key + prm->prm_privhashidmemb_off);
		psc_hashbkt_lock(b);
	}
}

void
psc_refmgr_unlock_generic(struct psc_refmgr *prm)
{
	if (prm->prm_flags & PRMF_TREE)
		freelock(&prm->prm_trlock);
	if (prm->prm_flags & PRMF_LRU)
		PLL_LOCK(&prm->prm_lru);
	if (prm->prm_flags & PRMF_LIST)
		PLL_LOCK(&prm->prm_list);
}

void
psc_refmgr_unlock_specific(struct psc_refmgr *prm, void *key)
{
	if (prm->prm_flags & PRMF_HASH) {
		psc_assert(key);
		b = psc_hashbkt_get(&prm->prm_hashtbl,
		    (char *)key + prm->prm_privhashidmemb_off);
		psc_hashtbl_lockbkt(&prm->prm_hash);
	}
	psc_refmgr_unlock_generic(prm);
}

void
psc_refmgr_tryreclaimobj(struct psc_refmgr *prm, struct psc_objref *pobj,
    struct psc_dynarray *da)
{
	struct psc_hashbkt *bkt = NULL;
	int locked;

	locked = 0; /* gcc */

	if (prm->prm_flags & PRMF_HASH) {
		bkt = psc_hashbkt_get(&prm->prm_hashtbl, key);
		locked = psc_hashbkt_rlock(bkt);
	}

	psc_objref_lock(pobj);
	if (pobj->pobj_refcnt == 0 &&
	    pobj->pobj_flags & (POBJF_BUSY | POBJF_DYING) == 0) {
		pobj->pobj_flags |= POBJF_DYING;
		if (prm->prm_flags & PRMF_LIST)
			lc_remove(&prm->prm_list, pobj);
		if (prm->prm_flags & PRMF_LRU)
			lc_remove(&prm->prm_lru, pobj);
		if (prm->prm_flags & PRMF_TREE)
			PSC_SPLAY_XREMOVE(psc_objref_tree,
			    &prm->prm_tree, pobj);
		if (prm->prm_flags & PRMF_HASH)
			psc_hashbkt_del_item(&prm->prm_hashtbl, bkt,
			    prm->prm_objcmpf, );

		psc_dynarray_add(da, pobj);
	}
	psc_objref_unlock(pobj);

	if (prm->prm_flags & PRMF_HASH)
		psc_hashbkt_ureqlock(bkt, locked);
}

int
psc_refmgr_reclaimcb(struct psc_poolmgr *m)
{
	struct psc_dynarray da = DYNARRAY_INIT;
	struct psc_objref *pobj;

	psc_refmgr_lock_generic(prm);
	if (prm->prm_flags & PRMF_LRU) {
		PLC_FOREACH_BACKWARDS(pobj, &prm->prm_lru)
			psc_refmgr_tryreclaimobj(prm, pobj, &da);
	} else if (prm->prm_flags & PRMF_LIST) {
		PLC_FOREACH(pobj, &prm->prm_list)
			psc_refmgr_tryreclaimobj(prm, pobj, &da);
	} else if (prm->prm_flags & PRMF_TREE) {
		SPLAY_FOREACH(psc_objref_tree, &prm->prm_tree, pobj)
			psc_refmgr_tryreclaimobj(prm, pobj, &da);
	} else if (prm->prm_flags & PRMF_HASH) {
		struct psc_hashbkt *bkt;

		PSC_HASHTBL_FOREACH_BUCKET(bkt, &prm->prm_hashtbl) {
			psc_hashbkt_lock(bkt);
			PSC_HASHBKT_FOREACH_ENTRY(&prm->prm_hashtbl,
			    pobj, bkt)
				psc_refmgr_tryreclaimobj(prm, pobj, &da);
			psc_hashbkt_unlock(bkt);
		}
	}
	psc_refmgr_unlock_generic(prm);

	psc_dynarray_free(&da);
}

void
psc_objref_repos_lru(struct psc_refmgr *prm, struct psc_objref *pobj)
{
	int locked;

	if ((prm->prm_flags & PRMF_LRU) == 0)
		return;

	locked = PLL_RLOCK(&prm->prm_lru);
	if (pll_first_entry(&prm->prm_lru) != pobj) {
		pll_remove(&prm->prm_lru, pobj);
		pll_addhead(&prm->prm_lru, pobj);
	}
	PLL_URLOCK(&prm->prm_lru, locked);
}

void *
psc_objref_getpriv(struct psc_refmgr *prm, struct psc_objref *pobj)
{
	psc_assert(pobj);
	return ((char *)pobj + prm->prm_private_offset);
}

void *
psc_refmgr_findobj(struct psc_refmgr *prm, int pref, void *key)
{
	struct psc_objref q;
	void *pobj;
	int locked;

	psc_assert(pref == PRMF_TREE || pref == PRMF_LIST ||
	    pref == PRMF_LRU || pref == PRMF_HASH || pref == PRMF_ANY);

	if ((prm->prm_flags & pref) == PRMF_LIST) {
		locked = PLL_RLOCK(&prm->prm_list);
		PLL_FOREACH(pobj, &prm->prm_list)
			if (prm->prm_objcmpf(key,
			    psc_objref_getpriv(pobj)) == 0) {
				psc_objref_lock(pobj);
				break;
			}
		PLL_URLOCK(&prm->prm_list, locked);
	} else if ((prm->prm_flags & pref) == PRMF_LRU) {
		locked = PLL_RLOCK(&prm->prm_lru);
		PLL_FOREACH(pobj, &prm->prm_)
			if (prm->prm_objcmpf(key,
			    psc_objref_getpriv(pobj)) == 0) {
				psc_objref_lock(pobj);
				break;
			}
		if (pobj)
			psc_objref_repos_lru(pobj);
		PLL_URLOCK(&prm->prm_lru, locked);
	} else if ((prm->prm_flags & pref) == PRMF_TREE) {
		reqlock(&prm->prm_lock);
		pobj = SPLAY_FIND(psc_objref_tree, &prm->prm_tree, key);
		if (pobj)
			psc_objref_lock(pobj);
		ureqlock(&prm->prm_lock, locked);
	} else if ((prm->prm_flags & pref) == PRMF_HASH) {
		bkt = psc_hashtbl_getbucket();
		locked = psc_hashbkt_reqlock(&prm->prm_hash);
		pobj = psc_hashbkt_search(&prm->prm_hashtbl, bkt,
		    prm->prm_objcmpf, NULL,
		    (char *)key + prm->prm_privhashidmemb_off);
		if (pobj)
			psc_objref_lock(pobj);
		psc_hashbkt_ureqlock(&prm->prm_hash, locked);
	}
	if (pobj == NULL)
		return (NULL);
	psc_objref_repos_lru(pobj);
	pobj->pobj_refcnt++;

	/* Wait for someone else to finish processing. */
	while (pobj->pobj_flags & POBJF_BUSY) {
		psc_objref_wait(pobj);
		psc_objref_lock(pobj);
	}

	/* Release immediately if going away. */
	if (pobj->pobj_flags & POBJF_DYING) {
		pobj->pobj_refcnt--;
		psc_objref_unlock(pobj);
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

int
psc_objref_cmp(const void *a, const void *b)
{
	struct psc_obj
}

void
psc_obj_share(struct psc_refmgr *prm, void *p)
{
	struct psc_objref *pobj, *pp;
	int locked;

	psc_assert(p);
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

	psc_assert(p);
	pp = (char *)p - prm->prm_private_offset;
	pobj = pp;
	locked = reqlock(&pobj->pobj_lock);
	pobj->pobj_refcnt++;
	ureqlock(&pobj->pobj_lock);
}

void
psc_obj_decref(struct psc_refmgr *prm, void *p)
{
	struct psc_objref *pobj, *freeobj;
	void *pp;

	freeobj = NULL;

	psc_assert(p);
	pp = (char *)p - prm->prm_private_offset;
	pobj = pp;
	psc_objref_lock(pobj);
	pobj->pobj_flags &= ~POBJF_BUSY;
	if (prm->prm_flags & PRMF_LINGER ||
	    psc_atomic32_read(&pobj->pobj_refcnt) != 1)
		psc_atomic32_dec(&pobj->pobj_refcnt);
	else {
		freelock(&pobj->pobj_lock);

		psc_refmgr_lock_specific(prm, pobj);
		psc_objref_lock(pobj);
		if (psc_atomic32_dec_and_test0(&pobj->pobj_refcnt)) {
			if (prm->prm_flags & PRMF_HASH)
				psc_hashtbl_del_item(&prm->prm_hashtbl, );
			if (prm->prm_flags & PRMF_TREE)
				PSC_SPLAY_XREMOVE(psc_objref_tree,
				    &prm->prm_tree, pobj);
			if (prm->prm_flags & PRMF_LRU)
				lc_remove(&prm->prm_lru, pobj);
			if (prm->prm_flags & PRMF_LIST)
				lc_remove(&prm->prm_list, pobj);
			freeobj = pobj;
			pobj = NULL;
		}
		psc_refmgr_unlock_specific(prm, pobj);

		if (freeobj) {
			/* XXXXXX WRONG, save pointer and store in obj */
			m = psc_poolmaster_getmgr(&prm->prm_pms);
			psc_pool_return(m, freeobj);
		}
	}
	if (pobj) {
		psc_objref_wake(pobj);
		psc_objref_unlock(pobj);
	}
}
