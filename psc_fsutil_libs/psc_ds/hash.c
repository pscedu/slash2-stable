/* $Id$ */

#include <sys/param.h>

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "psc_ds/hash.h"
#include "psc_ds/list.h"
#include "psc_util/alloc.h"
#include "psc_util/assert.h"
#include "psc_util/lock.h"
#include "psc_util/log.h"

struct psclist_head hashTablesList = PSCLIST_HEAD_INIT(hashTablesList);
psc_spinlock_t hashTablesListLock = LOCK_INITIALIZER;

/**
 * init_hash_table -   initialize a hash table
 * @hash_table: pointer to the hash table array
 * @size: size of the hash table
 */
void
init_hash_table(struct hash_table *t, int size, const char *fmt, ...)
{
	struct hash_bucket *b;
	va_list ap;
	int i;

	t->htable_size       = size;
	t->htable_buckets    = PSCALLOC(size * HASH_BUCKET_SZ);
	t->htable_strlen_max = PATH_MAX;
	t->htcompare         = NULL;

	LOCK_INIT(&t->htable_lock);

	for (i=0, b = t->htable_buckets; i < size; i++, b++) {
		LOCK_INIT(&b->hbucket_lock);
		INIT_PSCLIST_HEAD(&b->hbucket_list);
	}

	va_start(ap, fmt);
	vsnprintf(t->htable_name, sizeof(t->htable_name), fmt, ap);
	va_end(ap);

	INIT_PSCLIST_ENTRY(&t->htable_entry);

	spinlock(&hashTablesListLock);
	psclist_xadd(&t->htable_entry, &hashTablesList);
	freelock(&hashTablesListLock);
}

/**
 * init_hash_entry - initialize a hash entry
 * @hash_entry: pointer to the hash_entry which will be initialized
 * @id: pointer to the array of hash_entry pointers
 * @private: application data to be stored in the hash
 */
void
init_hash_entry(struct hash_entry *hentry, u64 *id, void *private)
{
	hentry->hentry_id = id;
	hentry->private   = private;
}

/**
 * get_hash_entry -   locate an address in the hash table
 * @t: hash table pointer
 * @id: identifier used to get hash bucket
 * @comp: value to compare to differentiate entries with same ID.
 * @cbf: callback routine when the entry is found, to prevent race
 * 	conditions while the bucket is locked.
 */
struct hash_entry *
get_hash_entry(const struct hash_table *h, u64 id, const void *comp,
    void (*cbf)(void *))
{
	struct hash_bucket *b;
	struct hash_entry *e;

	psc_assert(h->htable_size);
	if (h->htcompare)
		psc_assert(comp);
	else
		psc_assert(comp == NULL);

	b = GET_BUCKET(h, id);
	LOCK_BUCKET(b);
	psclist_for_each_entry(e, &b->hbucket_list, hentry_lentry)
		if (id == *e->hentry_id)
			if (h->htcompare == NULL ||
			    h->htcompare(comp, e->private)) {
				if (cbf)
					cbf(e->private);
				break;
			}
	ULOCK_BUCKET(b);
	return (e);
}

/**
 * del_hash_entry -   remove an entry in the hash table
 * @t: pointer to hash table
 * @id: identifier used to get hash bucket
 */
void *
del_hash_entry(const struct hash_table *h, u64 id)
{
	struct hash_bucket *b;
	struct hash_entry *e;

	psc_assert(h->htable_size);

	b = GET_BUCKET(h, id);
	LOCK_BUCKET(b);
	psclist_for_each_entry(e, &b->hbucket_list, hentry_lentry)
		if (id == *e->hentry_id) {
			psclist_del(&e->hentry_lentry);
			break;
		}
	ULOCK_BUCKET(b);
	return (e ? e->private : NULL);
}

/**
 * add_hash_entry -   add an entry in the hash table
 * @t: pointer to the hash table
 * @entry: pointer to entry to be added
 */
void
add_hash_entry(const struct hash_table *t, struct hash_entry *e)
{
	struct hash_bucket *b;

	psc_assert(t->htable_size);

	b = GET_BUCKET(t, *e->hentry_id);
	LOCK_BUCKET(b);
	psclist_xadd(&e->hentry_lentry, &b->hbucket_list);
	ULOCK_BUCKET(b);
}

