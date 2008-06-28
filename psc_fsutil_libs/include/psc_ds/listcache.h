/* $Id$ */

#ifndef __PFL_LISTCACHE_H__
#define __PFL_LISTCACHE_H__

#include <sys/types.h>

#include <errno.h>
#include <stdarg.h>
#include <stddef.h>
#include <string.h>
#include <time.h>

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

/* List cache which can be edited by multiple threads.  */
struct list_cache {
	struct psclist_head	lc_index_lentry;	/* link between caches       */
	char			lc_name[LC_NAME_MAX];	/* for lc mgmt               */

	ssize_t			lc_max;			/* max allowable entries     */
	ssize_t			lc_min;			/* keep at least this many   */
	ssize_t			lc_size;		/* current #items in list    */
	atomic_t		lc_total;		/* relative max              */

	size_t			lc_nseen;		/* total #items placed on us */

	size_t			lc_entsize;		/* size of entry on us       */
	size_t			lc_offset;		/* offset to entry member    */

	struct psclist_head	lc_list;		/* head/tail of list         */
	psc_spinlock_t		lc_lock;		/* exclusitivity ctl         */
	psc_waitq_t		lc_wq_want;		/* when someone wants an ent */
	psc_waitq_t		lc_wq_empty;		/* when we're empty          */
	psc_waitq_t		lc_wq_full;		/* when we're full           */
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
	psc_waitq_wakeup(&l->lc_wq_full);
}

static inline struct psclist_head *
_lc_gettimed(list_cache_t *l, struct timespec *abstime)
{
	struct psclist_head *e;
	int locked, rc;

	locked = reqlock(&l->lc_lock);
	if (0)
 start:
		reqlock(&l->lc_lock);

	if (psclist_empty(&l->lc_list)) {
		psc_notify("Timed wait on listcache %p : '%s'",
			   l, l->lc_name);
		rc = psc_waitq_timedwait(&l->lc_wq_empty,
		    &l->lc_lock, abstime);
		if (rc) {
			psc_assert(rc == ETIMEDOUT);
			errno = rc;
			return (NULL);
		}
		goto start;
	}
	e = psclist_first(&l->lc_list);
	lc_del(e, l);
	ureqlock(&l->lc_lock, locked);
	return (e);
}

static inline struct psclist_head *
lc_get(list_cache_t *l, int block)
{
	struct psclist_head *e;
	int locked;

	locked = reqlock(&l->lc_lock);
	if (0)
 start:
		reqlock(&l->lc_lock);

	if (psclist_empty(&l->lc_list)) {
		if (block) {
			/* Wait until the list is no longer empty. */
			if (block > 1)
				psc_warnx("Watiing on listcache %p : '%s'",
					  l, l->lc_name);
			else
				psc_notify("Waiting on listcache %p : '%s'",
					   l, l->lc_name);
			psc_waitq_wakeup(&l->lc_wq_want);
			psc_waitq_wait(&l->lc_wq_empty, &l->lc_lock);
			goto start;
		} else {
			ureqlock(&l->lc_lock, locked);
			return NULL;
		}
	}
	e = psclist_first(&l->lc_list);
	lc_del(e, l);
	ureqlock(&l->lc_lock, locked);
	return (e);
}

/**
 * lc_get - grab the item from the head of a listcache.
 * @l: the list cache to access
 * @block: should the get wait
 */
#define lc_getwait(l)		(void *)(((char *)lc_get((l), 1)) - (l)->lc_offset)

static inline void *
lc_getnb(list_cache_t *l)
{
	void *p = lc_get(l, 0);

	return (p ? (char *)p - l->lc_offset : NULL);
}

/**
 * lc_gettimed - try to grab an item from the head of a listcache.
 * @l: the list cache to access.
 * @abstime: timer which tells how long to wait.
 */
static inline void *
lc_gettimed(list_cache_t *l, struct timespec *abstime)
{
        void *p = _lc_gettimed(l, abstime);

        return (p ? (char *)p - l->lc_offset : NULL);
}

/**
 * lc_put - Bounded list put
 * @l: the list cache to access
 * @e: new list item
 */
static inline void
_lc_put(list_cache_t *l, struct psclist_head *e, int tails)
{
	int locked;

	psc_assert(e->znext == NULL);
	psc_assert(e->zprev == NULL);

	locked = reqlock(&l->lc_lock);
	if (0)
 start:
		reqlock(&l->lc_lock);

	if (l->lc_max > 0 && l->lc_size >= l->lc_max) {
		psc_waitq_wait(&l->lc_wq_full, &l->lc_lock);
		goto start;
	}

	if (tails)
		psclist_xadd_tail(e, &l->lc_list);
	else
		psclist_xadd(e, &l->lc_list);

	l->lc_size++;
	l->lc_nseen++;

	ureqlock(&l->lc_lock, locked);

	/*
	 * There is now an item available; wake up waiters
	 * who think the list is empty.
	 */
	psc_waitq_wakeup(&l->lc_wq_empty);
}

static inline void
_lc_add(list_cache_t *lc, void *p, int tails)
{
	void *e;

	psc_assert(p);
	e = (char *)p + lc->lc_offset;
	_lc_put(lc, e, tails);
}

#define lc_queue(l, e)		_lc_put(l, e, 1)
#define lc_stack(l, e)		_lc_put(l, e, 0)
#define lc_puthead(l, e)	_lc_put(l, e, 0)
#define lc_puttail(l, e)	_lc_put(l, e, 1)
#define lc_put(l, e)		_lc_put(l, e, 1)

