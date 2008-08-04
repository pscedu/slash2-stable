/* $Id$ */

#include "psc_util/assert.h"
#include "psc_util/atomic.h"
#include "psc_util/alloc.h"
#include "psc_ds/tree.h"

#include "cache_params.h"
#include "mds.h"
#include "mdsexpc.h"
#include "fidcache.h"
#include "../slashd/rpc.h"
#include "../slashd/cfd.h"

struct sl_fsops mdsFsops;

extern list_cache_t pndgCacheCbs;


__static SPLAY_GENERATE(fcm_exports, mexpfcm, 
			mexpfcm_fcm_tentry, mexpfcm_cache_cmp);

__static SPLAY_GENERATE(exp_bmaptree, mexpbcm, 
			mexpbcm_exp_tentry, mexpbmap_cache_cmp);

__static SPLAY_GENERATE(bmap_exports, mexpbcm, 
			mexpbcm_bmap_tentry, mexpbmap_cache_cmp);

void
fidc_mds_handle_init(void *p)
{
        struct fidc_memb_handle *f=p;
        struct fidc_mds_info *i;

        /* Private data must have already been freed and the pointer nullified.
	 */
        psc_assert(!f->fcmh_pri);
        /* Call the common initialization handler.
	 */
        fidc_gen_handle_init(f);
        /* Here are the 'mds' specific calls.
	 */
        f->fcmh_pri = PSCALLOC(sizeof(*i));
        SPLAY_INIT(&f->fmdsi_exports);
        atomic_set(&f->fmdsi_ref, 0);
}

void
mexpfcm_new(struct cfdent *c, struct pscrpc_export *e) {
	struct mexpfcm *m = PSCALLOC(sizeof(*m));
	struct fidc_memb_handle *f;
	struct fidc_mds_info *i;
	struct slashrpc_export *sexp;
	struct mexp_cli *mexp_cli;

	LOCK_ENSURE(&e->exp_lock);
	sexp = e->exp_private;
	psc_assert(sexp);	
	/* Allocate the private data if it does not already exist and 
	 *  initialize the import for callback rpc's.
	 */
	if (sexp->sexp_data) {
		psc_assert(sexp->sexp_type == MDS_CLI_EXP);
		mexp_cli = sexp->sexp_data;
		psc_assert(mexp_cli->mc_csvc);
	} else {
		sexp->sexp_type = MDS_CLI_EXP;
		mexp_cli = sexp->sexp_data = PSCALLOC(sizeof(*mexp_cli));
		mexp_cli->mc_csvc = rpc_csvc_create(SRCM_REQ_PORTAL, 
						    SRCM_REP_PORTAL);
		if (!mexp_cli->mc_csvc)
			psc_fatal("rpc_csvc_create() failed");
		/* See how this works out, I'm just going to borrow 
		 *   the export's  connection structure.
		 */
		psc_assert(e->exp_connection);
		mexp_cli->mc_csvc->csvc_import->imp_connection = 
			e->exp_connection;
	}

	SPLAY_INIT(&m->mecm_bmaps);
	/* Serialize access to our bmap cache tree.
	 */
	LOCK_INIT(&m->mecm_lock);
	/* Back pointer to our export.
	 */
	m->mecm_export = e;
	/* Locate our fcmh in the global cache, fidc_lookup_immns()
	 *  bumps the refcnt.  
	 */
	m->mecm_fcmh = f = fidc_lookup_immns(c->fid);
	/* Sanity for fcmh - if the cfdent has been found then the inode
	 *  should have already been created.
	 */
	psc_assert(f);
	psc_assert(atomic_read(&f->fcmh_refcnt) > 0);
	psc_assert(f->fcmh_pri);
	psc_assert(m->mecm_fcmh);
	/* Add ourselves to the fidc_mds_info structure's splay tree.
	 */
	i = f->fcmh_pri;
	spinlock(&f->fcmh_lock);
	atomic_inc(&i->fmdsi_ref);
	if (SPLAY_INSERT(fcm_exports, &i->fmdsi_exports, m))
		psc_fatalx("Tried to reinsert m(%p) "FIDFMT, 
			   m, FID_FMT_ARGS(&(mexpfcm2fidgen(m))));
	freelock(&f->fcmh_lock);
	/* Add the fidcache reference to the cfd's private slot.
	 */
	c->pri = m;
}

