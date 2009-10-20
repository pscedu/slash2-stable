/* $Id$ */

#define PSC_SUBSYS PSS_JOURNAL
#include "psc_util/subsys.h"

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

#define MAX_LOG_TRY		3			/* # of times of retry in case of a log write problem */

static int pjournal_logwrite(struct psc_journal_xidhndl *, int, void *, size_t);

/*
 * pjournal_xnew - obtain an unused journal transaction ID.
 * @pj: the owning journal.
 * Returns: new, unused transaction ID.
 */
struct psc_journal_xidhndl *
pjournal_xnew(struct psc_journal *pj)
{
	struct psc_journal_xidhndl *xh;

	xh = PSCALLOC(sizeof(*xh));

	xh->pjx_pj = pj;
	LOCK_INIT(&xh->pjx_lock);
	xh->pjx_tailslot = PJX_SLOT_ANY;
	INIT_PSCLIST_ENTRY(&xh->pjx_lentry);

	PJ_LOCK(pj);
	do {
		xh->pjx_xid = ++pj->pj_nextxid;
	} while (xh->pjx_xid == PJE_XID_NONE);
	PJ_ULOCK(pj);

	psc_warnx("Start a new transaction %p (xid = %"PRIx64") for journal %p.", xh, xh->pjx_xid, xh->pjx_pj);
	return (xh);
}

/*
 * This function is called to log changes to a piece of metadata.  We can't
 * reply to our clients until after the log entry is written.
 */
int
pjournal_xadd(struct psc_journal_xidhndl *xh, int type, void *data, size_t size)
{
	spinlock(&xh->pjx_lock);
	psc_assert(!(xh->pjx_flags & PJX_XCLOSED));
	freelock(&xh->pjx_lock);

	return (pjournal_logwrite(xh, type, data, size));
}

/*
 * This function is called after a piece of metadata has been updated in place
 * so that we can close the transaction that logs its changes.
 */
