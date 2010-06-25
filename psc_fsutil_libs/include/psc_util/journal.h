/* $Id$ */
/*
 * %PSC_START_COPYRIGHT%
 * -----------------------------------------------------------------------------
 * Copyright (c) 2006-2010, Pittsburgh Supercomputing Center (PSC).
 *
 * Permission to use, copy, and modify this software and its documentation
 * without fee for personal use or non-commercial use within your organization
 * is hereby granted, provided that the above copyright notice is preserved in
 * all copies and that the copyright and this permission notice appear in
 * supporting documentation.  Permission to redistribute this software to other
 * organizations or individuals is not permitted without the written permission
 * of the Pittsburgh Supercomputing Center.  PSC makes no representations about
 * the suitability of this software for any purpose.  It is provided "as is"
 * without express or implied warranty.
 * -----------------------------------------------------------------------------
 * %PSC_END_COPYRIGHT%
 */

#ifndef _PFL_JOURNAL_H_
#define _PFL_JOURNAL_H_

#include "psc_ds/dynarray.h"
#include "psc_util/atomic.h"
#include "psc_util/iostats.h"
#include "psc_util/lock.h"
#include "psc_util/thread.h"
#include "psc_util/waitq.h"

struct psc_journal;
struct psc_journal_enthdr;

#define	PJ_MAX_TRY			3		/* number of retry before giving up */
#define	PJ_MAX_BUF			1024		/* number of journal buffers to keep around */

#define PJ_LOCK(pj)			spinlock(&(pj)->pj_lock)
#define PJ_ULOCK(pj)			freelock(&(pj)->pj_lock)

#define PJH_MAGIC			UINT64_C(0x45678912aabbccff)
#define PJH_VERSION			0x02

#define PJH_OPT_NONE			0x00

/*
 * Embed handler is used to insert some information (such as a sequence number)
 * into the journal. It should be invoked periodically so that the lastest version
 * of the information is always available in the journal somewhere and never
 * overwritten.
 */
typedef int (*psc_embed_handler)(int);
typedef void (*psc_replay_handler)(struct psc_journal_enthdr *, int *);

/*
 * Distill handler is used to further process certain log entries. These
 * log entries carry information that we might need to preserve long
 * time.
 */
typedef void (*psc_distill_handler)(struct psc_journal_enthdr *, int);

struct psc_journal_hdr {
	uint64_t			 pjh_magic;
	uint64_t			 pjh_start_off;
	uint32_t			 pjh_entsz;
	uint32_t			 pjh_nents;
	uint32_t			 pjh_version;
	uint32_t			 pjh_options;
	uint32_t			 pjh_readahead;
	uint32_t			 pjh__pad;
	uint64_t			 pjh_chksum;	/* keep it last and aligned at a 8 byte boundary */
#define pjh_iolen pjh_start_off
};

struct psc_journal {
	psc_spinlock_t			 pj_lock;
	uint64_t			 pj_lastxid;	/* last transaction ID used */
	struct psc_journal_hdr		*pj_hdr;

	psc_spinlock_t			 pj_pendinglock;
	struct psc_lockedlist		 pj_pendingxids;

	psc_spinlock_t			 pj_distilllock;
	struct psc_lockedlist		 pj_distillxids;

	struct psc_dynarray		 pj_bufs;
	struct psc_waitq		 pj_waitq;
	int				 pj_fd;		/* open file descriptor to backing disk file */
	int				 pj_flags;
	uint32_t			 pj_nextwrite;	/* next entry slot to write to */
	psc_embed_handler		 pj_embed_handler;
	psc_distill_handler		 pj_distill_handler;
	struct psc_iostats		 pj_rdist;	/* read I/O stats */
	struct psc_iostats		 pj_wrist;	/* write I/O stats */
};

#define PJF_NONE			0
#define PJF_WANTBUF			(1 << 0)
#define PJF_WANTSLOT			(1 << 1)

#define PJE_XID_NONE			0		/* invalid transaction ID */
#define PJE_MAGIC			UINT64_C(0x45678912aabbccdd)

/*
 * Journal entry types - higher bits after _PJE_FLSHFT are used for
 * application-specific codes.
 */
