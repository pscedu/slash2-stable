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

/* List cache which can be edited by multiple threads. */
struct psc_listcache {
	struct psclist_head	lc_index_lentry;	/* link between caches       */
	char			lc_name[LC_NAME_MAX];	/* for lc mgmt               */

	ssize_t			lc_size;		/* current #items in list    */
	size_t			lc_nseen;		/* stat: total #times put()  */
	size_t			lc_entsize;		/* size of entry on us       */
	ptrdiff_t		lc_offset;		/* offset to entry member    */

	struct psclist_head	lc_listhd;		/* head/tail of list         */
	psc_spinlock_t		lc_lock;		/* exclusitivity ctl         */
	psc_waitq_t		lc_wq_want;		/* when someone wants an ent */
	psc_waitq_t		lc_wq_empty;		/* when we're empty          */
};
typedef struct psc_listcache list_cache_t;

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
	return (sz);
}

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
}

/**
 * lc_remove - remove an item from a listcache.
 * @l: the listcache.
 * @p: the item containing a psclist_head member.
 */
static inline void
lc_remove(list_cache_t *lc, void *p)
{
	int locked;
	void *e;

	psc_assert(p);
	e = (char *)p + lc->lc_offset;
	locked = reqlock(&lc->lc_lock);
	psclist_del(e);
	lc->lc_size--;
	psc_assert(lc->lc_size >= 0);
	ureqlock(&lc->lc_lock, locked);
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

	if (psclist_empty(&l->lc_listhd)) {
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
	e = psclist_first(&l->lc_listhd);
	lc_del(e, l);
	ureqlock(&l->lc_lock, locked);
	return (e);
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

static inline struct psclist_head *
lc_get(list_cache_t *l, int block)
{
	struct psclist_head *e;
	int locked;

	locked = reqlock(&l->lc_lock);
	if (0)
 start:
		reqlock(&l->lc_lock);

	if (psclist_empty(&l->lc_listhd)) {
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
	e = psclist_first(&l->lc_listhd);
	lc_del(e, l);
	ureqlock(&l->lc_lock, locked);
	return (e);
}

/**
 * lc_getwait - grab an item from a listcache,
 *	blocking if necessary until an item becomes available.
 * @lc: the list cache to access.
 */
#define lc_getwait(lc)		(void *)(((char *)lc_get((lc), 1)) - (lc)->lc_offset)

/**
 * lc_getnb - grab an item from a listcache or NULL if none are available.
 * @lc: the list cache to access.
 */
static inline void *
lc_getnb(list_cache_t *lc)
{
	void *p = lc_get(lc, 0);

	return (p ? (char *)p - lc->lc_offset : NULL);
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

	if (tails)
		psclist_xadd_tail(e, &l->lc_listhd);
	else
		psclist_xadd(e, &l->lc_listhd);

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

#define lc_queue(lc, e)		_lc_put((lc), (e), 1)
#define lc_stack(lc, e)		_lc_put((lc), (e), 0)
#define lc_puthead(lc, e)	_lc_put((lc), (e), 0)
#define lc_puttail(lc, e)	_lc_put((lc), (e), 1)
#define lc_put(lc, e)		_lc_put((lc), (e), 1)

#define lc_addstack(lc, p)	_lc_add((lc), (p), 0)
#define lc_addqueue(lc, p)	_lc_add((lc), (p), 1)
#define lc_addhead(lc, p)	_lc_add((lc), (p), 0)
#define lc_addtail(lc, p)	_lc_add((lc), (p), 1)
#define lc_add(lc, p)		_lc_add((lc), (p), 1)

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
	psclist_xadd_tail(n, &l->lc_listhd);
	ureqlock(&l->lc_lock, locked);
}

static inline void
_lc_init(list_cache_t *lc, ptrdiff_t offset, size_t entsize)
{
	lc->lc_size = 0;
	lc->lc_nseen = 0;
	lc->lc_entsize = entsize;
	lc->lc_offset = offset;
	INIT_PSCLIST_HEAD(&lc->lc_listhd);
	INIT_PSCLIST_ENTRY(&lc->lc_index_lentry);
	LOCK_INIT(&lc->lc_lock);
	psc_waitq_init(&lc->lc_wq_empty);
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
	_lc_reginit((lc), offsetof(type, member), sizeof(type), (fmt), ## __VA_ARGS__)

/**
 * lc_unregister - remove list cache external access registration.
 * @lc: the list cache to unregister, must be UNLOCKED.
 */
static inline void
lc_unregister(list_cache_t *lc)
{
	spinlock(&pscListCachesLock);
	spinlock(&lc->lc_lock);
	psclist_del(&lc->lc_index_lentry);
	freelock(&lc->lc_lock);
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
#define lc_empty(lc) psclist_empty(&(lc)->lc_listhd)

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
	psclist_for_each(e, &lc->lc_listhd)
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
	lc->lc_listhd.znext = (void *)(lc->lc_offset + (char *)p[0]);
	lc->lc_listhd.zprev = (void *)(lc->lc_offset + (char *)p[lc->lc_size - 1]);
	ureqlock(&lc->lc_lock, locked);
}

#endif /* __PFL_LISTCACHE_H__ */
