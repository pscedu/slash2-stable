/* $Id$ */

#include <string.h>

#include "psc_types.h"
#include "psc_ds/tree.h"
#include "psc_util/alloc.h"
#include "psc_util/lock.h"
#include "psc_util/random.h"

#include "psc_mount/dhfh.h"

int
fhcmp(const void *a, const void *b)
{
	const struct fhent *ta = a, *tb = b;

	if (ta->fh < tb->fh)
		return (-1);
	else if (ta->fh > tb->fh)
		return (1);
	return (0);
}

SPLAY_HEAD(fhtree, fhent);
SPLAY_PROTOTYPE(fhtree, fhent, entry, fhcmp);
SPLAY_GENERATE(fhtree, fhent, entry, fhcmp);

struct fhtree  fhtree;
psc_spinlock_t fhtreelock = LOCK_INITIALIZER;

struct fhent *
fh_lookup(u64 fh)
{
	struct fhent *t, tq;

	tq.fh = fh;

	spinlock(&fhtreelock);
	t = SPLAY_FIND(fhtree, &fhtree, &tq);
	freelock(&fhtreelock);

	return(t);
}

int
fh_remove(u64 fh)
{
	struct fhent *t, tq;
	int    rc=-1;

        tq.fh = fh;

        spinlock(&fhtreelock);
	if (SPLAY_REMOVE(fhtree, &fhtree, t))
		rc = 0;
	freelock(&fhtreelock);
	
	return (rc);		
}

/**
 * fh_register - place the 64-bit 'file handle' into the splay tree.
 * @fh: file handle
 * @fh_regcb: callback
 */
void
fh_register(u64 fh, void (*fh_regcb)(struct fhent *, int, void **), 
	    void *cb_args[])
{
	struct fhent *t, fh;
	int    op = FD_REG_EXIST;

	fh.fh = fh;

	spinlock(&fhtreelock);	
	t = SPLAY_FIND(fhtree, &fhtree, &fh);
        if (t == NULL) {				
		op = FD_REG_NEW;
		t = PSCALLOC(sizeof(*t));
		LOCK_INIT(t->lock);
		t->fh_pri = NULL;
		t->fh = fh
		/* Inform the cache of our status, this allows us to 
		 *   init without holding the fhtreelock.
		 */ 
		t->fd = FD_REG_INIT;
		if (SPLAY_INSERT(fhtree, &fhtree, t))
			psc_fatalx("Attempted to reinsert fd "LPX64, t->fh);
	}
	freelock(&fhtreelock);

	if (fh_regcb)
		/* Callback into the fd handler
		 */
		(*fh_regcb)(t, op, cb_args);

	return (t->fh);
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