void
mexpfcm_free(struct cfdent *c, struct pscrpc_export *e)
{	
	struct mexpfcm *m=c->pri;
	struct fidc_memb_handle *f=m->mecm_fcmh;
	struct fidc_mds_info *i=f->fcmh_pri;

	spinlock(&m->mecm_lock);
	psc_assert(!(m->mecm_flags & MEXPFCM_CLOSING));
	m->mecm_flags |= MEXPFCM_CLOSING;
	/* Verify that all of our bmap references have already been freed.
	 */
	psc_assert(SPLAY_EMPTY(&m->mecm_bmaps));
	freelock(&m->mecm_lock);
		
	spinlock(&f->fcmh_lock);
	atomic_dec(&i->fmdsi_ref);
	if (SPLAY_REMOVE(fcm_exports, &i->fmdsi_exports, m))
		psc_fatalx("Failed to remove m(%p) "FIDFMT,
                           m, FID_FMT_ARGS(&(mexpfcm2fidgen(m))));
        freelock(&f->fcmh_lock);

	c->pri = NULL;
	PSCFREE(m);
}


int
mds_stat_refresh_locked(struct fidc_memb_handle *f)
{	
	FCMH_LOCK_ENSURE(f);
	psc_assert(f->fcmh_fd >= 0);
	rc = fstat(f->fcmh_fd, &f->fcmh_memb.fcm_stb);
	if (rc < 0)
		psc_warn("failed for fid=%"_P_U64"x", fcmh_2_fid(f));

	return (rc);
}

__static int
mds_bmap_fsz_check_locked(struct fidc_memb_handle *f, sl_blkno_t n)
{
        sl_inodeh_t *i=&f->fcmh_memb.fcm_inodeh;
	int rc;

	FCMH_LOCK_ENSURE(f);
	if (rc = mds_stat_refresh_locked(f))
		return (rc);

	psc_trace("fid=%"_P_U64"x lblk=%zu fsz=%zu", 
		  fcmh_2_fid(fcmh), i->inoh_ino->ino_lblk, fcmh_2_fsz(fcmh));

	/* Verify that the inode agrees with file contents. 
	 *  XXX this assert is a bit too aggressive.
	 */
	psc_assert((((i->inoh_ino->ino_lblk+1) * BMAP_OD_SZ) - 1) 
		   <= fcmh_2_fsz(fcmh));
	return ((n > i->inoh_ino->ino_lblk) ? n : 0);
}

/**
 * mds_bmap_directio - queue mexpbcm's so that their clients may be notified of the cache policy change (to directio) for this bmap.  The mds_coherency thread is responsible for actually issuing and checking the status of the rpc's.  Communication between this thread and the mds_coherency thread occurs by placing the mexpbcm's onto the pndgCacheCbs listcache.
 * @bmap: the bmap which is going dio.
 * Notes:  XXX this needs help but it's making my brain explode right now.
 */
#define mds_bmap_directio_check(b) mds_bmap_directio(b, 1, 1)
#define mds_bmap_directio_set(b)   mds_bmap_directio(b, 1, 0)
#define mds_bmap_directio_unset(b) mds_bmap_directio(b, 0, 0)

