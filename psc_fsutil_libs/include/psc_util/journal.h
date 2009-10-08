/* $Id$ */

#ifndef _PFL_JOURNAL_H_
#define _PFL_JOURNAL_H_

#include "psc_ds/dynarray.h"
#include "psc_util/atomic.h"
#include "psc_util/lock.h"
#include "psc_util/thread.h"
#include "psc_util/waitq.h"

#define PJ_LOCK(pj)     spinlock(&(pj)->pj_lock)
#define PJ_ULOCK(pj)    freelock(&(pj)->pj_lock)

#define PJE_MAGIC       0x45678912aabbccddULL
#define PJE_FMT_MAGIC   0x45678912aabbccffULL
#define PJE_XID_NONE    0 /* invalid transaction ID */

/* Start writing journal entries (pje) at offset 4k, header must be smaller than PJE_OFFSET.
 */
#define PJE_OFFSET      0x1000
#define PJE_VERSION     0x01

struct psc_journal_hdr {
	uint32_t       pjh_entsz;
	uint32_t       pjh_nents;
	uint32_t       pjh_version;
	uint32_t       pjh_options;
	uint32_t       pjh_readahead;
	uint32_t       pjh_unused;
	uint64_t       pjh_start_off;
	uint64_t       pjh_magic;
};


struct psc_journal {
	psc_spinlock_t		 pj_lock;	/* contention lock */
	uint64_t		 pj_nextxid;	/* next transaction ID */
	uint32_t		 pj_nextwrite;	/* next entry slot to write to */
	int			 pj_genid;	/* current wrap generation */
	int			 pj_fd;		/* open file descriptor to disk */
	struct psclist_head 	 pj_pndgxids;
	struct dynarray 	 pj_bufs;
	struct psc_journal_hdr	*pj_hdr;
	struct psc_waitq	 pj_waitq;
};

typedef void (*psc_jhandler)(void *, int);

struct psc_journal_walker {
	uint32_t	pjw_pos;	/* current position */
	uint32_t	pjw_stop;	/* targetted end position */
	int		pjw_seen;	/* whether to terminate at stop_pos */
	psc_jhandler	pjw_cb;
};

#define PJET_LOG_GEN0  0x00001010
#define PJET_LOG_GEN1  0x0000fefe
#define PJET_LOG_GMASK 0x0000ffff
#define PJET_LOG_STMRK 0xffff0000

#define PJET_SLOT_ANY	(~0U)

/*
 * psc_journal_enthdr - journal entry header.
 * @pje_magic: validity check.
 * @pje_genmarker: field to detect log wrapping.
 * @pje_type: app-specific log entry type.
 * @pje_xid: journal transaction id.
 * @pje_sid: xid sub-id.
 * @pje_pad: alignment purposes.
 * @pje_data: application data.
 * Notes: at some point we may want to make this into a footer which has
 *    a crc field.
 */
struct psc_journal_enthdr {
	uint64_t		pje_magic;
	uint32_t		pje_genmarker;
	uint32_t		pje_type;
	uint64_t		pje_xid;
	uint32_t		pje_sid;
	uint32_t		pje_pad;
	char			pje_data[0];
};

#define PJ_PJESZ(p) (size_t)((sizeof(struct psc_journal_enthdr)) \
			     + (p)->pj_hdr->pjh_entsz)

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
struct psc_journal_xidhndl {
	uint64_t             pjx_xid;
	atomic_t             pjx_sid;
	uint32_t             pjx_tailslot;
	uint32_t             pjx_flags;
	struct psclist_head  pjx_lentry;
	psc_spinlock_t       pjx_lock;
	struct psc_journal  *pjx_pj;
};

/* Journal entry types. */
#define PJET_VOID	0		/* null journal record */
#define PJET_CORRUPT	(1<<0)		/* entry has failed magic */
#define PJET_CLOSED	(1<<1)		/* xid is closed */
#define PJET_XSTARTED	(1<<2)		/* transaction began */
#define PJET_XADD       (1<<3)
#define PJET_XEND	(1<<4)		/* transaction ended */
#define PJET_RESERVED   PJET_XEND       /* denote the last used bit */

struct psc_journal *
pjournal_load(const char *);

struct psc_journal_xidhndl *
pjournal_nextxid(struct psc_journal *);

void	*pjournal_alloclog(struct psc_journal *);
int	 pjournal_clearlog(struct psc_journal *, int);
int	 pjournal_dump(const char *);
void	 pjournal_format(const char *, uint32_t, uint32_t, uint32_t, uint32_t);
int	 pjournal_logread(struct psc_journal *, uint32_t, void *);
int	 pjournal_logwrite(struct psc_journal_xidhndl *, int, void *, size_t);
int	 pjournal_logwritex(struct psc_journal *, int, int, void *, size_t);
int	 pjournal_xadd(struct psc_journal_xidhndl *, int, void *, size_t);
int	 pjournal_xend(struct psc_journal_xidhndl *, int, void *, size_t);
void	 pjournal_xidhndl_free(struct psc_journal_xidhndl *);
int	 pjournal_xstart(struct psc_journal *, int, size_t);
int	 pjournal_walk(struct psc_journal *, struct psc_journal_walker *,
	    struct psc_journal_enthdr *);

#define pjournal_xidhndl_free(xh)	PSCFREE(xh)

#endif /* _PFL_JOURNAL_H_ */
