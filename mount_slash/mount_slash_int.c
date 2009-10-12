/* $Id$ */

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>

#include <dirent.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "pfl/pfl.h"
#include "psc_ds/dynarray.h"
#include "psc_ds/listcache.h"
#include "psc_rpc/rpc.h"
#include "psc_rpc/rpclog.h"
#include "psc_rpc/rsx.h"
#include "psc_util/cdefs.h"
#include "psc_util/log.h"

#include "bmap.h"
#include "buffer.h"
#include "cli_bmap.h"
#include "fidcache.h"
#include "mount_slash.h"
#include "offtree.h"
#include "slashrpc.h"
#include "slconfig.h"

#define MSL_PAGES_GET 0
#define MSL_PAGES_PUT 1

extern struct psc_listcache bmapFlushQ;

__static SPLAY_GENERATE(fhbmap_cache, msl_fbr, mfbr_tentry, fhbmap_cache_cmp);

/**
 * msl_oftrq_build - 
 * @newreq: newly created request will be placed here.
 * @b: the source bmap 
 * @off: the 'normalized' offset (ie bmap relative, not absolute)
 * @len: extent size
 * @op: read or write.
 */
__static void
msl_oftrq_build(struct offtree_req **newreq, struct bmapc_memb *b, off_t off,
    size_t len, int op)
{
	struct offtree_req *r;	
	int top=0;

	DEBUG_BMAP(PLL_TRACE, b, "adding req for (off=%zu) (size=%zu)", 
		   off, len);
	/* Ensure the offset fits within the range and mask off the
	 *  lower bits to align with the offtree's page size.
	 */
	psc_assert((off + len) <= SLASH_BMAP_SIZE);
	psc_assert(op == OFTREQ_OP_WRITE || op == OFTREQ_OP_READ);

	*newreq = r = PSCALLOC(sizeof(struct offtree_req));
	
	INIT_PSCLIST_ENTRY(&r->oftrq_lentry);
	/* Verify that the mds agrees that the bmap is writeable.
	 */
	if (op == OFTREQ_OP_WRITE)
		psc_assert(b->bcm_mode & BMAP_WR);

	r->oftrq_op = op;
	r->oftrq_bmap = b;
	r->oftrq_off = off;
	/* Set directio flag if the bmap is in dio mode, otherwise
	 *  allocate an array for cache iovs.
	 */
	if (b->bcm_mode & BMAP_DIO) {
		r->oftrq_op |= OFTREQ_OP_DIO;
		goto out;
	}
	/* Resume creating the cache-based request.
	 */
	r->oftrq_darray = PSCALLOC(sizeof(struct dynarray));
	r->oftrq_root   = bmap_2_msoftr(b);
	r->oftrq_memb   = &r->oftrq_root->oftr_memb;
	r->oftrq_width  = r->oftrq_depth = 0;
	r->oftrq_len    = len;
	r->oftrq_nblks  = 0;

	if (off & SLASH_BMAP_BLKMASK) {
		/* Unaligned offset, account for the partial block,
		 *   subtract the unaligned segment from the length 
		 *   and mask it off.
		 */
		r->oftrq_nblks++;
		len -= len & SLASH_BMAP_BLKMASK;
		top |= OFTREQ_OP_PRFFP;
	}	
	r->oftrq_nblks += len / SLASH_BMAP_BLKSZ;

	if (len & SLASH_BMAP_BLKMASK) {
		r->oftrq_nblks++;
		top |= OFTREQ_OP_PRFLP;
	}
	/* The inode should be relatively recent.  Lookup the size
	 *   of the file to determine whether a RBW is needed.
	 *   XXX actually with the new non-aligned code we shouldn't have
	 *   to do RBW unless the bmap is open in RW mode.
	 *   If that's the case then the DATARDY semantics need to be 
	 *     changed.
	 */
	if ((fcmh_2_fsz(b->bcm_fcmh) > off) && op == OFTREQ_OP_WRITE && top)
			r->oftrq_op |= top;
 out:
	DEBUG_OFFTREQ(PLL_TRACE, r, "newly built request (len=%zu)", len);
}


__static void
bmap_oftrq_add_locked(struct offtree_req *r)
{
	struct bmapc_memb *b=r->oftrq_bmap;

	BMAP_LOCK_ENSURE(b);

	DEBUG_BMAP(PLL_INFO, b, "add oftrq=%p list_empty(%d)", 
		   r, psclist_empty(&bmap_2_msbd(b)->msbd_oftrqs));

        psclist_xadd(&r->oftrq_lentry, &bmap_2_msbd(b)->msbd_oftrqs);
}
 

__static void
bmap_oftrq_del(struct bmapc_memb *b, struct offtree_req *r)
{
	BMAP_LOCK(b);

	if (r->oftrq_op & OFTREQ_OP_READ) {
		/* Read requests don't need to be tracked.		   
		 */
		DEBUG_BMAP(PLL_INFO, b, "oftrq=%p", r);		
		goto out;
	}

	if (!(r->oftrq_op & OFTREQ_OP_READ)) {
		psc_assert(b->bcm_mode & BMAP_DIRTY);		
		psc_assert(psclist_conjoint(&bmap_2_msbd(b)->msbd_lentry));		
		psclist_del(&r->oftrq_lentry);
	}
	
	DEBUG_BMAP(PLL_INFO, b, "remove oftrq=%p list_empty(%d)", 
		   r, psclist_empty(&bmap_2_msbd(b)->msbd_oftrqs));
	
	if (!psclist_empty(&bmap_2_msbd(b)->msbd_oftrqs))
		goto wake;
	
	else {
		/* Set the special flag to prevent races with others which 
		 *   can occur because we may drop the bmap lock before 
		 *   removing from the bmapFlushQ listcache.
		 */
		b->bcm_mode |= BMAP_DIRTY2LRU;
		b->bcm_mode &= ~BMAP_DIRTY;

		if (LIST_CACHE_TRYLOCK(&bmapFlushQ))
			goto remove;
		
		else {
			BMAP_ULOCK(b);
			/* Retake the locks in the correct order else 
			 *   deadlock.
			 */
			LIST_CACHE_LOCK(&bmapFlushQ);
			BMAP_LOCK(b);
			/* Only this thread may unset BMAP_DIRTY2LRU.
			 */
			psc_assert(b->bcm_mode & BMAP_DIRTY2LRU);

			if (b->bcm_mode & BMAP_DIRTY) {
				psc_assert(psclist_conjoint(&bmap_2_msbd(b)->msbd_lentry));
				psc_assert(!psclist_empty(&bmap_2_msbd(b)->msbd_oftrqs));

			} else {
			remove:
				psc_assert(psclist_empty(&bmap_2_msbd(b)->msbd_oftrqs));
				lc_remove(&bmapFlushQ, bmap_2_msbd(b));
			}
			LIST_CACHE_ULOCK(&bmapFlushQ);
			b->bcm_mode &= ~BMAP_DIRTY2LRU;
		}
	}
 wake:
	psc_waitq_wakeall(&b->bcm_waitq);
 out:
        BMAP_ULOCK(b);
}


__static void
msl_oftrq_destroy(struct offtree_req *r)
{
	struct bmapc_memb *b = r->oftrq_bmap;
	
	psc_assert(b);
	psc_assert(r->oftrq_darray);

	atomic_dec(&b->bcm_opcnt);
	psc_assert(atomic_read(&b->bcm_opcnt) >= 0);
	/* Remove the offtree request reference from the bmap.
	 */
	bmap_oftrq_del(b, r);

	dynarray_free(r->oftrq_darray);
	PSCFREE(r->oftrq_darray);

	if (r->oftrq_fill.oftfill_inprog) {
		dynarray_free(r->oftrq_fill.oftfill_inprog);
		PSCFREE(r->oftrq_fill.oftfill_inprog);
	}

	PSCFREE(r);
}


struct msl_fhent *
msl_fhent_new(struct fidc_membh *f)
{
	struct msl_fhent *mfh;

	mfh = PSCALLOC(sizeof(*mfh));
	LOCK_INIT(&mfh->mfh_lock);
	SPLAY_INIT(&mfh->mfh_fhbmap_cache);
	mfh->mfh_fcmh = f;
	return (mfh);
}

/**
 * bmapc_memb_init - initialize a bmap structure and create its offtree.
 * @b: the bmap struct
 * @f: the bmap's owner
 */
void
msl_bmap_init(struct bmapc_memb *b, struct fidc_membh *f, sl_blkno_t blkno)
{
	struct msbmap_data *msbd;
	u32 tmode = b->bcm_mode;
	
	memset(b, 0, sizeof(*b));
	LOCK_INIT(&b->bcm_lock);
	atomic_set(&b->bcm_opcnt, 0);
	psc_waitq_init(&b->bcm_waitq);
	b->bcm_pri = msbd = PSCALLOC(sizeof(*msbd));
	bmap_2_msoftr(b) = offtree_create(SLASH_BMAP_SIZE, SLASH_BMAP_BLKSZ,
					  SLASH_BMAP_WIDTH, SLASH_BMAP_DEPTH,
					  f, sl_buffer_alloc, sl_oftm_addref);
	/* Assign the backpointer.
	 */
	bmap_2_msbd(b)->msbd_bmap = b;

	psc_assert(bmap_2_msoftr(b));
	INIT_PSCLIST_HEAD(&msbd->msbd_oftrqs);

	b->bcm_blkno = blkno;
	b->bcm_fcmh = f;	
	b->bcm_mode = tmode;
}


