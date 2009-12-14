/* $Id$ */

/*
 * List guts comprise internals of mlists and listcaches.
 */

#ifndef _PFL_LISTGUTS_H_
#define _PFL_LISTGUTS_H_

#include <sys/types.h>

#include <stddef.h>

#include "psc_ds/list.h"
#include "psc_util/alloc.h"
#include "psc_util/lock.h"

#define PLG_NAME_MAX 32

/* XXX: use atomics? */
struct psc_listguts {
	struct psclist_head	plg_index_lentry;	/* link adjoining lists */
	psc_spinlock_t		plg_lock;		/* exclusitivity ctl */
	char			plg_name[PLG_NAME_MAX];	/* for list mgt */

	struct psclist_head	plg_listhd;		/* head and tail of list */
	int			plg_size;		/* current #items in list */
	size_t			plg_nseen;		/* stat: total #times put() */
	size_t			plg_entsize;		/* size of entry on us */
	ptrdiff_t		plg_offset;		/* psclist_head entry obj offset */
};

/* XXX add LOCK_ENSURE */
#define LISTGUTS_FOREACH(p, lg)					\
	psclist_for_each_entry2((p), &(lg)->plg_listhd, (lg)->plg_offset)

#define LISTGUTS_FOREACH_BACKWARDS(p, lg)			\
	psclist_for_each_entry2_backwards((p), &(lg)->plg_listhd, (lg)->plg_offset)

#define LISTGUTS_FOREACH_SAFE(p, t, lg)				\
	psclist_for_each_entry2_safe((p), (t), &(lg)->plg_listhd, (lg)->plg_offset)

static __inline void
psclg_init(struct psc_listguts *plg, ptrdiff_t offset, size_t entsize)
{
	plg->plg_size = 0;
	plg->plg_nseen = 0;
	plg->plg_entsize = entsize;
	plg->plg_offset = offset;
	INIT_PSCLIST_HEAD(&plg->plg_listhd);
	INIT_PSCLIST_ENTRY(&plg->plg_index_lentry);
	LOCK_INIT(&plg->plg_lock);
}

static __inline int
psclg_size(struct psc_listguts *plg)
{
	int locked, n;

	locked = reqlock(&plg->plg_lock);
	n = plg->plg_size;
	ureqlock(&plg->plg_lock, locked);
	return (n);
}

static __inline int
psclg_conjoint(struct psc_listguts *plg, void *p)
{
	struct psclist_head *e;
	int locked, conjoint;

	psc_assert(p);
	e = (struct psclist_head *)((char *)p + plg->plg_offset);
	locked = reqlock(&plg->plg_lock);
	/* XXX can scan list to ensure membership */
	conjoint = psclist_conjoint(e);
	ureqlock(&plg->plg_lock, locked);
	return (conjoint);
}

/**
 * psclg_add_sorted - Add an item to a list in its sorted position.
 * @lg: list to sort.
 * @sortf: sort routine, such as qsort(3) or mergesort(3).
 * @cmpf: comparison routine passed as argument to sortf().
 */
static __inline void
psclg_add_sorted(struct psc_listguts *plg, void *p,
    int (*cmpf)(const void *, const void *))
{
	int locked;
	void *e;

	psc_assert(p);
	e = (char *)p + plg->plg_offset;

	locked = reqlock(&plg->plg_lock);
	psclist_add_sorted(&plg->plg_listhd, e, cmpf, plg->plg_offset);
	ureqlock(&plg->plg_lock, locked);
}

/**
 * psclg_sort - sort items in a list.
 * @lg: list to sort.
 * @sortf: sort routine, such as qsort(3) or mergesort(3).
 * @cmpf: comparison routine passed as argument to sortf().
 */
static __inline void
psclg_sort(struct psc_listguts *plg, void (*sortf)(void *, size_t,
    size_t, int (*)(const void *, const void *)),
    int (*cmpf)(const void *, const void *))
{
	int locked;
	void *p;

	locked = reqlock(&plg->plg_lock);
	if (plg->plg_size > 1) {
		p = PSCALLOC(plg->plg_size * sizeof(p));
		psclist_sort(p, &plg->plg_listhd, plg->plg_size,
		    plg->plg_offset, sortf, cmpf);
		PSCFREE(p);
	}
	ureqlock(&plg->plg_lock, locked);
}

#endif /* _PFL_LISTGUTS_H_ */
