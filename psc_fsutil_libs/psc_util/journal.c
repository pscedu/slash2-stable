/* $Id$ */

#define PSC_SUBSYS PSS_JOURNAL

#define _XOPEN_SOURCE 600
#define _GNU_SOURCE

#include <sys/types.h>

#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>

#include "psc_types.h"
#include "psc_util/alloc.h"
#include "psc_util/atomic.h"
#include "psc_util/journal.h"
#include "psc_util/lock.h"
#include "psc_util/subsys.h"

/*
 * pjournal_init - initialize the in-memory representation of a journal.
 * @pj: the journal.
 * @start: disk address of beginning of journal.
 * @nents: number of entries journal can store before wrapping.
 * @entsz: size of a journal entry.
 */
void
pjournal_init(struct psc_journal *pj, const char *fn, daddr_t start,
	      int nents, int entsz, int ra)
{
	int fd;

	fd = open(fn, O_RDWR | O_CREAT);
	if (fd == -1)
		psc_fatal("open %s", fn);

	memset(pj, 0, sizeof(*pj));
	LOCK_INIT(&pj->pj_lock);
	pj->pj_daddr = start;
	pj->pj_nents = nents;
	pj->pj_entsz = entsz;
	pj->pj_fd = fd;
	pj->pj_readahead = ra;
	INIT_PSCLIST_HEAD(&pj->pj_pndgxids);
	psc_waitq_init(&pj->pj_waitq);
}

/*
 * pjournal_nextxid - obtain an unused journal transaction ID.
 * @pj: the journal.
 * Returns: new, unused transaction ID.
 */
struct psc_journal_xidhndl *
pjournal_nextxid(struct psc_journal *pj)
{
	struct psc_journal_xidhndl *xh;

	xh = PSCALLOC(sizeof(*xh));

	PJ_LOCK(pj);
	do {
		xh->pjx_xid = ++pj->pj_nextxid;
	} while (xh->pjx_xid == PJE_XID_NONE);
	PJ_ULOCK(pj);

	xh->pjx_pj = pj;
	return (xh);
}

/*
 * _pjournal_logwrite - store a new entry in a journal.
 * @pj: the journal.
 * @slot: position location in journal to write.
 * @type: the application-specific log entry type.
 * @xid: transaction ID of entry.
 * @data: the journal entry contents to store.
 * Returns: 0 on success, -1 on error.
 */
__static int
_pjournal_logwrite(struct psc_journal *pj, int xid, int slot, int type,
    void *data)
{
	struct psc_journal_enthdr *pje;
	int rc;

	psc_assert(slot < pj->pj_nents);

	pje = psc_alloc(PJ_PJESZ(pj), PAF_PAGEALIGN | PAF_LOCK);
	if (data)
		memcpy(pje->pje_data, data, pj->pj_entsz);

	psc_assert(pje->pje_genmarker == 0 ||
		   pje->pje_genmarker == PJET_LOG_STMRK);

	pje->pje_genmarker |= pj->pj_genid;
	pje->pje_magic = PJE_MAGIC;
	pje->pje_type = type;
	pje->pje_xid = xid;

	rc = pwrite(pj->pj_fd, pje, pj->pj_entsz,
	       (daddr_t)(pj->pj_daddr + (slot * pj->pj_entsz)));

	free(pje);
	return (rc);
}

/*
 * _pjournal_logwritex - store a new entry in a journal transaction.
 * @pj: the journal.
 * @type: the application-specific log entry type.
 * @xid: transaction ID.
 * @data: the journal entry contents to store.
 * Returns: 0 on success, -1 on error.
 */