int
msl_bmap_memrls_trylock(void *a)
{
	struct bmapc_memb *b = a;
	int got_it=0;

	DEBUG_BMAP(PLL_INFO, b, "tryget memrls lock");
	BMAP_LOCK(b);
	if (!(b->bcm_mode & BMAP_MEMRLS)) {
		b->bcm_mode |= BMAP_MEMRLS;
		got_it = 1;
	}
	BMAP_ULOCK(b);
	
	return (got_it);
}

void
msl_bmap_memrls_unlock(void *a)
{
	struct bmapc_memb *b = a;

	BMAP_LOCK(b);
	DEBUG_BMAP(PLL_INFO, b, "free memrls lock");
	b->bcm_mode &= ~BMAP_MEMRLS;
	psc_waitq_wakeall(&b->bcm_waitq);
	BMAP_ULOCK(b);
}

void
msl_init(void)
{
	slMemRlsUlock=msl_bmap_memrls_unlock;
	slMemRlsTrylock=msl_bmap_memrls_trylock;
}

/**
 * bmapc_memb_release - release a bmap structure and associated resources.
 * @b: the bmap.
 */
void
msl_bmap_release(struct bmapc_memb *b)
{
 retry:
	/* Mind lock ordering.
	 */
	FCMH_LOCK(b->bcm_fcmh);
	psc_assert(b->bcm_fcmh->fcmh_fcoo);

	BMAP_LOCK(b);
	psc_assert(b->bcm_mode & BMAP_CLOSING);
	psc_assert(!(b->bcm_mode & BMAP_DIRTY));	
	psc_assert(!atomic_read(&b->bcm_waitq.wq_nwaitors));
	psc_assert(!atomic_read(&b->bcm_wr_ref) && 
		   !atomic_read(&b->bcm_rd_ref));
	psc_assert(!atomic_read(&b->bcm_opcnt));
#if 0
	/* The flush thread may have yet to remove this bmap.
	 */
	if (psclist_conjoint(&bmap_2_msbd(b)->msbd_lentry)) {
		LIST_CACHE_LOCK(&bmapFlushQ);
		if (psclist_conjoint(&bmap_2_msbd(b)->msbd_lentry))
			lc_remove(&bmapFlushQ, bmap_2_msbd(b));
		LIST_CACHE_ULOCK(&bmapFlushQ);
	}
#endif
	psc_assert(psclist_disjoint(&bmap_2_msbd(b)->msbd_lentry));

	if (b->bcm_mode & BMAP_MEMRLS) {
		/* Don't race with the slab cache reaper, if he's 
		 *    releasing pages from offtree then we must
		 *    wait here.
		 */
		DEBUG_BMAP(PLL_WARN, b, "block on reaper");
		FCMH_ULOCK(b->bcm_fcmh);
		psc_waitq_wait(&b->bcm_waitq, &b->bcm_lock);
		goto retry;
	} else 
		/* Keep the slap reaper from dealing with our bmap's
		 *    offtree.
		 */
		b->bcm_mode |= BMAP_MEMRLS;

	DEBUG_BMAP(PLL_INFO, b, "freeing");	
	BMAP_ULOCK(b);

	if (!SPLAY_REMOVE(bmap_cache, &b->bcm_fcmh->fcmh_fcoo->fcoo_bmapc, b))
		DEBUG_BMAP(PLL_FATAL, b, "failed to locate bmap in fcoo");

	FCMH_ULOCK(b->bcm_fcmh);
		
	offtree_release_all(bmap_2_msoftr(b));
	offtree_destroy(bmap_2_msoftr(b));
	psc_assert(psclist_empty(&bmap_2_msbd(b)->msbd_oftrqs));

	atomic_dec(&b->bcm_fcmh->fcmh_fcoo->fcoo_bmapc_cnt);

	PSCFREE(b->bcm_pri);
	PSCFREE(b);
}

__static int
bmap_offtree_reqs_chkempty(struct bmapc_memb *b)
{
	int locked, rc;

	locked = BMAP_RLOCK(b);
	rc = psclist_empty(&bmap_2_msbd(b)->msbd_oftrqs);
	BMAP_URLOCK(b, locked);

	return (rc);
}


__static void
bmap_oftrq_waitempty(struct bmapc_memb *b)
{
 retry:
	BMAP_LOCK(b);	
	if (!psclist_empty(&bmap_2_msbd(b)->msbd_oftrqs)) {
		psc_waitq_wait(&b->bcm_waitq, &b->bcm_lock);
		goto retry;
	}
	psc_assert(psclist_empty(&bmap_2_msbd(b)->msbd_oftrqs));
	BMAP_ULOCK(b);
}

void 
msl_bmap_tryrelease(struct bmapc_memb *b)
{
	BMAP_LOCK(b);
	psc_assert(atomic_read(&b->bcm_wr_ref) >= 0);
	psc_assert(atomic_read(&b->bcm_rd_ref) >= 0);

	DEBUG_BMAP(PLL_INFO, b, "free me?");

	if (!atomic_read(&b->bcm_wr_ref) && !atomic_read(&b->bcm_rd_ref)) {
		b->bcm_mode |= BMAP_CLOSING;
		BMAP_ULOCK(b);
		/* Wait for any pending offtree reqs to clear.
		 */
		bmap_oftrq_waitempty(b);
		msl_bmap_release(b);
	}
}

void
msl_fbr_free(struct msl_fbr *r)
{
	struct bmapc_memb *b = r->mfbr_bmap;
	
	psc_assert(b);
	psc_assert(SPLAY_ENTRY_DISJOINT(fhbmap_cache, r));

	msl_fbr_unref(r);
	PSCFREE(r);	

	return (msl_bmap_tryrelease(b));
}

/**
 * msl_bmap_fetch - perform a blocking 'get' operation to retrieve 
 *    one or more bmaps from the MDS.
 * @f: pointer to the fid cache structure to which this bmap belongs.
 * @b: the block id to retrieve (block size == SLASH_BMAP_SIZE).
 * @n: the number of bmaps to retrieve (serves as a simple read-ahead mechanism)
 */
__static int
msl_bmap_fetch(struct bmapc_memb *bmap, sl_blkno_t b, int rw)
{
	struct pscrpc_bulk_desc *desc;
	struct pscrpc_request *rq;
	struct srm_bmap_req *mq;
	struct srm_bmap_rep *mp;
	struct msbmap_data *msbd;
	struct fidc_membh *f;
	struct iovec iovs[3];
	int nblks, rc=-1;
	u32 i, getreptbl=0;
	struct msl_fcoo_data *mfd;

	psc_assert(bmap->bcm_mode & BMAP_INIT);
	psc_assert(bmap->bcm_pri);
	psc_assert(bmap->bcm_fcmh);

	f = bmap->bcm_fcmh;

	FCMH_LOCK(f);
	psc_assert(f->fcmh_fcoo);
	if (!f->fcmh_fcoo->fcoo_pri) {
		f->fcmh_fcoo->fcoo_pri = mfd = PSCALLOC(sizeof(*mfd));
		mfd->mfd_flags |= MFD_RETRREPTBL;
		getreptbl = 1;
	}
	FCMH_ULOCK(f);

	/* Build the new RPC request.
	 */
	if ((rc = RSX_NEWREQ(mds_import, SRMC_VERSION,
			     SRMT_GETBMAP, rq, mq, mp)) != 0)
		return (rc);

	mq->sfdb  = *fcmh_2_fdb(f);
	mq->pios  = prefIOS; /* Tell MDS of our preferred ION */
	mq->blkno = b;
	mq->nblks = 1;
	mq->rw    = rw;
	mq->getreptbl = getreptbl ? 1 : 0;

	msbd = bmap->bcm_pri;
	bmap->bcm_mode |= (rw & SRIC_BMAP_WRITE) ? BMAP_WR : BMAP_RD;

	iovs[0].iov_base = &msbd->msbd_msbcr;
	iovs[0].iov_len  = sizeof(msbd->msbd_msbcr);

	iovs[1].iov_base = &msbd->msbd_bdb;
	iovs[1].iov_len  = sizeof(msbd->msbd_bdb);

	if (getreptbl) {
		/* This code only deals with SL_DEF_REPLICAS, not MAX.
		 */
		iovs[2].iov_base = &mfd->mfd_reptbl;
		iovs[2].iov_len  = sizeof(sl_replica_t) * INO_DEF_NREPLS;
	}

	DEBUG_FCMH(PLL_DEBUG, f, "retrieving bmap (s=%u) (rw=%d)", b, rw);

	nblks = 0;
	rsx_bulkclient(rq, &desc, BULK_PUT_SINK, SRMC_BULK_PORTAL, iovs, 
		       2 + getreptbl);

	if ((rc = RSX_WAITREP(rq, mp)) == 0) {
		/* Verify the return.
		 */
		if (!mp->nblks) {
			psc_errorx("MDS returned 0 bmaps");
			return (-1);
		}
		/* Add the bmaps to the tree.
		 */
		spinlock(&f->fcmh_lock);
		for (i=0; i < mp->nblks; i++) {
			SPLAY_INSERT(bmap_cache, &f->fcmh_fcoo->fcoo_bmapc,
				     bmap);
			atomic_inc(&f->fcmh_fcoo->fcoo_bmapc_cnt);
			bmap_2_msion(bmap) = mp->ios_nid;
		}
		freelock(&f->fcmh_lock);
	}

	bmap->bcm_mode &= ~BMAP_INIT;
	
	if (getreptbl) {
		/* XXX don't forget that on write we need to invalidate the local replication
		 *   table..
		 */
		FCMH_LOCK(f);
		mfd->mfd_flags = MFD_HAVEREPTBL;
		psc_waitq_wakeall(&f->fcmh_waitq);
		FCMH_ULOCK(f);
	}

	return (rc);
}