#define PJE_NONE			0		/* null journal record */
#define PJE_FORMAT			(1 << 0)	/* newly-formatted journal entry */
#define PJE_STRTUP			(1 << 1)	/* system startup */
#define PJE_XSTART			(1 << 2)	/* start a transaction */
#define PJE_XCLOSE			(1 << 3)	/* close a transaction */
#define PJE_XSNGL			(1 << 4)	/* transaction begins and ends insantly*/
#define PJE_XNORML			(1 << 5)	/* normal transaction data */
#define _PJE_FLSHFT			(1 << 6)	/* denote the last used bit */

/*
 * psc_journal_enthdr - journal entry header.
 * @pje_magic: validity check.
 * @pje_type: app-specific log entry type.
 * @pje_xid: journal transaction id.
 * @pje_sid: xid sub-id.
 * @pje_chksum: simple XOR checksum
 * @pje_data: application data.
 * Notes: at some point we may want to make this into a footer which has
 *    a crc field.
 *
 * Note that the fields in this structure are arranged in a way so that
 * the payload, if any, starts at a 64-bit boundary.
 */
struct psc_journal_enthdr {
	uint64_t			pje_magic;
	uint16_t			pje_type;	/* see above */
	/*
	 * This field is used to calculate the CRC checksum of the payload starting
	 * from pje_data[0]. It also indicates if the log entry is a special-purpose
	 * one (i.e., one without custom data).
	 */
	uint16_t			pje_len;
	/*
	 * This field can be used by the replay process to remove the CLOSE entry
	 * when all other log entries of the same transaction have been seen.
	 * Log entries of different transactions can be interleaved.
	 */
	uint32_t			pje_sid;
	uint64_t			pje_xid;
	uint64_t			pje_chksum;	/* last field before data */
	/*
	 * The length of the pje_data[0] is also embedded and can be figured out
	 * by log replay functions.
	 */
	char				pje_data[0];
} __packed;

#define	PJ_PJESZ(p)			((p)->pj_hdr->pjh_entsz)

/*
 * psc_journal_xidhndl - journal transaction ID handle.
 * @pjx_xid: the transaction ID.
 * @pjx_sid: the xid sub-operation ID.
 * @pjx_tailslot: the address of our starting / oldest slot.
 * @pjx_flags: app-specific log entry bits.
 * @pjx_lentry: open xid handles are chained in journal structure.
 * @pjx_lock: serialize.
 * @pjx_pj: backpointer to our journal.
 */
#define	PJX_SLOT_ANY			(~0U)

#define	PJX_NONE			0
#define	PJX_XSTART			(1 << 0)
#define	PJX_XCLOSE			(1 << 1)
#define	PJX_XSNGL			(1 << 2)

struct psc_journal_xidhndl {
	uint64_t			 pjx_xid;
	int				 pjx_sid;
	uint32_t			 pjx_tailslot;
	uint32_t			 pjx_flags;
	struct psclist_head		 pjx_lentry1;		/* pending transaction list */
	struct psclist_head		 pjx_lentry2;		/* distill transaction list */
	psc_spinlock_t			 pjx_lock;
	struct psc_journal		*pjx_pj;
	void				*pjx_data;
};

/* Actions to be taked after open the log file */
#define	PJOURNAL_LOG_DUMP		1
#define	PJOURNAL_LOG_REPLAY		2

/* definitions of journal handling functions */
int			 pjournal_dump(const char *, int);
int			 pjournal_format(const char *, uint32_t, uint32_t, uint32_t);
struct psc_journal	*pjournal_init(const char *, uint64_t, int, const char *,
				psc_embed_handler, psc_replay_handler, psc_distill_handler);

/* definitions of transaction handling functions */
struct psc_journal_xidhndl	*pjournal_xnew(struct psc_journal *);
int				 pjournal_xadd(struct psc_journal_xidhndl *, int, void *, size_t);
int				 pjournal_xend(struct psc_journal_xidhndl *);
int				 pjournal_xadd_sngl(struct psc_journal *, int, void *, size_t);

#define PJ_GETENTOFF(pj, i)	((off_t)(pj)->pj_hdr->pjh_start_off + (i) * PJ_PJESZ(pj))

#endif /* _PFL_JOURNAL_H_ */