int
pjournal_logwrite(struct psc_journal_xidhndl *xh, int type, void *data)
{
	int rc, slot, freexh=0;
	struct psc_journal *pj = xh->pjx_pj;
	struct psc_journal_xidhndl *t;

	if (type == PJET_VOID)
		psc_assert(!data);

	psc_assert(!(type & PJET_CORRUPT));
	psc_assert(!(type & PJET_XSTARTED));
	psc_assert(!(type & PJET_XEND));

	psc_assert(!(xh->pjx_flags & PJET_CLOSED));

 retry:
	PJ_LOCK(pj);
	/* This is the 'highest' available slot at the moment.
	 */
	slot = pj->pj_nextwrite;
	t = psclist_first_entry(&pj->pj_pndgxids, struct psc_journal_xidhndl,
				pjx_lentry);

	psc_trace("pj(%p) tail@slot(%d) my@slot(%d)",
		  pj, t->pjx_tailslot, slot);

	if (t->pjx_tailslot == slot) {
		psc_warnx("pj(%p) blocking on slot(%d) availability - "
			  "owned by xid (%p)", pj, slot, t);
		psc_waitq_wait(&pj->pj_waitq, &pj->pj_lock);
		goto retry;
	}

	if (atomic_dec_and_test(&xh->pjx_ref) &&
	    (xh->pjx_flags & PJET_XEND)) {
		if (xh->pjx_flags & PJET_XSTARTED) {
			xh->pjx_flags |= PJET_CLOSED;
			psclist_del(&xh->pjx_lentry);
			if (xh->pjx_tailslot == pj->pj_nextwrite) {
				/* We are the tail so unblock the journal.
				 */
				psc_warnx("pj(%p) unblocking slot(%d) - "
					  "owned by xid (%p)", pj, slot, xh);
				psc_waitq_wakeall(&pj->pj_waitq);
			}
		}
		freexh = 1;

	} else if (!(xh->pjx_flags & PJET_XSTARTED)) {
		/* Multi-step operation, mark the slot id here
		 *  so that the tail of the journal can be found
		 *  and that overwriting pending xids may be
		 *  prevented.
		 * Note:  self-contained ops (PJET_XEND and refcnt of 1)
		 *        cannot end up here.
		 */
		psc_assert(!(xh->pjx_flags & PJET_XEND));

		xh->pjx_tailslot = slot;
		psclist_xadd_tail(&xh->pjx_lentry, &pj->pj_pndgxids);
		xh->pjx_flags |= PJET_XSTARTED;
	}

	psc_assert(atomic_read(&xh->pjx_ref) >= 0);

	if (++pj->pj_nextwrite == pj->pj_nents) {
		pj->pj_nextwrite = 0;
		pj->pj_genid = (pj->pj_genid == PJET_LOG_GEN0) ?
			PJET_LOG_GEN1 : PJET_LOG_GEN0;

	} else
		psc_assert(pj->pj_nextwrite < pj->pj_nents);

	PJ_ULOCK(pj);

	rc = _pjournal_logwrite(pj, xh->pjx_xid, slot, type, data);

	if (freexh) {
		psc_dbg("pj(%p) freeing xid(%ld)@xh(%p) rc=%d ts=%d",
			pj, xh->pjx_xid, xh, rc, xh->pjx_tailslot);
		psc_assert(psclist_disjoint(&xh->pjx_lentry));
		PSCFREE(xh);
	}
	return (rc);
}

/*
 * pjournal_logread - get a specified entry from a journal.
 * @pj: the journal.
 * @slot: the position in the journal of the entry to obtain.
 * @pje: an entry to be filled in for the journal entry.
 * Returns: 'n' entries read on success, -1 on error.
 */