/**
 * msl_bmap_modeset -
 * Notes:  XXX I think this logic can be simplified when setting mode from
 *    WRONLY to RDWR.  In WRONLY this client already knows the address
 *    of the only ION from which this bmap can be read.  Therefore, it
 *    should be able to interface with that ION without intervention from
 *    the mds.
 */
__static int
msl_bmap_modeset(struct fidc_membh *f, sl_blkno_t b, int rw)
{
	struct pscrpc_request *rq;
	struct srm_bmap_chmode_req *mq;
	struct srm_generic_rep *mp;
	int rc;

	psc_assert(rw == SRIC_BMAP_WRITE || rw == SRIC_BMAP_READ);

	if ((rc = RSX_NEWREQ(mds_import, SRMC_VERSION,
			     SRMT_BMAPCHMODE, rq, mq, mp)) != 0)
		return (rc);

	mq->sfdb = *fcmh_2_fdb(f);
	mq->blkno = b;
	mq->rw = rw;

	if ((rc = RSX_WAITREP(rq, mp)) == 0) {
		if (mp->rc)
			psc_warn("msl_bmap_chmode() failed (f=%p) (b=%u)",
				 f, b);
	}
	return (rc);
}

void
msl_bmap_fhcache_clear(struct msl_fhent *mfh)
{
	struct msl_fbr *r, *n;

	spinlock(&mfh->mfh_lock);
	for (r = SPLAY_MIN(fhbmap_cache, &mfh->mfh_fhbmap_cache);
	    r; r = n) {
		n = SPLAY_NEXT(fhbmap_cache, &mfh->mfh_fhbmap_cache, r);
		psc_assert(SPLAY_REMOVE(fhbmap_cache,
		    &mfh->mfh_fhbmap_cache, r));
		msl_fbr_free(r);
	}
	freelock(&mfh->mfh_lock);
}

#define BML_NEW_BMAP  0
#define BML_HAVE_BMAP 1

__static void
msl_bmap_fhcache_ref(struct msl_fhent *mfh, struct bmapc_memb *b,
		     int mode, int rw)
{
	struct msl_fbr *r;

	/* Now handle the fhent's bmap cache, adding a new reference
	 *  if needed.
	 *
	 * A new bmap may not already exist in the file handle's
	 *  reference cache.  Lock around the fhcache_bmap_lookup()
	 *  test to prevent another thread from inserting before us.
	 */
	spinlock(&mfh->mfh_lock);
	r = fhcache_bmap_lookup(mfh, b);
	if (!r) {
		r = msl_fbr_new(b, (rw == SRIC_BMAP_WRITE ?
	    	      FHENT_WRITE : FHENT_READ));
		SPLAY_INSERT(fhbmap_cache, &mfh->mfh_fhbmap_cache, r);
	} else {
		/* Verify that the ref didn't not exist if the caller
		 *  specified BML_NEW_BMAP.
		 */
		psc_assert(mode != BML_NEW_BMAP);
		msl_fbr_ref(r, (rw == SRIC_BMAP_WRITE ?
			FHENT_WRITE : FHENT_READ));
	}
	freelock(&mfh->mfh_lock);
}

/**
 * msl_bmap_load - locate a bmap in fcache_memb_handle's splay tree or retrieve it from the MDS.
 * @f: the msl_fhent for the owning file.
 * @n: bmap number.
 * @prefetch: the number of subsequent bmaps to prefetch.
 * @rw: tell the mds if we plan to read or write.
 * Notes: XXX Need a way to detect bmap mode changes here (ie from read
 *	to rw) and take the neccessary actions to notify the mds, this
 *	detection will be done by looking at the refcnts on the bmap.
 * TODO:  XXX if bmap is already cached but is not in write mode (but
 *	rw==WRITE) then we must notify the mds of this.
 */
__static struct bmapc_memb *
msl_bmap_load(struct msl_fhent *mfh, sl_blkno_t n, u32 rw)
{
	struct fidc_membh *f = mfh->mfh_fcmh;
	struct bmapc_memb *b;

	int rc = 0, mode = BML_HAVE_BMAP;

	b = bmap_lookup_add(f, n, msl_bmap_init);
	psc_assert(b);

	if (b->bcm_mode & BMAP_INIT) {
		/* Retrieve the bmap from the sl_mds.
		 */
		rc = msl_bmap_fetch(b, n, rw);
		if (rc)
			return NULL;
		else {
			mode = BML_NEW_BMAP;
			psc_assert(!(b->bcm_mode & BMAP_INIT));
		}
		/* Verify that the mds has returned a 'write-enabled' bmap.
		 */
		if (rw == SRIC_BMAP_WRITE)
			psc_assert(b->bcm_mode & BMAP_WR);

		msl_bmap_fhcache_ref(mfh, b, mode, rw);
		return (b);
	}
	/* Else */
	/* Ref now, otherwise our bmap may get downgraded while we're
	 *  blocking on the waitq.
	 */
	msl_bmap_fhcache_ref(mfh, b, mode, rw);

	/* If our bmap is cached then we need to consider the current
	 *   caching policy and possibly notify the mds.  I.e. if our
	 *   bmap is read-only (but we'd like to write) then the mds
	 *   must be notified so that coherency amongst clients can
	 *   be maintained.
	 *  msl_io() has already verified that this file is writable.
	 *  XXX has it?
	 */
 retry:
	spinlock(&b->bcm_lock);
	if (rw != SRIC_BMAP_WRITE || (b->bcm_mode & BMAP_WR)) {
		/* Either we're in read-mode here or the bmap
		 *  has already been marked for writing therefore
		 *  the mds already knows we're writing.
		 */
		freelock(&b->bcm_lock);
		goto out;

	} else if (b->bcm_mode & BMAP_CLI_MCIP) {
		/* If some other thread has set BMAP_CLI_MCIP then HE
		 *  must set BMAP_CLI_MCC when he's done (at that time
		 *  he also must unset BMAP_CLI_MCIP and set BMAP_WR
		 */
		psc_waitq_wait(&b->bcm_waitq, &b->bcm_lock);
		/* Done waiting, double check our refcnt, it should be at
		 *  least '1' (our ref).
		 * XXX Not sure if both checks are needed.
		 */
		psc_assert(atomic_read(&b->bcm_opcnt) > 0);
		psc_assert(atomic_read(&b->bcm_wr_ref) > 0);
		if (b->bcm_mode & BMAP_CLI_MCC) {
			/* Another thread has completed the upgrade
			 *  in mode change.  Verify that the bmap
			 *  is in the appropriate state.
			 *  Note: since our wr_ref has been set above,
			 *   the bmap MUST have BMAP_WR set here.
			 */
			psc_assert(!(b->bcm_mode & BMAP_CLI_MCIP));
			psc_assert((b->bcm_mode & BMAP_WR));
		} else
			/* We were woken up for a different
			 *  reason - try again.
			 */
			goto retry;

	} else { /* !BMAP_CLI_MCIP not set, we will set it and
		  *    proceed with the modechange operation.
		  */
		psc_assert(!(b->bcm_mode & BMAP_WR)   &&
			   !(b->bcm_mode & BMAP_CLI_MCIP) &&
			   !(b->bcm_mode & BMAP_CLI_MCC));

		b->bcm_mode |= BMAP_CLI_MCIP;
		freelock(&b->bcm_lock);
		/* An interesting fallout here is that the mds may callback
		 *  to us causing our offtree cache to be purged :)
		 * Correction.. this is not true, since if there was another
		 *  writer then we would already be in directio mode.
		 */
		rc = msl_bmap_modeset(f, b->bcm_blkno, SRIC_BMAP_WRITE);
		psc_assert(!rc); /*  XXX for now.. */
		/* We're the only thread allowed here, these
		 *  bits could not have been set by another thread.
		 */
		spinlock(&b->bcm_lock);
		psc_assert(b->bcm_mode & BMAP_CLI_MCIP);
		psc_assert(!(b->bcm_mode & BMAP_CLI_MCC) &&
			   !(b->bcm_mode & BMAP_WR));
		b->bcm_mode &= ~BMAP_CLI_MCIP;
		b->bcm_mode |= (BMAP_WR | BMAP_CLI_MCC);
		freelock(&b->bcm_lock);
		psc_waitq_wakeall(&b->bcm_waitq);
	}
 out:
	return (b);
}

#define MIN_CONNECT_RETRY_SECS 30

