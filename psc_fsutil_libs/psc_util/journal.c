/* $Id$ */

#define PSC_SUBSYS PSS_JOURNAL
#include "psc_util/subsys.h"

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <sys/types.h>

#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <string.h>
#include <unistd.h>

#include "psc_util/alloc.h"
#include "psc_util/atomic.h"
#include "psc_util/journal.h"
#include "psc_util/lock.h"
#include "psc_ds/dynarray.h"

static struct psc_journal_xidhndl *
pjournal_xidhndl_new(struct psc_journal *pj)
{
	struct psc_journal_xidhndl *xh;

	xh = PSCALLOC(sizeof(*xh));

	xh->pjx_pj = pj;

	psc_warnx("xh=%p xh->pjx_pj=%p", xh, xh->pjx_pj);

	xh->pjx_tailslot = PJX_SLOT_ANY;
	INIT_PSCLIST_ENTRY(&xh->pjx_lentry);
	LOCK_INIT(&xh->pjx_lock);
	return (xh);
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

	psc_assert(xh->pjx_pj == pj);

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
pjournal_logwrite_internal(struct psc_journal *pj, struct psc_journal_xidhndl *xh,
			    uint32_t slot, int type, void *data, size_t size)
{
	struct psc_journal_enthdr *pje;
	int rc = 0, len;

	psc_assert(slot < pj->pj_hdr->pjh_nents);
	psc_assert(size <= PJ_PJESZ(pj));

	PJ_LOCK(pj);
	while (!(len = dynarray_len(&pj->pj_bufs))) {
		psc_waitq_wait(&pj->pj_waitq, &pj->pj_lock);
		PJ_LOCK(pj);
	}
	pje = dynarray_getpos(&pj->pj_bufs, len-1);
	psc_assert(pje);
	dynarray_remove(&pj->pj_bufs, pje);
	psc_notify("got pje=%p", pje);
	PJ_ULOCK(pj);

	if (data)
		memcpy(pje->pje_data, data, size);

	pje->pje_magic = PJE_MAGIC;
	pje->pje_type = type;
	pje->pje_xid = xh->pjx_xid;

	if (!(type & PJET_XEND))
		pje->pje_sid = atomic_inc_return(&xh->pjx_sid);
	else
		pje->pje_sid = atomic_read(&xh->pjx_sid);

#ifdef NOT_READY
	/* commit the log on disk before we can return */
	rc = pwrite(pj->pj_fd, pje, pj->pj_hdr->pjh_entsz,
		   (off_t)(pj->pj_hdr->pjh_start_off + (slot * pj->pj_hdr->pjh_entsz)));
	if (rc != -1 && rc != pj->pj_hdr->pjh_entsz)
		rc = -EAGAIN;
#endif

	PJ_LOCK(pj);
	dynarray_add(&pj->pj_bufs, pje);
	psc_waitq_wakeall(&pj->pj_waitq);
	PJ_ULOCK(pj);

	if (xh->pjx_flags & PJX_XCLOSED && xh->pjx_tailslot == pj->pj_nextwrite) {
		/* We are the tail so unblock the journal.  */
		psc_warnx("pj(%p) unblocking slot(%d) - "
			  "owned by xid (%p)",
			  pj, slot, xh);
		psc_waitq_wakeall(&pj->pj_waitq);
	}
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
	struct psc_journal_xidhndl	*t;
	int				 rc;
	struct psc_journal		*pj;
	uint32_t			 slot;
	uint32_t			 tail_slot;

	if (type & PJET_NODATA)
		psc_assert(!data);

	tail_slot = 0;
	pj = xh->pjx_pj;

	psc_assert(!(type & PJET_CORRUPT));
	psc_assert(!(type & PJET_XSTARTED));
	psc_assert(!(type & PJET_XEND));

	psc_assert(!(xh->pjx_flags & PJX_XCLOSED));

 retry:
	/*
	 * Make sure that the next slot to be written does not have a pending transaction.
	 * Since we add a new transaction at the tail of the pending transaction list, we
	 * only need to check the head of the list to find out the oldest pending transaction.
	 */
	PJ_LOCK(pj);
	slot = pj->pj_nextwrite;
	t = psclist_first_entry(&pj->pj_pndgxids, struct psc_journal_xidhndl, pjx_lentry);
	if (t) {
		if (t->pjx_tailslot == slot) {
			psc_warnx("pj(%p) blocking on slot(%d) "
				  "availability - owned by xid (%p)",
				  pj, slot, t);
			psc_waitq_wait(&pj->pj_waitq, &pj->pj_lock);
			goto retry;
		}
		tail_slot = t->pjx_tailslot;
	}

	psc_info("pj(%p) tail@slot(%d) my@slot(%d) xh_flags(%o)",
		 pj, tail_slot, slot, xh->pjx_flags);

	if (!(xh->pjx_flags & PJET_XSTARTED)) {
		/* Multi-step operation, mark the slot id here
		 *  so that the tail of the journal can be found
		 *  and that overwriting pending xids may be
		 *  prevented.
		 * Note:  self-contained ops (PJET_XEND)
		 *        cannot end up here.
		 */
		psc_assert(!(xh->pjx_flags & PJET_XEND));

		xh->pjx_tailslot = slot;
		psclist_xadd_tail(&xh->pjx_lentry, &pj->pj_pndgxids);
		xh->pjx_flags |= PJET_XSTARTED;
	}

	if ((++pj->pj_nextwrite) == pj->pj_hdr->pjh_nents) {
		pj->pj_nextwrite = 0;

	} else
		psc_assert(pj->pj_nextwrite < pj->pj_hdr->pjh_nents);

	PJ_ULOCK(pj);

	rc = pjournal_logwrite_internal(pj, xh, slot, type, data, size);

	if (xh->pjx_flags & PJET_XEND) {
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
 * @data: a pointer to buffer when we fill journal entries.
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

		if (h->pje_magic != PJE_MAGIC) {
			psc_warnx("pj(%p) slot@%d failed magic", pj, slot + i);
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
	freelock(&xh->pjx_lock);

	return (pjournal_logwrite(xh, type, data, size));
}

int
pjournal_start_mark(struct psc_journal *pj, int slot)
{
	struct psc_journal_enthdr *pje;
	int rc;

	pje = psc_alloc(PJ_PJESZ(pj), PAF_PAGEALIGN | PAF_LOCK);

	pje->pje_magic = PJE_MAGIC;
	pje->pje_xid = PJE_XID_NONE;
	pje->pje_type = PJET_NODATA;

	rc = pwrite(pj->pj_fd, pje, pj->pj_hdr->pjh_entsz,
		    (off_t)(pj->pj_hdr->pjh_start_off +
			    (slot * pj->pj_hdr->pjh_entsz)));

	psc_freenl(pje, PJ_PJESZ(pj));
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

__static void 
pjournal_remove_entries(struct psc_journal *pj, uint64_t xid)
{
	int				 i;
	struct psc_journal_enthdr	*pje;
	int				 scan;

	scan = 1;
	while (scan) {
		scan = 0;
		for (i = 0; i < dynarray_len(&pj->pj_bufs); i++) {
			pje = dynarray_getpos(&pj->pj_bufs, i);
			if (pje->pje_xid == xid) {
				dynarray_remove(&pj->pj_bufs, pje);
				psc_freenl(pje, PJ_PJESZ(pj));
				scan = 1;
				break;
			}
		}
        }
}

__static int
pjournal_xid_cmp(const void *x, const void *y)
{
	struct psc_journal_enthdr	*a;
	struct psc_journal_enthdr	*b;

	a = (struct psc_journal_enthdr *) x;
	b = (struct psc_journal_enthdr *) y;

	if (a->pje_xid < b->pje_xid)
		return (-1);

	if (a->pje_xid > b->pje_xid)
                return (1);

	return (0);
}


/*
 * Accumulate all journal entries that need to be replayed in memory.  To reduce memory 
 * usage, we remove those entries of closed transactions as soon as we find them.
 */
__static int
pjournal_scan_slots(struct psc_journal *pj)
{
	int				 i;
	int				 rc;
	int				 ra;
	struct psc_journal_enthdr	*pje;
	uint32_t			 ents;
	unsigned char			*jbuf;

	rc = 0;
	ents = 0;

	dynarray_init(&pj->pj_bufs);

	jbuf = pjournal_alloclog_ra(pj);
	while (ents < pj->pj_hdr->pjh_nents) {
		ra = pjournal_logread(pj, ents, jbuf);
		if (ra < 0) {
			rc = -1;
			break;
		}
		for (i = 0; i < ra; i++) {
			pje = (struct psc_journal_enthdr *)&jbuf[pj->pj_hdr->pjh_entsz * i];
			if (pje->pje_type & PJET_FORMAT) {
				continue;
			}
			if (pje->pje_type & PJET_XCLOSED) {
				pjournal_remove_entries(pj, pje->pje_xid);
				continue;
			}
			pje = psc_alloc(PJ_PJESZ(pj), PAF_PAGEALIGN | PAF_LOCK);
			dynarray_add(&pj->pj_bufs, pje);
			memcpy(pje, &jbuf[pj->pj_hdr->pjh_entsz * i], sizeof(*pje));
		}
		ents += ra;
	}
	qsort(pj->pj_bufs.da_items, pj->pj_bufs.da_pos, sizeof(void *), pjournal_xid_cmp);
	psc_freenl(jbuf, PJ_PJESZ(pj));
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
	int rc = 0;
	uint32_t nents=0;

	psc_assert(pj && pj_handler);

	rc = pjournal_scan_slots(pj);
	if (rc < 0)
		goto out;

	if (pjw.pjw_stop >= pjw.pjw_pos)
		nents = pjw.pjw_stop - pjw.pjw_pos;
	else
		nents = (pj->pj_hdr->pjh_nents - pjw.pjw_pos) + pjw.pjw_stop + 1;

	while (nents) {
		rc = pjournal_logread(pj, pjw.pjw_pos, jbuf);
		if (rc < 0)
			goto out;

		(pj_handler)(jbuf, rc);

		nents -= rc;

		if ((pjw.pjw_pos += rc) >= pj->pj_hdr->pjh_nents)
			pjw.pjw_pos = 0;
	}

 out:
	psc_freenl(jbuf, PJ_PJESZ(pj));
	return (rc);
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
	int				 i;
	ssize_t				 rc;
	struct psc_journal		*pj;
	struct psc_journal_hdr		*pjh;
	struct psc_journal_enthdr	*pje;

	pj = PSCALLOC(sizeof(*pj));
	pjh = psc_alloc(sizeof(*pjh), PAF_PAGEALIGN | PAF_LOCK);

	pj->pj_fd = open(fn, O_RDWR | O_SYNC | O_DIRECT);
	if (pj->pj_fd == -1)
		psc_fatal("open %s", fn);

	rc = pread(pj->pj_fd, pjh, sizeof(*pjh), 0);
	if (rc != sizeof(*pjh))
		psc_fatal("read journal header: want %zu got %zd",
		    sizeof(*pjh), rc);

	pj->pj_hdr = pjh;
	if (pjh->pjh_magic != PJH_MAGIC)
		psc_fatalx("Journal header has bad magic!");

	LOCK_INIT(&pj->pj_lock);
	INIT_PSCLIST_HEAD(&pj->pj_pndgxids);
	psc_waitq_init(&pj->pj_waitq);

	dynarray_init(&pj->pj_bufs);
	dynarray_ensurelen(&pj->pj_bufs, MAX_NUM_PJBUF);
	for (i = 0; i < MAX_NUM_PJBUF; i++) {
		pje = psc_alloc(PJ_PJESZ(pj), PAF_PAGEALIGN | PAF_LOCK);
		dynarray_add(&pj->pj_bufs, pje);
	}

	return (pj);
}

/*
 * Initialize a new journal file.
 */
void
pjournal_format(const char *fn, uint32_t nents, uint32_t entsz, uint32_t ra,
		uint32_t opts)
{
	uint32_t			 i;
	struct psc_journal_enthdr	*h;
	int				 fd;
	struct psc_journal		 pj;
	struct psc_journal_hdr		 pjh;
	unsigned char			*jbuf;
	uint32_t			 slot;

	pjh.pjh_entsz = entsz;
	pjh.pjh_nents = nents;
	pjh.pjh_version = PJH_VERSION;
	pjh.pjh_options = opts;
	pjh.pjh_readahead = ra;
	pjh.pjh_unused = 0;
	pjh.pjh_start_off = PJE_OFFSET;
	pjh.pjh_magic = PJH_MAGIC;

	pj.pj_hdr = &pjh;
	jbuf = pjournal_alloclog_ra(&pj);

	if ((fd = open(fn, O_WRONLY|O_CREAT|O_TRUNC, 0600)) < 0)
		psc_fatal("Could not create or truncate the journal %s", fn);

	if (pwrite(fd, &pjh, sizeof(pjh), 0) < 0)
		psc_fatal("Failed to write header");

	psc_assert(PJE_OFFSET >= sizeof(pjh));

	for (slot=0, ra=pjh.pjh_readahead; slot < pjh.pjh_nents; slot += ra) {
		/* Make sure we don't write past the end. */
		while ((slot + ra) > pjh.pjh_nents)
			ra--;

		for (i = 0; i < ra; i++) {
			h = (void *)&jbuf[pjh.pjh_entsz * i];
			h->pje_magic = PJE_MAGIC;
			h->pje_type = PJET_FORMAT;
			h->pje_xid = PJE_XID_NONE;
			h->pje_sid = PJE_XID_NONE;
		}

		if (pwrite(fd, jbuf, (pjh.pjh_entsz * ra),
			   (off_t)(PJE_OFFSET + (slot * pjh.pjh_entsz))) < 0)
			psc_fatal("Failed to write entries");
	}
	if (close(fd) < 0)
		psc_fatal("Failed to close journal fd");

	psc_freenl(jbuf, PJ_PJESZ(&pj));
}

void
pjournal_close(struct psc_journal *pj)
{
	struct psc_journal_enthdr *pje;
	int n;

	DYNARRAY_FOREACH(pje, n, &pj->pj_bufs)
		psc_freenl(pje, PJ_PJESZ(pj));
	dynarray_free(&pj->pj_bufs);
	psc_freenl(pj->pj_hdr, sizeof(*pj->pj_hdr));
	PSCFREE(pj);
}

/*
 * Dump the contents of a journal file.
 */
int
pjournal_dump(const char *fn)
{
	uint32_t			 i;
	struct psc_journal_enthdr	*h;
	uint32_t			 ra;
	struct psc_journal		*pj;
	struct psc_journal_hdr		*pjh;
	uint32_t			 slot;
	unsigned char			*jbuf;

	pj = pjournal_load(fn);
	pjh = pj->pj_hdr;

	psc_info("entsz=%u nents=%u vers=%u opts=%u ra=%u "
		"off=%"PRIx64" magic=%"PRIx64,
		pjh->pjh_entsz, pjh->pjh_nents, pjh->pjh_version, pjh->pjh_options,
		pjh->pjh_readahead, pjh->pjh_start_off, pjh->pjh_magic);

	jbuf = pjournal_alloclog_ra(pj);

	for (slot = 0, ra=pjh->pjh_readahead; slot < pjh->pjh_nents; slot += ra) {
		/* Make sure we don't read past the end. */
		while ((slot + ra) > pjh->pjh_nents)
			ra--;

		if (pread(pj->pj_fd, jbuf, (pjh->pjh_entsz * ra),
			   (off_t)(PJE_OFFSET + (slot * pjh->pjh_entsz)))
		    != (pjh->pjh_entsz * ra))
			psc_fatal("Failed to read entries");

		for (i = 0; i < ra; i++) {
			h = (void *)&jbuf[pjh->pjh_entsz * i];

			psc_info("slot=%u magic=%"PRIx64
				" type=%x xid=%"PRIx64" sid=%d\n",
				(slot+i), h->pje_magic,
				h->pje_type, h->pje_xid, h->pje_sid);

			if (h->pje_magic != PJE_MAGIC)
				psc_warnx("journal entry %u has bad magic!",
					  (slot+i));
		}
	}

	if (close(pj->pj_fd) < 0)
		psc_fatal("Failed to close journal fd");

	psc_freenl(jbuf, PJ_PJESZ(pj));
	pjournal_close(pj);
	return (0);
}
