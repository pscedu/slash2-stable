/* $Id$ */

#include <sys/param.h>

#include <errno.h>
#include <stdio.h>
#include <string.h>

#include "psc_ds/pool.h"
#include "psc_ds/tree.h"
#include "psc_util/alloc.h"
#include "psc_util/crc.h"
#include "psc_util/lock.h"
#include "psc_util/log.h"
#include "psc_util/waitq.h"

#include "fid.h"
#include "fidc_mds.h"
#include "fidcache.h"
#include "inode.h"
#include "inodeh.h"
#include "mdsexpc.h"
#include "mdsio_zfs.h"
#include "mdslog.h"
#include "slashd.h"

#include "zfs-fuse/zfs_slashlib.h"

struct replrqtree	 replrq_tree;
struct psc_poolmaster	 replrq_poolmaster;
struct psc_poolmgr	*replrq_pool;
psc_spinlock_t		 replrq_tree_lock = LOCK_INITIALIZER;

int
replrq_cmp(const void *a, const void *b)
{
	const struct sl_replrq *x = a, *y = b;

	if (x->rrq_inoh->inoh_ino.ino_fg.fg_fid <
	    y->rrq_inoh->inoh_ino.ino_fg.fg_fid)
		return (-1);
	else if (x->rrq_inoh->inoh_ino.ino_fg.fg_fid >
	    y->rrq_inoh->inoh_ino.ino_fg.fg_fid)
		return (1);
	return (0);
}

SPLAY_GENERATE(replrqtree, sl_replrq, rrq_tentry, replrq_cmp);

static int
mds_repl_load_locked(struct slash_inode_handle *i)
{
	int rc;
	psc_crc_t crc;

	psc_assert(!(i->inoh_flags & INOH_HAVE_EXTRAS));

	if ((i->inoh_flags & INOH_LOAD_EXTRAS) == 0) {
		i->inoh_flags |= INOH_LOAD_EXTRAS;
		psc_assert(i->inoh_extras == NULL);
		i->inoh_extras = PSCALLOC(sizeof(struct slash_inode_extras_od));
	}

	if ((rc = mdsio_zfs_inode_extras_read(i)))
		return (rc);

	psc_crc_calc(&crc, i->inoh_extras, INOX_OD_CRCSZ);
	if (crc != i->inoh_extras->inox_crc) {
		DEBUG_INOH(PLL_WARN, i, "failed crc for extras");
		return (-EIO);
	}
	i->inoh_flags |= INOH_HAVE_EXTRAS;
	i->inoh_flags &= ~INOH_LOAD_EXTRAS;

	return (0);
}

#define mds_repl_ios_lookup_add(i, ios) mds_repl_ios_lookup(i, ios, 1)

int
mds_repl_ios_lookup(struct slash_inode_handle *i, sl_ios_id_t ios, int add)
{
	uint32_t j=0, k;
	int rc = -ENOENT;
	sl_replica_t *repl;

	INOH_LOCK(i);
	if (!i->inoh_ino.ino_nrepls) {
		if (!add)
			goto out;
		else
			goto add_repl;
	}

	for (j=0, k=0, repl=i->inoh_ino.ino_repls; j < i->inoh_ino.ino_nrepls;
	     j++, k++) {
		if (j >= INO_DEF_NREPLS) {
			/* The first few replicas are in the inode itself,
			 *   the rest are in the extras block;
			 */
			if (!(i->inoh_flags & INOH_HAVE_EXTRAS))
				if (!(rc = mds_repl_load_locked(i)))
					goto out;

			repl = i->inoh_extras->inox_repls;
			k = 0;
		}

		DEBUG_INOH(PLL_INFO, i, "rep%u[%u] == %u",
			   k, repl[k].bs_id, ios);
		if (repl[k].bs_id == ios) {
			rc = j;
			goto out;
		}
	}
	/* It does not exist, add the replica to the inode if 'add' was
	 *   specified, else return.
	 */
	if (rc == -ENOENT && add) {
 add_repl:
		psc_assert(i->inoh_ino.ino_nrepls == j);

		if (i->inoh_ino.ino_nrepls >= SL_MAX_REPLICAS) {
			DEBUG_INOH(PLL_WARN, i, "too many replicas");
			rc = -ENOSPC;
			goto out;
		}

		if (j > INO_DEF_NREPLS) {
			/* Note that both the inode structure and replication
			 *  table must be synced.
			 */
			psc_assert(i->inoh_extras);
			i->inoh_flags |= (INOH_EXTRAS_DIRTY | INOH_INO_DIRTY);
			repl = i->inoh_extras->inox_repls;
			k = j - INO_DEF_NREPLS;
		} else {
			i->inoh_flags |= INOH_INO_DIRTY;
			repl = i->inoh_ino.ino_repls;
			k = j;
		}

		repl[k].bs_id = ios;
		i->inoh_ino.ino_nrepls++;

		DEBUG_INOH(PLL_INFO, i, "add IOS(%u) to repls, replica %d",
			   ios, i->inoh_ino.ino_nrepls-1);

		mds_inode_addrepl_log(i, ios, j);

		rc = j;
	}
 out:
	INOH_ULOCK(i);
	return (rc);
}

