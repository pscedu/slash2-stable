/* $Id$ */

#define PSC_SUBSYS PSS_JOURNAL
#include "psc_util/subsys.h"

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

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

static struct psc_journal_xidhndl *
pjournal_xidhndl_new(struct psc_journal *pj)
{
	struct psc_journal_xidhndl *xh;

	xh = PSCALLOC(sizeof(*xh));

	xh->pjx_pj = pj;
	INIT_PSCLIST_ENTRY(&xh->pjx_lentry);
	LOCK_INIT(&xh->pjx_lock);
	return (xh);
}

void
pjournal_xidhndl_free(struct psc_journal_xidhndl *xh)
{
	PSCFREE(xh);
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

	xh = pjournal_xidhndl_new(pj);

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
_pjournal_logwrite(struct psc_journal *pj, struct psc_journal_xidhndl *xh,
		   uint32_t slot, int type, void *data, size_t size)
{
	struct psc_journal_enthdr *pje;
	int rc;

	psc_assert(slot < pj->pj_hdr->pjh_nents);
	psc_assert(size <= pj->pj_hdr->pjh_entsz);

	pje = psc_alloc(PJ_PJESZ(pj), PAF_PAGEALIGN | PAF_LOCK);
	if (data)
		memcpy(pje->pje_data, data, size);

	psc_assert(!pje->pje_genmarker);

	pje->pje_genmarker |= pj->pj_genid;
	pje->pje_magic = PJE_MAGIC;
	pje->pje_type = type;
	pje->pje_xid = xh->pjx_xid;

	if (!(type & PJET_XEND))
		pje->pje_sid = atomic_inc_return(&xh->pjx_sid);
	else
		pje->pje_sid = atomic_read(&xh->pjx_sid);

	//XXX	rc = pwrite(pj->pj_fd, pje, pj->pj_hdr->pjh_entsz,
	//XXX       (off_t)(pj->pj_hdr->pjh_start_off + (slot * pj->pj_hdr->pjh_entsz)));

	psc_freel(pje, PJ_PJESZ(pj));
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
pjournal_logwrite(struct psc_journal_xidhndl *xh, int type, void *data,
		  size_t size)
{
	int rc, freexh=0;
	uint32_t slot;
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
	/* The 'highest' available slot at the moment.
	 */
	slot = pj->pj_nextwrite;
	/* The head of this list represents the oldest pending transaction
	 *  slot.  By checking it, we're trying to prevent the journal from
	 *  over writing slots belonging to open transactions.
	 */
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

	if ((++pj->pj_nextwrite) == pj->pj_hdr->pjh_nents) {
		pj->pj_nextwrite = 0;
		pj->pj_genid = (pj->pj_genid == PJET_LOG_GEN0) ?
			PJET_LOG_GEN1 : PJET_LOG_GEN0;

	} else
		psc_assert(pj->pj_nextwrite < pj->pj_hdr->pjh_nents);

	PJ_ULOCK(pj);

	rc = _pjournal_logwrite(pj, xh, slot, type, data, size);

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
pjournal_logread(struct psc_journal *pj, uint32_t slot, void *data)
{
	daddr_t addr;
	char *p = data;
	int i, ra=pj->pj_hdr->pjh_readahead;
	ssize_t rc;

	if ((unsigned long)data & (pscPageSize - 1))
		psc_fatal("data is not page-aligned");

	if (slot >= pj->pj_hdr->pjh_nents)
		return (-1);

	while ((slot + ra) >= pj->pj_hdr->pjh_nents)
		ra--;

	addr = pj->pj_hdr->pjh_start_off + slot * pj->pj_hdr->pjh_entsz;

	rc = pread(pj->pj_fd, data, pj->pj_hdr->pjh_entsz * ra, addr);
	if (rc < 0) {
		psc_warn("pj(%p) failed read (errno=%d)", pj, errno);
		return (-errno);

	} else if (rc != pj->pj_hdr->pjh_entsz * ra) {
		psc_warnx("pj(%p) failed read, sz(%d) != rc(%zd)", pj,
			  pj->pj_hdr->pjh_entsz * ra, rc);
		return (-1);
	}

	for (i=0; i < ra; i++) {
		struct psc_journal_enthdr *h;

		h = (void *)&p[pj->pj_hdr->pjh_entsz * i];

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
pjournal_xadd(struct psc_journal_xidhndl *xh, int type, void *data,
	      size_t size)
{
	spinlock(&xh->pjx_lock);
	psc_assert(!(xh->pjx_flags & PJET_XEND));
	atomic_inc(&xh->pjx_ref);
	freelock(&xh->pjx_lock);

	return (pjournal_logwrite(xh, type, data, size));
}

int
pjournal_xend(struct psc_journal_xidhndl *xh, int type, void *data,
	      size_t size)
{
	spinlock(&xh->pjx_lock);
	psc_assert(!(xh->pjx_flags & PJET_XEND));
	xh->pjx_flags |= PJET_XEND;
	atomic_inc(&xh->pjx_ref);
	freelock(&xh->pjx_lock);

	return (pjournal_logwrite(xh, type, data, size));
}

int
pjournal_start_mark(struct psc_journal *pj, int slot)
{
	struct psc_journal_enthdr *pje;
	int rc;

	pje = psc_alloc(PJ_PJESZ(pj), PAF_PAGEALIGN | PAF_LOCK);

	pje->pje_genmarker = PJET_LOG_STMRK;
	pje->pje_magic = PJE_MAGIC;
	pje->pje_xid = PJE_XID_NONE;
	pje->pje_type = PJET_VOID;

	rc = pwrite(pj->pj_fd, pje, pj->pj_hdr->pjh_entsz,
		    (off_t)(pj->pj_hdr->pjh_start_off +
			    (slot * pj->pj_hdr->pjh_entsz)));

	psc_freel(pje, PJ_PJESZ(pj));
	return (rc);
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
	psc_trace("rasz=%zd", PJ_PJESZ(pj) * pj->pj_hdr->pjh_readahead);
	return (psc_alloc(PJ_PJESZ(pj) * pj->pj_hdr->pjh_readahead,
			  PAF_PAGEALIGN | PAF_LOCK));
}


__static int
pjournal_headtail_get(struct psc_journal *pj, struct psc_journal_walker *pjw)
{
	unsigned char *jbuf=pjournal_alloclog_ra(pj);
	int rc=0, i;
        uint32_t tm=PJET_SLOT_ANY, sm=PJET_SLOT_ANY, lastgen, ents=0;

	lastgen = 0; /* gcc */
	pjw->pjw_pos = pjw->pjw_stop = 0;

	while (ents < pj->pj_hdr->pjh_nents) {
		rc = pjournal_logread(pj, pjw->pjw_pos, jbuf);
		if (rc < 0)
			return (-1);

		for (i=0; i < rc; i++) {
			struct psc_journal_enthdr *h;

			h = (void *)&jbuf[pj->pj_hdr->pjh_entsz * i];
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
	pjw->pjw_stop = ((tm != PJET_SLOT_ANY) ? tm : (uint32_t)(pj->pj_hdr->pjh_nents-1));
 out:
	psc_info("journal pos (S=%d) (E=%d) (rc=%d)",
		 pjw->pjw_pos, pjw->pjw_stop, rc);
	PSCFREE(jbuf);
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
	int rc;
	uint32_t nents=0;

	psc_assert(pj && pj_handler);

	rc = pjournal_headtail_get(pj, &pjw);
	if (rc < 0)
		return (rc);

	if (pjw.pjw_stop >= pjw.pjw_pos)
		nents = pjw.pjw_stop - pjw.pjw_pos;
	else
		nents = (pj->pj_hdr->pjh_nents - pjw.pjw_pos) + pjw.pjw_stop + 1;

	while (nents) {
		rc = pjournal_logread(pj, pjw.pjw_pos, jbuf);
		if (rc < 0)
			return (-1);

		(pj_handler)(jbuf, rc);

		nents -= rc;

		if ((pjw.pjw_pos += rc) >= pj->pj_hdr->pjh_nents)
			pjw.pjw_pos = 0;
	}

	PSCFREE(jbuf);
	return (0);
}

/*
 * pjournal_init - initialize the in-memory representation of a journal.
 * @pj: the journal.
 * @start: disk address of beginning of journal.
 * @nents: number of entries journal can store before wrapping.
 * @entsz: size of a journal entry.
 */
struct psc_journal *
pjournal_load(const char *fn)
{
	struct psc_journal_hdr *pjh = PSCALLOC(sizeof(*pjh));
	struct psc_journal *pj = PSCALLOC(sizeof(*pj));
	void *hdr = psc_alloc(PJE_OFFSET, PAF_PAGEALIGN);

	pj->pj_fd = open(fn, O_RDWR|O_DIRECT);
	if (pj->pj_fd < 0)
		psc_fatal("open %s", fn);

	if (pread(pj->pj_fd, hdr, PJE_OFFSET, 0) != PJE_OFFSET)
		psc_fatal("Failed to read journal header");

	memcpy(pjh, hdr, sizeof(*pjh));
	pj->pj_hdr = pjh;

	psc_freen(hdr);

	if (pjh->pjh_magic != PJE_MAGIC)
		psc_fatalx("Journal header has bad magic!");

	LOCK_INIT(&pj->pj_lock);
	INIT_PSCLIST_HEAD(&pj->pj_pndgxids);
	psc_waitq_init(&pj->pj_waitq);

	return (pj);
}


int
pjournal_dump(const char *fn)
{
	struct psc_journal *pj;
	struct psc_journal_hdr *pjh;
	struct psc_journal_enthdr *h;
	uint32_t slot, ra, i;
	unsigned char *jbuf;

	pj = pjournal_load(fn);
	pjh = pj->pj_hdr;

	fprintf(stdout, "entsz=%u nents=%u vers=%u opts=%u ra=%u "
		"off=%"PRIx64" magic=%"PRIx64,
		pjh->pjh_entsz, pjh->pjh_nents, pjh->pjh_version, pjh->pjh_options,
		pjh->pjh_readahead, pjh->pjh_start_off, pjh->pjh_magic);

	if (pjh->pjh_magic != PJE_MAGIC)
		psc_warnx("journal %s has bad magic!", fn);

	jbuf = pjournal_alloclog_ra(pj);

	for (slot=0, ra=pjh->pjh_readahead; slot < pjh->pjh_nents;
	     slot += pjh->pjh_readahead) {
		/* Make sure we don't read past the end.
		 */
		while ((slot + ra) > pjh->pjh_nents)
			ra--;

		if (pread(pj->pj_fd, jbuf, (pjh->pjh_entsz * ra),
			   (off_t)(PJE_OFFSET + (slot * pjh->pjh_entsz)))
		    != (pjh->pjh_entsz * ra))
			psc_fatal("Failed to write entries");

		for (i=0; i < ra; i++) {
			h = (void *)&jbuf[pjh->pjh_entsz * i];

			fprintf(stdout, "slot=%u gmrkr=%x magic=%"PRIx64
				" type=%x xid=%"PRIx64" sid=%d\n",
				(slot+i), h->pje_genmarker, h->pje_magic,
				h->pje_type, h->pje_xid, h->pje_sid);

			if (h->pje_magic != PJE_FMT_MAGIC &&
			    h->pje_magic != PJE_MAGIC)
				psc_warnx("journal entry %u has bad magic!",
					  (slot+i));
		}
	}

	if (close(pj->pj_fd) < 0)
		psc_fatal("Failed to close journal fd");

	PSCFREE(jbuf);
	return (0);
}

void
pjournal_format(const char *fn, uint32_t nents, uint32_t entsz, uint32_t ra,
		uint32_t opts)
{
	struct psc_journal pj;
	struct psc_journal_hdr pjh = {entsz, nents, PJE_VERSION, opts, ra,
				      0, PJE_OFFSET, PJE_MAGIC};
	struct psc_journal_enthdr *h;
	unsigned char *jbuf;
	uint32_t slot, i;
	int fd;

	pj.pj_hdr = &pjh;
	jbuf = pjournal_alloclog_ra(&pj);

	if ((fd = open(fn, O_WRONLY|O_CREAT|O_TRUNC, 0700)) < 0)
		psc_fatal("Could not create or truncate the journal %s", fn);

	if (pwrite(fd, &pjh, sizeof(pjh), 0) < 0)
		psc_fatal("Failed to write header");

	for (slot=0, ra=pjh.pjh_readahead; slot < pjh.pjh_nents;
	     slot += pjh.pjh_readahead) {
		/* Make sure we don't read past the end.
		 */
		while ((slot + ra) > pjh.pjh_nents)
			ra--;

		for (i=0; i < ra; i++) {
			h = (void *)&jbuf[pjh.pjh_entsz * i];
			h->pje_magic = PJE_FMT_MAGIC;
			h->pje_type = PJET_VOID;
			h->pje_xid = PJE_XID_NONE;
			h->pje_sid = PJE_XID_NONE;
		}

		if (pwrite(fd, jbuf, (pjh.pjh_entsz * ra),
			   (off_t)(PJE_OFFSET + (slot * pjh.pjh_entsz))) < 0)
			psc_fatal("Failed to write entries");
	}
	if (close(fd) < 0)
		psc_fatal("Failed to close journal fd");

	PSCFREE(jbuf);
}