/**
 * init_hash_entry_str -   initialize a string hash entry
 * @hentry: pointer to the hash_entry which will be initialized
 * @id: pointer to the array of hash_entry pointers
 * @private: application data to be stored in the hash
 */
void
init_hash_entry_str(struct hash_entry_str *hentry, const char *id,
    void *private)
{
	hentry->hentry_str_id = id;
	hentry->private       = private;
}

/**
 * get_hash_entry_str -   locate an address in the hash table
 * @h: pointer to the hash table
 * @id: identifier used to get hash bucket
 */
struct hash_entry_str *
get_hash_entry_str(const struct hash_table *h, const char *id)
{
	struct hash_bucket     *b;
	struct hash_entry_str  *e = NULL;

	psc_assert(h->htable_size);

	b = SGET_BUCKET(h, id);
	LOCK_BUCKET(b);

	psclist_for_each_entry(e, &b->hbucket_list, hentry_str_lentry)
		if (!strncmp(id, e->hentry_str_id, h->htable_strlen_max))
			break;
	ULOCK_BUCKET(b);
	return (e);
}

/**
 * del_hash_entry_str -   remove an entry in the hash table
 * @t: pointer to the hash table
 * @size: the match string
 */
void *
del_hash_entry_str(const struct hash_table *h, const char *id)
{
	struct hash_entry_str *e;
	struct hash_bucket *b;

	psc_assert(h->htable_size);

	b = SGET_BUCKET(h, id);
	LOCK_BUCKET(b);
	psclist_for_each_entry(e, &b->hbucket_list, hentry_str_lentry)
		if (!strncmp(id, e->hentry_str_id, h->htable_strlen_max)) {
			psclist_del(&e->hentry_str_lentry);
			break;
		}
	ULOCK_BUCKET(b);
	return (e ? e->private : NULL);
}

/**
 * add_hash_entry_str -  add an entry in the hash table
 * @t: pointer to the hash table
 * @entry: pointer to entry to be added
 */
void
add_hash_entry_str(const struct hash_table *t, struct hash_entry_str *e)
{
	struct hash_bucket *b;

	psc_assert(t->htable_size);

	b = SGET_BUCKET(t, e->hentry_str_id);
	LOCK_BUCKET(b);
	psclist_xadd(&e->hentry_str_lentry, &b->hbucket_list);
	ULOCK_BUCKET(b);
}

/**
 * hash_table_stats - query a hash table for its bucket usage stats.
 * @t: pointer to the hash table.
 * @totalbucks: value-result pointer to # of buckets available.
 * @usedbucks: value-result pointer to # of buckets in use.
 * @nents: value-result pointer to # items in hash table.
 * @maxbucklen: value-result pointer to maximum bucket length.
 */
void
hash_table_stats(const struct hash_table *t, int *totalbucks,
    int *usedbucks, int *nents, int *maxbucklen)
{
	struct hash_bucket *b;
	struct psclist_head *ent;
	int bucklen;
	int i;

	*nents = 0;
	*usedbucks = 0;
	*maxbucklen = 0;
	*totalbucks = t->htable_size;
	for (i = 0, b = t->htable_buckets; i < t->htable_size; i++, b++) {
		bucklen = 0;
		LOCK_BUCKET(b);
		psclist_for_each(ent, &b->hbucket_list)
			bucklen++;
		ULOCK_BUCKET(b);

		if (bucklen) {
			++*usedbucks;
			*nents += bucklen;
			*maxbucklen = MAX(*maxbucklen, bucklen);
		}
	}
}

/**
 * hash_table_printstats - print hash table bucket usage stats.
 * @t: pointer to the hash table.
 */
void
hash_table_printstats(const struct hash_table *t)
{
	int totalbucks, usedbucks, nents, maxbucklen;

	hash_table_stats(t, &totalbucks, &usedbucks, &nents,
	    &maxbucklen);
	printf("used %d/total %d, nents=%d, maxlen=%d\n",
	    usedbucks, totalbucks, nents, maxbucklen);
}