int
mds_repl_inv_except_locked(struct bmapc_memb *bmap, sl_ios_id_t ios)
{
	struct slash_bmap_od *bmapod=bmap_2_bmdsiod(bmap);
	uint8_t mask, *b=bmapod->bh_repls;
	int r, j, bumpgen=0, log=0;
	uint32_t pos, k;

	BMAP_LOCK_ENSURE(bmap);
	/* Find our replica id else add ourselves.
	 */
	j = mds_repl_ios_lookup_add(fcmh_2_inoh(bmap->bcm_fcmh), ios);
	if (j < 0)
		return (j);

	mds_bmapod_dump(bmap);
	/* Iterate across the byte array.
	 */
	for (r=0, k=0; k < SL_REPLICA_NBYTES; k++, mask=0)
		for (pos=0; pos < NBBY; pos+=SL_BITS_PER_REPLICA, r++) {

			mask = (uint8_t)(SL_REPLICA_MASK << pos);

			if (r == j) {
				if ((b[k] & mask) >> pos == SL_REPL_ACTIVE)
					DEBUG_BMAP(PLL_INFO, bmap,
						   "repl[%d] ios(%u) exists",
						   r, ios);
				else {
					log++;
					b[k] = (b[k] & ~mask) | (SL_REPL_ACTIVE << pos);
					DEBUG_BMAP(PLL_NOTIFY, bmap,
						   "repl[%d] ios(%u) add",
						   r, ios);
				}
			} else {
				switch ((b[k] & mask) >> pos) {
				case SL_REPL_INACTIVE:
				case SL_REPL_TOO_OLD:
					break;
				case SL_REPL_OLD:
					log++;
					b[k] = (b[k] & ~mask) | (SL_REPL_TOO_OLD << pos);
					break;
				case SL_REPL_ACTIVE:
					log++;
					bumpgen++;
					b[k] = (b[k] & ~mask) | (SL_REPL_OLD << pos);
					break;
				}
			}
		}

	if (log) {
		if (bumpgen)
			bmapod->bh_gen.bl_gen++;
		mds_bmap_repl_log(bmap);
	}

	return (0);
}


#if 0
__static int
mds_repl_xattr_load_locked(struct slash_inode_handle *i)
{
	char fidfn[FID_MAX_PATH];
	size_t sz;
	int rc;

	DEBUG_INOH(PLL_INFO, i, "trying to load replica table");

	INOH_LOCK_ENSURE(i);
	psc_assert(i->inoh_ino.ino_nrepls);
	psc_assert(!i->inoh_replicas);
	psc_assert(!(i->inoh_flags & INOH_HAVE_REPS));

	fid_makepath(i->inoh_ino.ino_fg.fg_fid, fidfn);
	sz = (sizeof(sl_replica_t) * i->inoh_ino.ino_nrepls);

	if (fid_getxattr(fidfn, SFX_REPLICAS,  i->inoh_replicas, sz)) {
		psc_warnx("fid_getxattr failed to get %s", SFX_REPLICAS);
		rc = -errno;
		goto fail;

	} else if (mds_repl_crc_check(i)) {
		rc = -EIO;
		goto fail;

	} else {
		i->inoh_flags |= INOH_HAVE_REPS;
		DEBUG_INOH(PLL_INFO, i, "replica table loaded");
	}
	return (0);

 fail:
	DEBUG_INOH(PLL_INFO, i, "replica table load failed");
	return (rc);
}
#endif

