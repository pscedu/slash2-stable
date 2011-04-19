/* $Id$ */
/*
 * %PSC_START_COPYRIGHT%
 * -----------------------------------------------------------------------------
 * Copyright (c) 2006-2011, Pittsburgh Supercomputing Center (PSC).
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

#include <sys/param.h>

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "pfl/hashtbl.h"
#include "pfl/str.h"
#include "psc_ds/list.h"
#include "psc_ds/lockedlist.h"
#include "psc_util/alloc.h"
#include "psc_util/lock.h"
#include "psc_util/log.h"

struct psc_lockedlist psc_hashtbls =
    PLL_INIT_NOLOG(&psc_hashtbls, struct psc_hashtbl, pht_lentry);

/*
 * "This well-known hash function was used in P.J. Weinberger's
 * C compiler (cf. Compilers: Principles, Techniques, and Tools,
 * by Aho, Sethi & Ullman, Addison-Wesley, 1988, p. 436)."
 */
int
_psc_str_hashify(const char *s, int len)
{
	unsigned h = 0, g;
	const char *p;

	if (s == NULL)
		return (-1);
	for (p = s; *p != '\0' && len; p++, len--) {
		h = (h << 4) + *p;
		if ((g = h & 0xf0000000)) {
			h = h ^ (g >> 24);
			h = h ^ g;
		}
	}
	return (h);
}

void
_psc_hashtbl_init(struct psc_hashtbl *t, int flags,
    ptrdiff_t idoff, ptrdiff_t hentoff, int nbuckets,
    int (*cmpf)(const void *, const void *), const char *fmt, ...)
{
	struct psc_hashbkt *b;
	int i, pafl;
	va_list ap;

	psc_assert(nbuckets > 0);

	pafl = 0;
	if (flags & PHTF_NOMEMGUARD)
	    pafl |= PAF_NOGUARD;
	if (flags & PHTF_NOLOG)
	    pafl |= PAF_NOLOG;

	memset(t, 0, sizeof(*t));
	INIT_PSC_LISTENTRY(&t->pht_lentry);
	INIT_SPINLOCK(&t->pht_lock);
	t->pht_nbuckets = nbuckets;
	t->pht_buckets = psc_alloc(nbuckets * sizeof(*t->pht_buckets), pafl);
	t->pht_flags = flags;
	t->pht_idoff = idoff;
	t->pht_hentoff = hentoff;
	t->pht_cmpf = cmpf;

	va_start(ap, fmt);
	vsnprintf(t->pht_name, sizeof(t->pht_name), fmt, ap);
	va_end(ap);

	for (i = 0, b = t->pht_buckets; i < nbuckets; i++, b++) {
		INIT_PSCLIST_HEAD(&b->phb_listhd);
		INIT_SPINLOCK_NOLOG(&b->phb_lock);
		psc_atomic32_set(&b->phb_nitems, 0);
	}
	pll_add(&psc_hashtbls, t);
}

/**
 * psc_hashtbl_lookup - Find a hash table by its name.
 * @name: name of hash table.
 */
struct psc_hashtbl *
psc_hashtbl_lookup(const char *name)
{
	struct psc_hashtbl *t;

	PLL_LOCK(&psc_hashtbls);
	PLL_FOREACH(t, &psc_hashtbls)
		if (strcmp(t->pht_name, name) == 0)
			break;
	PLL_ULOCK(&psc_hashtbls);
	return (t);
}

/**
 * psc_hashtbl_destroy - Reclaim a hash table's resources.
 * @t: table to destroy.
 */
void
psc_hashtbl_destroy(struct psc_hashtbl *t)
{
	int i;

	pll_remove(&psc_hashtbls, t);
	for (i = 0; i < t->pht_nbuckets; i++)
		if (psc_atomic32_read(&t->pht_buckets[i].phb_nitems))
			psc_fatalx("psc_hashtbl_destroy: "
			    "hash table not empty");
	PSCFREE(t->pht_buckets);
}

/**
 * psc_hashbkt_get - Locate the bucket containing an item with the
 *	given ID.
 * @t: table to search.
 * @key: search key.
 */
struct psc_hashbkt *
psc_hashbkt_get(const struct psc_hashtbl *t, const void *key)
{
	struct psc_hashbkt *b;

	if (t->pht_flags & PHTF_STR)
		b = &t->pht_buckets[psc_str_hashify(key) %
		    t->pht_nbuckets];
	else
		b = &t->pht_buckets[*(uint64_t *)key % t->pht_nbuckets];

	return (b);
}

void *
_psc_hashtbl_search(const struct psc_hashtbl *t, int flags,
    const void *cmp, void (*cbf)(void *), const void *key)
{
	struct psc_hashbkt *b;

	b = psc_hashbkt_get(t, key);
	return (_psc_hashbkt_search(t, b, flags, cmp, cbf, key));
}

