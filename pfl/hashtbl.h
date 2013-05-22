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

#ifndef _PFL_HASHTBL_H_
#define _PFL_HASHTBL_H_

#include "pfl/list.h"
#include "pfl/lockedlist.h"
#include "psc_util/atomic.h"
#include "psc_util/lock.h"
#include "psc_util/log.h"

#define PSC_HASHTBL_LOCK(t)	spinlock(&(t)->pht_lock)
#define PSC_HASHTBL_ULOCK(t)	freelock(&(t)->pht_lock)

#define PSC_HTNAME_MAX		30

struct psc_hashbkt {
	struct psclist_head	  phb_listhd;
	psc_spinlock_t		  phb_lock;
	psc_atomic32_t		  phb_nitems;
	int			  phb_refcnt;
};

struct psc_hashtbl {
	char			  pht_name[PSC_HTNAME_MAX];
	struct psc_listentry	  pht_lentry;
	psc_spinlock_t		  pht_lock;
	ptrdiff_t		  pht_idoff;	/* offset into item to its ID field */
	ptrdiff_t		  pht_hentoff;	/* offset to the hash table linkage */
	int			  pht_flags;	/* hash table flags, see below */
	int			  pht_nbuckets;
	struct psc_hashbkt	 *pht_buckets;
	int			(*pht_cmpf)(const void *, const void *);
};

struct psc_hashent {
	struct psc_listentry	  phe_lentry;
};
#define psc_hashentry psc_hashent

/* Table flags. */
#define PHTF_NONE	0		/* no table flags specified */
#define PHTF_STR	(1 << 0)	/* IDs are strings */
#define PHTF_RESORT	(1 << 1)	/* reorder queues on lookup */
#define PHTF_NOMEMGUARD	(1 << 2)	/* disable memalloc guard */
#define PHTF_NOLOG	(1 << 3)	/* do not psclog */

/* Lookup flags. */
#define PHLF_NONE	0		/* no lookup flags specified */
#define PHLF_DEL	(1 << 0)	/* find and remove item from table */

#define PSC_HASHTBL_FOREACH_BUCKET(b, t)				\
	for ((b) = (t)->pht_buckets;					\
	    (b) - (t)->pht_buckets < (t)->pht_nbuckets;			\
	    (b)++)

#define PSC_HASHBKT_FOREACH_ENTRY(t, p, b)				\
	psclist_for_each_entry2(p, &(b)->phb_listhd, (t)->pht_hentoff)

/**
 * psc_hashtbl_init - Initialize a hash table.
 * @t: hash table to initialize.
 * @flags: optional modifier flags.
 * @type: type of the structure to be put into the hash table.
 * @idmemb: the field that stores the ID information (aka key).
 * @hentmemb: the field used to link the structure into the hash table.
 * @nb: number of buckets to create.
 * @cmpf: optional function to differentiate items of the same ID.
 * @fmt: printf(3)-like name of hash for lookups and external control.
 */
