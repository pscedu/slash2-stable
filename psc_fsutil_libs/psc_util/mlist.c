/* $Id$ */

/*
 * Multilockable lists are essentially multilock-aware list caches.
 */

#include <stdarg.h>

#include "psc_ds/list.h"
#include "psc_ds/lockedlist.h"
#include "psc_util/lock.h"
#include "psc_util/log.h"
#include "psc_util/mlist.h"
#include "psc_util/multilock.h"

struct psc_lockedlist psc_mlists =
    PLL_INITIALIZER(&psc_mlists, struct psc_mlist, pml_index_lentry);

/**
 * psc_mlist_size - determine number of elements on an mlist.
 * @pml: the multilockable list to inspect.
 */
int
psc_mlist_size(struct psc_mlist *pml)
{
	int locked, size;

	locked = reqlock(&pml->pml_lock);
	size = pml->pml_size;
	ureqlock(&pml->pml_lock, locked);
	return (size);
}

/**
 * psc_mlist_tryget - get an item from a multilockable list.
 * @pml: the multilockable list to access.
 * Note: returns an item locked.
 */
void *
psc_mlist_tryget(struct psc_mlist *pml)
{
	struct psclist_head *e;
	int locked;
	void *p;

	locked = reqlock(&pml->pml_lock);
	if (psclist_empty(&pml->pml_listhd)) {
		ureqlock(&pml->pml_lock, locked);
		return (NULL);
	}
	e = psclist_first(&pml->pml_listhd);
	psclist_del(e);
	psc_assert(pml->pml_size-- > 0);
	p = (char *)e - pml->pml_offset;
	ureqlock(&pml->pml_lock, locked);
	return (p);
}

/**
 * psc_mlist_add - put an item on a multilockable list.
 * @pml: the multilockable list to access.
 * @p: item to return.
 */
void
psc_mlist_add(struct psc_mlist *pml, void *p)
{
	int locked;

	psc_assert(p);
	locked = reqlock(&pml->pml_lock);
	psclist_xadd_tail((struct psclist_head *)((char *)p +
	    pml->pml_offset), &pml->pml_listhd);
	pml->pml_size++;
	pml->pml_nseen++;
	multilock_cond_wakeup(&pml->pml_mlcond_empty);
	ureqlock(&pml->pml_lock, locked);
}

/**
 * psc_mlist_remove - remove an item's membership from a multilockable list.
 * @pml: the multilockable list to access.
 * @p: item to unlink.
 */
void
psc_mlist_remove(struct psc_mlist *pml, void *p)
{
	int locked;

	psc_assert(p);
	locked = reqlock(&pml->pml_lock);
	psclist_del((struct psclist_head *)((char *)p +
	    pml->pml_offset));
	psc_assert(pml->pml_size-- > 0);
	ureqlock(&pml->pml_lock, locked);
}

/**
 * _psc_mlist_initv - initialize a multilockable list.
 * @pml: mlist to initialize.
 * @flags: multilock condition flags.
 * @arg: multilock condition to use for availability notification.
 * @entsize: size of an entry on this mlist.
 * @offset: offset into entry for the psclist_head for list mgt.
 * @fmt: printf(3) format for mlist name.
 * @ap: va_list of arguments to mlist name printf(3) format.
 */
void
_psc_mlist_initv(struct psc_mlist *pml, int flags, void *arg,
    size_t entsize, ptrdiff_t offset, const char *fmt, va_list ap)
{
	int rc;

	memset(pml, 0, sizeof(*pml));
	INIT_PSCLIST_HEAD(&pml->pml_listhd);
	LOCK_INIT(&pml->pml_lock);
	pml->pml_entsize = entsize;
	pml->pml_offset = offset;

	rc = vsnprintf(pml->pml_name, sizeof(pml->pml_name), fmt, ap);
	if (rc == -1)
		psc_fatal("vsnprintf");
	else if (rc >= (int)sizeof(pml->pml_name))
		psc_fatalx("mlist name is too long: %s", fmt);

	multilock_cond_init(&pml->pml_mlcond_empty, arg, flags,
	    "%s-empty", pml->pml_name);
}

/**
 * _psc_mlist_init - initialize a multilockable list.
 * @pml: mlist to initialize.
 * @arg: multilock condition to use for availability notification.
 * @entsize: size of an entry on this mlist.
 * @offset: offset into entry for the psclist_head for list mgt.
 * @fmt: printf(3) format for mlist name.
 */
void
_psc_mlist_init(struct psc_mlist *pml, int flags, void *arg,
    size_t entsize, ptrdiff_t offset, const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	_psc_mlist_initv(pml, flags, arg, entsize, offset, fmt, ap);
	va_end(ap);
}

/**
 * _psc_mlist_reginit - initialize a multilockable list.
 * @pml: mlist to initialize.
 * @arg: multilock condition to use for availability notification.
 * @entsize: size of an entry on this mlist.
 * @offset: offset into entry for the psclist_head for list mgt.
 * @fmt: printf(3) format for mlist name.
 */
void
psc_mlist_register(struct psc_mlist *pml)
{
	PLL_LOCK(&psc_mlists);
	spinlock(&pml->pml_lock);
	pll_addtail(&psc_mlists, pml);
	freelock(&pml->pml_lock);
	PLL_ULOCK(&psc_mlists);
}

/**
 * _psc_mlist_reginit - initialize a multilockable list.
 * @pml: mlist to initialize.
 * @arg: multilock condition to use for availability notification.
 * @entsize: size of an entry on this mlist.
 * @offset: offset into entry for the psclist_head for list mgt.
 * @fmt: printf(3) format for mlist name.
 */
void
_psc_mlist_reginit(struct psc_mlist *pml, int flags, void *arg,
    size_t entsize, ptrdiff_t offset, const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	_psc_mlist_initv(pml, flags, arg, entsize, offset, fmt, ap);
	va_end(ap);

	psc_mlist_register(pml);
}