__static int
msl_ion_connect(lnet_nid_t nid, struct bmap_info_cli *c)
{
	int rc;

	psc_dbg("Creating new import to %s", 
		libcfs_nid2str(nid));

 retry:
	spinlock(&c->bmic_lock);	
       
	if (c->bmic_flags & BMIC_CONNECTED) {
		psc_assert(c->bmic_import);
		freelock(&c->bmic_lock);
		return (0);

	} else if (c->bmic_flags & BMIC_CONNECTING) {
		psc_waitq_wait(&c->bmic_waitq, &c->bmic_lock);
		goto retry;
	}
	
	if (c->bmic_flags & BMIC_CONNECT_FAIL) {
		struct timespec ts;
		
		clock_gettime(CLOCK_REALTIME, &ts);
		if ((ts.tv_sec - c->bmic_connect_time.tv_sec) < 
		    MIN_CONNECT_RETRY_SECS)
			return (-EAGAIN);
	} else 
		c->bmic_flags &= ~BMIC_CONNECT_FAIL;
	

	psc_assert(!c->bmic_import);
	psc_assert(!((c->bmic_flags & BMIC_CONNECTED) ||
		     (c->bmic_flags & BMIC_CONNECTING)));
	
	c->bmic_flags |= BMIC_CONNECTING;
	freelock(&c->bmic_lock);

	if ((c->bmic_import = new_import()) == NULL)
		psc_fatalx("new_import");
	
	c->bmic_import->imp_client = PSCALLOC(sizeof(struct pscrpc_client));
	c->bmic_import->imp_client->cli_request_portal = SRIC_REQ_PORTAL;
	c->bmic_import->imp_client->cli_reply_portal = SRIC_REP_PORTAL;
	clock_gettime(CLOCK_REALTIME, &c->bmic_connect_time);

	rc = rpc_issue_connect(nid, c->bmic_import,
			       SRIC_MAGIC, SRIC_VERSION);

	spinlock(&c->bmic_lock);
	if (rc) {
		psc_errorx("rpc_issue_connect() to %s", libcfs_nid2str(nid));
		PSCFREE(c->bmic_import->imp_client);
		c->bmic_import->imp_client = NULL;		
		c->bmic_flags |= BMIC_CONNECT_FAIL;
	} else
		c->bmic_flags |= BMIC_CONNECTED;

	c->bmic_flags &= ~BMIC_CONNECTING;
	psc_waitq_wakeall(&c->bmic_waitq);
	freelock(&c->bmic_lock);

	return (rc);
}
/**
 * msl_bmap_to_import - Given a bmap, perform a series of lookups to
 *	locate the ION import.  The ION was chosen by the mds and
 *	returned in the msl_bmap_fetch routine. msl_bmap_to_import
 *	queries the configuration to find the ION's private info - this
 *	is where the import pointer is kept.  If no import has yet been
 *	allocated a new is made.
 * @b: the bmap
 * Notes: the bmap is locked to avoid race conditions with import checking.
 *        the bmap's refcnt must have been incremented so that it is not freed from under us.
 * XXX Dev Needed: If the bmap is a read-only then any replica may be
 *	accessed (so long as it is recent).  Therefore
 *	msl_bmap_to_import() should have logic to accommodate this.
 */
struct pscrpc_import *
msl_bmap_to_import(struct bmapc_memb *b, int add)
{
	struct bmap_info_cli *c;
	sl_resm_t *r;
	int locked;

	/* Sanity check on the opcnt.
	 */
	psc_assert(atomic_read(&b->bcm_opcnt) > 0);

	locked = reqlock(&b->bcm_lock);
	r = libsl_nid2resm(bmap_2_msion(b));
	if (!r)
		psc_fatalx("Failed to lookup %s, verify that the slash configs"
			   " are uniform across all servers",
			   libcfs_nid2str(bmap_2_msion(b)));
	
	c = r->resm_pri;
	ureqlock(&b->bcm_lock, locked);

	if (!c->bmic_import && add)
		msl_ion_connect(bmap_2_msion(b), c);
	
	return (c->bmic_import);
}


struct pscrpc_import *
msl_bmap_choose_replica(struct bmapc_memb *b)
{
	struct bmap_info_cli *c;
	struct resprof_cli_info *r;
	struct msl_fcoo_data *mfd;
	sl_resource_t *res;
	sl_resm_t *resm;
	lnet_nid_t repl_nid;
	int n;
	
	psc_assert(atomic_read(&b->bcm_opcnt) > 0);
	psc_assert(b->bcm_fcmh->fcmh_fcoo);
	
	mfd = b->bcm_fcmh->fcmh_fcoo->fcoo_pri;
	psc_assert(mfd);
	/*  XXX need a more intelligent algorithm here.
	 */
	res = libsl_id2res(mfd->mfd_reptbl[0].bs_id);
	if (!res)
		psc_fatalx("Failed to lookup iosid %u, verify that the slash "
			   "configs are uniform across all servers",
			   mfd->mfd_reptbl[0].bs_id);

	r = res->res_pri;
	spinlock(&r->rci_lock);
	n = r->rci_cnt++;
	if (r->rci_cnt >= (int)res->res_nnids)
		n = r->rci_cnt = 0;
	repl_nid = res->res_nids[n];
	freelock(&r->rci_lock);

	psc_trace("trying res(%s) ion(%s)",
		  res->res_name, libcfs_nid2str(repl_nid));
	
	resm = libsl_nid2resm(repl_nid);
	if (!resm)
		psc_fatalx("Failed to lookup %s, verify that the slash configs"
			   " are uniform across all servers",
			   libcfs_nid2str(repl_nid));

	c = resm->resm_pri;
	msl_ion_connect(repl_nid, c);

	return (c->bmic_import);	
}


__static void
msl_oftrq_unref(struct offtree_req *r, int op)
{
	/* v->oftiov_memb can't be used for decref 
	 *   purposes because it may have been remapped to a different 
	 *   oft node.  NEvertheless, the oftrq has a pointer to the original
	 *   oft which has the correct ref cnt.  So all of these callbacks 
	 *   must be modified to use the oft from the request.
	 */
	struct dynarray *oftiovs=r->oftrq_darray;
	struct offtree_iov *v;
	struct offtree_memb *m;
	int i;

	DEBUG_OFT(PLL_INFO, r->oftrq_memb, "node");
	DEBUG_OFFTREQ(PLL_INFO, r, "request");

	for (i=0; i < dynarray_len(oftiovs); i++) {
		v = dynarray_getpos(oftiovs, i);
		
		DEBUG_OFFTIOV(PLL_INFO, v, "iov %d", i);
		/* Avoid racing with offtree_putnode which may reassign the 
		 *   v->oftiov_memb pointer.  Taking the lock here should prevent 
		 *   v->oftiov_memb from being assigned.
		 */
		spinlock(&v->oftiov_memb->oft_lock);
		m = v->oftiov_memb;		
		
		if (m == r->oftrq_memb) {
			if (op == SRMT_READ) {
				oftm_read_prepped_verify(m);
			} else {
				oftm_write_prepped_verify(m);
			}
		} else {
			DEBUG_OFFTIOV(PLL_TRACE, v, "no longer bound to..");
			DEBUG_OFT(PLL_TRACE, r->oftrq_memb, ".. no longer bound to");
			oftm_node_verify(r->oftrq_memb);
			oftm_leaf_verify(m);
		}
		
		switch (op) {
		case SRMT_READ:
			if (!(v->oftiov_flags & OFTIOV_DATARDY)) {
				psc_assert((v->oftiov_flags & OFTIOV_FAULTPNDG) &&
					   (v->oftiov_flags & OFTIOV_FAULTING));
				v->oftiov_flags &= ~(OFTIOV_FAULTING|OFTIOV_FAULTPNDG);
				/* Set datardy but leave OFT_READPNDG (in the oftm)
				 *   until the memcpy -> application buffer has
				 *   taken place.
				 */
				ATTR_SET(v->oftiov_flags, OFTIOV_DATARDY);
				DEBUG_OFFTIOV(PLL_TRACE, v, "OFTIOV_DATARDY");
			}
			break;

		case SRMT_WRITE:
			/* Manage the offtree leaf and decrement the slb.
			 */
			slb_pin_cb(v, SL_BUFFER_UNPIN);
			/* DATARDY must always be true here since the buffer
			 *  has already been copied to with valid data.
			 */
			psc_assert(v->oftiov_flags & OFTIOV_DATARDY);
			ATTR_UNSET(v->oftiov_flags, OFTIOV_PUSHING);
			ATTR_UNSET(v->oftiov_flags, OFTIOV_PUSHPNDG);
			DEBUG_OFFTIOV(PLL_TRACE, v, "PUSH DONE");
			break;

		default:
			psc_fatalx("How did this opcode(%d) happen?", op);
		}
		psc_assert(v->oftiov_memb == m);
		freelock(&m->oft_lock);
	}
	if (op == SRMT_WRITE)
		/* Have to use r->oftrq_memb so that we adjust the refcnt
		 *   of the offtree node who was responsible for the buffer
		 *   at the time of request creation which is not 
		 *   necessarily the current owner.
		 */
		oft_refcnt_dec(r, r->oftrq_memb);
}
/**
 * msl_io_cb - this function is called from the pscrpc layer as the RPC
 *	request completes.  Its task is to set various state machine
 *	flags and call into the slb layer to decref the inflight counter.
 *	On SRMT_WRITE the IOV's owning pin ref is decremented.
 * @rq: the rpc request.
 * @arg: opaque reference to pscrpc_async_args. pscrpc_async_args is
 *	where we stash pointers of use to the callback.
 * @status: return code from rpc.
 * Note:  'b' and 'r' are only used for debugging purposes.  msl_io_cb()
 *   solely relies on the offtree_iov for its work.
 */
