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
 * An object reference has the following memory layout:
 *
 *	+-------------------------------------------------------+
 *	| struct pfl_objref					|
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

#include "pfl/dynarray.h"
#include "pfl/hashtbl.h"
#include "pfl/list.h"
#include "pfl/lockedlist.h"
#include "pfl/tree.h"
#include "pfl/lock.h"
#include "pfl/log.h"
#include "pfl/pool.h"
#include "pfl/refmgr.h"

SPLAY_GENERATE(pfl_objref_tree, pfl_objref, pobj_tree_entry, pfl_objref_cmp)

void
pfl_refmgr_tryreclaimobj(struct psc_refmgr *prm, struct pfl_objref *pobj,
    struct psc_dynarray *da)
{
	struct psc_hashbkt *bkt = NULL;
	int locked;

	locked = 0; /* gcc */

	if (prm->prm_flags & PRMF_HASH) {
		bkt = psc_hashbkt_get(&prm->prm_hashtbl, key);
		locked = psc_hashbkt_rlock(bkt);
	}

	pfl_objref_lock(prm, pobj);
	if (pobj->pobj_refcnt == 0 &&
	    pobj->pobj_flags & (POBJF_BUSY | POBJF_DYING) == 0) {
		pobj->pobj_flags |= POBJF_DYING;
		if (prm->prm_flags & PRMF_LIST)
			lc_remove(&prm->prm_list, pobj);
		if (prm->prm_flags & PRMF_LRU)
			lc_remove(&prm->prm_lru, pobj);
		if (prm->prm_flags & PRMF_TREE)
			PSC_SPLAY_XREMOVE(pfl_objref_tree,
			    &prm->prm_tree, pobj);
		if (prm->prm_flags & PRMF_HASH)
			psc_hashbkt_del_item(&prm->prm_hashtbl, bkt,
			    prm->prm_objcmpf, key);

		psc_dynarray_add(da, pobj);
	}
	pfl_objref_unlock(prm, pobj);

	if (prm->prm_flags & PRMF_HASH)
		psc_hashbkt_ureqlock(bkt, locked);
}

int
pfl_refmgr_reclaimcb(struct psc_poolmgr *m)
{
	struct psc_dynarray da = DYNARRAY_INIT;
	struct pfl_objref *pobj;

	pfl_refmgr_lock_generic(prm);
	if (prm->prm_flags & PRMF_LRU) {
		PLC_FOREACH_BACKWARDS(pobj, &prm->prm_lru)
			pfl_refmgr_tryreclaimobj(prm, pobj, &da);
	} else if (prm->prm_flags & PRMF_LIST) {
		PLC_FOREACH(pobj, &prm->prm_list)
			pfl_refmgr_tryreclaimobj(prm, pobj, &da);
	} else if (prm->prm_flags & PRMF_TREE) {
		SPLAY_FOREACH(pfl_objref_tree, &prm->prm_tree, pobj)
			pfl_refmgr_tryreclaimobj(prm, pobj, &da);
	} else if (prm->prm_flags & PRMF_HASH) {
		struct psc_hashbkt *bkt;

		PSC_HASHTBL_FOREACH_BUCKET(bkt, &prm->prm_hashtbl) {
			psc_hashbkt_lock(bkt);
			PSC_HASHBKT_FOREACH_ENTRY(&prm->prm_hashtbl,
			    pobj, bkt)
				pfl_refmgr_tryreclaimobj(prm, pobj, &da);
			psc_hashbkt_unlock(bkt);
		}
	}
	pfl_refmgr_unlock_generic(prm);

	psc_dynarray_free(&da);
}