void
mds_bmap_directio(struct bmapc_memb *bmap, int enable_dio, int check)
{	
	struct mexpbcm *bref;
	struct bmap_mds_info *mdsi=b->bcm_mds_pri;
	int mode=bref->mexpbcm_mode;
	
	psc_assert(mdsi && mdsi->bmdsi_wr_ion);
	BMAP_LOCK_ENSURE(bmap);

	DEBUG_BMAP(PLL_TRACE, bmap, "enable=%d check=%d", 
		   enable_dio, check);

	/* Iterate over the tree and pick up any clients which still cache 
	 *   this bmap.
	 */
	SPLAY_FOREACH(bref, bmap_exports, &bmap->bmdsi_exports) {
		struct list_head *e=&bref->mexpbcm_lentry;
		/* Lock while the attributes of the this bref are 
		 *  tested.
		 */
		MEXPBCM_LOCK(bref);
		psc_assert(bref->mexpbcm_export);		
		/* Don't send rpc if the client is already using DIO or
		 *  has an rpc in flight (_REQD).
		 */
		if (enable_dio &&                   /* turn dio on */
		    !(mode & MEXPBCM_CDIO) &&       /* client allows dio */		    
		    (!((mode & MEXPBCM_DIO) ||       /* dio is not already on */
		       (mode & MEXPBCM_DIO_REQD)) ||  /* dio is not coming on */
		     (mode & MEXPBCM_CIO_REQD))) { /* dio is being disabled */
			if (check) {
				DEBUG_BMAP(PLL_WARN, bref, 
					   "cli(%s) has not acknowledged dio "
					   "rpc for bmap(%p) bref(%p) "
					   "sent:(%d 0==true)", 
					   nid2str(mexpbcm2nid(bref)), bmap, 
					   atomic_read(&bref->mexpbcm_msgcnt));
					   continue;
			}
			bref->mexpbcm_mode |= MEXPBCM_DIO_REQD;
		
			if (mode & MEXPBCM_CIO_REQD) {
				/* This bref is already enqueued and may 
				 *   have completed.  
				 *  Verify the current inflight mode.
				 */
				mdscoh_infmode_chk(bref, MEXPBCM_CIO_REQD);
				psc_assert(psclist_conjoint(e));
				if (!bref->mexpbcm_net_inf) {
					/* Unschedule this rpc, the coh thread will
					 *  remove it from the listcache.
					 */					
					bref->mexpbcm_mode &= ~MEXPBCM_CIO_REQD;
					bref->mexpbcm_net_cmd = MEXPBCM_RPC_CANCEL;
					
				} else {
					/* Inform the coh thread to requeue.
					 */
					bref->mexpbcm_net_cmd = MEXPBCM_DIO_REQD;
					bref->mexpbcm_mode |= MEXPBCM_DIO_REQD;
				}
			} else {
				/* Queue this one.
				 */
				bref->mexpbcm_mode |= MEXPBCM_DIO_REQD;
				bref->mexpbcm_net_cmd = MEXPBCM_DIO_REQD;
				psc_assert(psclist_disjoint(e));
				lc_queue(&pndgCacheCbs, e);
			}
			
		} else if (!enable_dio && 
			   ((mode & MEXPBCM_DIO) || 
			    (mode & MEXPBCM_DIO_REQD))) {

			psc_assert(!mode & MEXPBCM_CDIO);
			if (mode & MEXPBCM_DIO_REQD) {
				/* We'd like to disable DIO mode but a re-enable request
				 *  has been queued recently.  Determine if it's inflight
				 *  of if it still queued.
				 */
				psc_assert(psclist_conjoint(e));
				mdscoh_infmode_chk(bref, MEXPBCM_DIO_REQD);
				if (!bref->mexpbcm_net_inf) {
					/* Unschedule this rpc, the coh thread will
					 *  remove it from the listcache.
					 */					
					bref->mexpbcm_mode &= ~MEXPBCM_DIO_REQD;
					bref->mexpbcm_net_cmd = MEXPBCM_RPC_CANCEL;
				} else {
					bref->mexpbcm_net_cmd = MEXPBCM_CIO_REQD;
					bref->mexpbcm_mode |= MEXPBCM_CIO_REQD;
				}
			} else {
				bref->mexpbcm_mode |= MEXPBCM_CIO_REQD;
				bref->mexpbcm_net_cmd = MEXPBCM_CIO_REQD;
				psc_assert(psclist_disjoint(e));
				lc_queue(&pndgCacheCbs, e);
			}
		}
		MEXPBCM_ULOCK(bref);
	}
}

__static void
mds_mion_init(struct mexp_ion *mion, sl_resm_t *resm)
{
	dynarray_init(&mion->mi_bmaps);
	dynarray_init(&mion->mi_bmaps_deref);

}