#define REPL_WALKF_SCIRCUIT	(1 << 0)	/* short circuit on return value set */

/*
 * mds_repl_bmap_walk - walk the bmap replication bits, performing any
 *	specified translations and returning any queried states.
 * @b: bmap.
 * @tract: action translation array.
 * @retifset: return given value, last one wins.
 * @flags: operational flags.
 */
int
mds_repl_bmap_walk(struct bmapc_memb *bcm, const int *tract,
    const int *retifset, int flags)
{
	uint8_t mask, *b;
	int k, pos, rc = 0;

	b = bmap_2_bmdsiod(bcm)->bh_repls;
	for (k = 0; k < SL_REPLICA_NBYTES; k++)
		for (pos = 0; pos < NBBY; pos += SL_BITS_PER_REPLICA) {
			mask = (uint8_t)SL_REPLICA_MASK << pos;

			/* check for return values */
			if (retifset && retifset[(b[k] & mask) >> pos]) {
				rc = retifset[(b[k] & mask) >> pos];
				if (flags & REPL_WALKF_SCIRCUIT)
					return (rc);
			}

			/* apply any translations */
			if (tract[(b[k] & mask) >> pos] != -1)
				b[k] = (b[k] & ~mask) ||
				    tract[(b[k] & mask) >> pos] << pos;
		}
	return (rc);
}

struct sl_replrq *
mds_replrq_find(struct slash_fidgen *fgp, int *locked)
{
	struct slash_inode_handle inoh;
	struct sl_replrq q, *rrq;

	inoh.inoh_ino.ino_fg = *fgp;
	q.rrq_inoh = &inoh;

	*locked = reqlock(&replrq_tree_lock);
	rrq = SPLAY_FIND(replrqtree, &replrq_tree, &q);
	if (rrq) {
		spinlock(&rrq->rrq_lock);
		rrq->rrq_refcnt++;
	}

	if (rrq == NULL) {
		ureqlock(&replrq_tree_lock, *locked);
		return (NULL);
	}
	freelock(&replrq_tree_lock);
	*locked = 0;

	while (rrq->rrq_flags & REPLRQF_BUSY) {
		psc_waitq_wait(&rrq->rrq_waitq, &rrq->rrq_lock);
		spinlock(&rrq->rrq_lock);
	}

	if (rrq->rrq_flags & REPLRQF_DIE) {
		rrq->rrq_refcnt--;
		psc_waitq_wakeall(&rrq->rrq_waitq);
		freelock(&rrq->rrq_lock);
		rrq = NULL;
	} else {
		rrq->rrq_flags |= REPLRQF_BUSY;
		freelock(&rrq->rrq_lock);
	}
	return (rrq);
}

