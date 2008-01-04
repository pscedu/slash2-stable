/* $Id$ */

#ifndef _PFL_LISTCACHE_H_
#define _PFL_LISTCACHE_H_

#include <sys/types.h>

#include <stdarg.h>
#include <string.h>

#include "psc_ds/list.h"
#include "psc_util/lock.h"
#include "psc_util/alloc.h"
#include "psc_util/assert.h"
#include "psc_util/atomic.h"
#include "psc_util/waitq.h"
#include "psc_util/alloc.h"

#define LC_NAME_MAX 32

extern struct psclist_head	pscListCaches;
extern psc_spinlock_t		pscListCachesLock;

/*
 * List cache which can be edited by multiple
 *  threads.
 */
struct list_cache {
	struct psclist_head lc_list;		/* head/tail of list         */
	struct psclist_head lc_index_lentry;	/* link between caches       */
	char		    lc_name[LC_NAME_MAX]; /* for lc mgmt             */
	ssize_t		    lc_max;		/* max allowable entries     */
	ssize_t		    lc_min;		/* keep at least this many   */
	ssize_t		    lc_size;		/* current #items in list    */
	size_t		    lc_nseen;		/* total #items placed on us */
	size_t		    lc_offset;		/* offset to entry member    */
	atomic_t	    lc_total;		/* relative max              */
	psc_spinlock_t	    lc_lock;		/* exclusitivity ctl         */
	psc_waitq_t	    lc_waitq_empty;	/* when we're empty          */
	psc_waitq_t	    lc_waitq_full;	/* when we're full           */
};
typedef struct list_cache list_cache_t;

#define LIST_CACHE_LOCK(l)  spinlock(&(l)->lc_lock)
#define LIST_CACHE_ULOCK(l) freelock(&(l)->lc_lock)

/**
 * lc_sz - how many items are in here.
 * This should use atomics at some point
 */
static inline ssize_t
lc_sz(list_cache_t *l)
{
	int locked;
	ssize_t sz;

	psc_assert(l);
	locked = reqlock(&l->lc_lock);
	sz = l->lc_size;
	ureqlock(&l->lc_lock, locked);

	return sz;
}

/**
 * lc_del - remove an entry from the list
 * @e: the entry
 * @l: the entry's respective list
 */
static inline void
lc_del(struct psclist_head *e, list_cache_t *l)
{
	int locked;

	psc_assert(e && l);

	locked = reqlock(&l->lc_lock);

	psclist_del(e);
	l->lc_size--;

	psc_assert(l->lc_size >= 0);

	ureqlock(&l->lc_lock, locked);
	/*
	 * An item was popped from our list, so wakeup other
	 * threads waiting to use the spot on this list.
	 */
	psc_waitq_wakeup(&l->lc_waitq_full);
}

/**
 * lc_get
 * @l: the list cache to access
 * @block: should the get wait
 */
static inline struct psclist_head *
_lc_get(list_cache_t *l, int block, int peek)
{
	struct psclist_head *e;

 start:
	LIST_CACHE_LOCK(l);

	if (psclist_empty(&l->lc_list)) {
		if (block) {
			/* Wait until the list is no longer empty. */
			if (block > 1)
				psc_warnx("Watiing on listcache %p : '%s'",
					  l, l->lc_name);
			else
				psc_notify("Waiting on listcache %p : '%s'",
					   l, l->lc_name);
			psc_waitq_wait(&l->lc_waitq_empty, &l->lc_lock);
			goto start;

		} else {
			LIST_CACHE_ULOCK(l);
			return NULL;
		}
	}
	e = psclist_first(&l->lc_list);

	if (!peek)
		lc_del(e, l);

	LIST_CACHE_ULOCK(l);

	return e;
}

/*
 *  Wrapper calls for the old lc_get()
 */
static inline struct psclist_head *
lc_get(list_cache_t *l, int block)
{
	return (_lc_get(l, block, 0));
}

/**
 * lc_put - Bounded list put
 * @l: the list cache to access
 * @n: new list item
 */
static inline void
_lc_put(list_cache_t *l, struct psclist_head *n, int qors)
{
	psc_assert(n->znext == NULL);
	psc_assert(n->zprev == NULL);

 start:
	LIST_CACHE_LOCK(l);

	if ((l->lc_max > 0) && (l->lc_size >= l->lc_max)) {
		psc_waitq_wait(&l->lc_waitq_full, &l->lc_lock);
		goto start;
	}

	if (qors)
		psclist_add_tail(n, &l->lc_list);
	else
		psclist_add(n, &l->lc_list);

	l->lc_size++;
	l->lc_nseen++;

	LIST_CACHE_ULOCK(l);
	/*
	 * There is now an item available; wake up waiters
	 * who think the list is empty.
	 */
	psc_waitq_wakeup(&l->lc_waitq_empty);
}