__static void
mds_mion_init(struct mexp_ion *mion, sl_resm_t *resm)
{
        dynarray_init(&mion->mi_bmaps);
        dynarray_init(&mion->mi_bmaps_deref);
	PSCLIST_ENTRY_INIT(&mion->mi_lentry);
	atomic_set(&mion->mi_refcnt, 0);
	mion->mi_resm = resm;
	mion->mi_csvc = rpc_csvc_create(SRIM_REQ_PORTAL, SRIM_REP_PORTAL);
}

__static int
mds_bmap_ion_assign(struct mexpbcm *bref, sl_ios_id_t pios)
{
	struct bmapc_memb *bmap=bref->mexpbcm_bmap;
	struct bmap_mds_info *mdsi=bmap->bcm_mds_pri;
	struct mexp_ion *mion;
	sl_resource_t *res=libsl_id2res(pios);
	sl_resm_t *resm;
	struct resprof_mds_info *rmi;
	int n, x, rc=0;

	psc_assert(!mdsi->bmdsi_wr_ion);
	psc_assert(!atomic_read(bmap->bcm_wr_ref));
	psc_assert(!atomic_read(mdsi->bmdsi_wr_ref));	

	if (!res) {
		psc_warnx("Failed to find pios %d", pios);
		return (-1);
	}
	rmi = res->res_pri;
	x = res->res_nnids;
	
	do {
		spinlock(&rmi->rmi_lock);
		n = rmi->rmi_cnt++;
		if (rmi->rmi_cnt > res->res_nnids)
			rmi->rmi_cnt = 0;
		
		psc_trace("trying res(%s) ion(%s)", 
			  res->res_name, libcfs_nid2str(res->res_nids[n]));
		
		resm = libsl_nid2resm(res->res_nids[n]);
		if (!resm)
			psc_fatalx("Failed to lookup %s, verify that slash "
				   "configs are uniform across all servers",
				   libcfs_nid2str(res->res_nids[n]));
	
		if (!resm->resm_pri) {
			/* First time this resm has been used.
			 */
			resm->resm_pri = PSCALLOC(sizeof(*mion));
			mion = resm->resm_pri;
			mds_mion_init(mion, resm);
		}
		freelock(&rmi->rmi_lock);
		
		psc_trace("res(%s) ion(%s) init=%d, failed=%d", 
			  res->res_name, libcfs_nid2str(res->res_nids[n]), 
			  mion->mi_csvc->csvc_initialized, 
			  mion->mi_csvc->csvc_failed);

		if (mion->mi_csvc->csvc_failed)
			continue;

		if (!mion->mi_csvc->csvc_initialized) {
			struct pscrpc_connection *c=
				mion->mi_csvc->csvc_import->imp_connection;

			rc = rpc_issue_connect(res->res_nids[n], 
					       mion->mi_csvc->csvc_import,
					       SRIM_MAGIC, SRIM_VERSION);
			if (rc) {
				mion->mi_csvc->csvc_failed = 1;
				continue;				
			} else
				mion->mi_csvc->csvc_initialized = 1;
		}

		mdsi->bmdsi_wr_ion = mion;
	} while (--x);

	if (!mdsi->bmdsi_wr_ion)
		return (-1);

	psc_trace("using res(%s) ion(%s)", 
		  res->res_name, libcfs_nid2str(res->res_nids[n]));	

	return (0);
}

/** 
 * mds_bmap_ref_add - add a read or write reference to the bmap's tree and refcnts.  This also calls into the directio_[check|set] calls depending on the number of read and/or write clients of this bmap.
 * @bref: the bref to be added, it must have a bmapc_memb already attached.
 * @rw: the explicit read/write flag from the rpc.  It is probably unwise to use bref's flag. 
 */