#define lc_addstack(l, p)	_lc_add(l, p, 0)
#define lc_addqueue(l, p)	_lc_add(l, p, 1)
#define lc_addhead(l, p)	_lc_add(l, p, 0)
#define lc_addtail(l, p)	_lc_add(l, p, 1)
#define lc_add(l, p)		_lc_add(l, p, 1)

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
	psclist_xadd_tail(n, &l->lc_list);
	ureqlock(&l->lc_lock, locked);
}

static inline void
_lc_init(list_cache_t *lc, ptrdiff_t offset, size_t entsize)
{
	lc->lc_size  = 0;
	lc->lc_max   = -1;
	lc->lc_entsize = entsize;
	lc->lc_offset = offset;
	atomic_set(&lc->lc_total, 0);
	INIT_PSCLIST_HEAD(&lc->lc_list);
	INIT_PSCLIST_ENTRY(&lc->lc_index_lentry);
	LOCK_INIT(&lc->lc_lock);
	psc_waitq_init(&lc->lc_wq_empty);
	psc_waitq_init(&lc->lc_wq_full);
	psc_waitq_init(&lc->lc_wq_want);
}

/**
 * lc_init - initialize a list cache.
 * @lc: the list cache to initialize.
 * @type: type of variable the list will contain.
 * @member: member name in type linking entries together.
 */
#define lc_init(lc, type, member) \
	_lc_init((lc), offsetof(type, member), sizeof(type))

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

	spinlock(&pscListCachesLock);
	locked = reqlock(&lc->lc_lock);

	rc = vsnprintf(lc->lc_name, sizeof(lc->lc_name), name, ap);
	if (rc == -1)
		psc_fatal("vsnprintf");
	else if (rc > (int)sizeof(lc->lc_name))
		psc_fatalx("lc_name is too large (%s)", name);

	psclist_xadd(&lc->lc_index_lentry, &pscListCaches);

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

static inline void
_lc_reginit(list_cache_t *lc, ptrdiff_t offset, size_t entsize,
    const char *name, ...)
{
	va_list ap;

	_lc_init(lc, offset, entsize);

	va_start(ap, name);
	lc_vregister(lc, name, ap);
	va_end(ap);
}

/**
 * lc_reginit - initialize and register a list cache.
 * @lc: the list cache.
 * @type: type of variable the list will contain.
 * @member: member name in type linking entries together.
 * @fmt: printf(3) format of name for list.
 */
#define lc_reginit(lc, type, member, fmt, ...) \
	_lc_reginit(lc, offsetof(type, member), sizeof(type), fmt, ## __VA_ARGS__)

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
	list_cache_t *lc;

	lc = NULL;
	spinlock(&pscListCachesLock);
	psclist_for_each_entry(lc, &pscListCaches, lc_index_lentry)
		if (strcmp(name, lc->lc_name) == 0) {
			LIST_CACHE_LOCK(lc);
			break;
		}
	freelock(&pscListCachesLock);
	return (lc);
}

/**
 * lc_empty - determine if the list cache has elements currently.
 * @lc: list cache to check.
 */
#define lc_empty(lc) psclist_empty(&(lc)->lc_list)

static inline int
lc_grow(list_cache_t *lc, ssize_t n, void (*initf)(void *))
{
	ssize_t z = lc_sz(lc);
	void *p;
	int i;

	if (z > lc->lc_max) {
		psc_warnx("Cache %s has overgrown", lc->lc_name);
		return (0);
	}
	if (z == lc->lc_max)
		return (0);
	if (z + n > lc->lc_max)
		n = lc->lc_max - z;

	for (i = 0; i < n; i++) {
		if (atomic_read(&lc->lc_total) >= lc->lc_max)
			return (i);
		p = TRY_PSCALLOC(lc->lc_entsize);
		if (p == NULL) {
			errno = ENOMEM;
			return (-1);
		}
		if (initf)
			initf(p);
		INIT_PSCLIST_ENTRY((struct psclist_head *)
		    (lc->lc_offset + (char *)p));
		lc_put(lc, (struct psclist_head *)
		    (lc->lc_offset + (char *)p));
		atomic_inc(&lc->lc_total);
	}
	return (i);
}

static inline int
lc_shrink(list_cache_t *lc, ssize_t n, void (*freef)(void *))
{
	ssize_t z = lc_sz(lc);
	void *p;
	int i;

	if (z < lc->lc_min) {
		psc_warnx("Cache %s is underfilled", lc->lc_name);
		return (0);
	}
	if (z == lc->lc_min)
		return (0);
	if (z - n > lc->lc_min)
		n = z - lc->lc_min;

	for (i = 0; i < n; i++) {
		if (atomic_read(&lc->lc_total) <= lc->lc_min)
			break;
		p = lc_getnb(lc);
		psc_assert(p);
		freef(p);
		atomic_dec(&lc->lc_total);
	}
	return (i);
}

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
	int j, locked;

	j = 0;
	locked = reqlock(&lc->lc_lock);
	if (lc->lc_size == 0 || lc->lc_size == 1) {
		ureqlock(&lc->lc_lock, locked);
		return;
	}
	p = PSCALLOC(lc->lc_size * sizeof(*p));
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

#endif /* __PFL_LISTCACHE_H__ */
