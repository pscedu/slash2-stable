/* $Id$ */
/*
 * %ISC_START_LICENSE%
 * ---------------------------------------------------------------------
 * Copyright 2015-2016, Google, Inc.
 * Copyright 2007-2016, Pittsburgh Supercomputing Center
 * All rights reserved.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the
 * above copyright notice and this permission notice appear in all
 * copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL
 * WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS.  IN NO EVENT SHALL THE
 * AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
 * PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 * --------------------------------------------------------------------
 * %END_LICENSE%
 */

#ifndef _PFL_JOURNAL_H_
#define _PFL_JOURNAL_H_

#include "pfl/atomic.h"
#include "pfl/dynarray.h"
#include "pfl/opstats.h"
#include "pfl/lock.h"
#include "pfl/thread.h"
#include "pfl/waitq.h"

struct psc_journal;
struct psc_journal_enthdr;
struct psc_ctlop;

#define	PJ_MAX_TRY			3		/* number of retry before giving up */
#define	PJ_MAX_BUF			8192		/* number of journal buffers to keep around */

#define PJH_MAGIC			UINT64_C(0x45678912aabbccff)
#define PJH_VERSION			0x02

typedef int (*psc_replay_handler_t)(struct psc_journal_enthdr *);
/*
 * Distill handler is used to further process certain log entries.
 * These log entries carry information that we might need to preserve a
 * longer time, and outside the journal.
 */
typedef int (*psc_distill_handler_t)(struct psc_journal_enthdr *,
    uint64_t, int, int);

#define PJRNL_TXG_GET			0
#define PJRNL_TXG_PUT			1

#define PJRNL_CURSOR_MAGIC		UINT64_C(0x12345678abcd1234)
#define PJRNL_CURSOR_VERSION		1

/*
 * The cursor, stored on disk, is used to remember where we are in terms
 * of processing the log entries.  In addition, this file lives in ZFS,
 * so we don't need do any checksum.
 *
 * We can store more information (e.g. stats) into this file. In fact,
 * we may be able to treat this file as the superblock of slash2.
 */
struct psc_journal_cursor {
	uint64_t			 pjc_magic;
	uint64_t			 pjc_version;
	uint64_t			 pjc_timestamp;		/* format time */
	unsigned char			 pjc_uuid[16];
	/*
	 * pjc_commit_txg is the only trustworthy information recorded
	 * in the file after a crash.  By making this information
	 * accurate, we can be sure that a replay should succeed.  Other
	 * information can be stale and need to be adjusted before use.
	 *
	 * 10/20/2017: After ZFS re-starts, it can commit some transaction
	 * groups before become available.
	 */
	uint64_t			 pjc_commit_txg;	/* last synced ZFS transaction group number */
	uint64_t			 pjc_distill_xid;	/* last XID whose entry has been distilled */
	uint64_t			 pjc_fid;		/* last SLASH2 FID */
	uint64_t			 pjc_seqno_lwm;		/* low water mark of last bmap sequence number */
	uint64_t			 pjc_seqno_hwm;		/* high water mark of last bmap sequence number */
	uint64_t			 pjc_tail;		/* tail slot of our system journal */
	/*
	 * We need the following if the system log does not have any
	 * entries to be distilled.
	 */
	uint64_t			 pjc_update_seqno;
	uint64_t			 pjc_reclaim_seqno;

	uint64_t			 pjc_replay_xid;	/* replay progress in case we crash again */

#if 0
	uint64_t			 pjc_total_dirs;
	uint64_t			 pjc_total_files;
	uint64_t			 pjc_total_bytes;
#endif

};

struct psc_journal_hdr {
	uint64_t			 pjh_magic;
	uint64_t			 pjh_start_off;
	int32_t				 pjh_entsz;
	uint32_t			 pjh_nents;
	uint32_t			 pjh_version;
	int32_t				 pjh_readsize;
	uint64_t			 pjh_timestamp;		/* format time */
	uint64_t			 pjh_fsuuid;
	uint64_t			 pjh_chksum;		/* keep it last and aligned at a 8-byte boundary */
#define pjh_iolen pjh_start_off
};