__static void 
mds_bmap_ref_add(struct mexpbcm *bref, struct srm_bmap_req *mq)
{
	struct bmapc_memb *bmap=bref->mexpbcm_bmap;
	struct bmap_mds_info *mdsi=b->bcm_mds_pri;
	atomic_t *a=(rw == SRIC_BMAP_READ ? &mdsi->bmdsi_rd_ref : 
		     &mdsi->bmdsi_wr_ref);
	int rdrs, wtrs, rw=mq->rw,
		mode=(rw == SRIC_BMAP_READ ? SRIC_BMAP_READ : SRIC_BMAP_WRITE);

	if (rw == SRIC_BMAP_READ)
		psc_assert(bref->mexpbcm_mode & MEXPBCM_RD);
	else if (rw == SRIC_BMAP_WRITE)
		psc_assert(bref->mexpbcm_mode & MEXPBCM_WR);
	else
		psc_fatalx("mode value (%d) is invalid", rw);
	
	BMAP_LOCK(bmap);
	if (!atomic_read(a)) {
		/* There are no refs for this mode, therefore the 
		 *   bcm_bmapih.bmapi_mode should not be set.
		 */		
		psc_assert(!(bmap->bcm_bmapih.bmapi_mode & mode));
		bmap->bcm_bmapih.bmapi_mode |= mode;
	}
	/* Set and check ref cnts now.
	 */
	atomic_inc(a);

	wtrs = atomic_read(&mdsi->bmdsi_wr_ref);
	rdrs = atomic_read(&mdsi->bmdsi_rd_ref);
	/* No negative refs.
	 */
	psc_assert(wtrs >= 0 && rdrs >= 0);

	if (wtrs == 1) {
		psc_assert(!mdsi->bmdsi_wr_ion);
		/* Should not send connect rpc's here while
		 *  the bmap is locked.
		 */
		mds_bmap_ion_assign(mq->pios);
	}
	/* Do directio checks here.
	 */
	if (wtrs > 2)
		/* It should have already been set.
		 */
		mds_bmap_directio_check(bmap);

	else if (wrtrs == 2 || (wtrs == 1 && rdrs))
		/* These represent the two possible 'add' related transitional
		 *  states, more than 1 writer or the first writer amidst 
		 *  existing readers.
		 */
		mds_bmap_directio_set(bmap);
	/* Pop it on the tree.
	 */
	if (SPLAY_INSERT(bmap_exports, &mdsi->bmdsi_exports, bref))
		psc_fatalx("found duplicate bref on bmap_exports");

	DEBUG_BMAP(PLL_TRACE, bmap, "done with ref_add");
	BMAP_ULOCK(bmap);
}

/** 
 * mds_bmap_ref_add - add a read or write reference to the bmap's tree and refcnts.  This also calls into the directio_[check|set] calls depending on the number of read and/or write clients of this bmap.
 * @bref: the bref to be added, it must have a bmapc_memb already attached.
 * @rw: the explicit read/write flag from the rpc.  It is probably unwise to use bref's flag. 
 */
__static void
mds_bmap_ref_del(struct mexpbcm *bref, int rw)
{
	struct bmapc_memb *bmap=bref->mexpbcm_bmap;
	struct bmap_mds_info *mdsi=b->bcm_mds_pri;
	atomic_t *a=(rw == SRIC_BMAP_READ ? &mdsi->bmdsi_rd_ref : 
		     &mdsi->bmdsi_wr_ref);
	int rdrs, wtrs, 
		mode=(rw == SRIC_BMAP_READ ? SRIC_BMAP_READ : SRIC_BMAP_WRITE);

	if (rw == SRIC_BMAP_READ)
		psc_assert(bref->mexpbcm_mode & MEXPBCM_RD);
	else if (rw == SRIC_BMAP_WRITE)
		psc_assert(bref->mexpbcm_mode & MEXPBCM_WR);
	else
		psc_fatalx("mode value (%d) is invalid", rw);
	
	BMAP_LOCK(bmap);
	psc_assert(atomic_read(a));
	if (atomic_dec_and_test(a)) {
		psc_assert(bmap->bcm_bmapih.bmapi_mode & mode);
		bmap->bcm_bmapih.bmapi_mode &= ~mode;
	}
	wtrs = atomic_read(&mdsi->bmdsi_wr_ref);
	rdrs = atomic_read(&mdsi->bmdsi_rd_ref);

	psc_assert(wtrs >= 0 && rdrs >= 0);

	if ((!wtrs || (wtrs == 1 && !rdrs)) && (rw == SRIC_BMAP_WRITE))
		/* Decremented a writer.
		 */
		mds_bmap_directio_unset(bmap);

	if (!SPLAY_REMOVE(bmap_exports, &mdsi->bmdsi_exports, bref))
		psc_fatalx("found duplicate bref on bmap_exports");

	DEBUG_BMAP(PLL_TRACE, bmap, "done with ref_del");
	BMAP_ULOCK(bmap);
}