int
msl_io_cb(struct pscrpc_request *rq, struct pscrpc_async_args *args)
{
	struct bmapc_memb  *b;
	struct dynarray     *a = args->pointer_arg[MSL_IO_CB_POINTER_SLOT];
	struct offtree_req  *r = args->pointer_arg[MSL_OFTRQ_CB_POINTER_SLOT];
	struct offtree_iov  *v;
	int op=rq->rq_reqmsg->opc, i;
	
	
	b = r->oftrq_bmap;
	psc_assert(b);
	
	psc_assert(op == SRMT_READ || op == SRMT_WRITE);
	psc_assert(a);

	DEBUG_BMAP(PLL_INFO, b, "callback");
	DEBUG_OFFTREQ(PLL_INFO, r, "callback bmap=%p", b);

	if (rq->rq_status) {
		DEBUG_REQ(PLL_ERROR, rq, "non-zero status status %d",
		    rq->rq_status);
		psc_fatalx("Resolve issues surrounding this failure");
		// XXX Freeing of dynarray, offtree state, etc
		return (rq->rq_status);
	}
	/* Call the inflight CB only on the iov's in the dynarray - 
	 *   not the iov's in the request since some of those may 
	 *   have already been staged in.
	 */
	for (i=0; i < dynarray_len(a); i++) {
                v = dynarray_getpos(a, i);
		slb_inflight_cb(v, SL_INFLIGHT_DEC);
	}

	msl_oftrq_unref(r, op);	
	/* Free the dynarray which was allocated in msl_pages_prefetch().
	 */
	PSCFREE(a);
	return (0);
}
 
 
int
msl_io_rpcset_cb_old(__unusedx struct pscrpc_request_set *set, void *arg, int rc)
{
	struct offtree_req *r=arg;

	msl_oftrq_destroy(r);

	return (rc);
}


int
msl_io_rpcset_cb(__unusedx struct pscrpc_request_set *set, void *arg, int rc)
{
	struct dynarray *oftrqs = arg;
	struct offtree_req *r;
	int i;
	
	for (i=0; i < dynarray_len(oftrqs); i++) {
		r = dynarray_getpos(oftrqs, i);
		msl_oftrq_unref(r, SRMT_WRITE);
		msl_oftrq_destroy(r);
	}
	PSCFREE(oftrqs);
	
	return (rc);
}


int
msl_io_rpc_cb(__unusedx struct pscrpc_request *req, struct pscrpc_async_args *args)
{
	struct dynarray *oftrqs;
	struct offtree_req *r;
	int i;

	oftrqs = args->pointer_arg[0];

	DEBUG_REQ(PLL_INFO, req, "oftrqs=%p", oftrqs);

	for (i=0; i < dynarray_len(oftrqs); i++) {
                r = dynarray_getpos(oftrqs, i);
		
                msl_oftrq_unref(r, SRMT_WRITE);
                msl_oftrq_destroy(r);
        }
	
	PSCFREE(oftrqs);

	return (0);
}


int
msl_dio_cb(struct pscrpc_request *rq, __unusedx struct pscrpc_async_args *args)
{
	struct srm_io_req *mq;
	int op=rq->rq_reqmsg->opc;

	psc_assert(op == SRMT_READ || op == SRMT_WRITE);

	mq = psc_msg_buf(rq->rq_reqmsg, 0, sizeof(*mq));
	psc_assert(mq);

	if (rq->rq_status) {
		DEBUG_REQ(PLL_ERROR, rq, "dio req, non-zero status %d",
		    rq->rq_status);
		psc_fatalx("Resolve issues surrounding this failure");
		// XXX Freeing of dynarray, offtree state, etc
		return (rq->rq_status);
	}
	DEBUG_REQ(PLL_TRACE, rq, "completed dio req (op=%d) o=%u s=%u",
	    op, mq->offset, mq->size);

	pscrpc_req_finished(rq);
	return (0);
}


__static void
msl_track_readreq(struct offtree_req *r)
{
	struct bmapc_memb *b=r->oftrq_bmap;

	psc_assert(psclist_disjoint(&r->oftrq_lentry));

        BMAP_LOCK(b);
	clock_gettime(CLOCK_REALTIME, &r->oftrq_start);
        bmap_oftrq_add_locked(r);
	BMAP_ULOCK(b);
}

/**
 * msl_pagereq_finalize - this function is the intersection point of many
 *	slash subssytems (pscrpc, sl_config, and bmaps).  Its job is to
 *	prepare a reqset of read or write requests and ship them to the
 *	correct I/O server.
 * @r:  the offtree request.
 * @a:  the array of iov's involved.
 * @op: GET or PUT.
 */
__static void
msl_pagereq_finalize(struct offtree_req *r, struct dynarray *a, int op)
{
	struct pscrpc_import      *imp;
	struct pscrpc_request_set *rqset = NULL;
	struct pscrpc_request     *req;
	struct pscrpc_bulk_desc   *desc;
	struct bmapc_memb    	  *bcm;
	struct msbmap_data    	  *msbd;
	struct iovec              *iovs;
	struct offtree_iov        *v;
	struct srm_io_req         *mq;
	struct srm_io_rep         *mp;
	int    i, n=dynarray_len(a), tblks=0, rc=0;

	v = NULL; /* gcc */
	psc_assert(n);
	psc_assert(op == MSL_PAGES_PUT || op == MSL_PAGES_GET);
	
	if (op == MSL_PAGES_GET)
		msl_track_readreq(r);
	/* Get a new request set if one doesn't already exist.
	 */
	if (!r->oftrq_fill.oftfill_reqset)
		rqset = r->oftrq_fill.oftfill_reqset = pscrpc_prep_set();

	psc_assert(rqset);
	/* Initialize the request set.
	 */
	if (op == MSL_PAGES_PUT) {
		rqset->set_interpret = msl_io_rpcset_cb_old;
		rqset->set_arg = r;
	}
	/* Point to our bmap handle, it has the import information needed
	 *  for the RPC request.  (FID and ION id's)
	 */
	bcm = r->oftrq_bmap;
	msbd = bcm->bcm_pri;

	imp = (op == MSL_PAGES_PUT ? msl_bmap_to_import(bcm, 1) : 
	       msl_bmap_choose_replica(bcm));
	
	/* This pointer is only valid in DIO mode.
	 */
	psc_assert(r->oftrq_bmap);

	if ((rc = RSX_NEWREQ(imp, SRIC_VERSION,
			     (op == MSL_PAGES_PUT ? SRMT_WRITE : SRMT_READ),
			     req, mq, mp)) != 0) {
		errno = -rc;
		psc_fatalx("RSX_NEWREQ() bad time to fail :(");
	}
	/* Setup the callback, supplying the dynarray as a argument.
	 */
	req->rq_interpret_reply = msl_io_cb;
	req->rq_async_args.pointer_arg[MSL_IO_CB_POINTER_SLOT] = a;
	req->rq_async_args.pointer_arg[MSL_OFTRQ_CB_POINTER_SLOT] = r;

	/* Prep the iovs and bulk descriptor
	 */
	iovs = PSCALLOC(sizeof(*iovs) * n);
	for (i=0; i < n; i++) {
		v = dynarray_getpos(a, i);

		if (!i)
			mq->offset = v->oftiov_off;

		if (op == MSL_PAGES_GET) {
			psc_assert(v->oftiov_flags &  OFTIOV_FAULTPNDG);
			v->oftiov_flags |= OFTIOV_FAULTING;
		} else {
			psc_assert(v->oftiov_flags &  OFTIOV_PUSHPNDG);
			v->oftiov_flags |= OFTIOV_PUSHING;
		}

		tblks += v->oftiov_nblks;
		DEBUG_OFFTIOV(PLL_TRACE, v,
			      "iov%d tblks=%d off=%u OFT_IOV2E_OFF_(%zu)",
			      i, tblks, mq->offset, OFT_IOV2E_OFF_(v));

		/* Make an iov for lnet.
		 */
		oftiov_2_iov(v, &iovs[i]);
		/* Tell the slb layer that this offtree_iov is going
		 *   for a ride.
		 */
		slb_inflight_cb(v, SL_INFLIGHT_INC);
	}
	//mq->size = v->oftiov_blksz * tblks;
	mq->size = r->oftrq_len;
	psc_warnx("mq->sz == %u", mq->size);
	mq->op = (op == MSL_PAGES_PUT ? SRMIO_WR : SRMIO_RD);
	memcpy(&mq->sbdb, &msbd->msbd_bdb, sizeof(mq->sbdb));

	/* Seems counter-intuitive, but it's right.  MSL_PAGES_GET is a
	 * 'PUT' to the client, MSL_PAGES_PUSH is a server get.
	 */
	rc = rsx_bulkclient(req, &desc,
			    (op == MSL_PAGES_GET ?
			     BULK_PUT_SINK : BULK_GET_SOURCE),
			    SRIC_BULK_PORTAL, iovs, n);
	if (rc)
		psc_fatalx("rsx_bulkclient() failed with %d", rc);

	/* The bulk descriptor copies these iovs so it's OK to free them.
	 */
	PSCFREE(iovs);
	/* Push onto our request set and send it out the door.
	 */
	pscrpc_set_add_new_req(rqset, req);
	if (pscrpc_push_req(req)) {
		DEBUG_REQ(PLL_ERROR, req, "pscrpc_push_req() failed");
		psc_fatalx("pscrpc_push_req(), no failover yet");
	}
}