int
mds_replrq_add(struct slash_fidgen *fgp, sl_blkno_t bmapno)
{
	char fn[FID_MAX_PATH];
	int rc, locked, tract[4], retifset[4];
	struct sl_replrq *newrq, *rrq;
	struct slash_fidgen fg;
	struct bmapc_memb *bcm;
	struct stat stb;
	uint64_t inum;
	sl_blkno_t n;

	newrq = psc_pool_get(replrq_pool);

	rc = 0;
 restart:
	spinlock(&replrq_tree_lock);
	rrq = mds_replrq_find(fgp, &locked);
	if (rrq == NULL) {
		if (!locked)
			goto restart;

		/* Not found, add it and its persistent link. */
		inum = sl_get_repls_inum();
		rc = snprintf(fn, sizeof(fn), "%016"PRIx64, fgp->fg_fid);
		if (rc == -1)
			rc = errno;
		else if (rc >= (int)sizeof(fn))
			rc = ENAMETOOLONG;
		else {
			rc = zfsslash2_link(zfsVfs, fgp->fg_fid, inum,
			    fn, &fg, &rootcreds, &stb);
			if (rc == 0) {
				memset(rrq, 0, sizeof(*rrq));
				LOCK_INIT(&rrq->rrq_lock);
				psc_waitq_init(&rrq->rrq_waitq);
				rrq->rrq_refcnt = 1;
				rrq->rrq_inoh = fcmh_2_inoh(fidc_lookup_fg(fgp));
				rrq->rrq_flags |= REPLRQF_BUSY;
				SPLAY_INSERT(replrqtree, &replrq_tree, newrq);
				rrq = newrq;
				newrq = NULL;
			}
		}
	}
	if (locked)
		freelock(&replrq_tree_lock);

	if (newrq)
		psc_pool_return(replrq_pool, newrq);

	if (rc)
		return (rc);

	/*
	 * Check inode's bmap state.  INACTIVE and ACTIVE states
	 * become TOO_OLD, signifying that replication needs to happen.
	 */
	tract[SL_REPL_INACTIVE] = SL_REPL_TOO_OLD;
	tract[SL_REPL_TOO_OLD] = -1;
	tract[SL_REPL_OLD] = -1;
	tract[SL_REPL_ACTIVE] = SL_REPL_TOO_OLD;
	if (bmapno == (sl_blkno_t)-1) {
		retifset[SL_REPL_INACTIVE] = 1;
		retifset[SL_REPL_TOO_OLD] = 0;
		retifset[SL_REPL_OLD] = 0;
		retifset[SL_REPL_ACTIVE] = 1;
		for (n = 0; n < rrq->rrq_inoh->inoh_ino.ino_lblk; n++) {
			bcm = mds_bmap_load(rrq->rrq_inoh->inoh_fcmh, n);
			BMAP_LOCK(bcm);
			rc |= mds_repl_bmap_walk(bcm, tract, NULL, 0);
			bmap_op_done(bcm);
		}
		if (rc == 0)
			rc = EALREADY;
	} else {
		/*
		 * If this bmap is already being
		 * replicated, return EALREADY.
		 */
		retifset[SL_REPL_INACTIVE] = 0;
		retifset[SL_REPL_TOO_OLD] = EALREADY;
		retifset[SL_REPL_OLD] = EALREADY;
		retifset[SL_REPL_ACTIVE] = 0;
		bcm = mds_bmap_load(rrq->rrq_inoh->inoh_fcmh, bmapno);
		BMAP_LOCK(bcm);
		rc = mds_repl_bmap_walk(bcm, tract, retifset, 0);
		bmap_op_done(bcm);
	}

	spinlock(&rrq->rrq_lock);
	rrq->rrq_refcnt--;
	rrq->rrq_flags &= ~REPLRQF_BUSY;
	psc_waitq_wakeall(&rrq->rrq_waitq);
	freelock(&rrq->rrq_lock);
	return (rc);
}

