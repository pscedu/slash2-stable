/* $Id$ */

#include <string.h>

#include "psc_types.h"
#include "psc_ds/tree.h"
#include "psc_mount/dhfh.h"
#include "psc_util/alloc.h"
#include "psc_util/lock.h"
#include "psc_util/random.h"

int
fhcmp(const void *a, const void *b)
{
	const struct fhent *ta = a, *tb = b;

	if (ta->fh_id < tb->fh_id)
		return (-1);
	else if (ta->fh_id > tb->fh_id)
		return (1);
	return (0);
}

SPLAY_HEAD(fhtree, fhent);
SPLAY_PROTOTYPE(fhtree, fhent, fh_entry, fhcmp);
SPLAY_GENERATE(fhtree, fhent, fh_entry, fhcmp);

struct fhtree  fhtree;
psc_spinlock_t fhtreelock = LOCK_INITIALIZER;

struct fhent *
_fh_lookup(u64 fhid, int rm)
{
	struct fhent *t, tq;

	tq.fh_id = fhid;
	spinlock(&fhtreelock);
	t = SPLAY_FIND(fhtree, &fhtree, &tq);
	if (t && rm)
		if (SPLAY_REMOVE(fhtree, &fhtree, t) == NULL)
			psc_fatalx("unable to remove element");
	freelock(&fhtreelock);
	return (t);
}

/**
 * fh_register - place the 64-bit 'file handle' into the splay tree.
 * @fh: file handle
 * @fh_regcb: callback
 */
void
fh_register(u64 fhid, int oflag,
	    void (*fh_regcb)(struct fhent *, int, void **), void *cb_args[])
{
	struct fhent *t, fh;
	int    op = FD_REG_EXIST;

	psc_assert((oflag & FHENT_WRITE) || 
		   (oflag & FHENT_READ));

	fh.fh_id = fhid;
	spinlock(&fhtreelock);
	t = SPLAY_FIND(fhtree, &fhtree, &fh);
	if (t == NULL) {
		op = FD_REG_NEW;
		t = PSCALLOC(sizeof(*t));
		LOCK_INIT(&t->fh_lock);
		spinlock(&t->fh_lock);
		t->fh_pri = NULL;
		t->fh_id = fhid;
		/* Inform the cache of our status, this allows us to
		 *   init without holding the fhtreelock.
		 */
		t->fh_state = FHENT_INIT | oflag;
		if (SPLAY_INSERT(fhtree, &fhtree, t))
			psc_fatalx("Attempted to reinsert fd %"_P_U64"x", 
				   t->fh_id);
	} else {
		if (!(oflag & t->fh_state)) {
			/* The fd needs to change state.
			 */
			psc_debug("Fhent (%p) fd %"_P_U64"x: adding state : "
				  "from (%d) to (%d)",
				  t, t->fh_id, t->fh_state, 
				  (t->fh_state | oflag));		
			t->fh_state |= oflag;
		}
	}
	freelock(&fhtreelock);

	if (fh_regcb) {
		/* Callback into the fd handler */
		(*fh_regcb)(t, op, cb_args);
		freelock(&t->fh_lock);
	}
}

int
fh_reap(void)
{
	/*
	 * Not sure what to do here just yet.
	 * We need to save FIDs after name lookup
	 * and issue requests to open by FID later.
	 */
	return (0);
}