__static void
msl_pages_dio_getput(struct offtree_req *r, char *b)
{
	struct pscrpc_import      *imp;
	struct pscrpc_request_set *rqset;
	struct pscrpc_request     *req;
	struct pscrpc_bulk_desc   *desc;
	struct bmapc_memb    	  *bcm;
	struct msbmap_data    	  *msbd;
	struct iovec              *iovs;
	struct srm_io_req         *mq;
	struct srm_io_rep         *mp;

	size_t len, nbytes, size=oftrq_size_get(r);
	int i, op, n=0, rc=1;

	psc_assert(ATTR_TEST(r->oftrq_op, OFTREQ_OP_DIO));
	psc_assert(!r->oftrq_root);
	psc_assert(r->oftrq_bmap);
	psc_assert(size);

	bcm = r->oftrq_bmap;
	msbd = bcm->bcm_pri;

	op = (ATTR_TEST(r->oftrq_op, OFTREQ_OP_WRITE) ?
	      SRMT_WRITE : SRMT_READ);

	imp = msl_bmap_to_import(bcm, 1);
	rqset = pscrpc_prep_set();
	/* This buffer hasn't been segmented into LNET sized
	 *  chunks.  Set up buffers into 1MB chunks or smaller.
	 */
	n = (r->oftrq_len / LNET_MTU) + ((r->oftrq_len % LNET_MTU) ? 1 : 0);

	iovs = PSCALLOC(sizeof(*iovs) * n);

	for (i=0, nbytes=0; i < n; i++, nbytes += len) {
		len = MIN(LNET_MTU, (size-nbytes));

		rc = RSX_NEWREQ(imp, SRIC_VERSION, op, req, mq, mp);
		if (rc)
			psc_fatalx("RSX_NEWREQ() failed %d", rc);

		req->rq_interpret_reply = msl_dio_cb;

		iovs[i].iov_base = b + nbytes;
		iovs[i].iov_len  = len;

		rc = rsx_bulkclient(req, &desc,
				    (op == SRMT_WRITE ?
				     BULK_GET_SOURCE : BULK_PUT_SINK),
				    SRIC_BULK_PORTAL, &iovs[i], 1);
		if (rc)
			psc_fatalx("rsx_bulkclient() failed %d", rc);

		mq->offset = r->oftrq_off + nbytes;
		mq->size = len;
		mq->op = (op == SRMT_WRITE ? SRMIO_WR : SRMIO_RD);
		memcpy(&mq->sbdb, &msbd->msbd_bdb, sizeof(mq->sbdb));

		pscrpc_set_add_new_req(rqset, req);
		if (pscrpc_push_req(req)) {
			DEBUG_REQ(PLL_ERROR, req, "pscrpc_push_req() failed");
			psc_fatalx("pscrpc_push_req(), no failover yet");
		}
	}
	psc_assert(nbytes == size);
	pscrpc_set_wait(rqset);
	pscrpc_set_destroy(rqset);
	PSCFREE(iovs);
}

/**
 * msl_pages_track_pending - called when an IOV is being faulted in by
 *	another thread.  msl_pages_track_pending() saves a reference to
 *	the ongoing RPC so that later (msl_pages_blocking_load()) it may
 *	be checked for completion.
 * @r: offtree_req to which the iov is attached.
 * @v: the iov being tracked.
 * Notes:  At this time no retry mechanism is in place.
 */
__static void
msl_pages_track_pending(struct offtree_req *r, struct offtree_iov *v)
{
	struct offtree_memb *m = (struct offtree_memb *)v->oftiov_memb;
	
	DEBUG_OFFTREQ(PLL_WARN, r, "pending...");
	DEBUG_OFFTIOV(PLL_WARN, v, "...pending");
	/* This would be a problem..
	 */
	psc_assert(!ATTR_TEST(v->oftiov_flags, OFTIOV_DATARDY));
	/* There must be at least 2 pending operations
	 *  (ours plus the one who set OFTIOV_FAULT*)
	 */
	//psc_assert(atomic_read(&m->oft_rdop_ref) > 1);
	psc_assert(atomic_read(&m->oft_rdop_ref) > 0);
	/* Allocate a new 'in progress' dynarray if
	 *  one does not already exist.
	 */
	if (!r->oftrq_fill.oftfill_inprog)
		r->oftrq_fill.oftfill_inprog =
		    PSCALLOC(sizeof(struct dynarray)); /* XXX not freed */
	/* This iov is being loaded in by another
	 *  thread, place it in our 'fill' structure
	 *  and check on it later.
	 */
	dynarray_add(r->oftrq_fill.oftfill_inprog, v);
}

__static void 
msl_pages_schedflush(struct offtree_req *r)
{
       	struct bmapc_memb *b=r->oftrq_bmap;
	int put_dirty=0;

	psc_assert(psclist_disjoint(&r->oftrq_lentry));

	BMAP_LOCK(b);

	if (b->bcm_mode & BMAP_DIRTY)
		psc_assert(!psclist_empty(&bmap_2_msbd(b)->msbd_oftrqs));

	else if (b->bcm_mode & BMAP_DIRTY2LRU) 
		/* Deal with a race which can occur with bmap_oftrq_del() due to 
		 *    his requirement to drop the bmap lock prior to removing 
		 *    the bmap from the bmapFlushQ listcache.  By replacing the 
		 *    dirty flag bmap_oftrq_del() will leave the bmap on the 
		 *    bmap_oftrq_del().
		 */
		b->bcm_mode |= BMAP_DIRTY;

	else {
		/* Set it to dirty and hand it off to the bmap flush thread.
		 */		
		psc_assert(psclist_empty(&bmap_2_msbd(b)->msbd_oftrqs));
		psc_assert(psclist_disjoint(&bmap_2_msbd(b)->msbd_lentry));
		b->bcm_mode |= BMAP_DIRTY;
		put_dirty = 1;
	}
	clock_gettime(CLOCK_REALTIME, &r->oftrq_start);
	bmap_oftrq_add_locked(r);

	BMAP_ULOCK(b);

	if (put_dirty)
		lc_addtail(&bmapFlushQ, bmap_2_msbd(b));
}

/**
 * msl_pages_getput - called to stage pages in or out of the client-side
 *	cache.  msl_pages_getput() does the proper coalescing for puts
 *	and gets, managing the semantics and dynarray allocation for
 *	each.  Dynarray's allocated here are freed in the msl_io_cb.
 *
 *	On MSL_PAGES_GET, msl_pages_getput() checks the cache for valid
 *	pages, pages being loaded by other threads are tracked via
 *	msl_pages_track_pending() and blocked-on for completion.  IOV's
 *	which are already in 'DATARDY' state are protected by
 *	oft_rdop_ref which was inc'd in offtree_region_preprw().
 *	msl_pages_copyout() does the decref on oft_rdop_ref.
 * @r: the request.
 * @op: used by the below macros.
 * Notes: both MSL_PAGES_PUT and MSL_PAGES_GET call msl_pagereq_finalize()
 *	to issue the necessary RPC requests (which are gathered within the
 *	rpcset attached to the oftrq.
 */
#define msl_pages_prefetch(r) msl_pages_getput(r, MSL_PAGES_GET)
#define msl_pages_flush(r)    msl_pages_getput(r, MSL_PAGES_PUT)

__static void
msl_pages_getput(struct offtree_req *r, int op)
{
	struct offtree_iov  *v;
	struct offtree_memb *m;
	struct dynarray     *a=r->oftrq_darray, *coalesce=NULL;
	off_t                o=OFT_REQ_SOFFA(r), toff=0;
	size_t               niovs=dynarray_len(a);
	u32                  i, n;
	int                  nc_blks=0, t=0;

#define launch_cb {					\
		psc_assert(coalesce);			\
		msl_pagereq_finalize(r, coalesce, op);	\
		coalesce = NULL;			\
		nc_blks = 0;				\
	}

