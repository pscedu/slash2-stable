/* $Id$ */

#ifndef _PFL_JOURNAL_H_
#define _PFL_JOURNAL_H_

#include "psc_types.h"
#include "psc_util/atomic.h"
#include "psc_util/lock.h"
#include "psc_util/thread.h"
#include "psc_util/waitq.h"

#define PJ_LOCK(pj)     spinlock(&(pj)->pj_lock)
#define PJ_ULOCK(pj)    freelock(&(pj)->pj_lock)

#define PJE_MAGIC       0x45678912aabbccddULL
#define PJE_FMT_MAGIC   0x45678912aabbccffULL
#define PJE_XID_NONE    0               /* invalid transaction ID */

struct psc_journal {
	psc_spinlock_t	pj_lock;	/* contention lock */
	uint32_t	pj_entsz;	/* sizeof log entry */
	uint32_t	pj_nents;	/* #ent slots in journal */
	uint32_t	pj_readahead;	/* grab lots of entries */
	off_t		pj_daddr;	/* disk offset of starting ent */
	uint32_t	pj_nextwrite;	/* next entry slot to write to */
	int		pj_genid;	/* current wrap generation */
	uint64_t	pj_nextxid;	/* next transaction ID */
	int		pj_fd;		/* open file descriptor to disk */
	struct psclist_head pj_pndgxids;
	psc_waitq_t	pj_waitq;
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

struct psc_journal_enthdr {
	uint64_t		pje_magic;	/* validity check */
	uint32_t		pje_genmarker;  /* field to detect log wrapping */
	uint32_t		pje_type;	/* app-specific log entry type */
	uint64_t		pje_xid;	/* journal transaction id */
	uint32_t		pje_sid;	/* xid sub-id */
	uint32_t		pje_pad;
	char		pje_data[0];
};

#define PJ_PJESZ(p) (size_t)((sizeof(struct psc_journal_enthdr)) \
			     + (p)->pj_entsz)

struct psc_journal_xidhndl {
	uint64_t             pjx_xid;
	atomic_t            pjx_sid;
	uint32_t             pjx_tailslot;
	uint32_t             pjx_flags;   /* app-specific log entry type */
	struct psclist_head pjx_lentry;  /* chain on journal */
	psc_spinlock_t      pjx_lock;    /* serialize */
	struct psc_journal *pjx_pj;
	atomic_t            pjx_ref;
};

/* Journal entry types. */
#define PJET_VOID	0		/* null journal record */
#define PJET_CORRUPT	(2<<27)		/* entry has failed magic */
#define PJET_CLOSED	(2<<28)		/* xid is closed */
#define PJET_XSTARTED	(2<<29)		/* transaction began */
#define PJET_XADD       (2<<30)
#define PJET_XEND	(2<<31)		/* transaction ended */

void	 pjournal_init(struct psc_journal *, const char *, off_t, int, int, int);
struct psc_journal_xidhndl *
	 pjournal_nextxid(struct psc_journal *);
int	 pjournal_xstart(struct psc_journal *, int);
int	 pjournal_xend(struct psc_journal_xidhndl *, int, void *);
int	 pjournal_clearlog(struct psc_journal *, int);
void	*pjournal_alloclog(struct psc_journal *);
int	 pjournal_logwritex(struct psc_journal *, int, int, void *);
int	 pjournal_logwrite(struct psc_journal_xidhndl *, int, void *);
int	 pjournal_logread(struct psc_journal *, uint32_t, void *);
int	 pjournal_walk(struct psc_journal *, struct psc_journal_walker *,
	    struct psc_journal_enthdr *);
int
pjournal_xadd(struct psc_journal_xidhndl *xh, int type, void *data);


#endif /* _PFL_JOURNAL_H_ */
