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

#include <sys/param.h>

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "pfl/alloc.h"
#include "pfl/hashtbl.h"
#include "pfl/list.h"
#include "pfl/lock.h"
#include "pfl/lockedlist.h"
#include "pfl/log.h"
#include "pfl/str.h"

struct psc_lockedlist psc_hashtbls =
    PLL_INIT_NOLOG(&psc_hashtbls, struct psc_hashtbl, pht_lentry);

#if PFL_DEBUG > 0
# include <math.h>
#endif

void
_psc_hashbkt_init(struct psc_hashtbl *t, struct psc_hashbkt *b)
{
	INIT_PSCLIST_HEAD(&b->phb_listhd);
	INIT_SPINLOCK_NOLOG(&b->phb_lock);
	psc_atomic32_set(&b->phb_nitems, 0);
	b->phb_gen = t->pht_gen;
}

int
_psc_hashtbl_getmemflags(struct psc_hashtbl *t)
{
	int pafl = 0;

	if (t->pht_flags & PHTF_NOMEMGUARD)
		pafl |= PAF_NOGUARD;
	if (t->pht_flags & PHTF_NOLOG)
		pafl |= PAF_NOLOG;
	return (pafl);
}

void
_psc_hashtbl_init(struct psc_hashtbl *t, int flags,
    ptrdiff_t idoff, ptrdiff_t hentoff, int nb,
    int (*cmpf)(const void *, const void *), const char *fmt, ...)
{
	struct psc_hashbkt *b;
	va_list ap;
	int i;

	psc_assert(nb > 0);

	memset(t, 0, sizeof(*t));
	INIT_PSC_LISTENTRY(&t->pht_lentry);
	INIT_SPINLOCK(&t->pht_lock);
	_psc_waitq_init(&t->pht_waitq, "hash table", flags & PHTF_NOLOG ?
	    PWQF_NOLOG : 0);
	t->pht_nbuckets = nb;
	if (flags & PHTF_STRP)
		flags |= PHTF_STR;
	t->pht_flags = flags;
	t->pht_buckets = psc_alloc(nb * sizeof(*t->pht_buckets),
	    _psc_hashtbl_getmemflags(t));
	t->pht_idoff = idoff;
	t->pht_hentoff = hentoff;
	t->pht_cmpf = cmpf;

	va_start(ap, fmt);
	vsnprintf(t->pht_name, sizeof(t->pht_name), fmt, ap);
	va_end(ap);

#if PFL_DEBUG > 0
 {
	double nearest, diff;

	nearest = log2(nb);
	diff = fabs(nearest - (int)nearest);
	if (nb % 2 == 0 || diff > .75 || diff < .25)
		psclog_warnx("%s nbuckets %d should be a large prime not too "
		    "close to a power of two (2**i-1)", t->pht_name, nb);
 }
#endif

	for (i = 0, b = t->pht_buckets; i < nb; i++, b++)
		_psc_hashbkt_init(t, b);
	pll_add(&psc_hashtbls, t);
}

/*
 * Find a hash table by its name.
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

/*
 * Reclaim a hash table's resources.
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

void
psc_hashbkt_put(struct psc_hashtbl *t, struct psc_hashbkt *b)
{
	int locked;

	psc_hashbkt_reqlock(b);

	if (--b->phb_refcnt)
		goto out;

	if (b->phb_gen == t->pht_gen ||
	    (t->pht_flags & PHTF_RESIZING) == 0)
		goto out;

	if (b->phb_died)
		goto out;
	b->phb_died = 1;

	locked = PSC_HASHTBL_RLOCK(t);
	if (--t->pht_ocntr == 0) {
		psc_free(t->pht_obuckets, _psc_hashtbl_getmemflags(t));
		t->pht_obuckets = NULL;
		t->pht_flags &= ~PHTF_RESIZING;
	}
	PSC_HASHTBL_URLOCK(t, locked);
	b = NULL;

 out:
	if (b)
		psc_hashbkt_unlock(b);
	psc_waitq_wakeall(&t->pht_waitq);
}

#define GETBKT(t, bv, nb, key)						\
	&(bv)[ ( (t)->pht_flags & PHTF_STR ? psc_str_hashify(key) :	\
	    *(uint64_t *)(key) ) % (nb) ]

/*
 * Locate the bucket containing an item with the given ID.
 * @t: table to search.
 * @key: search key.
 */