/**
 * mds_bmap_new_rd - called when a read request offset exceeds the bounds of the file causing a new bmap to be created.
 * Notes:  Bmap creation race conditions are prevented because the bmap handle already exists at this time with bmapi_mode == BMAP_MDS_INIT.  This causes other threads to block on the waitq until read / creation has completed.
 * More Notes:  this bmap is not written to disk until a client actually writes something to it.
 */
__static void
mds_bmap_new(struct fidc_memb_handle *f, sl_blkno_t n, sl_blkh_t **b)
{
	int i;	
	
	*b = PSCALLOC(BMAP_OD_SZ);	
	for (i=0; i < SL_CRCS_PER_BMAP; i++)
		(*b)->bh_crcs[i] = SL_NULL_CRC;	
	
	PSC_CRC_CALC(((*b)->bh_bhcrc), (*b), BMAP_OD_CRCSZ);
}

int
mds_bmap_read(struct fidc_memb_handle *fcmf, struct srm_bmap_req *mq, 
	      sl_blkh_t **bmapod)
{
	sl_inodeh_t *inoh=&fcmh->fcmh_memb.fcm_inodeh;	
	int rc=0;

	if (fcmh->fcmh_fd == -1) {
		psc_warn("fid=%"_P_U64"x has a failed fd", fcmh_2_fid(fcmh));
		rc = -EIO; 
		goto fail;
		
	} else if (fcmh->fcmh_fd == FID_FD_NOTOPEN) {
		fcmh->fcmh_fd = fid_open(fcmh_2_fid(fcmh), O_RDWR);
		psc_warn("fid_open() failed for fid=%"_P_U64"x", 
			 fcmh_2_fid(fcmh));
		if (fcmh->fcmh_fd == -1) {
			rc = -EIO;
			goto fail;
		}
	}

	*bmapod = PSCALLOC(BMAP_OD_SZ);	
	/* Try to pread() the bmap from the mds file.
	 */
	szrc = pread(fcmh->fcmh_fd, *bmapod, BMAP_OD_SZ, 
		     (mq->blkno * BMAP_OD_SZ));

	if (szrc != BMAP_OD_SZ) {
		psc_warn("failed to pread BMAP_OD_SZ bytes for fid=%"_P_U64"x"
			 ", got (%zd) instead", fcmh_2_fid(fcmh), szrc);
		rc = -errno;
		goto fail;
	}

	PSC_CRC_CALC(&crc, *bmapod, BMAP_OD_CRCSZ);
	if (crc != (*bmapod)->bh_bhcrc) {
		psc_warn("bmap (%zu) crc failed fid=%"_P_U64"x",
                         mq->blkno, fcmh_2_fid(fcmh), szrc);
		rc = -EIO;
		goto fail;
	}
 fail:
	return (rc);
}

int
mds_bmap_load(struct mexpfcm *fref, struct srm_bmap_req *mq, 
	      struct bmapc_memb **bmap)
{
	struct bmapc_memb tbmap;
	struct fidc_memb_handle *fcmf=fref->mecm_fcmh;
	sl_inodeh_t *inoh=&fcmh->fcmh_memb.fcm_inodeh;
	sl_blkh_t *bmap_blk;
	int rc=0;
	ssize_t szrc;
	psc_crc_t crc;
	struct mexpbcm *bref, tbref;

	psc_assert(fref);
	psc_assert(!*bmap);

	if ((mq->rw != SRIC_BMAP_READ) || (mq->rw != SRIC_BMAP_WRITE))
		return -EINVAL;

