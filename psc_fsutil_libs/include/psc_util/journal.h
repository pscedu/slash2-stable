/* $Id$ */

#ifndef _PFL_JOURNAL_H_
#define _PFL_JOURNAL_H_

#include <sys/user.h>

#include "psc_ds/dynarray.h"
#include "psc_util/atomic.h"
#include "psc_util/lock.h"
#include "psc_util/thread.h"
#include "psc_util/waitq.h"

#define PJ_LOCK(pj)     spinlock(&(pj)->pj_lock)
#define PJ_ULOCK(pj)    freelock(&(pj)->pj_lock)

#define PJH_MAGIC	0x45678912aabbccffULL	/* magic number of the journal header */

#define PJH_VERSION     0x01

struct _psc_journal_hdr {			
	uint32_t	_pjh_entsz;
	uint32_t	_pjh_nents;
	uint32_t	_pjh_version;
	uint32_t	_pjh_options;
	uint32_t	_pjh_readahead;
	uint32_t	_pjh_unused;
	uint64_t	_pjh_start_off;
	uint64_t	_pjh_magic;		
	uint64_t	_pjh_chksum;		/* keep it last and aligned at a 8 byte boundary */
};

#define PJH_ALIGN_SIZE	PSC_ALIGN(sizeof(struct _psc_journal_hdr), PAGE_SIZE)

struct psc_journal_hdr {
	struct _psc_journal_hdr pjh;
	char			pjg__pad[PJH_ALIGN_SIZE - sizeof(struct _psc_journal_hdr)];
#define pjh_entsz	pjh._pjh_entsz
#define pjh_nents	pjh._pjh_nents
#define pjh_version	pjh._pjh_version
#define pjh_options	pjh._pjh_options
#define pjh_readahead	pjh._pjh_readahead
#define pjh_unused	pjh._pjh_unused
#define pjh_start_off	pjh._pjh_start_off
#define pjh_magic	pjh._pjh_magic
#define pjh_chksum	pjh._pjh_chksum
};

#define	MAX_NUM_PJBUF		 8		/* number of journal buffers to keep around */

struct psc_journal {
	psc_spinlock_t		 pj_lock;	/* contention lock */
	int			 pj_fd;		/* open file descriptor to disk */
	char			*pj_logname;	/* log file name */
	uint64_t		 pj_nextxid;	/* next transaction ID */
	uint32_t		 pj_nextwrite;	/* next entry slot to write to */
	struct psclist_head	 pj_pndgxids;
	struct dynarray		 pj_bufs;
	struct psc_journal_hdr	*pj_hdr;
	struct psc_waitq	 pj_waitq;
};

typedef void (*psc_jhandler)(struct dynarray *, int);

/*
 * Start offset to write journal entries (i.e., pje). This means that the log header must be smaller than PJE_OFFSET.
 */
#define PJE_OFFSET		PJH_ALIGN_SIZE

#define PJE_XID_NONE		0			/* invalid transaction ID */
#define PJE_MAGIC		0x45678912aabbccddULL	/* magic number for each journal entry */

/*
 * Journal entry types - higher bits after PJET_LASTBIT are used to identify different log users.
 */
#define PJE_NONE		(0 << 0)		/* null journal record */
#define PJE_FORMAT		(1 << 1)		/* newly-formatted journal record */
#define PJE_XCLOSED		(1 << 2)		/* xid is closed */
#define PJE_XSTARTED		(1 << 3)		/* transaction began */
#define PJE_STARTUP		(1 << 4)		/* system startup */
#define PJE_LASTBIT		 4			/* denote the last used bit */

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
 */
struct psc_journal_enthdr {
	uint64_t		pje_magic;
	uint32_t		pje_type;		/* see above */
	uint64_t		pje_xid;
	uint32_t		pje_sid;
	uint32_t		pje_len;		/* for calculating checksum of the following data */
	uint64_t		pje_chksum;
	/*
	 * The length of the pje_data[0] is also embedded and can be figured out
	 * by log replay functions.
	 */
	char			pje_data[0];
};

#define	PJ_PJESZ(p)		((p)->pj_hdr->pjh_entsz)

/*
 * psc_journal_xidhndl - journal transaction id handle.
 * @pjx_xid: the transaction id.
 * @pjx_sid: the xid sub-operation id.
 * @pjx_tailslot: the address of our starting / oldest slot.
 * @pjx_flags: app-specific log entry bits.
 * @pjx_lentry: open xid handles are chained in journal structure.
 * @pjx_lock: serialize.
 * @pjx_pj: backpointer to our journal.
 */
#define	PJX_SLOT_ANY		 (~0U)

#define	PJX_NONE		 (0 << 0)
#define	PJX_XSTARTED		 (1 << 0)
#define	PJX_XCLOSED		 (1 << 1)

struct psc_journal_xidhndl {
	uint64_t		 pjx_xid;
	atomic_t		 pjx_sid;
	uint32_t		 pjx_tailslot;
	uint32_t		 pjx_flags;
	struct psclist_head	 pjx_lentry;
	psc_spinlock_t		 pjx_lock;
	struct psc_journal	*pjx_pj;
};

/* definitions of journal handling functions */
struct psc_journal *		pjournal_load(const char *);
int				pjournal_dump(const char *);
void				pjournal_format(const char *, uint32_t, uint32_t, uint32_t, uint32_t);

/* definitions of transaction handling functions */
struct psc_journal_xidhndl *	pjournal_xnew(struct psc_journal *);
int				pjournal_xadd(struct psc_journal_xidhndl *, int, void *, size_t);
int				pjournal_xend(struct psc_journal_xidhndl *);

#endif /* _PFL_JOURNAL_H_ */