struct psc_hashbkt *
psc_hashbkt_get(struct psc_hashtbl *t, const void *key)
{
	struct psc_hashbkt *b;
	int locked;

 retry: 
	b = GETBKT(t, t->pht_buckets, t->pht_nbuckets, key);

	/*
 	 * 08/08/2017: Some unusual replication add/removal tests
 	 * shows we are stuck here with the original thread taking
 	 * the lock is gone. Time to get rid of recursive locking
 	 * in this area.
 	 */
	psc_hashbkt_reqlock(b);
	b->phb_refcnt++;
	if (t->pht_gen != b->phb_gen) {
		psc_hashbkt_put(t, b);
		locked = PSC_HASHTBL_RLOCK(t);
		while (t->pht_flags & PHTF_RESIZING)
			psc_waitq_wait(&t->pht_waitq,
			    &t->pht_lock);
		PSC_HASHTBL_URLOCK(t, locked);
		goto retry;
	}

	return (b);
}

void *
_psc_hashtbl_search(struct psc_hashtbl *t, int flags,
    int (*cmpf)(const void *, const void *), const void *cmp,
    void (*cbf)(void *, void *), void *arg, const void *key)
{
	struct psc_hashbkt *b;
	void *p;

	b = psc_hashbkt_get(t, key);
	p = _psc_hashbkt_search(t, b, flags, cmpf, cmp, cbf, arg, key);
	psc_hashbkt_put(t, b);
	return (p);
}

void *
_psc_hashbkt_search(struct psc_hashtbl *t, struct psc_hashbkt *b,
    int flags, int (*cmpf)(const void *, const void *), const void *cmp,
    void (*cbf)(void *, void *), void *arg, const void *key)
{
	void *p, *pk;
	int locked;

	if (cmpf == NULL)
		cmpf = t->pht_cmpf;
	if (cmpf)
		psc_assert(cmp);
	else
		psc_assert(cmp == NULL);

	locked = reqlock(&b->phb_lock);
	PSC_HASHBKT_FOREACH_ENTRY(t, p, b) {
		pk = (char *)p + t->pht_idoff;
		if (t->pht_flags & PHTF_STR) {
			if (t->pht_flags & PHTF_STRP)
				pk = *(char **)pk;
			if (strcmp(key, pk))
				continue;
		} else if (*(uint64_t *)key != *(uint64_t *)pk)
			continue;
		if (cmpf == NULL || cmpf(cmp, p)) {
			if (cbf)
				cbf(p, arg);
			break;
		}
	}
	if (p && (flags & PHLF_DEL)) {
		psclist_del(psc_hashent_getlentry(t, p),
		    &b->phb_listhd);
		psc_atomic32_dec(&b->phb_nitems);
	}
	ureqlock(&b->phb_lock, locked);
	return (p);
}

/*
 * Remove an item from the hash table it is in.
 * @t: the hash table.
 * @p: the item to remove from hash table.
 */
void
psc_hashent_remove(struct psc_hashtbl *t, void *p)
{
	struct psc_hashbkt *b;
	void *pk;

	psc_assert(p);
	pk = PSC_AGP(p, t->pht_idoff);
	b = psc_hashbkt_get(t, pk);
	psc_hashbkt_del_item(t, b, p);
	psc_hashbkt_put(t, b);
}

struct psc_hashbkt *
psc_hashent_getbucket(struct psc_hashtbl *t, void *p)
{
	void *pk;

	psc_assert(p);
	pk = PSC_AGP(p, t->pht_idoff);
	return (psc_hashbkt_get(t, pk));
}

/*
 * Remove an item from the hash table bucket it is in.
 * @t: the hash table.
 * @b: bucket to remove from.
 * @p: item to remove.
 */