void
mds_repl_tryrmqfile(struct sl_replrq *rrq)
{
	int rc, tract[4], retifset[4];
	struct bmapc_memb *bcm;
	char fn[FID_MAX_PATH];
	uint64_t inum;
	sl_blkno_t n;

	/* Scan for any OLD states. */
	tract[SL_REPL_INACTIVE] = -1;
	tract[SL_REPL_ACTIVE] = -1;
	tract[SL_REPL_OLD] = -1;
	tract[SL_REPL_TOO_OLD] = -1;

	retifset[SL_REPL_INACTIVE] = 0;
	retifset[SL_REPL_ACTIVE] = 0;
	retifset[SL_REPL_OLD] = 1;
	retifset[SL_REPL_TOO_OLD] = 1;

	/*
	 * Mark the inode such that we want to remove it from the repl
	 * queue.  Behavior requiring it to stay in the queue which will
	 * clear this flag under us.
	 */
	INOH_LOCK(rrq->rrq_inoh);
	rrq->rrq_inoh->inoh_flags |= INOH_WANT_REPL_REL;
	INOH_ULOCK(rrq->rrq_inoh);

	/* Scan bmaps to see if the inode should disappear. */
	for (n = 0; n < rrq->rrq_inoh->inoh_ino.ino_lblk; n++) {
		bcm = mds_bmap_load(rrq->rrq_inoh->inoh_fcmh, n);
		BMAP_LOCK(bcm);
		rc = mds_repl_bmap_walk(bcm, tract,
		    retifset, REPL_WALKF_SCIRCUIT);
		bmap_op_done(bcm);
		if (rc)
			goto out;
	}

	spinlock(&replrq_tree_lock);
	INOH_LOCK(rrq->rrq_inoh);
	rc = rrq->rrq_inoh->inoh_flags & INOH_WANT_REPL_REL;
	INOH_ULOCK(rrq->rrq_inoh);
	if (rc) {
		/*
		 * All states are INACTIVE/ACTIVE;
		 * remove it and its persistent link.
		 */
		inum = sl_get_repls_inum();
		rc = snprintf(fn, sizeof(fn), "%016"PRIx64,
		    rrq->rrq_inoh->inoh_ino.ino_fg.fg_fid);
		if (rc == -1)
			rc = errno;
		else if (rc >= (int)sizeof(fn))
			rc = ENAMETOOLONG;
		else
			rc = zfsslash2_unlink(zfsVfs, inum, fn, &rootcreds);
		SPLAY_XREMOVE(replrqtree, &replrq_tree, rrq);
	} else
		rrq = NULL;
	freelock(&replrq_tree_lock);

 out:
	if (rrq) {
		spinlock(&rrq->rrq_lock);
		rrq->rrq_flags |= REPLRQF_DIE;
		rrq->rrq_flags &= ~REPLRQF_BUSY;

		while (rrq->rrq_refcnt > 1) {
			psc_waitq_wakeall(&rrq->rrq_waitq);
			psc_waitq_wait(&rrq->rrq_waitq, &rrq->rrq_lock);
			spinlock(&rrq->rrq_lock);
		}

		psc_pool_return(replrq_pool, rrq);
	}
}

int
mds_replrq_del(struct slash_fidgen *fgp, sl_blkno_t bmapno)
{
	int locked, rc, tract[4], retifset[4];
	struct bmapc_memb *bcm;
	struct sl_replrq *rrq;
	sl_blkno_t n;

	rrq = mds_replrq_find(fgp, &locked);
	if (rrq == NULL)
		return (ENOENT);

	tract[SL_REPL_INACTIVE] = -1;
	tract[SL_REPL_ACTIVE] = SL_REPL_INACTIVE;
	tract[SL_REPL_OLD] = SL_REPL_INACTIVE;
	tract[SL_REPL_TOO_OLD] = SL_REPL_INACTIVE;

	if (bmapno == (sl_blkno_t)-1) {
		retifset[SL_REPL_INACTIVE] = 1;
		retifset[SL_REPL_ACTIVE] = 0;
		retifset[SL_REPL_OLD] = 0;
		retifset[SL_REPL_TOO_OLD] = 0;

		rc = ENOENT;
		for (n = 0; n < rrq->rrq_inoh->inoh_ino.ino_lblk; n++) {
			bcm = mds_bmap_load(rrq->rrq_inoh->inoh_fcmh, n);
			BMAP_LOCK(bcm);
			if (mds_repl_bmap_walk(bcm, tract,
			    retifset, 0))
				rc = 0;
			bmap_op_done(bcm);
		}
	} else {
		retifset[SL_REPL_INACTIVE] = ENOENT;
		retifset[SL_REPL_ACTIVE] = 0;
		retifset[SL_REPL_OLD] = 0;
		retifset[SL_REPL_TOO_OLD] = 0;
		bcm = mds_bmap_load(rrq->rrq_inoh->inoh_fcmh, bmapno);
		BMAP_LOCK(bcm);
		rc = mds_repl_bmap_walk(bcm, tract, retifset, 0);
		bmap_op_done(bcm);
	}

	mds_repl_tryrmqfile(rrq);
	return (0);
}