#define psc_hashtbl_init(t, flags, type, idmemb, hentmemb, nb, cmpf,	\
	    fmt, ...)							\
	do {								\
		if (sizeof(((type *)NULL)->hentmemb) !=			\
		    sizeof(struct psc_hashent))				\
			psc_fatalx("invalid hash ID field");		\
		_psc_hashtbl_init((t), (flags), offsetof(type, idmemb),	\
		    offsetof(type, hentmemb), (nb), (cmpf), (fmt),	\
		    ## __VA_ARGS__);					\
	} while (0)

/**
 * psc_hashtbl_search - Search a hash table for an item by its hash ID.
 * @t: the hash table.
 * @cmp: optional value to compare with to differentiate entries with same ID.
 * @cbf: optional callback routine invoked when the entry is found, executed
 *	while the bucket is locked.
 * @key: search key pointer to either of:
 *	- uint64_t hash ID value
 *	- const char * string ID
 */
#define psc_hashtbl_search(t, cmp, cbf, key)				\
	_psc_hashtbl_search((t), 0, (cmp), (cbf), (key))

/**
 * psc_hashtbl_searchdel - Search a hash table for an item by its hash ID
 *	and remove and return if found.
 * @t: the hash table.
 * @cmp: optional value to compare with to differentiate entries with same ID.
 * @key: search key pointer to either of:
 *	- uint64_t hash ID value
 *	- const char * string ID
 */
#define psc_hashtbl_searchdel(t, cmp, key)				\
	_psc_hashtbl_search((t), PHLF_DEL, (cmp), NULL, (key))

struct psc_hashtbl *
	 psc_hashtbl_lookup(const char *);
void	 psc_hashtbl_add_item(const struct psc_hashtbl *, void *);
void	 psc_hashtbl_prstats(const struct psc_hashtbl *);
void	 psc_hashtbl_getstats(const struct psc_hashtbl *, int *, int *, int *, int *);
void	 psc_hashtbl_destroy(struct psc_hashtbl *);
void	*_psc_hashtbl_search(const struct psc_hashtbl *, int, const void *,
	    void (*)(void *), const void *);
void	_psc_hashtbl_init(struct psc_hashtbl *, int, ptrdiff_t, ptrdiff_t, int,
	    int (*)(const void *, const void *), const char *, ...);

/**
 * psc_hashbkt_search - Search a bucket for an item by its hash ID.
 * @t: the hash table.
 * @b: the bucket to search.
 * @cmp: optional value to compare with to differentiate entries with same ID.
 * @cbf: optional callback routine invoked when the entry is found, executed
 *	while the bucket is locked.
 * @key: search key pointer to either of:
 *	- uint64_t hash ID value
 *	- const char * string ID
 */
#define	psc_hashbkt_search(t, b, cmp, cbf, key)				\
	_psc_hashbkt_search((t), (b), 0, (cmp), (cbf), (key))

/**
 * psc_hashtbl_searchdel - Search a bucket for an item by its hash ID and
 *	remove and return if found.
 * @t: the hash table.
 * @b: the bucket to search.
 * @cmp: optional value to compare with to differentiate entries with same ID.
 * @key: search key pointer to either of:
 *	- uint64_t hash ID value
 *	- const char * string ID
 */
#define	psc_hashbkt_searchdel(t, b, cmp, key)				\
	_psc_hashbkt_search((t), (b), PHLF_DEL, (cmp), NULL, (key))

struct psc_hashbkt *
	 psc_hashbkt_get(const struct psc_hashtbl *, const void *);
void	 psc_hashbkt_del_item(const struct psc_hashtbl *,
		struct psc_hashbkt *, void *);
void	 psc_hashbkt_add_item(const struct psc_hashtbl *,
		struct psc_hashbkt *, void *);
void	*_psc_hashbkt_search(const struct psc_hashtbl *,
		struct psc_hashbkt *, int, const void *, void (*)(void *),
		const void *);

#define psc_hashbkt_lock(b)		spinlock(&(b)->phb_lock)
#define psc_hashbkt_unlock(b)		freelock(&(b)->phb_lock)
#define psc_hashbkt_trylock(b)		trylock(&(b)->phb_lock)
#define psc_hashbkt_reqlock(b)		reqlock(&(b)->phb_lock)
#define psc_hashbkt_ureqlock(b, lk)	ureqlock(&(b)->phb_lock, (lk))

void	 psc_hashent_init(const struct psc_hashtbl *, void *);
void	 psc_hashent_remove(const struct psc_hashtbl *, void *);
int	 psc_hashent_conjoint(const struct psc_hashtbl *, void *);

#define psc_hashent_disjoint(t, p)	psclist_disjoint(			\
					    psc_hashent_getlentry((t), (p)))
#define psc_hashent_init(t, p)		INIT_PSC_LISTENTRY(			\
					    psc_hashent_getlentry((t), (p)))

extern struct psc_lockedlist psc_hashtbls;

static __inline struct psclist_head *
psc_hashent_getlentry(const struct psc_hashtbl *t, void *p)
{
	void *e;

	psc_assert(p);
	e = (char *)p + t->pht_hentoff;
	return (e);
}

static __inline const void *
psc_hashent_getid(const struct psc_hashtbl *t, const void *p)
{
	psc_assert(p);
	return ((const char *)p + t->pht_idoff);
}

#endif /* _PFL_HASHTBL_H_ */