void
psc_hashbkt_del_item(struct psc_hashtbl *t, struct psc_hashbkt *b,
    void *p)
{
	int locked;

	locked = reqlock(&b->phb_lock);
	psclist_del(psc_hashent_getlentry(t, p), &b->phb_listhd);
	psc_assert(psc_atomic32_read(&b->phb_nitems) > 0);
	psc_atomic32_dec(&b->phb_nitems);
	ureqlock(&b->phb_lock, locked);
}

/*
 * Add an item to a hash table bucket.
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

/*
 * Add an item to a hash table.
 * @t: the hash table.
 * @p: item to add.
 */
void
psc_hashtbl_add_item(struct psc_hashtbl *t, void *p)
{
	struct psc_hashbkt *b;
	void *pk;

	psc_assert(p);
	pk = PSC_AGP(p, t->pht_idoff);
	b = psc_hashbkt_get(t, pk);
	psc_hashbkt_add_item(t, b, p);
	psc_hashbkt_put(t, b);
}

int
psc_hashent_conjoint(struct psc_hashtbl *t, void *p)
{
	struct psc_hashbkt *b;
	int conjoint;
	void *pk;

	psc_assert(p);
	pk = PSC_AGP(p, t->pht_idoff);
	b = psc_hashbkt_get(t, pk);
	conjoint = psclist_conjoint(psc_hashent_getlentry(t, p),
	    &b->phb_listhd);
	psc_hashbkt_put(t, b);
	return (conjoint);
}

/*
 * Query a hash table for its bucket usage stats.
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

/*
 * Print hash table bucket usage stats.
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

/*
 * Estimate ideal number of buckets to allocate for a hash table.
 * Given a number of items N, the hash table should be roughly twice the
 * size to statistically reduce chance of collision.
 * Good hashing functions can do a good job creating a fair distribution
 * but a generic one intended for all use can only do such a great job,
 * so assist it by using a prime number of buckets not too close to a
 * power of two.
 * @n: number of elements intended to place in hash table.
 */
int
psc_hashtbl_estnbuckets(int n)
{
	int t;

	t = n * 3;
	if (t % 2 == 0)
		t--;
	return (t);
}

void
psc_hashtbl_resize(struct psc_hashtbl *t, int nb)
{
	struct psc_hashbkt *bnew, *b, *bn;
	void *p, *pn, *pk;
	int i, oldnb;

	bnew = psc_alloc(nb * sizeof(*b), _psc_hashtbl_getmemflags(t));

	PSC_HASHTBL_LOCK(t);
	while (t->pht_flags & PHTF_RESIZING) {
		psc_waitq_wait(&t->pht_waitq, &t->pht_lock);
		PSC_HASHTBL_LOCK(t);
	}

	t->pht_gen++;

	for (b = bnew, i = 0; i < nb; i++, b++)
		_psc_hashbkt_init(t, b);

	t->pht_flags |= PHTF_RESIZING;
	t->pht_obuckets = t->pht_buckets;
	t->pht_ocntr = oldnb = t->pht_nbuckets;

	for (i = 0, b = t->pht_buckets; i < oldnb; i++, b++) {
		psc_hashbkt_lock(b);
		while (b->phb_refcnt) {
			psc_waitq_wait(&t->pht_waitq, &t->pht_lock);
			psc_hashbkt_lock(b);
		}

		PSC_HASHBKT_FOREACH_ENTRY_SAFE(t, p, pn, b) {
			psc_hashbkt_del_item(t, b, p);

			pk = PSC_AGP(p, t->pht_idoff);
			bn = GETBKT(t, bnew, nb, pk);
			psc_hashbkt_add_item(t, bn, p);
		}
		psc_hashbkt_unlock(b);
	}

	t->pht_buckets = bnew;
	t->pht_nbuckets = nb;
	PSC_HASHTBL_ULOCK(t);

	for (i = 0, b = t->pht_obuckets; i < oldnb; i++, b++) {
		psc_hashbkt_lock(b);
		b->phb_refcnt++;
		psc_hashbkt_put(t, b);
	}
}