void
pfl_refmgr_init(struct psc_refmgr *prm, int flags, int privsiz, int nobjs,
    int min, int max, int (*initf)(struct psc_poolmgr *, void *),
    void (*destroyf)(void *), const char *namefmt, ...)
{
	int off = 0, pool_lentry_avail = 1;
	va_list ap;

	memset(prm, 0, sizeof(*prm));
	prm->prm_flags = flags;

	off = offsetof(struct pfl_objref, pobj_tree_entry);
	if (prm->prm_flags & PRMF_TREE) {
		off += sizeof(SPLAY_ENTRY(pfl_objref));

		prm->prm_refmgrtree_offset = off;
		off += sizeof(void *);
	}
	if (prm->prm_flags & PRMF_LIST) {
		prm->prm_list_offset = offsetof(struct pfl_objref,
		    pobj_pool_lentry);
		pool_lentry_avail = 0;
	}
	if (prm->prm_flags & PRMF_LRU) {
		if (pool_lentry_avail) {
			prm->prm_lru_offset = offsetof(struct pfl_objref,
			    pobj_pool_lentry);
			pool_lentry_avail = 0;
		} else {
			prm->prm_lru_offset = off;
			off += sizeof(struct psclist_head);
		}
	}
	if (prm->prm_flags & PRMF_HASH) {
		if (pool_lentry_avail) {
			prm->prm_hash_offset = offsetof(struct pfl_objref,
			    pobj_pool_lentry);
			pool_lentry_avail = 0;
		} else {
			prm->prm_hash_offset = off;
			off += sizeof(struct pfl_hashentry);
		}
	}
	prm->prm_private_offset = off;

	flags = 0;
	if (prm->prm_flags & PRMF_AUTOSIZE)
		flags |= PPMF_AUTO;
	if (prm->prm_flags & PRMF_PIN)
		flags |= PPMF_PIN;
	if (prm->prm_flags & PRMF_MLIST)
		flags |= PPMF_MLIST;

	va_start(ap, namefmt);
	_psc_poolmaster_initv(&prm->prm_pms, off + privsiz, off, flags,
	    nobjs, min, max, NULL,
	    pfl_refmgr_reclaimcb, namefmt, ap);
	va_end(ap);

	if (prm->prm_flags & PRMF_LIST)
		pll_init(&prm->prm_list, struct pfl_objref,
		    prm->prm_list_offset);
	if (prm->prm_flags & PRMF_LRU)
		pll_init(&prm->prm_lru, struct pfl_objref,
		    prm->prm_lru_offset);
	if (prm->prm_flags & PRMF_TREE) {
		SPLAY_INIT(&prm->prm_tree);
		INIT_SPINLOCK(&prm->prm_trlock);
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
 * pfl_refmgr_lock - Lock a reference manager for a write operation.
 */
void
pfl_refmgr_lock_generic(struct psc_refmgr *prm)
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
pfl_refmgr_lock4obj(struct psc_refmgr *prm, struct pfl_objref *pobj)
{
	struct psc_hashbkt *b;

	pfl_refmgr_lock_generic(prm);
	if (prm->prm_flags & PRMF_HASH) {
		psc_assert(key);
		b = psc_hashbkt_get(&prm->prm_hashtbl, (char *)pobj +
		    prm->prm_private_off + prm->prm_privhashidmemb_off);
		psc_hashbkt_lock(b);
	}
}

void
pfl_refmgr_lock4priv(struct psc_refmgr *prm, void *key)
{
	struct psc_hashbkt *b;

	pfl_refmgr_lock_generic(prm);
	if (prm->prm_flags & PRMF_HASH) {
		psc_assert(key);
		b = psc_hashbkt_get(&prm->prm_hashtbl,
		    (char *)key + prm->prm_privhashidmemb_off);
		psc_hashbkt_lock(b);
	}
}

void
pfl_refmgr_unlock_generic(struct psc_refmgr *prm)
{
	if (prm->prm_flags & PRMF_TREE)
		freelock(&prm->prm_trlock);
	if (prm->prm_flags & PRMF_LRU)
		PLL_LOCK(&prm->prm_lru);
	if (prm->prm_flags & PRMF_LIST)
		PLL_LOCK(&prm->prm_list);
}

void
pfl_refmgr_unlock_specific(struct psc_refmgr *prm, void *key)
{
	struct psc_hashbkt *b;

	if (prm->prm_flags & PRMF_HASH) {
		psc_assert(key);
		b = psc_hashbkt_get(&prm->prm_hashtbl,
		    (char *)key + prm->prm_privhashidmemb_off);
		psc_hashtbl_lockbkt(&prm->prm_hash);
	}
	pfl_refmgr_unlock_generic(prm);
}

void
pfl_objref_repos_lru(struct psc_refmgr *prm, struct pfl_objref *pobj)
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
pfl_refmgr_findobj(struct psc_refmgr *prm, int pref, void *key)
{
	struct pfl_objref q;
	void *pobj;
	int locked;

	psc_assert(pref == PRMF_TREE || pref == PRMF_LIST ||
	    pref == PRMF_LRU || pref == PRMF_HASH || pref == PRMF_ANY);

	if ((prm->prm_flags & pref) == PRMF_LIST) {
		locked = PLL_RLOCK(&prm->prm_list);
		PLL_FOREACH(pobj, &prm->prm_list)
			if (prm->prm_objcmpf(key,
			    pfl_objref_getpriv(prm, pobj)) == 0) {
				pfl_objref_lock(prm, pobj);
				break;
			}
		PLL_URLOCK(&prm->prm_list, locked);
	} else if ((prm->prm_flags & pref) == PRMF_LRU) {
		locked = PLL_RLOCK(&prm->prm_lru);
		PLL_FOREACH(pobj, &prm->prm_)
			if (prm->prm_objcmpf(key,
			    pfl_objref_getpriv(prm, pobj)) == 0) {
				pfl_objref_lock(prm, pobj);
				break;
			}
		/* we must reposition on the LRU before unlocking */
		if (pobj)
			pfl_objref_repos_lru(prm, pobj);
		PLL_URLOCK(&prm->prm_lru, locked);
	} else if ((prm->prm_flags & pref) == PRMF_TREE) {
		reqlock(&prm->prm_lock);
		pobj = SPLAY_FIND(pfl_objref_tree, &prm->prm_tree, key);
		if (pobj)
			pfl_objref_lock(prm, pobj);
		ureqlock(&prm->prm_lock, locked);
	} else if ((prm->prm_flags & pref) == PRMF_HASH) {
		bkt = psc_hashtbl_getbucket();
		locked = psc_hashbkt_reqlock(&prm->prm_hash);
		pobj = psc_hashbkt_search(&prm->prm_hashtbl, bkt,
		    prm->prm_objcmpf, NULL,
		    (char *)key + prm->prm_privhashidmemb_off);
		if (pobj)
			pfl_objref_lock(prm, pobj);
		psc_hashbkt_ureqlock(&prm->prm_hash, locked);
	}
	if (pobj == NULL)
		return (NULL);
	pfl_objref_repos_lru(prm, pobj);
	pobj->pobj_refcnt++;

	/* Wait for someone else to finish processing. */
	while (pobj->pobj_flags & POBJF_BUSY) {
		pfl_objref_wait(prm, pobj);
		pfl_objref_lock(prm, pobj);
	}

	/* Release immediately if going away. */
	if (pobj->pobj_flags & POBJF_DYING) {
		pobj->pobj_refcnt--;
		pfl_objref_unlock(prm, pobj);
		return (NULL);
	}
	pobj->pobj_flags |= POBJF_BUSY;
	pfl_objref_unlock(prm, pobj);
	return (PSC_OBJREF_GETPRIVATE(prm, pobj));
}

void *
pfl_refmgr_getobj(struct psc_refmgr *prm, int pref, void *key)
{
	struct pfl_objref *pobj;
	void *p;

	p = pfl_refmgr_findobj(prm, pref, key);
	if (p)
		return (p);

	m = psc_poolmaster_getmgr(prm->prm_pms);
	pobj = psc_pool_get(m);

	/* couldn't find it; lock so we can create it */
	pfl_refmgr_lock(prm);
	p = pfl_refmgr_findobj(prm, pref, key);
	if (p) {
		pfl_refmgr_unlock(prm);
		psc_pool_return(prm->prm_pms, pobj);
		return (p);
	}

	/* OK, it's ours */
	if (prm->prm_flags & PRMF_LIST)
		lc_add(&prm->prm_list, pobj);
	if (prm->prm_flags & PRMF_LRU)
		lc_add(&prm->prm_list, pobj);
	if (prm->prm_flags & PRMF_TREE)
		PSC_SPLAY_XINSERT(pfl_objref_tree,
		    &prm->prm_tree, pobj);
	if (prm->prm_flags & PRMF_HASH)
		psc_hashtbl_add(&prm->prm_hashtbl, pobj);
	pfl_refmgr_unlock(prm);
	return (PSC_OBJREF_GETPRIVATE(prm, pobj));
}

int
pfl_objref_cmp(const void *a, const void *b)
{
	const struct pfl_objref *x = a, *y = b;

	return (CMP(x, y));
}

void
pfl_obj_share(struct psc_refmgr *prm, void *p)
{
	struct pfl_objref *pobj;
	int locked;

	pobj = pfl_obj_getref(prm, p);
	locked = pfl_objref_reqlock(prm, pobj);
	pobj->pobj_flags &= ~POBJF_BUSY;
	pfl_objref_ureqlock(prm, pobj, locked);
}

void
pfl_obj_incref(struct psc_refmgr *prm, void *p)
{
	struct pfl_objref *pobj, *pp;
	int locked;

	pobj = pfl_obj_getref(prm, p);
	locked = pfl_objref_reqlock(prm, pobj);
	pobj->pobj_refcnt++;
	pfl_objref_ureqlock(prm, pobj);
}

void
pfl_obj_decref(struct psc_refmgr *prm, void *p)
{
	struct pfl_objref *pobj, *freeobj;
	void *pp;

	freeobj = NULL;

	pobj = pfl_obj_getref(prm, p);
	pfl_objref_lock(prm, pobj);
	pobj->pobj_flags &= ~POBJF_BUSY;
	if (prm->prm_flags & PRMF_LINGER ||
	    pobj->pobj_refcnt != 1)
		pobj->pobj_refcnt--;
	else {
		pfl_objref_unlock(prm, pobj);

		pfl_refmgr_lock_specific(prm, pobj);
		pfl_objref_lock(prm, pobj);
		if (--pobj->pobj_refcnt == 0) {
			if (prm->prm_flags & PRMF_HASH)
				psc_hashtbl_del_item(&prm->prm_hashtbl,
				    prm->prm_objcmpf, key);
			if (prm->prm_flags & PRMF_TREE)
				PSC_SPLAY_XREMOVE(pfl_objref_tree,
				    &prm->prm_tree, pobj);
			if (prm->prm_flags & PRMF_LRU)
				lc_remove(&prm->prm_lru, pobj);
			if (prm->prm_flags & PRMF_LIST)
				lc_remove(&prm->prm_list, pobj);
			freeobj = pobj;
			pobj = NULL;
		}
		pfl_refmgr_unlock_specific(prm, pobj);

		if (freeobj) {
			/* XXXXXX WRONG, save pointer and store in obj */
			m = psc_poolmaster_getmgr(&prm->prm_pms);
			psc_pool_return(m, freeobj);
		}
	}
	if (pobj) {
		pfl_objref_wake(prm, pobj);
		pfl_objref_unlock(prm, pobj);
	}
}
