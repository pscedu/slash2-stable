/* $Id$ */

#ifndef _SLASHD_MDS_BMAP_H_
#define _SLASHD_MDS_BMAP_H_

#include "psc_ds/tree.h"

#include "jflush.h"
#include "bmap.h"
#include "mdslog.h"

struct mexpbcm;
struct mexpfcm;

SPLAY_HEAD(bmap_exports, mexpbcm);

/*
 * bmap_mds_info - the bcm_pri data structure for the slash2 mds.
 *   Bmap_mds_info holds all bmap specific context for the mds which
 *   includes the journal handle, ref counts for client readers and writers
 *   a point to our ION, a tree of our client's exports, a pointer to the
 *   on-disk structure, a receipt for the odtable, and a reqset for issuing
 *   callbacks (XXX is that really needed?).
 * Notes: both read and write clients are stored to bmdsi_exports, the ref
 *   counts are used to determine the number of both and hence the caching
 *   mode used at the clients.   Bmdsi_wr_ion is a shortcut pointer used
 *   only when the bmap has client writers - all writers (and readers) are
 *   directed to this ion once a client has invoked write mode on the bmap.
 */
struct bmap_mds_info {
	uint32_t			 bmdsi_xid;	/* last op recv'd from ION */
	struct jflush_item		 bmdsi_jfi;	/* journal handle          */
	atomic_t			 bmdsi_rd_ref;	/* reader clients          */
	atomic_t			 bmdsi_wr_ref;	/* writer clients          */
	struct mexp_ion			*bmdsi_wr_ion;	/* pointer to write ION    */
	struct bmap_exports		 bmdsi_exports;	/* tree of client exports  */
	struct slash_bmap_od		*bmdsi_od;	/* od-disk pointer         */
	struct odtable_receipt		*bmdsi_assign;	/* odtable receipt         */
	struct pscrpc_request_set	*bmdsi_reqset;	/* cache callback rpc's    */
};

/* Note that n + BMAP_RSVRD_MODES must be < 32.
 */
enum mds_bmap_modes {
        BMAP_MDS_FAILED = (1 << (0 + BMAP_RSVRD_MODES)), /* crc failure */
        BMAP_MDS_EMPTY  = (1 << (1 + BMAP_RSVRD_MODES)), /* new bmap, not yet committed to disk */
        BMAP_MDS_CRC_UP = (1 << (2 + BMAP_RSVRD_MODES)), /* crc update in progress */
	BMAP_MDS_CRCWRT = (1 << (3 + BMAP_RSVRD_MODES)),
	BMAP_MDS_NOION  = (1 << (4 + BMAP_RSVRD_MODES)),
	BMAP_MDS_INIT   = BMAP_INIT
};

static inline void
bmap_mds_info_init(struct bmapc_memb *bmap)
{
	struct bmap_mds_info *bmdsi = bmap->bcm_pri;

	jfi_init(&bmdsi->bmdsi_jfi, mds_bmap_sync, bmap);
	bmdsi->bmdsi_xid = 0;
	atomic_set(&bmdsi->bmdsi_rd_ref, 0);
	atomic_set(&bmdsi->bmdsi_wr_ref, 0);
}

/*
 * bmi_assign - the structure used for tracking the mds's bmap / ion
 *   assignments.  These structures are stored in a odtable.
 */
struct bmi_assign {
	lnet_nid_t   bmi_ion_nid;
	sl_ios_id_t  bmi_ios;
	slfid_t      bmi_fid;
	sl_blkno_t   bmi_bmapno;
	time_t       bmi_start;
};

#define bmap_2_bmdsi(b)		((struct bmap_mds_info *)(b)->bcm_pri)
#define bmap_2_bmdsiod(b)	bmap_2_bmdsi(b)->bmdsi_od
#define bmap_2_bmdsjfi(b)	(&bmap_2_bmdsi(b)->bmdsi_jfi)
#define bmap_2_bmdsassign(b)	bmap_2_bmdsi(b)->bmdsi_assign

int  mds_bmap_crc_write(struct srm_bmap_crcup *, lnet_nid_t);
int  mds_bmap_load(struct mexpfcm *, struct srm_bmap_req *, struct bmapc_memb **);
void mds_bmapod_dump(const struct bmapc_memb *);

#endif /* _SLASHD_MDS_BMAP_H_ */
