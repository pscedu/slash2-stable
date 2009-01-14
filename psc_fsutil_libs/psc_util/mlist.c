/* $Id$ */

/*
 * Multilockable lists are essentially multilock-aware list caches.
 */

#include "psc_ds/list.h"
#include "psc_ds/lockedlist.h"
#include "psc_util/assert.h"
#include "psc_util/lock.h"
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
	size = psclist_empty(&pml->pml_listhd);
	ureqlock(&pml->pml_lock, locked);
	return (size);
}

/**
 * psc_mlist_get - get an item from a multilockable list.
 * @pml: the multilockable list to access.
 */
struct psclist_head *
psc_mlist_tryget(struct psc_mlist *pml)
{
	struct psclist_head *e;
	int locked;

	locked = reqlock(&pml->pml_lock);
	if (psclist_empty(&pml->pml_listhd)) {
		ureqlock(&pml->pml_lock, locked);
		return (NULL);
	}
	e = psclist_first(&pml->pml_listhd);
	psclist_del(e);
	psc_assert(pml->pml_size-- > 0);
	ureqlock(&pml->pml_lock, locked);
	return (e);
}

/**
 * psc_mlist_put - put an item on a multilockable list.
 * @pml: the multilockable list to access.
 * @n: new list item.
 */
void
psc_mlist_put(struct psc_mlist *pml, struct psclist_head *n)
{
	int locked;

	locked = reqlock(&pml->pml_lock);
	psclist_xadd_tail(n, &pml->pml_listhd);
	pml->pml_size++;
	pml->pml_nseen++;
	ureqlock(&pml->pml_lock, locked);

	multilock_cond_wakeup(&pml->pml_mlcond_empty);
}

/**
 * psc_mlist_del - remove an item from a multilockable list.
 * @pml: the multilockable list to access.
 * @n: item to unlink.
 */
void
psc_mlist_del(struct psc_mlist *pml, struct psclist_head *n)
{
	int locked;

	locked = reqlock(&pml->pml_lock);
	psclist_del(n);
	psc_assert(pml->pml_size-- > 0);
	ureqlock(&pml->pml_lock, locked);
}

/**
 * psc_mlist_init - initialize a multilockable list.
 * @pml: mlist to initialize.
 * @fmt: printf(3) format for mlist name.
 */
void
psc_mlist_init(struct psc_mlist *pml, void *arg, const char *fmt, ...)
{
	va_list ap;
	int rc;

	memset(pml, 0, sizeof(*pml));
	INIT_PSCLIST_HEAD(&pml->pml_listhd);
	LOCK_INIT(&pml->pml_lock);

	va_start(ap, fmt);
	rc = vsnprintf(pml->pml_name, sizeof(pml->pml_name), fmt, ap);
	va_end(ap);

	if (rc == -1)
		psc_fatal("vsnprintf");
	else if (rc > (int)sizeof(pml->pml_name))
		psc_fatalx("mlist name is too long: %s", fmt);

	multilock_cond_init(&pml->pml_mlcond_empty, arg, 0,
	    "%s-empty", pml->pml_name);

	pll_add(&psc_mlists, pml);
}