	tbmap.bcm_blkno = mq->blkno;
	tbref.mexpbcm_bmap = &tbmap;
	/* This bmap load *should* be for a bmap which the client has not 
	 *   already referenced and therefore no mexpbcm should exist.  The 
	 *   mexpbcm exists for each bmap that the client has cached.
	 */
	MEXPFCM_LOCK(fref);
	bref = SPLAY_FIND(exp_bmaptree, &fref->mecm_bmaps, &tbref);
	if (bref) {
		/* If we're here then the same client tried to cache this
		 *  bmap more than once which is an invalid behavior.
		 */
		MEXPFCM_ULOCK(fref);
		return (-EBADF);

	} else {
		/* Establish a reference here, note that mexpbcm_bmap will
		 *   be null until either the bmap is loaded or pulled from 
		 *   the cache.
 		 */
		bref = PSCALLOC(sizeof(*bref));
		bref->mexpbcm_mode = MEXPBCM_INIT;
		bref->mexpbcm_blkno = mq->blkno;
		bref->mexpbcm_export = fref->mecm_export;
		SPLAY_INSERT(exp_bmaptree, &fref->mecm_bmaps, bref);
	}
	MEXPFCM_ULOCK(fref);
	/* Ok, the bref has been initialized and loaded into the tree.  We 
	 *  still need to set the bmap pointer mexpbcm_bmap though.  Lock the 
	 *  fcmh during the bmap lookup.
	 */
	FCMH_LOCK(fcmh);
	*bmap = SPLAY_FIND(bmap_cache, &fcmh->fcmh_bmapc, &tbmap);
	if (*bmap) {
		/* Found it, still don't know if we're in directio mode..
		 */
		FCMH_ULOCK(fcmh);
	retry:
		BMAP_LOCK(*bmap);
		if ((*bmap)->bcm_bmapih.bmapi_mode == BMAP_MDS_INIT) {
			psc_assert(!(*bmap)->bcm_mds_pri);
			psc_assert(!(*bmap)->bcm_bmapih.bmapi_data);
			psc_assert(!(*bmap)->bcm_fcmh);
			/* Block until the other thread has completed the io.
			 */
			psc_waitq_wait(&(*bmap)->bcm_waitq, 
				       &(*bmap)->bcm_lock);
			goto retry;
		} else {
			/* Ensure that the INIT bit is not set and that all 
			 *  relevant pointers are in place.
			 */
			psc_assert(!((*bmap)->bcm_bmapih.bmapi_mode & 
				     BMAP_MDS_INIT));
			psc_assert((*bmap)->bcm_mds_pri);
			psc_assert((*bmap)->bcm_bmapih.bmapi_data);
			psc_assert((*bmap)->bcm_fcmh);
		}

	} else {
		int rc;
		/* Create and initialize the new bmap while holding the 
		 *  fcmh lock which is needed for atomic tree insertion.
		 */
		*bmap = PSCALLOC(sizeof(struct bmapc_memb));
		(*bmap)->bcm_blkno = mq->blkno;
		(*bmap)->bcm_bmapih.bmapi_mode = BMAP_MDS_INIT;
		(*bmap)->bcm_mds_pri = PSCALLOC(sizeof(struct bmap_mds_info));
		LOCK_INIT(&(*bmap)->bcm_lock);
		psc_waitq_init(&(*bmap)->bcm_waitq);		
		(*bmap)->bcm_fcmh = fcmh;
		/* It's ready to go, place it in the tree.
		 */
		SPLAY_INSERT(bmap_cache, &fcmh->fcmh_bmapc, *bmap);
		/* The placeholder is set, check the inode sizes to determine
		 *  whether or not a bmap actually exists at this offset.
		 */
		rc = mds_bmap_fsz_check_locked(fcmh, mq->blkno);
		/* Finally, the fcmh may be unlocked.
		 */
		FCMH_ULOCK(fcmh);
		if (rc < 0)
			goto fail;

		else if (rc > 0) {
			/* There's a race with inoh->lblk here so mds_bmap_new_rd
			 *   wants the fcmf locked.
			 */
			rc = mds_bmap_new(fcmf, mq->blkno, 
					  &(*bmap)->bcm_bmapih.bmapi_data);
			if (rc)
				goto fail;
		} else {
			rc = mds_bmap_read(fcmf, mq, 
					   &(*bmap)->bcm_bmapih.bmapi_data);
			if (rc)				
				goto fail;
		}
		(*bmap)->bcm_bmapih.bmapi_mode = 0;
		/* Notify other threads that this bmap has been loaded.
		 */
		psc_waitq_wakeall(&(*bmap)->bcm_waitq);
	}