void *
_psc_hashbkt_search(const struct psc_hashtbl *t, struct psc_hashbkt *b,
    int flags, const void *cmp, void (*cbf)(void *), const void *key)
{
	void *p, *pk;
	int locked;

	if (t->pht_cmpf)
		psc_assert(cmp);
	else
		psc_assert(cmp == NULL);

	locked = reqlock(&b->phb_lock);
	PSC_HASHBKT_FOREACH_ENTRY(t, p, b) {
		pk = (char *)p + t->pht_idoff;
		if (t->pht_flags & PHTF_STR) {
			if (strcmp(key, pk))
				continue;
		} else if (*(uint64_t *)key != *(uint64_t *)pk)
			continue;
		if (t->pht_cmpf == NULL || t->pht_cmpf(cmp, p)) {
			if (cbf)
				cbf(p);
			break;
		}
	}
	if (p && (flags & PHLF_DEL)) {
		psclist_del(psc_hashent_getlentry(t, p), &b->phb_listhd);
		psc_atomic32_dec(&b->phb_nitems);
	}
	ureqlock(&b->phb_lock, locked);
	return (p);
}

/**
 * psc_hashent_remove - Remove an item from the hash table it's in.
 * @t: the hash table.
 * @p: the item to remove from hash table.
 */
void
psc_hashent_remove(const struct psc_hashtbl *t, void *p)
{
	struct psc_hashbkt *b;
	void *pk;

	psc_assert(p);
	pk = (char *)p + t->pht_idoff;
	b = psc_hashbkt_get(t, pk);

	psc_hashbkt_del_item(t, b, p);
}

/**
 * psc_hashbkt_del_item - Remove an item from the hash table bucket it's
 *	in.
 * @t: the hash table.
 * @b: bucket to remove from.
 * @p: item to add.
 */
void
psc_hashbkt_del_item(const struct psc_hashtbl *t, struct psc_hashbkt *b,
    void *p)
{
	int locked;

	locked = reqlock(&b->phb_lock);
	psclist_del(psc_hashent_getlentry(t, p), &b->phb_listhd);
	psc_assert(psc_atomic32_read(&b->phb_nitems) > 0);
	psc_atomic32_dec(&b->phb_nitems);
	ureqlock(&b->phb_lock, locked);
}

/**
 * psc_hashbkt_add_item - Add an item to a hash table bucket.
 * @t: the hash table.
 * @p: item to add.
 */
void
psc_hashbkt_add_item(const struct psc_hashtbl *t, struct psc_hashbkt *b,
    void *p)
{
	int locked;

	locked = reqlock(&b->phb_lock);
	psclist_add(psc_hashent_getlentry(t, p), &b->phb_listhd);
	psc_atomic32_inc(&b->phb_nitems);
	ureqlock(&b->phb_lock, locked);
}

/**
 * psc_hashtbl_add_item - Add an item to a hash table.
 * @t: the hash table.
 * @p: item to add.
 */
void
psc_hashtbl_add_item(const struct psc_hashtbl *t, void *p)
{
	struct psc_hashbkt *b;
	void *pk;

	psc_assert(p);
	pk = (char *)p + t->pht_idoff;
	b = psc_hashbkt_get(t, pk);
	psc_hashbkt_add_item(t, b, p);
}

int
psc_hashent_conjoint(const struct psc_hashtbl *t, void *p)
{
	struct psc_hashbkt *b;
	void *pk;

	psc_assert(p);
	pk = (char *)p + t->pht_idoff;
	b = psc_hashbkt_get(t, pk);
	return (psclist_conjoint(psc_hashent_getlentry(t, p),
	    &b->phb_listhd));
}

/**
 * psc_hashtbl_getstats - Query a hash table for its bucket usage stats.
 * @t: the hash table.
 * @totalbucks: value-result pointer to # of buckets available.
 * @usedbucks: value-result pointer to # of buckets in use.
 * @nents: value-result pointer to # items in hash table.
 * @maxbucklen: value-result pointer to maximum bucket length.
 */
void
psc_hashtbl_getstats(const struct psc_hashtbl *t, int *totalbucks,
    int *usedbucks, int *nents, int *maxbucklen)
{
	struct psc_hashbkt *b;
	int bucklen;

	*nents = 0;
	*usedbucks = 0;
	*maxbucklen = 0;
	*totalbucks = t->pht_nbuckets;
	PSC_HASHTBL_FOREACH_BUCKET(b, t) {
		bucklen = psc_atomic32_read(&b->phb_nitems);
		if (bucklen) {
			++*usedbucks;
			*nents += bucklen;
			*maxbucklen = MAX(*maxbucklen, bucklen);
		}
	}
}

/**
 * psc_hashtbl_prstats - Print hash table bucket usage stats.
 * @t: the hash table.
 */
void
psc_hashtbl_prstats(const struct psc_hashtbl *t)
{
	int totalbucks, usedbucks, nents, maxbucklen;

	psc_hashtbl_getstats(t, &totalbucks, &usedbucks, &nents,
	    &maxbucklen);
	printf("used %d/total %d, nents=%d, maxlen=%d\n",
	    usedbucks, totalbucks, nents, maxbucklen);
}

void
psc_hashtbl_walk(const struct psc_hashtbl *t, void (*f)(void *))
{
	struct psc_hashbkt *b;
	int rc, locked;
	void *p;

	PSC_HASHTBL_FOREACH_BUCKET(b, t) {
		rc = tryreqlock(&b->phb_lock, &locked);
		PSC_HASHBKT_FOREACH_ENTRY(t, p, b)
			f(p);
		if (rc)
			ureqlock(&b->phb_lock, locked);
	}
}
