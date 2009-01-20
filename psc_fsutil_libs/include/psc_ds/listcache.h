/* $Id$ */

#ifndef _PFL_LISTCACHE_H_
#define _PFL_LISTCACHE_H_

#include <sys/types.h>

#include <errno.h>
#include <stdarg.h>
#include <stddef.h>
#include <string.h>
#include <time.h>

#include "psc_ds/list.h"
#include "psc_ds/lockedlist.h"
#include "psc_util/lock.h"
#include "psc_util/alloc.h"
#include "psc_util/assert.h"
#include "psc_util/atomic.h"
#include "psc_util/waitq.h"
#include "psc_util/alloc.h"

#define LC_NAME_MAX 32

extern struct psc_lockedlist	pscListCaches;

/* List cache which can be edited by multiple threads. */
struct psc_listcache {
	struct psclist_head	lc_index_lentry;	/* link between caches       */
	char			lc_name[LC_NAME_MAX];	/* for lc mgmt               */
	int			lc_flags;

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

#define PLCF_DYING	(1 << 0)	/* listcache is about to go away */

#define LIST_CACHE_LOCK(l)  spinlock(&(l)->lc_lock)
#define LIST_CACHE_ULOCK(l) freelock(&(l)->lc_lock)

/**
 * lc_sz - how many items are in here.
 * This should use atomics at some point
 */
static inline ssize_t
lc_sz(struct psc_listcache *lc)
{
	int locked;
	ssize_t sz;

	locked = reqlock(&lc->lc_lock);
	sz = lc->lc_size;
	ureqlock(&lc->lc_lock, locked);
	return (sz);
}

static inline void
lc_del(struct psclist_head *e, struct psc_listcache *lc)
{
	int locked;

	locked = reqlock(&lc->lc_lock);
	psclist_del(e);
	lc->lc_size--;
	psc_assert(lc->lc_size >= 0);
	ureqlock(&lc->lc_lock, locked);
}

/**
 * lc_remove - remove an item from a list cache.
 * @lc: the list cache.
 * @p: the item containing a psclist_head member.
 */
static inline void
lc_remove(struct psc_listcache *lc, void *p)
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

/* lc_get() flags */
#define PLCGF_TAIL	(1 << 0)
#define PLCGF_STACK	(1 << 0)
#define PLCGF_HEAD	(1 << 1)
#define PLCGF_QUEUE	(1 << 1)
#define PLCGF_NOBLOCK	(1 << 2)
#define PLCGF_WARN	(1 << 3)
#define PLCGF_PEEK	(1 << 4)

static __inline struct psclist_head *
_lc_get(struct psc_listcache *lc, struct timespec *abstime, int flags)
{
	struct psclist_head *e;
	int locked, rc;

	/* Ensure either head or tail is set. */
	psc_assert((flags & PLCGF_HEAD) ^ (flags & PLCGF_TAIL));

	locked = reqlock(&lc->lc_lock);
	while (psclist_empty(&lc->lc_listhd)) {
		if ((lc->lc_flags & PLCF_DYING) ||
		    (flags & PLCGF_NOBLOCK)) {
			ureqlock(&lc->lc_lock, locked);
			return (NULL);
		}

		/* Alert listeners who want to know about exhaustion. */
		psc_waitq_wakeall(&lc->lc_wq_want);
		psc_logx(flags & PLCGF_WARN ? PLL_WARN : PLL_NOTICE,
		    "lc_get(%s:%p): waiting %p", lc->lc_name, lc, abstime);
		if (abstime) {
			rc = psc_waitq_timedwait(&lc->lc_wq_empty,
			    &lc->lc_lock, abstime);
			/* XXX subtract from abstime */
			if (rc) {
				psc_assert(rc == ETIMEDOUT);
				errno = rc;
				return (NULL);
			}
		} else
			psc_waitq_wait(&lc->lc_wq_empty, &lc->lc_lock);
		spinlock(&lc->lc_lock);
	}
	e = flags & PLCGF_HEAD ?
	    psclist_first(&lc->lc_listhd) :
	    psclist_last(&lc->lc_listhd);
	if ((flags & PLCGF_PEEK) == 0) {
		psclist_del(e);
		lc->lc_size--;
	}
	ureqlock(&lc->lc_lock, locked);
	return (e);
}

/**
 * lc_gettimed - try to grab an item from the head of a listcache.
 * @l: the list cache to access.
 * @abstime: timer which tells how long to wait.
 */
static inline void *
lc_gettimed(struct psc_listcache *lc, struct timespec *abstime)
{
	void *p = _lc_get(lc, abstime, PLCGF_HEAD);

	return (p ? (char *)p - lc->lc_offset : NULL);
}

/**
 * lc_getwait - grab an item from a listcache,
 *	blocking if necessary until an item becomes available.
 * @lc: the list cache to access.
 */
static inline void *
lc_getwait(struct psc_listcache *lc)
{
	void *p = _lc_get(lc, NULL, PLCGF_HEAD);

	return (p ? (char *)p - lc->lc_offset : NULL);
}

/**
 * lc_getnb - grab an item from a listcache or NULL if none are available.
 * @lc: the list cache to access.
 */
static inline void *
lc_getnb(struct psc_listcache *lc)
{
	void *p = _lc_get(lc, NULL, PLCGF_HEAD | PLCGF_NOBLOCK);

	return (p ? (char *)p - lc->lc_offset : NULL);
}

/**
 * lc_gettailnb - grab tail item or NULL if unavailable.
 * @lc: the list cache to access.
 */
static inline void *
lc_peektail(struct psc_listcache *lc)
{
	void *p = _lc_get(lc, NULL, PLCGF_TAIL | PLCGF_NOBLOCK | PLCGF_PEEK);

	return (p ? (char *)p - lc->lc_offset : NULL);
}

/*
 * lc_kill - list wants to go away, notify waitors.
 * @lc: list cache to kill.
 */
static inline void
lc_kill(struct psc_listcache *lc)
{
	spinlock(&lc->lc_lock);
	lc->lc_flags |= PLCF_DYING;
	psc_waitq_wakeall(&lc->lc_wq_empty);
	freelock(&lc->lc_lock);
}

/**
 * lc_put - Bounded list put
 * @l: the list cache to access
 * @e: new list item
 */
static inline void
_lc_put(struct psc_listcache *lc, struct psclist_head *e, int tails)
{
	int locked;

	psc_assert(e->znext == NULL);
	psc_assert(e->zprev == NULL);

	locked = reqlock(&lc->lc_lock);

	if (tails)
		psclist_xadd_tail(e, &lc->lc_listhd);
	else
		psclist_xadd(e, &lc->lc_listhd);

	lc->lc_size++;
	lc->lc_nseen++;

	ureqlock(&lc->lc_lock, locked);

	/*
	 * There is now an item available; wake up waiters
	 * who think the list is empty.
	 */
	psc_waitq_wakeall(&lc->lc_wq_empty);
}

static inline void
_lc_add(struct psc_listcache *lc, void *p, int tails)
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
lc_requeue(struct psc_listcache *lc, struct psclist_head *n)
{
	int locked;

	locked = reqlock(&lc->lc_lock);
	psclist_del(n);
	psclist_xadd_tail(n, &lc->lc_listhd);
	ureqlock(&lc->lc_lock, locked);
}

static inline void
_lc_init(struct psc_listcache *lc, ptrdiff_t offset, size_t entsize)
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
lc_vregister(struct psc_listcache *lc, const char *name, va_list ap)
{
	int rc;

	PLL_LOCK(&pscListCaches);
	spinlock(&lc->lc_lock);

	rc = vsnprintf(lc->lc_name, sizeof(lc->lc_name), name, ap);
	if (rc == -1)
		psc_fatal("vsnprintf");
	else if (rc > (int)sizeof(lc->lc_name))
		psc_fatalx("lc_name is too large (%s)", name);

	pll_add(&pscListCaches, lc);

	freelock(&lc->lc_lock);
	PLL_ULOCK(&pscListCaches);
}

/**
 * lc_register - register a list cache for external access.
 * @lc: the list cache to register.
 * @name: printf(3) format of name for list.
 */
static inline void
lc_register(struct psc_listcache *lc, const char *name, ...)
{
	va_list ap;

	va_start(ap, name);
	lc_vregister(lc, name, ap);
	va_end(ap);
}

static inline void
_lc_reginit(struct psc_listcache *lc, ptrdiff_t offset, size_t entsize,
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
lc_unregister(struct psc_listcache *lc)
{
	PLL_LOCK(&pscListCaches);
	spinlock(&lc->lc_lock);
	pll_remove(&pscListCaches, lc);
	freelock(&lc->lc_lock);
	PLL_ULOCK(&pscListCaches);
}

/**
 * lc_lookup - find a list cache by its registration name.
 * @name: name of list cache.
 * Notes: returns the list cache locked if found.
 */
static inline struct psc_listcache *
lc_lookup(const char *name)
{
	struct psc_listcache *lc;

	PLL_LOCK(&pscListCaches);
	psclist_for_each_entry(lc,
	    &pscListCaches.pll_listhd, lc_index_lentry)
		if (strcmp(name, lc->lc_name) == 0) {
			LIST_CACHE_LOCK(lc);
			break;
		}
	PLL_ULOCK(&pscListCaches);
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
lc_sort(struct psc_listcache *lc,
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

#endif /* _PFL_LISTCACHE_H_ */