#define new_cb {					\
		psc_assert(!nc_blks);			\
		psc_assert(!coalesce);			\
		coalesce = PSCALLOC(sizeof(*coalesce)); \
	}

	psc_assert(!ATTR_TEST(r->oftrq_op, OFTREQ_OP_DIO));

	for (i=0; i < niovs; i++, n=0) {
		v = dynarray_getpos(a, i);
		DEBUG_OFFTIOV(PLL_TRACE, v,
			      "iov%d rq_off=%zu OFT_IOV2E_OFF_(%zu)",
			      i, r->oftrq_off,  OFT_IOV2E_OFF_(v));

		/* Assert that oft iovs are contiguous.
		 */
		if (i)
			psc_assert(toff == v->oftiov_off);

		toff = OFT_IOV2E_OFF_(v) + 1;
		/* The dynarray may contain iov's that we don't need,
		 *  skip them.
		 */
		if (o > OFT_IOV2E_OFF_(v))
			continue;

		/* On read-before-write, only check first and last iov's
		 *  if they have been specified for retrieval.
		 * 't' is used to denote the first valid iov.
		 */
		if ((op == MSL_PAGES_GET) &&
		    (ATTR_TEST(r->oftrq_op, OFTREQ_OP_WRITE))) {
			if (!(((t == 0 &&
				ATTR_TEST(r->oftrq_op, OFTREQ_OP_PRFFP))) ||
			      ((i == niovs - 1) &&
			       ATTR_TEST(r->oftrq_op, OFTREQ_OP_PRFLP))))
				continue;
			else
				if (!t)
					t = 1;
		}
		/* Map the offtree leaf holding this iov.
		 */
		m = (struct offtree_memb *)v->oftiov_memb;
		DEBUG_OFFTIOV(PLL_INFO, v, "processing..");
		DEBUG_OFT(PLL_INFO, m, "..processing");

		spinlock(&m->oft_lock);
		/* Ensure the offtree leaf is sane.
		 */
		if (op == MSL_PAGES_GET) {
			oftm_read_prepped_verify(m);
		} else {
			oftm_write_prepped_verify(m);
		}

		//nblks += v->oftiov_nblks;
		/* oftiov_nblks cannot be bigger than slCacheNblks!
		 */
		psc_assert(v->oftiov_nblks <= (u32)slCacheNblks);
		psc_assert(nc_blks < slCacheNblks);
		/* Iov's cannot be split across requests but they may be
		 *  coalesced into one request.  We're assured that the
		 *  largest contiguous piece of memory is <= to LNET_MTU.
		 * For MSL_PAGES_PUT, the entire array is always sent.
		 */
		if (op == MSL_PAGES_PUT ||
		    ((op == MSL_PAGES_GET) &&
		     (!ATTR_TEST(v->oftiov_flags, OFTIOV_DATARDY) &&
		      !ATTR_TEST(v->oftiov_flags, OFTIOV_FAULTPNDG)))) {
			/* Mark it.
			 */
			if (op == MSL_PAGES_GET)
				ATTR_SET(v->oftiov_flags, OFTIOV_FAULTPNDG);
			else
				ATTR_SET(v->oftiov_flags, OFTIOV_PUSHPNDG);

			DEBUG_OFFTIOV(PLL_TRACE, v,
				      "iov%d rq_off=%zu OFT_IOV2E_OFF_(%zu)",
				      i, r->oftrq_off,  OFT_IOV2E_OFF_(v));

			if ((int)(v->oftiov_nblks + nc_blks) < slCacheNblks) {
				/* It fits, coalesce.
				 */
				if (!coalesce)
					new_cb;

				dynarray_add(coalesce, v);
				nc_blks += v->oftiov_nblks;
				/* Just to be sure..
				 */
				psc_assert(nc_blks < slCacheNblks);

			} else if ((int)(v->oftiov_nblks + nc_blks) ==
				   slCacheNblks) {
				/* A perfect fit, send this one out.
				 */
				dynarray_add(coalesce, v);
				nc_blks += v->oftiov_nblks;
				launch_cb;

			} else {
				/* The new iov won't fit, send out the cb as
				 *  is.  Launch the current coalesce buffer
				 *  (which must exist).
				 */
				launch_cb;
				/* Make a new cb and add the new iov.
				 */
				new_cb;
				dynarray_add(coalesce, v);
				/* The new one may constitute an entire I/O,
				 *  if so send it on its merry way.
				 */
				if (v->oftiov_nblks == (u32)slCacheNblks) {
					launch_cb;
				} else
					nc_blks = v->oftiov_nblks;
			}
		} else {
			/* This iov is being or already has been handled which
			 *  means that any existing coalesce buffer must be
			 *  pushed.
			 */
			psc_assert(op == MSL_PAGES_GET);

			if (coalesce)
				launch_cb;

			DEBUG_OFFTIOV(PLL_TRACE, v,
				      "iov%d rq_off=%zu OFT_IOV2E_OFF_(%zu)",
				      i, r->oftrq_off,  OFT_IOV2E_OFF_(v));

			if (ATTR_TEST(v->oftiov_flags, OFTIOV_FAULTPNDG))
				msl_pages_track_pending(r, v);
			else
				psc_assert(ATTR_TEST(v->oftiov_flags,
						     OFTIOV_DATARDY));
		}
		/* Finished testing / setting attrs, release the lock.
		 */
		freelock(&m->oft_lock);
	}
	/* There may be a cb lingering.
	 */
	if (coalesce)
		launch_cb;
}

/**
 * msl_pages_copyin - copy user pages into buffer cache and schedule the 
 *    slabs to be sent to the IOS backend.
 * @r: array of request structs.
 * @buf: the source (application) buffer.
 * Notes:  the iov's (contained in the offtree_req dynarray) are unpinned 
 *    via msl_io_cb.
 */
__static void
msl_pages_copyin(struct offtree_req *r, char *buf)
{
	struct dynarray     *a;
	struct offtree_iov  *v;
	struct offtree_memb *m;
	int    i, n, x=0;
	off_t  roff=(r->oftrq_off & SLASH_BMAP_BLKMASK);
	size_t nbytes;
	ssize_t tsize;
	char  *b, *p;
	int    rbw=0;

	p = buf;
	tsize = oftrq_size_get(r);
	a = r->oftrq_darray;
	n = dynarray_len(a);

	for (i=0; i < n; i++) {
		v = dynarray_getpos(a, i);
		m = (struct offtree_memb *)v->oftiov_memb;

		if (!tsize)
			break;

		/* Set the starting buffer pointer into
		 *  our cache vector.
		 */
		b = (char *)v->oftiov_base;

		m = (struct offtree_memb *)v->oftiov_memb;
		spinlock(&m->oft_lock);
		oftm_write_prepped_verify(m);
		if (!x) {
			/* The first iovs are not always needed.
			 */
			if (r->oftrq_off > OFT_IOV2E_OFF_(v)) {
				freelock(&m->oft_lock);
				continue;
			}
			/* Check for offset alignment, the latter half 
			 *    of this stmt checks the end of the write.
			 */
			if ((r->oftrq_flags & OFTREQ_OP_PRFLP) ||
			    (r->oftrq_flags & OFTREQ_OP_PRFFP)) {
				/* Partial iov writes must have been
				 *  pre-faulted (ie. read).
				 */
				oftm_read_prepped_verify(m);
				/* Verify that the iov is DATARDY.
				 */
				psc_assert(v->oftiov_flags & OFTIOV_DATARDY);
				/* Set the read-before-write notifier
				 *  (first iov).
				 */
				rbw = 1;
				/* Bump our cache vector destination
				 *  pointer.
				 */
			}
			if (roff)
				b += roff;

			x = 1;
			nbytes = MIN(OFT_IOVSZ(v) - roff, tsize);

		} else if (i == (n-1)) {
			/* Last iov, if the write size is smaller than
			 *  the iov the data must have been faulted in.
			 * Note:  This does not need to be run in
			 *   addition to the above 'if' since all that
			 *   really needs to happen here is to check
			 *   for OFTIOV_DATARDY and set rbw=1.
			 */
			if (tsize < OFT_IOVSZ(v)) {
				oftm_read_prepped_verify(m);
				psc_assert(ATTR_TEST(v->oftiov_flags,
						     OFTIOV_DATARDY));
				/* Set the read-before-write notifier.
				 *  (last iov).
				 */
				rbw = 1;
			}
			nbytes = MIN(OFT_IOVSZ(v), tsize);

		} else {
			psc_assert(tsize >= OFT_IOVSZ(v));
			nbytes = MIN(OFT_IOVSZ(v), tsize);
		}

		DEBUG_OFFTIOV(PLL_NOTIFY, v, "iov%d rq_off=%zu " 
			      "OFT_IOV2E_OFF_(%zu) bufp=%p oft_bufsz=%zu "
			      "tsz=%zd nbytes=%zu t=%zu",
			      i, r->oftrq_off, OFT_IOV2E_OFF_(v),
			      p, oftrq_size_get(r), tsize, nbytes, roff);
		/* Do the deed.
		 */
		memcpy(b, p, nbytes);
		/* Notify others that this buffer is now valid for
		 *  reads or unaligned writes.  Note that the buffer
		 *  must still be pinned for it has not been sent yet.
		 */
		if (rbw) {
			psc_assert(ATTR_TEST(v->oftiov_flags,
					     OFTIOV_DATARDY));
			/* Drop the rbw reference.
			 */
			atomic_dec(&m->oft_rdop_ref);
			rbw = 0;
		} else
			ATTR_SET(v->oftiov_flags, OFTIOV_DATARDY);

		freelock(&m->oft_lock);

		p += nbytes;
		tsize -= nbytes;
		/* XXX the details of this wakeup may need to be
		 *  sorted out.
		 */
		psc_waitq_wakeall(&m->oft_waitq);
	}
	/* Queue these iov's for send to IOS.
	 */
	msl_pages_schedflush(r);
	psc_assert(!tsize);
}