int
pjournal_xend(struct psc_journal_xidhndl *xh)
{
	spinlock(&xh->pjx_lock);
	psc_assert(!(xh->pjx_flags & PJX_XCLOSED));
	xh->pjx_flags |= PJX_XCLOSED;
	freelock(&xh->pjx_lock);

	return (pjournal_logwrite(xh, PJE_NONE, NULL, 0));
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
static int
pjournal_logwrite_internal(struct psc_journal *pj, struct psc_journal_xidhndl *xh,
			    uint32_t slot, int type, void *data, size_t size)
{
	int				 rc;
	int				 len;
	struct psc_journal_enthdr	*pje;
	int				 ntries;

	rc = 0;
	ntries = MAX_LOG_TRY;
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

	if (!(type & PJE_XCLOSED))
		pje->pje_sid = atomic_inc_return(&xh->pjx_sid);
	else
		pje->pje_sid = atomic_read(&xh->pjx_sid);


#ifdef NOT_READY

	/* commit the log on disk before we can return */
	while (ntries) {
		rc = pwrite(pj->pj_fd, pje, pj->pj_hdr->pjh_entsz, 
			   (off_t)(pj->pj_hdr->pjh_start_off + (slot * pj->pj_hdr->pjh_entsz)));
		if (rc == -1 && errno == EAGAIN) {
			ntries--;
			usleep(100);
			continue;
		}
	}
	/* we may want to turn off logging at this point and force write-through instead */
	if (rc == -1 || rc != pj->pj_hdr->pjh_entsz) {
		rc = -1;
		psc_errorx("Problem writing journal log entries");
	} else 
		rc = 0;
#endif

	PJ_LOCK(pj);
	dynarray_add(&pj->pj_bufs, pje);
	psc_waitq_wakeall(&pj->pj_waitq);
	PJ_ULOCK(pj);

	if ((xh->pjx_flags & PJX_XCLOSED) && (xh->pjx_tailslot == pj->pj_nextwrite)) {
		/* We are the tail so unblock the journal.  */
		psc_warnx("pj (%p) unblocking slot %d - owned by xid %"PRIx64, pj, slot, xh->pjx_xid);
		psc_waitq_wakeall(&pj->pj_waitq);
	}
	return (rc);
}

/*
 * _pjournal_logwrite - store a new entry in a journal transaction.
 * @pj: the journal.
 * @type: the application-specific log entry type.
 * @xid: transaction ID.
 * @data: the journal entry contents to store.
 * Returns: 0 on success, -1 on error.
 */
static int
pjournal_logwrite(struct psc_journal_xidhndl *xh, int type, void *data,
		  size_t size)
{
	struct psc_journal_xidhndl	*t;
	int				 rc;
	struct psc_journal		*pj;
	uint32_t			 slot;
	uint32_t			 tail_slot;

	pj = xh->pjx_pj;

 retry:
	/*
	 * Make sure that the next slot to be written does not have a pending transaction.
	 * Since we add a new transaction at the tail of the pending transaction list, we
	 * only need to check the head of the list to find out the oldest pending transaction.
	 */
	PJ_LOCK(pj);
	slot = pj->pj_nextwrite;
	tail_slot = PJX_SLOT_ANY;
	t = psclist_first_entry(&pj->pj_pndgxids, struct psc_journal_xidhndl, pjx_lentry);
	if (t) {
		if (t->pjx_tailslot == slot) {
			psc_warnx("Journal %p write is blocked on slot %d "
				  "owned by transaction %p (xid = %"PRIx64")", 
				   pj, pj->pj_nextwrite, t, t->pjx_xid);
			psc_waitq_wait(&pj->pj_waitq, &pj->pj_lock);
			goto retry;
		}
		tail_slot = t->pjx_tailslot;
	} else {
		type |= PJE_STARTUP;
	}

	if (!(xh->pjx_flags & PJX_XSTARTED)) {
		type |= PJE_XCLOSED;
		xh->pjx_tailslot = slot;
		psclist_xadd_tail(&xh->pjx_lentry, &pj->pj_pndgxids);
		xh->pjx_flags |= PJX_XSTARTED;
	}

	if ((++pj->pj_nextwrite) == pj->pj_hdr->pjh_nents) {
		pj->pj_nextwrite = 0;
	} else
		psc_assert(pj->pj_nextwrite < pj->pj_hdr->pjh_nents);

	PJ_ULOCK(pj);

	psc_info("Writing transaction %p into journal %p: transaction tail = %d, log tail = %d",
		  xh, pj, xh->pjx_tailslot, tail_slot);

	rc = pjournal_logwrite_internal(pj, xh, slot, type, data, size);

	if (xh->pjx_flags & PJX_XCLOSED) {
		psc_dbg("Transaction %p (xid = %"PRIx64") removed from journal %p: tail slot = %d, rc = %d",
			 xh, xh->pjx_xid, pj, xh->pjx_tailslot, rc);
		psclist_del(&xh->pjx_lentry);
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
static int
pjournal_logread(struct psc_journal *pj, int32_t slot, int32_t count, void *data)
{
	int		rc;
	off_t		addr;
	ssize_t		size;

	rc = 0;
	addr = pj->pj_hdr->pjh_start_off + slot * pj->pj_hdr->pjh_entsz;
	size = pread(pj->pj_fd, data, pj->pj_hdr->pjh_entsz * count, addr);
	if (size < 0 || size != pj->pj_hdr->pjh_entsz * count) {
		psc_warn("Fail to read %ld bytes from journal %p: rc = %d, errno = %d", size, pj, rc, errno);
		rc = -1;
	}
	return (rc);
}

int
pjournal_start_mark(struct psc_journal *pj, int slot)
{
	struct psc_journal_enthdr *pje;
	int rc;

	pje = psc_alloc(PJ_PJESZ(pj), PAF_PAGEALIGN | PAF_LOCK);

	pje->pje_magic = PJE_MAGIC;
	pje->pje_xid = PJE_XID_NONE;
	pje->pje_type = PJE_STARTUP;

	rc = pwrite(pj->pj_fd, pje, pj->pj_hdr->pjh_entsz,
		    (off_t)(pj->pj_hdr->pjh_start_off +
			    (slot * pj->pj_hdr->pjh_entsz)));

	psc_freenl(pje, PJ_PJESZ(pj));
	return (rc);
}

void *
pjournal_alloclog_ra(struct psc_journal *pj)
{
	psc_trace("rasz=%zd", PJ_PJESZ(pj) * pj->pj_hdr->pjh_readahead);
	return (psc_alloc(PJ_PJESZ(pj) * pj->pj_hdr->pjh_readahead,
			  PAF_PAGEALIGN | PAF_LOCK));
}

/*
 * Remove all journal entries with the given xid from the journal.
 */
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
	const struct psc_journal_enthdr	*a = x, *b = y;

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
	struct psc_journal_enthdr	*pje;
	uint32_t			 slot;
	unsigned char			*jbuf;
	int				 count;
	int				 nopen;
	int				 nclose;
	int				 nformat;
	uint64_t			 last_xid;
	int				 last_slot;

	rc = 0;
	slot = 0;
	nopen = 0;
	nclose = 0;
	nformat = 0;
	last_xid = PJE_XID_NONE;
	last_slot = PJX_SLOT_ANY;

	dynarray_init(&pj->pj_bufs);
	jbuf = pjournal_alloclog_ra(pj);
	while (slot < pj->pj_hdr->pjh_nents) {
		if (pj->pj_hdr->pjh_nents - slot >= pj->pj_hdr->pjh_readahead) {
			count = pj->pj_hdr->pjh_readahead;
		} else {
			count = pj->pj_hdr->pjh_nents - slot;
		}
		if (pjournal_logread(pj, slot, count, jbuf) < 0) {
			rc = -1;
			break;
		}
		for (i = 0; i < count; i++) {
			pje = (struct psc_journal_enthdr *)&jbuf[pj->pj_hdr->pjh_entsz * i];
			if (pje->pje_xid >= last_xid) {
				last_xid = pje->pje_xid;
				last_slot = slot + i;
			}
			if (pje->pje_type & PJE_FORMAT) {
				nformat++;
				continue;
			}
			if (pje->pje_type & PJE_XCLOSED) {
				pjournal_remove_entries(pj, pje->pje_xid);
				nclose++;
				continue;
			}
			nopen++;
			pje = psc_alloc(PJ_PJESZ(pj), PAF_PAGEALIGN | PAF_LOCK);
			dynarray_add(&pj->pj_bufs, pje);
			memcpy(pje, &jbuf[pj->pj_hdr->pjh_entsz * i], sizeof(*pje));
		}
		slot += count;
	}
	pj->pj_nextwrite = last_slot;
	qsort(pj->pj_bufs.da_items, pj->pj_bufs.da_pos, sizeof(void *), pjournal_xid_cmp);
	psc_freenl(jbuf, PJ_PJESZ(pj));
	psc_warnx("Journal statistics: %d format, %d close, %d open, %d scan, %d total", 
		   nformat, nclose, nopen, slot, pj->pj_hdr->pjh_nents);
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
	int count = 0;
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
		rc = pjournal_logread(pj, pjw.pjw_pos, count, jbuf);
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
			h->pje_type = PJE_FORMAT;
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