#define PJ_NAME_MAX			16

struct psc_journal {
	char				 pj_devname[NAME_MAX + 1];
	char				 pj_name[PJ_NAME_MAX];
	struct psc_listentry		 pj_lentry;

	psc_spinlock_t			 pj_lock;
	uint32_t			 pj_flags;
	uint32_t			 pj_npeers;		/* the number of MDS peers XXX move into slash */

	uint32_t			 pj_inuse;		/* slots in use */
	uint32_t			 pj_total;		/* total # slots avail */
	uint32_t			 pj_resrv;		/* # slots in reserve */

	uint64_t			 pj_lastxid;		/* last transaction ID used */
	uint64_t			 pj_current_txg;
	uint64_t			 pj_commit_txg;		/* committed ZFS transaction group number  */
	uint64_t			 pj_replay_xid;		/* last transaction ID replay */
	uint64_t			 pj_distill_xid;	/* last transaction ID distilled */
	struct psc_journal_hdr		*pj_hdr;

	struct psc_lockedlist		 pj_pendingxids;
	struct psc_lockedlist		 pj_distillxids;

	struct psc_dynarray		 pj_bufs;
	struct psc_waitq		 pj_waitq;
	uint32_t			 pj_nextwrite;		/* next entry slot to write to */
	uint64_t			 pj_wraparound;		/* stats only */
	psc_distill_handler_t		 pj_distill_handler;
	int				 pj_fd;			/* file descriptor to backing disk file */

	struct pfl_iostats_rw		 pj_iostats;		/* read/write I/O stats */
	struct pfl_opstat		*pj_opst_reserves;
	struct pfl_opstat		*pj_opst_commits;
	struct pfl_opstat		*pj_opst_distills;
};

#define PJF_NONE			0
#define PJF_WANTBUF			(1 << 0)
#define PJF_WANTSLOT			(1 << 1)
#define PJF_ISBLKDEV			(1 << 2)
#define PJF_REPLAYINPROG		(1 << 3)		/* journal replay in progress */

#define PJ_LOCK(pj)			spinlock(&(pj)->pj_lock)
#define PJ_ULOCK(pj)			freelock(&(pj)->pj_lock)

#define pjournal_has_peers(pj)		(pj)->pj_npeers

#define DEBUG_JOURNAL(lvl, jrnl, fmt, ...)				\
	psclog((lvl), "journal@%p[%s] " fmt,				\
	    (jrnl), (jrnl)->pj_name, ## __VA_ARGS__)

#define PJE_XID_NONE			0			/* invalid transaction ID */
#define PJE_MAGIC			UINT32_C(0x4567abcd)

/*
 * Journal entry types - lower bits are used internally, higher bits
 * after _PJE_FLSHFT are used for application-specific codes.
 */
#define PJE_NONE			0			/* no flag */
#define PJE_FORMAT			(1 << 0)		/* newly-formatted */
#define PJE_NORMAL			(1 << 1)		/* has data */
#define PJE_DISTILL			(1 << 2)		/* needs distill */
#define _PJE_FLSHFT			(1 << 3)		/* denote the last used bit */

/*
 * psc_journal_enthdr - Journal entry header.
 * @pje_magic: validity check.
 * @pje_type: app-specific log entry type.
 * @pje_xid: journal transaction id.
 * @pje_sid: xid sub-id.
 * @pje_chksum: simple XOR checksum
 * @pje_data: application data.
 * Notes: at some point we may want to make this into a footer which has
 *    a CRC field.
 *
 * Note that the fields in this structure are arranged in a way so that
 * the payload, if any, starts at a 64-bit boundary.
 */
struct psc_journal_enthdr {
	uint32_t			pje_magic;
	uint16_t			pje_type;	/* see above */
	/*
	 * This field is used to calculate the CRC checksum of the
	 * payload starting from pje_data[0].  It should always be
	 * greater than zero and it does NOT include the header.
	 */
	uint16_t			pje_len;
	uint64_t			pje_xid;
	/*
	 * Even if our journal lives outside of the ZFS pools, this
	 * field tells us exactly if we should apply a log entry.  And
	 * we can expect success if we do.  No need to poke inside ZFS
	 * for clues.
	 */
	uint64_t			pje_txg;
	uint64_t			pje_chksum;	/* must be the last field before data */
	char				pje_data[0];
} __packed;

#define PJE_DATA(pje)			((void *)(pje)->pje_data)
#define DATA_2_PJE(data)		((struct psc_journal_enthdr *)((char *)(data) -	\
					    offsetof(struct psc_journal_enthdr, pje_data)))