	bmap_set_accesstime(*bmap);	
	/* Not sure if these are really needed on the mds.
	 */
	if (rw == SRIC_BMAP_WRITE)
		atomic_inc(&(*bmap)->bcm_wr_ref);
	else 
		atomic_inc(&(*bmap)->bcm_rd_ref);
	/* Sanity checks, make sure that we didn't let the client in 
	 *  before this bmap was ready.
	 */ 
	psc_assert(bref->mexpbcm_mode == MEXPBCM_INIT);
	
	bref->mexpbcm_bmap = *bmap;
	bref->mexpbcm_mode = ((rw == SRIC_BMAP_WRITE) ? MEXPBCM_WR : 0);
	/* Check if the client requested directio, if so tag it in the 
	 *  bref.
	 */
	if (mq->dio)
		bref->mexpbcm_mode |= MEXPBCM_CDIO;
	/* Place our bref on the tree, manage any mode changes that result 
	 *  from this new reference.  Also, on write choose an ION if needed.
	 */
	mds_bmap_ref_add(bref, mq);

	return (0);
 fail:	
	(*bmap)->bcm_bmapih.bmapi_mode = BMAP_MDS_FAILED;
	/* XXX think about policy updates in fail mode.
	 */
	return (rc);
}

/** 
 * mds_fidfs_lookup - "lookup file id via filesystem".  This call does a getattr on the provided pathname, loads (and verifies) the info from the getattr and then does a lookup into the fidcache.
 * XXX clean me up.. extract crc stuff..
 * XXX until dcache is done, this must be done for every lookup.
 */
#if 0
int
mds_fidfs_lookup(const char *path, struct slash_creds *creds,
		 struct fidc_memb_handle **fcmh)
{
	int rc;
	sl_inodeh_t  inoh;
	size_t       sz=sizeof(sl_inode_t);
	psc_crc_t    crc;

	psc_assert(fcmh && !(*fcmh));

	rc = access_fsop(ACSOP_GETATTR, creds->uid, creds->gid, path, 
			 SFX_INODE, &inoh.inoh_ino, sz);

	if (rc < 0)
		psc_warn("Attr lookup on (%s) failed", path);
	else if (rc != sz)
		psc_warn("Attr lookup on (%s) gave invalid sz (%d)", path, rc);
	else
		rc=0;

	PSC_CRC_CALC(&crc, &inoh.inoh_ino, sz);
	if (crc != inoh.inoh_ino.ino_crc) {
		psc_warnx("Crc failure on inode");
		errno = EIO;
		return -1;		
	}
	if (inoh.inoh_ino.ino_nrepls) {
		sz = sizeof(sl_replica_t) * inoh.inoh_ino.ino_nrepls;
		inoh->inoh_replicas = PSCALLOC(sz);
		rc = access_fsop(ACSOP_GETATTR, creds->uid, creds->gid, path,
				 SFX_REPLICAS, inoh.inoh_replicas, sz);
		if (rc < 0)
			psc_warn("Attr lookup on (%s) failed", path);
		else if (rc != sz)
			psc_warn("Attr lookup on (%s) gave invalid sz (%d)", 
				 path, rc);
		else
			rc=0;

		PSC_CRC_CALC(&crc, inoh.inoh_replicas, sz);
		if (crc != inoh.inoh_ino.ino_rs_crc) {
			psc_warnx("Crc failure on replicas");
			errno = EIO;
			*fcmh = NULL;
			return -1;		
		}
	}	
	*fcmh = fidc_lookup_ino(&inoh.inoh_ino);
	psc_assert(*fcmh);

	return(rc);
}
#endif


__static void
mds_cfdops_init(void) 
{
	cfdOps = PSCALLOC(sizeof(*cfdOps));
	cfdOps->cfd_new = mexpfcm_new;
	cfdOps->cfd_free = mexpfcm_free;
	cfdOps->cfd_insert = NULL;
}



void
mds_init(void)
{
	mds_cfdops_init();

	mdsFsops.slfsop_getattr  = mds_fidfs_lookup;
	mdsFsops.slfsop_fgetattr = mds_fid_lookup;

	slFsops = &mdsFsops;
	
	mdscoh_init();
}
