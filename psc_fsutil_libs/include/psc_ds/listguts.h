/* $Id$ */

/*
 * List guts comprise internals of mlists and listcaches.
 */

#ifndef _PFL_LISTGUTS_H_
#define _PFL_LISTGUTS_H_

#include <sys/types.h>

#include <stddef.h>

#include "psc_ds/list.h"
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

#define listguts_for_each(p, lg)					\
	psclist_for_each_entry2((p), &(lg)->plg_listhd, (lg)->plg_offset)

static inline int
psclg_size(struct psc_listguts *plg)
{
	int locked, n;

	locked = reqlock(&plg->plg_lock);
	n = plg->plg_size;
	ureqlock(&plg->plg_lock, locked);
	return (n);
}

#endif /* _PFL_LISTGUTS_H_ */
