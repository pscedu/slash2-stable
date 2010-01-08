/* $Id$ */
/*
 * %PSC_START_COPYRIGHT%
 * -----------------------------------------------------------------------------
 * Copyright (c) 2006-2009, Pittsburgh Supercomputing Center (PSC).
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

#ifndef _PFL_HASH_H_
#define _PFL_HASH_H_

#include "pfl/cdefs.h"
#include "pfl/types.h"
#include "psc_ds/list.h"
#include "psc_util/lock.h"

#define LOCK_BUCKET(hb)		spinlock(&(hb)->hbucket_lock)
#define ULOCK_BUCKET(hb)	freelock(&(hb)->hbucket_lock)

#define LOCK_HASHTBL(ht)	spinlock(&(ht)->htable_lock)
#define ULOCK_HASHTBL(ht)	freelock(&(ht)->htable_lock)

#define GET_BUCKET(t,i)		&(t)->htable_buckets[(i) % (t)->htable_size]

#define HTNAME_MAX 30

struct hash_bucket {
	struct psclist_head	  hbucket_list;	/* Entry list head */
	psc_spinlock_t		  hbucket_lock;	/* Spinlock for this bucket */
};

#define HASH_BUCKET_SZ sizeof(struct hash_bucket)

struct hash_table {
	char			  htable_name[HTNAME_MAX];
	struct psclist_head	  htable_entry;

	int			  htable_size;
	int			  htable_strlen_max;
	psc_spinlock_t		  htable_lock;
	struct hash_bucket	 *htable_buckets;
	int			(*htcompare)(const void *, const void *);
};

/* hash table flags */
#define HTF_RESORT	(1 << 0)	/* resort buckets on lookup */
#define HTF_ALLOWDUPS	(1 << 1)	/* use comparator for items with same ID */
#define HTF_STR		(1 << 2)	/* use strings for ID */

struct hash_entry {
	struct psclist_head	  hentry_lentry;/* Entry list pointers */
	const uint64_t		 *hentry_id;	/* Pointer to the hash element id */
	void			 *private;	/* pointer to private data */
};

/*
 * String Hash Defines
 */
#define SGET_BUCKET(t, i) &(t)->htable_buckets[str_hash((i)) % (t)->htable_size]

struct hash_entry_str {
	struct psclist_head	  hentry_str_lentry;/* Entry list pointers */
	const char		 *hentry_str_id;   /* Pointer to the hash element str */
	void			 *private;	   /* pointer to private data  */
};

/*
 * "This well-known hash function
 *    was used in P.J. Weinberger's C compiler
 *    (cf. Compilers: Principles, Techniques,
 *    and Tools, by Aho, Sethi & Ullman,
 *    Addison-Wesley, 1988, p. 436)."
 */
static inline int
str_hash(const char *s)
{
	const char *p;
	unsigned h = 0, g;

	if ( s == NULL )
		return -1;

	for (p = s; *p != '\0'; p++) {
		h = (h << 4) + (*p);
		if ((g = h & 0xf0000000)) {
			h = h ^ (g >> 24);
			h = h ^ g;
		}
	}
	return (h);
}

#define HASHTBL_FOREACH_BUCKET(b, ht)					\
	for ((b) = (ht)->htable_buckets;				\
	    (b) - (ht)->htable_buckets < (ht)->htable_size;		\
	    (b)++)

#define HASHBUCKET_FOREACH_ENTRY(p, b)					\
	psclist_for_each_entry(p, &(b)->hbucket_list, hentry_lentry)

#define hashbkt_lock(b)			LOCK_BUCKET(b)
#define hashbkt_unlock(b)		ULOCK_BUCKET(b)
#define hashbkt_get(t, id)		GET_BUCKET(t, id)

#define hashbkt_del_item(t, b, id, cmp)					\
	_hashbkt_search((t), (b), (id), (cmp), NULL, 1)

#define hashbkt_search(t, b, id, cmp, cbf)				\
	_hashbkt_search((t), (b), (id), (cmp), (cbf), 0)

struct hash_entry *_hashbkt_search(const struct hash_table *,
	struct hash_bucket *, uint64_t, const void *, void (*)(void *), int);

struct hash_entry * get_hash_entry(const struct hash_table *, uint64_t, const void *, void (*)(void *));
struct hash_entry_str * get_hash_entry_str(const struct hash_table *, const char *);

void hashbkt_del_entry(struct hash_bucket *, struct hash_entry *);
void hashbkt_add_entry(struct hash_bucket *, struct hash_entry *);

void *del_hash_entry(const struct hash_table *, uint64_t);
void *del_hash_entry_str(const struct hash_table *, const char *);
void add_hash_entry(const struct hash_table *, struct hash_entry *);
void add_hash_entry_str(const struct hash_table *, struct hash_entry_str *);
void hash_table_printstats(const struct hash_table *);
void hash_table_stats(const struct hash_table *, int *, int *, int *, int *);
void init_hash_entry(struct hash_entry *, const uint64_t *, void *);
void init_hash_entry_str(struct hash_entry_str *, const char *, void *);
void init_hash_table(struct hash_table *, int, const char *, ...);
void spinlock_hash_bucket(const struct hash_table *, uint64_t);
int  trylock_hash_bucket(const struct hash_table *, uint64_t);
void freelock_hash_bucket(const struct hash_table *, uint64_t);

/* XXX use a lockedlist */
extern struct psclist_head hashTablesList;
extern psc_spinlock_t hashTablesListLock;

#endif /* _PFL_HASH_H_ */
