/* $Id$ */

/*
 * mlists are psclist_caches that can interface with multiwaits.
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
 * psc_mlist_size - Determine number of elements on an mlist.
 * @pml: the mlist to inspect.
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
 * psc_mlist_tryget - Get an item from an mlist.
 * @pml: the mlist to access.
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
 * psc_mlist_add - Put an item on an mlist.
 * @pml: the mlist to access.
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
	psc_multiwaitcond_wakeup(&pml->pml_mwcond_empty);
	ureqlock(&pml->pml_lock, locked);
}

/**
 * psc_mlist_remove - Remove an item's membership from a mlist.
 * @pml: the mlist to access.
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
 * _psc_mlist_initv - Initialize an mlist.
 * @pml: mlist to initialize.
 * @flags: multiwaitcond flags.
 * @mwcarg: multiwaitcond to use for availability notification.
 * @entsize: size of an entry on this mlist.
 * @offset: offset into entry for the psclist_head for list mgt.
 * @fmt: printf(3) format for mlist name.
 * @ap: va_list of arguments to mlist name printf(3) format.
 */
void
_psc_mlist_initv(struct psc_mlist *pml, int flags, void *mwcarg,
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

	psc_multiwaitcond_init(&pml->pml_mwcond_empty, mwcarg, flags,
	    "%s-empty", pml->pml_name);
}

/**
 * _psc_mlist_init - Initialize an mlist.
 * @pml: mlist to initialize.
 * @mwcarg: multiwaitcond to use for availability notification.
 * @entsize: size of an entry on this mlist.
 * @offset: offset into entry for the psclist_head for list mgt.
 * @fmt: printf(3) format for mlist name.
 */
void
_psc_mlist_init(struct psc_mlist *pml, int flags, void *mwcarg,
    size_t entsize, ptrdiff_t offset, const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	_psc_mlist_initv(pml, flags, mwcarg, entsize, offset, fmt, ap);
	va_end(ap);
}

/**
 * _psc_mlist_reginit - Register an mlist for external control.
 * @pml: mlist to register.
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
 * _psc_mlist_reginit - Initialize and register an mlist.
 * @pml: mlist to initialize.
 * @mwcarg: multiwaitcond to use for availability notification.
 * @entsize: size of an entry on this mlist.
 * @offset: offset into entry for the psclist_head for list mgt.
 * @fmt: printf(3) format for mlist name.
 */
void
_psc_mlist_reginit(struct psc_mlist *pml, int flags, void *mwcarg,
    size_t entsize, ptrdiff_t offset, const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	_psc_mlist_initv(pml, flags, mwcarg, entsize, offset, fmt, ap);
	va_end(ap);

	psc_mlist_register(pml);
}