static inline void
lc_queue(list_cache_t *l, struct psclist_head *n)
{
	return (_lc_put(l, n, 1));
}

static inline void
lc_stack(list_cache_t *l, struct psclist_head *n)
{
	return (_lc_put(l, n, 0));
}

#define lc_put lc_queue

/**
 * lc_requeue - move an existing entry to the end of the queue
 *
 */
static inline void
lc_requeue(list_cache_t *l, struct psclist_head *n)
{
	int locked;

	locked = reqlock(&l->lc_lock);
	psclist_del(n);
	psclist_add_tail(n, &l->lc_list);
	ureqlock(&l->lc_lock, locked);
}

/**
 * lc_init - initialize a list cache.
 * @l: the list cache to initialize.
 */
static inline void
lc_init(list_cache_t *lc)
{
	lc->lc_size  = 0;
	lc->lc_max   = -1;
	atomic_set(&lc->lc_total, 0);
	INIT_PSCLIST_HEAD(&lc->lc_list);
	INIT_PSCLIST_ENTRY(&lc->lc_index_lentry);
	LOCK_INIT(&lc->lc_lock);
	psc_waitq_init(&lc->lc_waitq_empty);
	psc_waitq_init(&lc->lc_waitq_full);
}

/**
 * lc_vregister - register a list cache for external access.
 * @lc: the list cache to register.
 * @name: printf(3) format of name for list.
 * @ap: variable argument list for printf(3) name argument.
 */
static inline void
lc_vregister(list_cache_t *lc, const char *name, va_list ap)
{
	int rc, locked;

	if (lc->lc_index_lentry.znext ||
	    lc->lc_index_lentry.zprev)
		psc_fatalx("lc is already registered");

	spinlock(&pscListCachesLock);
	locked = reqlock(&lc->lc_lock);

	rc = vsnprintf(lc->lc_name, sizeof(lc->lc_name), name, ap);
	if (rc == -1)
		psc_fatal("vsnprintf");
	else if (rc > (int)sizeof(lc->lc_name))
		psc_fatalx("lc_name is too large (%s)", name);

	psc_assert(
	    lc->lc_index_lentry.znext == NULL &&
	    lc->lc_index_lentry.zprev == NULL);

	psclist_add(&lc->lc_index_lentry, &pscListCaches);

	ureqlock(&lc->lc_lock, locked);
	freelock(&pscListCachesLock);
}

/**
 * lc_register - register a list cache for external access.
 * @lc: the list cache to register.
 * @name: printf(3) format of name for list.
 */
static inline void
lc_register(list_cache_t *lc, const char *name, ...)
{
	va_list ap;

	va_start(ap, name);
	lc_vregister(lc, name, ap);
	va_end(ap);
}

/**
 * lc_reginit - initialize and register a list cache.
 * @lc: the list cache.
 * @name: printf(3) format of name for list.
 */
static inline void
lc_reginit(list_cache_t *lc, const char *name, ...)
{
	va_list ap;

	lc_init(lc);

	va_start(ap, name);
	lc_vregister(lc, name, ap);
	va_end(ap);
}

/**
 * lc_unregister - remove list cache external access registration.
 * @lc: the list cache to unregister.
 */
static inline void
lc_unregister(list_cache_t *lc)
{
	int locked;

	spinlock(&pscListCachesLock);
	locked = reqlock(&lc->lc_lock);
	psclist_del(&lc->lc_index_lentry);
	ureqlock(&lc->lc_lock, locked);
	freelock(&pscListCachesLock);
}

/**
 * lc_lookup - find a list cache by its registration name.
 * @name: name of list cache.
 * Notes: returns the list cache locked if found.
 */
static inline list_cache_t *
lc_lookup(const char *name)
{
	struct psclist_head *e;
	list_cache_t *lc;
	int found;

	lc = NULL; /* gcc */
	found = 0;
	spinlock(&pscListCachesLock);
	psclist_for_each(e, &pscListCaches) {
		lc = psclist_entry(e, list_cache_t, lc_index_lentry);
		if (strcmp(name, lc->lc_name) == 0) {
			LIST_CACHE_LOCK(lc);
			found = 1;
			break;
		}
	}
	freelock(&pscListCachesLock);
	if (found)
		return (lc);
	return (NULL);
}