#define	PJ_PJESZ(p)			((p)->pj_hdr->pjh_entsz)

#define PJ_GETENTOFF(pj, i)		((off_t)(pj)->pj_hdr->pjh_start_off + (i) * PJ_PJESZ(pj))

#define DPRINTF_PJE(lvl, pje, fmt, ...)					\
	psclog((lvl), "pje@%p type=%#x len=%hd xid=%#"PRIx64" "		\
	    "txg=%#"PRIx64" " fmt,					\
	    (pje), (pje)->pje_type, (pje)->pje_len, (pje)->pje_xid,	\
	    (pje)->pje_txg, ## __VA_ARGS__)

/**
 * psc_journal_xidhndl - Journal transaction ID handle.
 * @pjx_xid: the transaction ID.
 * @pjx_sid: the xid sub-operation ID.
 * @pjx_tailslot: the address of our starting/oldest slot.
 * @pjx_flags: app-specific log entry bits.
 * @pjx_lentry: open xid handles are chained in journal structure.
 * @pjx_lock: serialize.
 * @pjx_pj: backpointer to our journal.
 */
struct psc_journal_xidhndl {
	uint64_t			 pjx_txg;		/* associated ZFS transaction group number */
	uint64_t			 pjx_xid;		/* debugging only */
	uint32_t			 pjx_slot;
	uint32_t			 pjx_flags;
	struct psclist_head		 pjx_lentry;
	struct psclist_head		 pjx_pndg_lentry;	/* pending transaction list - ordered by slot number assigned */
	struct psclist_head		 pjx_dstl_lentry;	/* distill transaction list - ordered by transaction ID */
	psc_spinlock_t			 pjx_lock;
	struct psc_journal		*pjx_pj;
	void				*pjx_data;
};

#define	PJX_SLOT_ANY			(~0U)

#define	PJX_NONE			0
#define	PJX_DISTILL			(1 << 0)
#define	PJX_WRITTEN			(1 << 1)

/* Actions to be take after the journal log is open */
#define	PJOURNAL_LOG_DUMP		1
#define	PJOURNAL_LOG_REPLAY		2

/* definitions of journal handling functions */
struct psc_journal
	*pjournal_open(const char *, const char *);
struct psc_thread
	*pjournal_replay(struct psc_journal *, int, const char *,
	    psc_replay_handler_t, psc_distill_handler_t);

void	 pjournal_update_txg(struct psc_journal *, uint64_t);
uint64_t pjournal_next_replay(struct psc_journal *);
uint64_t pjournal_next_distill(struct psc_journal *);
uint64_t pjournal_next_reclaim(struct psc_journal *);

int	 pjournal_reserve_slot(struct psc_journal *, int);
void	 pjournal_unreserve_slot(struct psc_journal *, int);

void	*pjournal_get_buf(struct psc_journal *, size_t);
void	 pjournal_put_buf(struct psc_journal *, void *);

uint32_t pjournal_add_entry(struct psc_journal *, uint64_t, int, int,
	    void *, int);
uint64_t pjournal_next_xid(struct psc_journal *);

void	 pfl_journal_register_ctlops(struct psc_ctlop *);

#endif /* _PFL_JOURNAL_H_ */