int
pjournal_logread(struct psc_journal *pj, int slot, void *data)
{
	daddr_t addr;
	char *p = data;
	int i, ra=pj->pj_readahead;
	ssize_t rc;

	if ((unsigned long)data & (pscPageSize - 1))
		psc_fatal("data is not page-aligned");

	if (slot >= pj->pj_nents)
		return (-1);

	while ((slot + ra) >= pj->pj_nents)
		ra--;

	addr = pj->pj_daddr + slot * pj->pj_entsz;

	rc = pread(pj->pj_fd, data, pj->pj_entsz * ra, addr);
	if (rc < 0) {
		psc_warn("pj(%p) failed read (errno=%d)", pj, errno);
		return (-errno);

	} else if (rc != pj->pj_entsz * ra) {
		psc_warnx("pj(%p) failed read, sz(%d) != rc(%zd)", pj,
			  pj->pj_entsz * ra, rc);
		return (-1);
	}

	for (i=0; i < ra; i++) {
		struct psc_journal_enthdr *h;
		
		h = (void *)&p[pj->pj_entsz * i];

		if ((h->pje_magic != PJE_MAGIC ||
		     h->pje_magic != PJE_FMT_MAGIC) ||
		    (h->pje_magic == PJE_FMT_MAGIC &&
		     h->pje_xid != PJE_XID_NONE)) {
			psc_warnx("pj(%p) slot@%d failed magic", pj, slot + i);
			h->pje_type |= PJET_CORRUPT;

		} else if (h->pje_genmarker != PJET_LOG_GEN0 &&
			   h->pje_genmarker != PJET_LOG_GEN1) {
			psc_warnx("pj(%p) slot@%d bad gen marker", pj, slot + i);
			h->pje_type |= PJET_CORRUPT;
		}
	}
	return (ra);
}


int
pjournal_xadd(struct psc_journal_xidhndl *xh, int type, void *data)
{
	spinlock(&xh->pjx_lock);
	psc_assert(!(xh->pjx_flags & PJET_XEND));
	atomic_inc(&xh->pjx_ref);
	freelock(&xh->pjx_lock);

	return(pjournal_logwrite(xh, type, data));
}

int
pjournal_xend(struct psc_journal_xidhndl *xh, int type, void *data)
{
	spinlock(&xh->pjx_lock);
	psc_assert(!(xh->pjx_flags & PJET_XEND));
	xh->pjx_flags |= PJET_XEND;
	atomic_inc(&xh->pjx_ref);
	freelock(&xh->pjx_lock);

	return (pjournal_logwrite(xh, type, data));
}

int
pjournal_start_mark(struct psc_journal *pj, int slot)
{
	struct psc_journal_enthdr *pje;

	pje = psc_alloc(PJ_PJESZ(pj), PAF_PAGEALIGN | PAF_LOCK);
	pje->pje_genmarker = PJET_LOG_STMRK;
	_pjournal_logwrite(pj, PJE_XID_NONE, slot, PJET_VOID, pje);
	free(pje);
	return (0);
}

/*
 * pjournal_alloclog - allocate a log for I/O to a journal.
 * @pj: the journal.
 * Returns: data buffer pointer valid for journal I/O.
 */
void *
pjournal_alloclog(struct psc_journal *pj)
{
	return (psc_alloc(PJ_PJESZ(pj), PAF_PAGEALIGN | PAF_LOCK));
}

void *
pjournal_alloclog_ra(struct psc_journal *pj)
{
	return (psc_alloc(PJ_PJESZ(pj) * pj->pj_readahead,
	    PAF_PAGEALIGN | PAF_LOCK));
}

int
pjournal_format(struct psc_journal *pj)
{
	unsigned char *jbuf=pjournal_alloclog_ra(pj);
	daddr_t addr;
	int ra, rc, i, slot=0;

	for (ra=pj->pj_readahead, slot=0; slot < pj->pj_nents; slot += ra) {
		while ((slot + ra) >= pj->pj_nents)
			ra--;

		for (i=0; i < ra; i++) {
			struct psc_journal_enthdr *h;
			
			h = (void *)&jbuf[pj->pj_entsz * i]; 
			h->pje_magic = 0x45678912aabbccffULL;
			h->pje_type = PJET_VOID;
			h->pje_xid = PJE_XID_NONE;
		}
		addr = pj->pj_daddr + slot * pj->pj_entsz;
		rc = pwrite(pj->pj_fd, jbuf, pj->pj_entsz * ra, addr);
		if (rc < 0)
			return (-errno);
	}
	return (0);
}