/**
 * msl_pages_copyout - copy pages to the user application buffer,
 *	release rdop references held in the offtree_memb, and unpin the
 *	iov's owning slabs.  Also sets the IOV to DATARDY so that other
 *	threads may access the data cached there.
 * @r: the offtree req array.
 * @buf: application source buffer.
 * @off: file-logical offset.
 */
__static void
msl_pages_copyout(struct offtree_req *r, char *buf)
{
	struct dynarray     *a;
	struct offtree_iov  *v;
	struct offtree_memb *m;
	int    n, j, x=0;
	off_t  t;
	size_t nbytes;
	ssize_t tsize;
	char  *b, *p;
	
	psc_assert(r->oftrq_darray);
	/* Relative offset into the buffer.
	 */
	tsize = oftrq_size_get(r);
	/* Remember that oftrq_off is the aligned starting offset but
	 *  not necessarily the start of the buffers in the dynarray.
	 * 't' gives the offset into the first usable iov.
	 */
	t = (r->oftrq_off & SLASH_BMAP_BLKMASK);
	p = buf;
	a = r->oftrq_darray;      
	n = dynarray_len(a);

	for (j=0; j < n; j++) {
		v = dynarray_getpos(a, j);
		b = (char *)v->oftiov_base;

		if (!x) {
			if (r->oftrq_off > OFT_IOV2E_OFF_(v))
				/* These pages aren't involved, skip.
				 */
				continue;
			x  = 1;
			b += t;
			nbytes = MIN(OFT_IOVSZ(v) - t, tsize);
		} else
			nbytes = MIN(OFT_IOVSZ(v), tsize);

		DEBUG_OFFTIOV(PLL_TRACE, v, "iov%d rq_off=%zu "
		      "OFT_IOV2E_OFF_(%zu) bufp=%p sz=%zu tsz=%zd nbytes=%zu",
		      j, r->oftrq_off, OFT_IOV2E_OFF_(v), p, oftrq_size_get(r),
		      tsize, nbytes);

		//m = (struct offtree_memb *)v->oftiov_memb;
		m = r->oftrq_memb;
		spinlock(&m->oft_lock);
		/* The below check fails due to offtree memb promotion.
		 */
		//oftm_read_prepped_verify(m);
		psc_assert(ATTR_TEST(v->oftiov_flags, OFTIOV_DATARDY));
		freelock(&m->oft_lock);

		memcpy(p, b, nbytes);
		p += nbytes;
		tsize -= nbytes;
		/* As far as we're concerned, we're done with all vital
		 *  operations on this iov.  It can go away if needed.
		 * Manage the offtree leaf and decrement the slb.
		 */
		spinlock(&v->oftiov_memb->oft_lock);
		slb_pin_cb(v, SL_BUFFER_UNPIN);		
		atomic_dec(&m->oft_rdop_ref);
		freelock(&v->oftiov_memb->oft_lock);
		/* XXX the details of this wakeup may need to be
		 *  sorted out.  The main thing to know is that there
		 *  are several sleepers / wakers accessing this q
		 *  for various reasons.
		 */
		psc_waitq_wakeall(&m->oft_waitq);
		if (!tsize)
			break;
	}
	psc_assert(!tsize);
	msl_oftrq_destroy(r);
}

/**
 * msl_pages_blocking_load - manage data prefetching activities.  This
 *	includes waiting on other thread to complete RPC for data in
 *	which we're interested.
 * @r: array of offtree requests.
 * @n: number of offtree reqs.
 */
__static int
msl_pages_blocking_load(struct offtree_req *r)
{
	struct offtree_fill *f = &r->oftrq_fill;
	struct psc_waitq *w;
	int i, rc=0;

	psc_assert(!(r->oftrq_op & OFTREQ_OP_DIO));
	/* If a request set is present then block on its completion.
	 */
	if (f->oftfill_reqset) {
		if ((rc = pscrpc_set_wait(f->oftfill_reqset))) {
			psc_error("pscrpc_set_wait rc=%d", rc);
			return (rc);
		}
	}
	/* Wait for our friends to finish their page-ins which we
	 *  are also blocking on.
	 */
	if (!f->oftfill_inprog)
		return (rc);

 retry:
	for (i=0; i < dynarray_len(f->oftfill_inprog); i++) {
		struct offtree_iov  *v = dynarray_getpos(f->oftfill_inprog, i);
		struct offtree_memb *m = v->oftiov_memb;

		w = &m->oft_waitq;
		spinlock(&m->oft_lock);
		/* Has the other thread finished working on this?
		 */
		if (!ATTR_TEST(v->oftiov_flags, OFTIOV_DATARDY)) {
			psc_waitq_wait(w, &m->oft_lock);
			goto retry;
		} else
			freelock(&m->oft_lock);
	}
	return (rc);
}

/**
 * msl_io - I/O gateway routine which bridges FUSE and the slash2 client
 *	cache and backend.  msl_io() handles the creation of offtree_req's
 *	and the loading of bmaps (which are attached to the file's
 *	fcache_memb_handle and is ultimately responsible for data being
 *	prefetched (as needed), copied into or from the cache, and (on
 *	write) being pushed to the correct io server.
 * @fh: file handle structure passed to us by FUSE which contains the
 *	pointer to our fcache_memb_handle *.
 * @buf: the application source/dest buffer.
 * @size: size of buffer.
 * @off: file logical offset similar to pwrite().
 * @op: the operation type (MSL_READ or MSL_WRITE).
 */
int
msl_io(struct msl_fhent *mfh, char *buf, size_t size, off_t off, int op)
{
#define MAX_BMAPS_REQ 4
	struct offtree_req *r[MAX_BMAPS_REQ];
	struct bmapc_memb *b;
	sl_blkno_t s, e;
	size_t tlen, tsize=size;
	off_t roff;
	int nr, j, rc;
	char *p;

	psc_assert(mfh);
	psc_assert(mfh->mfh_fcmh);
	/* Are these bytes in the cache?
	 *  Get the start and end block regions from the input parameters.
	 */
	//XXX beware, I think 'e' can be short by 1.
	s = off / mslfh_2_bmapsz(mfh);
	e = (off + size) / mslfh_2_bmapsz(mfh);
	
	if ((e - s) > MAX_BMAPS_REQ)
		return (-EINVAL);
	/* Relativize the length and offset (roff is not aligned).
	 */
	roff  = off - (s * SLASH_BMAP_SIZE);
	/* Length of the first bmap request.
	 */
	tlen  = MIN((size_t)(SLASH_BMAP_SIZE - roff), size);
	/* Foreach block range, get its bmap and make a request into its
	 *  offtree.  This first loop retrieves all the pages.
	 */
	for (nr=0; s <= e; s++, nr++) {		
		DEBUG_FCMH(PLL_INFO, mfh->mfh_fcmh, 
			   "sz=%zu tlen=%zu off=%"PRIdOFF" roff=%"PRIdOFF" op=%d", 
			   tsize, tlen, off, roff, op);
		/* Load up the bmap, if it's not available then we're out of
		 *  luck because we have no idea where the data is!
		 */
		b = msl_bmap_load(mfh, s, (op == MSL_READ) ?
				  SRIC_BMAP_READ : SRIC_BMAP_WRITE);
		if (!b) {
			rc = -EIO;
			goto out;
		}
		/* Malloc offtree request and pass to the initializer.
		 */
		msl_oftrq_build(&r[nr], b, roff, tlen,
				(op == MSL_READ) ? OFTREQ_OP_READ :
				OFTREQ_OP_WRITE);
		/* Retrieve offtree region.
		 */
		if (!(r[nr]->oftrq_op & OFTREQ_OP_DIO)) {
			if ((rc = offtree_region_preprw(r[nr]))) {
				psc_error("offtree_region_preprw rc=%d", rc);
				goto out;
			}
			/* Start prefetching our cached buffers.
			 */
			msl_pages_prefetch(r[nr]);
		}
		roff += tlen;
		tsize -= tlen;
		tlen  = MIN(SLASH_BMAP_SIZE, tsize);
	}

	/* Note that the offsets used here are file-wise offsets not
	 *   offsets into the buffer.
	 */
	for (j=0, p=buf; j < nr; j++, p+=tlen) {
		if (!(r[j]->oftrq_op & OFTREQ_OP_DIO)) {
			/* Now iterate across the array of completed offtree
			 *  requests paging in data where needed.
			 */
			if (op == MSL_READ ||
			    ((r[j]->oftrq_op & OFTREQ_OP_PRFFP) ||
			     (r[j]->oftrq_op & OFTREQ_OP_PRFLP)))
				if ((rc = msl_pages_blocking_load(r[j])))
					goto out;

			if (op == MSL_READ)
				/* Copying into the application buffer, and
				 *   managing the offtree.
				 */
				msl_pages_copyout(r[j], p);
			else
				/* Copy pages into the system cache and queue
				 *  them for xfer to the IOS.
				 */
				msl_pages_copyin(r[j], p);
		} else
			/* The directio path.
			 */
			msl_pages_dio_getput(r[j], p);
	}
	rc = size;
 out:
	return (rc);
}