/**
 * lc_empty - determine if the list cache has elements currently.
 * @lc: list cache to check.
 */
static inline int
lc_empty(const list_cache_t *lc)
{
	return (psclist_empty(&lc->lc_list));
}

#define _lc_grow(l, n, type, init_fn, __ret)				\
	do {								\
		int     i;						\
		type   *ptr;						\
		list_cache_t *lc = l;					\
		ssize_t z = lc_sz(lc);					\
									\
		__ret = 0;						\
		if (z >= lc->lc_max) {					\
			psc_warnx("Cache %s has overgrown", lc->lc_name);	\
			break;						\
		}							\
		if (z == lc->lc_max)					\
			break;						\
		if ((z + n) > lc->lc_max)				\
			n = lc->lc_max - z;				\
									\
		for (i=0; i < n; i++, __ret++) {			\
			if (atomic_read(&lc->lc_total) >= lc->lc_max) {	\
				__ret = i;				\
				break;					\
			}						\
			ptr = TRY_PSCALLOC(sizeof(type));		\
			if (ptr == NULL) {				\
				if (!i)					\
					__ret = -ENOMEM;		\
				break;					\
			}						\
			init_fn(ptr);					\
			atomic_inc(&lc->lc_total);			\
		}							\
	} while(0)

#define lc_grow(lc, n, type, init_fn)					\
({									\
	int __ret;							\
									\
	_lc_grow(lc, n, type, init_fn, __ret);				\
	__ret;								\
})

#define _lc_shrink(l, n, type, free_fn, __ret)				\
	do {								\
		int     i;						\
		type   *ptr;						\
		list_cache_t *lc = l;					\
		ssize_t z = lc_sz(lc);					\
		struct psclist_head *e;					\
									\
		__ret = 0;						\
		if (z < lc->lc_min) {					\
			zwarn("Cache %s it underfilled", lc->lc_name);	\
			break;						\
		}							\
		if (z == lc->lc_min)					\
			break;						\
		if ((z - n) > lc->lc_min)				\
			n = z - lc->lc_min;				\
		for (i=0; i < n; i++, __ret++) {			\
			if (atomic_read(&lc->lc_total) <= lc->lc_min)	\
				break;					\
			e = lc_get((lc), 0);				\
			ptr = psclist_entry(e, type, lc_list);		\
			psc_assert((ptr));				\
			free_fn((ptr));					\
			atomic_dec(&lc->lc_total);			\
		}							\
	} while(0)

#define lc_shrink(lc, n, type, free_fn)					\
	({								\
		int                 __ret;				\
									\
		_lc_shrink(lc, n, type, free_fn, __ret);		\
		__ret;							\
	})

/**
 * lc_sort - sort items in a list cache.
 * @lc: list cache to sort.
 * @sortf: sort routine, such as qsort(3) or mergesort(3).
 * @cmpf: comparision routine passed as argument to sortf().
 */
static inline void
lc_sort(list_cache_t *lc,
	 void (*sortf)(void *, size_t, size_t, int (*)(const void *, const void *)),
	 int (*cmpf)(const void *, const void *))
{
	void **p, *next, *prev;
	struct psclist_head *e;
	int locked;
	int j;

	j = 0;
	p = PSCALLOC(lc->lc_size * sizeof(*p));
	locked = reqlock(&lc->lc_lock);
	if (lc->lc_size == 0 || lc->lc_size == 1) {
		ureqlock(&lc->lc_lock, locked);
		return;
	}
	psclist_for_each(e, &lc->lc_list)
		p[j++] = ((char *)e) - lc->lc_offset;
	sortf(p, lc->lc_size, sizeof(*p), cmpf);
	prev = lc->lc_offset + (char *)p[lc->lc_size - 1];
	for (j = 0; j < lc->lc_size; j++, prev = e) {
		e = (void *)(lc->lc_offset + (char *)p[j]);
		if (j < lc->lc_size - 1)
			next = lc->lc_offset + (char *)p[j + 1];
		else
			next = lc->lc_offset + (char *)p[0];
		e->zprev = prev;
		e->znext = next;
	}
	lc->lc_list.znext = (void *)(lc->lc_offset + (char *)p[0]);
	lc->lc_list.zprev = (void *)(lc->lc_offset + (char *)p[lc->lc_size - 1]);
	ureqlock(&lc->lc_lock, locked);
}

#endif /* _PFL_LISTCACHE_H_ */