__static int
pjournal_headtail_get(struct psc_journal *pj, struct psc_journal_walker *pjw)
{
	unsigned char *jbuf=pjournal_alloclog_ra(pj);
	int rc=0, i, ents=0;
	u32 tm=PJET_SLOT_ANY, sm=PJET_SLOT_ANY, lastgen;

	pjw->pjw_pos = pjw->pjw_stop = 0;

	while (ents < pj->pj_nents) {
		rc = pjournal_logread(pj, pjw->pjw_pos, jbuf);
		if (rc < 0)
			return (-1);

		for (i=0; i < rc; i++) {
			struct psc_journal_enthdr *h;

			h = (void *)&jbuf[pj->pj_entsz * i];
			/* Punt for now.
			 */
			if (h->pje_type & PJET_CORRUPT) {
				rc = -1;
				goto out;
			}
			if (!ents && !i)
				lastgen = h->pje_genmarker;

			if (h->pje_magic == PJE_FMT_MAGIC) {
				/* Newly formatted log.
				 */
				pjw->pjw_pos = (sm == PJET_SLOT_ANY ? 0 : sm);
				pjw->pjw_stop = (ents + i) - 1;
				rc = 0;
				goto out;
			}
			if ((lastgen & PJET_LOG_GMASK) !=
			    (h->pje_genmarker & PJET_LOG_GMASK)) {
				psc_trace("found tm @slot(%d)", (ents + i)-1);
				/* Found a transition marker, everything from
				 *  here until the end is from the previous
				 *  log wrap.
				 */
				tm = (ents + i) - 1;
				if (sm != PJET_SLOT_ANY) {
					/* Here's the case where the tm > sm
					 *  which means the log didn't wrap.
					 */
					pjw->pjw_pos = sm;
					pjw->pjw_stop = tm;
					rc = 0;
					goto out;
				}
				/* Else..
				 * The sm is either after the tm and of a
				 *  different gen or there is no sm which
				 *  would mean that tm+1 would be the sm.
				 */
			}
			if (h->pje_genmarker & PJET_LOG_STMRK)
				sm = ents + i;
		}
		ents += rc;
	}

	pjw->pjw_pos  = ((sm != PJET_SLOT_ANY) ? sm : (tm+1));
	/* This catches the case where the tm is at the very last slot
	 *  which means that the log didn't wrap but was about to.
	 */
	pjw->pjw_stop = ((tm != PJET_SLOT_ANY) ? tm : (pj->pj_nents-1));
 out:
	psc_info("journal pos (S=%d) (E=%d) (rc=%d)",
		 pjw->pjw_pos, pjw->pjw_stop, rc);
	free(jbuf);
	return (rc);
}

/*
 * pjournal_walk - traverse each entry in a journal.
 * @pj: the journal.
 * @pjw: a walker for the journal.
 * @pje: an entry to be filled in for the next journal entry.
 * Returns: 0 on success, -1 on error, -2 on end of journal.
 */
int
pjournal_replay(struct psc_journal *pj, psc_jhandler pj_handler)
{
	void *jbuf=pjournal_alloclog_ra(pj);
	struct psc_journal_walker pjw;
	int rc, nents;

	psc_assert(pj && pj_handler);

	rc = pjournal_headtail_get(pj, &pjw);
	if (rc < 0)
		return (rc);

	if (pjw.pjw_stop >= pjw.pjw_pos)
		nents = pjw.pjw_stop - pjw.pjw_pos;
	else
		nents = (pj->pj_nents - pjw.pjw_pos) + pjw.pjw_stop + 1;

	while (nents) {
		rc = pjournal_logread(pj, pjw.pjw_pos, jbuf);
		if (rc < 0)
			return (-1);

		if (pjw.pjw_cb)
			(pj_handler)(jbuf, rc);

		nents -= rc;

		if ((pjw.pjw_pos += rc) >= pj->pj_nents)
			pjw.pjw_pos = 0;
	}

	free(jbuf);
	return (0);
}
