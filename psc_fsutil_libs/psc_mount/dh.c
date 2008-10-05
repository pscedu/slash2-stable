/* $Id$ */

#include <dirent.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "psc_types.h"
#include "psc_ds/tree.h"
#include "psc_util/alloc.h"
#include "psc_util/lock.h"
#include "psc_util/random.h"

int
dhcmp(const void *a, const void *b)
{
	const struct dhent *da = a, *db = b;

	if (da->fh < db->fh)
		return (-1);
	else if (da->fh > db->fh)
		return (1);
	return (0);
}

SPLAY_HEAD(dhtree, dhent);
SPLAY_PROTOTYPE(dhtree, dhent, entry, dhcmp);
SPLAY_GENERATE(dhtree, dhent, entry, dhcmp);

struct dhtree dhtree;
psc_spinlock_t dhtreelock = LOCK_INITIALIZER;

struct dhent *
_dh_lookup(u64 fh, int remove)
{
	struct dhent *d, dq;

	dq.fh = fh;

	spinlock(&dhtreelock);
	d = SPLAY_FIND(dhtree, &dhtree, &dq);
	if (d == NULL)
		psc_fatalx("no dh entry found");
	spinlock(&d->lock);
	d->refcnt++;
	if (remove)
		SPLAY_REMOVE(dhtree, &dhtree, d);
	freelock(&dhtreelock);
	return (d);
}

void
dh_release(struct dhent *d)
{
	if (--d->refcnt)
		freelock(&d->lock);
	else {
		/*
		 * We have to lock the tree here ensure that the refcnt
		 * really is zero as dh_lookup() may be in the middle of
		 * handing out another reference.
		 */
		while (!trylock(&dhtreelock)) {
			freelock(&d->lock);
			spinlock(&d->lock);
		}
		if (d->refcnt == 0) {
			close(d->dfd);
			free(d);
		} else
			freelock(&d->lock);
		freelock(&dhtreelock);
	}
}

/*
 * XXX: pass path or FID instead, and return the
 * dfd if it is already open.
 */
u64
dh_register(int dfd)
{
	struct dhent *d;

	d = PSCALLOC(sizeof(*d));
	LOCK_INIT(&d->lock);
	d->dfd = dfd;
	d->refcnt = 1;

	spinlock(&dhtreelock);
	do {
		d->fh = psc_random64();
	} while (SPLAY_INSERT(dhtree, &dhtree, d));
	freelock(&dhtreelock);
	return (d->fh);
}

void
dh_destroy(struct dhent *de)
{
	de->refcnt--;
	dh_release(de);
}

int
dh_reap(void)
{
#if 0
	struct dhent *d;

	spinlock(&dhtreelock);
	SPLAY_FOREACH(e, dhtree, &dhtree) {
	}
	freelock(&dhtreelock);
#endif
	return (0);
}
