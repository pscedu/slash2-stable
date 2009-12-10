/* $Id$ */

#define PSC_SUBSYS PSS_JOURNAL
#include "psc_util/subsys.h"

#include <sys/types.h>

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <string.h>
#include <unistd.h>

#include "pfl/types.h"
#include "psc_ds/dynarray.h"
#include "psc_util/alloc.h"
#include "psc_util/atomic.h"
#include "psc_util/crc.h"
#include "psc_util/journal.h"
#include "psc_util/lock.h"

#define MAX_LOG_TRY		3	/* # of times of retry in case of a log write problem */

static int pjournal_logwrite(struct psc_journal_xidhndl *, int, void *, size_t);

/*
 * pjournal_xnew - start a new transaction with a unique ID in the given journal.
 * @pj: the owning journal.
 * Returns: new transaction handle
 */
struct psc_journal_xidhndl *
pjournal_xnew(struct psc_journal *pj)
{
	struct psc_journal_xidhndl *xh;

	xh = PSCALLOC(sizeof(*xh));

	xh->pjx_pj = pj;
	LOCK_INIT(&xh->pjx_lock);
	xh->pjx_flags = PJX_NONE;
	xh->pjx_sid = 0;
	xh->pjx_tailslot = PJX_SLOT_ANY;
	INIT_PSCLIST_ENTRY(&xh->pjx_lentry);

	/*
	 * Note that even though we issue xids in increasing order here, it does not
	 * necessarily mean transactions will end up in the log in the same order.
	 */
	PJ_LOCK(pj);
	do {
		xh->pjx_xid = ++pj->pj_lastxid;
	} while (xh->pjx_xid == PJE_XID_NONE);
	PJ_ULOCK(pj);

	psc_warnx("Start a new transaction %p (xid = %"PRIx64") in journal %p.", xh, xh->pjx_xid, pj);
	return (xh);
}

/*
 * This function is called to log changes to a piece of metadata (i.e., journal flush item). 
 * We can't reply to our clients until after the log entry is written.
 */
int
pjournal_xadd(struct psc_journal_xidhndl *xh, int type, void *data, size_t size)
{
	spinlock(&xh->pjx_lock);
	psc_assert(!(xh->pjx_flags & PJX_XCLOSE));
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
	psc_assert(!(xh->pjx_flags & PJX_XCLOSE));
	xh->pjx_flags |= PJX_XCLOSE;
	freelock(&xh->pjx_lock);

	return (pjournal_logwrite(xh, PJE_NONE, NULL, 0));
}

/*
 * pjournal_logwrite_internal - write a new log entry for a transaction
 * @xh: the transaction handle.
 * @slot: position location in journal to write.
 * @type: the application-specific log entry type.
 * @xid: transaction ID of entry.
 * @data: the journal entry contents to store.
 * Returns: 0 on success, -1 on error.
 */
static int
pjournal_logwrite_internal(struct psc_journal_xidhndl *xh, int32_t slot, int type, void *data, size_t size)
{
	int				 rc;
	ssize_t				 sz;
	struct psc_journal		*pj;
	struct psc_journal_enthdr	*pje;
	int				 ntries;
	uint64_t			 chksum;
	int				 wakeup;

	rc = 0;
	pj = xh->pjx_pj;
	psc_assert(slot >= 0);
	psc_assert(slot < pj->pj_hdr->pjh_nents);
	psc_assert(size + offsetof(struct psc_journal_enthdr, pje_data) <= (size_t)PJ_PJESZ(pj));

	PJ_LOCK(pj);
	while (!psc_dynarray_len(&pj->pj_bufs)) {
		pj->pj_flags |= PJ_WANTBUF;
		psc_waitq_wait(&pj->pj_waitq, &pj->pj_lock);
		PJ_LOCK(pj);
	}
	pje = psc_dynarray_getpos(&pj->pj_bufs, 0);
	psc_dynarray_remove(&pj->pj_bufs, pje);
	psc_assert(pje);
	PJ_ULOCK(pj);

	/* fill in contents for the log entry */
	pje->pje_magic = PJE_MAGIC;
	pje->pje_type = type;
	pje->pje_xid = xh->pjx_xid;
	pje->pje_len = size;

	spinlock(&xh->pjx_lock);
	pje->pje_sid = xh->pjx_sid++;
	freelock(&xh->pjx_lock);

	if (data) {
		psc_assert(size);
		memcpy(pje->pje_data, data, size);
	}
	/* calculating the CRC checksum, excluding the checksum field itself */
	PSC_CRC64_INIT(&chksum);
	psc_crc64_add(&chksum, pje, offsetof(struct psc_journal_enthdr, pje_chksum));
	psc_crc64_add(&chksum, pje->pje_data, pje->pje_len);
	PSC_CRC64_FIN(&chksum);
	pje->pje_chksum = chksum;
	
	/* commit the log entry on disk before we can return */
	ntries = MAX_LOG_TRY;
	while (ntries) {
		sz = pwrite(pj->pj_fd, pje, PJ_PJESZ(pj),
			   (off_t)(pj->pj_hdr->pjh_start_off + (slot * PJ_PJESZ(pj))));
		if (sz == -1 && errno == EAGAIN) {
			ntries--;
			usleep(100);
			continue;
		}
		break;
	}
	/* we may want to turn off logging at this point and force write-through instead */
	if (sz == -1 || sz != PJ_PJESZ(pj)) {
		rc = -1;
		psc_errorx("Problem writing journal log entries at slot %d", slot);
	}

	PJ_LOCK(pj);
	psc_dynarray_add(&pj->pj_bufs, pje);
	wakeup = 0;
	if (pj->pj_flags & PJ_WANTBUF) {
		wakeup = 1;
		pj->pj_flags &= ~PJ_WANTBUF;
	}
	if ((pj->pj_flags & PJ_WANTSLOT) &&
	    (xh->pjx_flags & PJX_XCLOSE) && (xh->pjx_tailslot == pj->pj_nextwrite)) {
		wakeup = 1;
		pj->pj_flags &= ~PJ_WANTSLOT;
		psc_warnx("Journal %p unblocking slot %d - owned by xid %"PRIx64, pj, slot, xh->pjx_xid);
	}
	if (wakeup) {
		psc_waitq_wakeall(&pj->pj_waitq);
	}
	PJ_ULOCK(pj);

	return (rc);
}

/*
 * pjournal_logwrite - store a new entry in a journal transaction.
 * @xh: the transaction to receive the log entry.
 * @type: the application-specific log entry type.
 * @data: the journal entry contents to store.
 * @size: size of the custom data
 * Returns: 0 on success, -1 on error.
 */
static int
pjournal_logwrite(struct psc_journal_xidhndl *xh, int type, void *data,
		  size_t size)
{
	struct psc_journal_xidhndl	*t;
	int				 rc;
	struct psc_journal		*pj;
	int32_t			 	 slot;
	int				 normal;
	int32_t			 	 tail_slot;

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
			pj->pj_flags |= PJ_WANTSLOT;
			psc_waitq_wait(&pj->pj_waitq, &pj->pj_lock);
			goto retry;
		}
		tail_slot = t->pjx_tailslot;
	}

	normal = 1;
	if (!(xh->pjx_flags & PJX_XSTART)) {
		normal = 0;
		type |= PJE_XSTART;
		xh->pjx_flags |= PJX_XSTART;
		psc_assert(size != 0);
		psc_assert(xh->pjx_tailslot == PJX_SLOT_ANY);
		xh->pjx_tailslot = slot;
		/* note we add transactions in the order of their starting point */
		psclist_xadd_tail(&xh->pjx_lentry, &pj->pj_pndgxids);
	}
	if (xh->pjx_flags & PJX_XCLOSE) {
		normal = 0;
		psc_assert(size == 0);
		psc_assert(xh->pjx_tailslot != slot);
		psc_assert(xh->pjx_tailslot != PJX_SLOT_ANY);
		type |= PJE_XCLOSE;
	}
	if (normal) {
		type |= PJE_XNORML;
		psc_assert(size != 0);
	}

	/* Update the next slot to be written by a new log entry */
	psc_assert(pj->pj_nextwrite < pj->pj_hdr->pjh_nents);
	if ((++pj->pj_nextwrite) == pj->pj_hdr->pjh_nents) {
		pj->pj_nextwrite = 0;
	}

	PJ_ULOCK(pj);

	psc_info("Writing a log entry for transaction %"PRIx64": transaction tail = %d, log tail = %d",
		  xh->pjx_xid, xh->pjx_tailslot, tail_slot);

	rc = pjournal_logwrite_internal(xh, slot, type, data, size);

	PJ_LOCK(pj);
	if (xh->pjx_flags & PJX_XCLOSE) {
		psc_dbg("Transaction %p (xid = %"PRIx64") removed from journal %p: tail slot = %d, rc = %d",
			 xh, xh->pjx_xid, pj, xh->pjx_tailslot, rc);
		psclist_del(&xh->pjx_lentry);
		PSCFREE(xh);
	}
	PJ_ULOCK(pj);
	return (rc);
}

/*
 * pjournal_logread - get a specified entry from a journal.
 * @pj: the journal.
 * @slot: the position in the journal of the entry to obtain.
 * @count: the number of slots to read.
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
	addr = pj->pj_hdr->pjh_start_off + slot * PJ_PJESZ(pj);
	size = pread(pj->pj_fd, data, PJ_PJESZ(pj) * count, addr);
	if (size < 0 || size != PJ_PJESZ(pj) *  count) {
		psc_warn("Fail to read %ld bytes from journal %p: rc = %d, errno = %d", size, pj, rc, errno);
		rc = -1;
	}
	return (rc);
}

static void *
pjournal_alloc_buf(struct psc_journal *pj)
{
	return (psc_alloc(PJ_PJESZ(pj) * pj->pj_hdr->pjh_readahead,
			  PAF_PAGEALIGN | PAF_LOCK));
}

/*
 * Remove a journal entry if it either has the given xid (mode = 1) or has a xid that is
 * less than the give xid (mode = 2).
 */
__static int
pjournal_remove_entries(struct psc_journal *pj, uint64_t xid, int mode)
{
	int				 i;
	struct psc_journal_enthdr	*pje;
	int				 scan;
	int				 count;

	scan = 1;
	count = 0;
	while (scan) {
		scan = 0;
		for (i = 0; i < psc_dynarray_len(&pj->pj_bufs); i++) {
			pje = psc_dynarray_getpos(&pj->pj_bufs, i);
			if (mode == 1 && pje->pje_xid == xid) {
				psc_dynarray_remove(&pj->pj_bufs, pje);
				psc_freenl(pje, PJ_PJESZ(pj));
				scan = 1;
				count++;
				break;
			}
			if (mode == 2 && pje->pje_xid < xid) {
				psc_dynarray_remove(&pj->pj_bufs, pje);
				psc_freenl(pje, PJ_PJESZ(pj));
				scan = 1;
				count++;
				break;
			}
		}
        }
	return count;
}

/*
 * Order transactions based on xid and sub id.
 */
__static int
pjournal_xid_cmp(const void *x, const void *y)
{
	const struct psc_journal_enthdr	*a = x, *b = y;
	int rc;

	rc = CMP(a->pje_xid, b->pje_xid);
	if (rc)
		return (rc);
	return (CMP(a->pje_sid, b->pje_sid));
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
	int32_t			 	 slot;
	unsigned char			*jbuf;
	int				 count;
	int				 nopen;
	int				 nscan;
	int				 nmagic;
	int				 nentry;
	int				 nclose;
	struct psc_journal_enthdr	*tmppje;
	uint64_t			 chksum;
	int				 nchksum;
	uint64_t			 last_xid;
	int32_t				 last_slot;
	struct psc_dynarray		 closetrans;
	uint64_t			 last_startup;

	rc = 0;
	slot = 0;
	nopen = 0;
	nscan = 0;
	nmagic = 0;
	nclose = 0;
	nchksum = 0;
	last_xid = PJE_XID_NONE;
	last_slot = PJX_SLOT_ANY;
	last_startup = PJE_XID_NONE;

	/*
	 * We scan the log from the first entry to the last one regardless where the log
	 * really starts.  This poses a problem: we might see the CLOSE entry of a transaction
	 * before its other entries due to log wraparound.  As a result, we must save these 
	 * CLOSE entries until we have seen all the entries of the transaction (some of them 
	 * might have already been overwritten, but that is perfectly fine).
	 */
	psc_dynarray_init(&closetrans);
	psc_dynarray_ensurelen(&closetrans, pj->pj_hdr->pjh_nents / 2);

	psc_dynarray_init(&pj->pj_bufs);
	jbuf = pjournal_alloc_buf(pj);
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
			nscan++;
			pje = (struct psc_journal_enthdr *)&jbuf[PJ_PJESZ(pj) * i];
			if (pje->pje_magic != PJE_MAGIC) {
				nmagic++;
				psc_warnx("Journal %p: slot %d has a bad magic number!", pj, slot+i);
				rc = -1;
				continue;
			}

			PSC_CRC64_INIT(&chksum);
			psc_crc64_add(&chksum, pje, offsetof(struct psc_journal_enthdr, pje_chksum));
			psc_crc64_add(&chksum, pje->pje_data, pje->pje_len);
			PSC_CRC64_FIN(&chksum);

			if (pje->pje_chksum != chksum) {
				psc_warnx("Journal %p: slot %d has a bad checksum!", pj, slot+i);
				nchksum++;
				rc = -1;
				continue;
			}
			psc_assert((pje->pje_type & PJE_XSTART) || (pje->pje_type & PJE_XCLOSE) ||
				   (pje->pje_type & PJE_STRTUP) || (pje->pje_type & PJE_FORMAT) || 
				   (pje->pje_type & PJE_XNORML));
			/*
			 * We start from the first log entry. If we see a formatted log entry,
			 * there should be no more real log entries after that.
			 */
			if (pje->pje_type & PJE_FORMAT) {
				psc_assert(pje->pje_len == 0);
				goto done;
			}
			if (pje->pje_xid >= last_xid) {
				last_xid = pje->pje_xid;
				last_slot = slot + i;
			}
			if (pje->pje_type & PJE_STRTUP) {
				psc_assert(pje->pje_len == 0);
				psc_info("Journal %p: found a startup entry at slot %d!", pj, slot+i);
				if (pje->pje_xid > last_startup) {
					last_startup = pje->pje_xid;
				}
				continue;
			}
			if (pje->pje_type & PJE_XCLOSE) {
				nclose++;
				psc_assert(pje->pje_len == 0);
				nentry = pjournal_remove_entries(pj, pje->pje_xid, 1);
				psc_assert(nentry <= (int) pje->pje_sid);
				if (nentry == (int) pje->pje_sid) {
					continue;
				}
			}
			/*
			 * Okay, we need to keep this log entry for now.
			 */
			tmppje = psc_alloc(PJ_PJESZ(pj), PAF_PAGEALIGN|PAF_LOCK);
			memcpy(tmppje, &jbuf[PJ_PJESZ(pj) * i], sizeof(*tmppje));
			if (pje->pje_type & PJE_XCLOSE) {
				psc_dynarray_add(&closetrans, tmppje);
			} else {
				psc_dynarray_add(&pj->pj_bufs, tmppje);
			}
		}
		slot += count;
	}
done:
	if (last_startup != PJE_XID_NONE) {
		pjournal_remove_entries(pj, last_startup, 2);
	}
	pj->pj_lastxid = last_xid;
	/* If last_slot is PJX_SLOT_ANY, then nextwrite will be 0 */
	pj->pj_nextwrite = (last_slot == (int)pj->pj_hdr->pjh_nents - 1) ? 0 : (last_slot + 1);
	qsort(pj->pj_bufs.da_items, pj->pj_bufs.da_pos, sizeof(void *), pjournal_xid_cmp);
	psc_freenl(jbuf, PJ_PJESZ(pj) * pj->pj_hdr->pjh_readahead);

	/*
	 * We need this code because we don't start from the beginning of the log.
	 * On the other hand, I don't expect either array will be long.
	 */
	while (psc_dynarray_len(&closetrans)) {
		pje = psc_dynarray_getpos(&closetrans, 0);
		(void)pjournal_remove_entries(pj, pje->pje_xid, 1);
		psc_dynarray_remove(&closetrans, pje);
		psc_freenl(pje, PJ_PJESZ(pj));
	}
	psc_dynarray_free(&closetrans);

	nopen = psc_dynarray_len(&pj->pj_bufs);
	psc_warnx("Journal statistics: %d close, %d open, %d magic, %d chksum, %d scan, %d total", 
		   nclose, nopen, nmagic, nchksum, nscan, pj->pj_hdr->pjh_nents);
	return (rc);
}


/*
 * pjournal_load - initialize the in-memory representation of a journal.
 * return: pj on success, NULL on failure
 */
static struct psc_journal *
pjournal_load(const char *fn)
{
	ssize_t				 rc;
	struct psc_journal		*pj;
	struct psc_journal_hdr		*pjh;
	uint64_t			 chksum;
	struct stat			 statbuf;

	pj = PSCALLOC(sizeof(struct psc_journal));
	pjh = psc_alloc(sizeof(struct psc_journal_hdr), PAF_PAGEALIGN | PAF_LOCK);
	/*
	 * To quote open(2), the O_DIRECT flag may impose alignment restrictions on the length 
	 * and address of userspace buffers and the file offset of I/Os. Note that we are using
	 * 512 byte log entries.
	 */
	pj->pj_fd = open(fn, O_RDWR | O_SYNC | O_DIRECT);
	if (pj->pj_fd == -1) {
		psc_fatal("Fail to open log file %s", fn);
	}
	rc = fstat(pj->pj_fd, &statbuf);
	if (rc < 0) {
		psc_fatal("Fail to stat log file %s", fn);
	}
	if (statbuf.st_size < (off_t)sizeof(*pjh)) {
		psc_fatal("Log file size is smaller than a log header");
	}
	rc = pread(pj->pj_fd, pjh, sizeof(*pjh), 0);
	if (rc != sizeof(*pjh)) {
		psc_fatal("Fail to read journal header: want %zu got %zd",
		    sizeof(*pjh), rc);
	}
	pj->pj_hdr = pjh;
	if (pjh->pjh_magic != PJH_MAGIC) {
		PSCFREE(pj);
		psc_freenl(pjh, sizeof(struct psc_journal_hdr));
		pj = NULL;
		psc_errorx("Journal header has a bad magic number!");
		goto done; 
	}
	if (pjh->pjh_version != PJH_VERSION) {
		psc_errorx("Journal header has an invalid version number!");
		psc_freenl(pjh, sizeof(struct psc_journal_hdr));
		PSCFREE(pj);
		pj = NULL;
		goto done; 
	}

	PSC_CRC64_INIT(&chksum);
	psc_crc64_add(&chksum, pjh, offsetof(struct _psc_journal_hdr, _pjh_chksum));
	PSC_CRC64_FIN(&chksum);

	if (pjh->pjh_chksum != chksum) {
		psc_errorx("Journal header has an invalid checksum value "
		    "%"PSCPRIxCRC64" vs %"PSCPRIxCRC64, pjh->pjh_chksum, chksum);
		psc_freenl(pjh, sizeof(struct psc_journal_hdr));
		PSCFREE(pj);
		pj = NULL;
		goto done; 
	}
	if (statbuf.st_size != (off_t)(sizeof(*pjh) + pjh->pjh_nents * PJ_PJESZ(pj))) {
		psc_errorx("Size of the log file does not match specs in its header");
		psc_freenl(pjh, sizeof(struct psc_journal_hdr));
		PSCFREE(pj);
		pj = NULL;
		goto done; 
	}
	/*
	 * The remaining two fields pj_lastxid and pj_nextwrite will be filled after log replay.
 	 */
	LOCK_INIT(&pj->pj_lock);
	INIT_PSCLIST_HEAD(&pj->pj_pndgxids);
	psc_waitq_init(&pj->pj_waitq);
	pj->pj_flags = PJ_NONE;
	psc_dynarray_init(&pj->pj_bufs);
	pj->pj_logname = strdup(fn);
	if (pj->pj_logname == NULL)
		psc_fatal("strdup");

done:
	return (pj);
}

static void
pjournal_close(struct psc_journal *pj)
{
	int				 n;
	struct psc_journal_enthdr	*pje;

	DYNARRAY_FOREACH(pje, n, &pj->pj_bufs)
		psc_freenl(pje, PJ_PJESZ(pj));
	psc_dynarray_free(&pj->pj_bufs);
	psc_freenl(pj->pj_hdr, sizeof(*pj->pj_hdr));
	free(pj->pj_logname);
	PSCFREE(pj);
}

/*
 * Initialize a new journal file.
 */
int
pjournal_format(const char *fn, uint32_t nents, uint32_t entsz, uint32_t ra,
		uint32_t opts)
{
	int32_t			 	 i;
	int				 rc;
	int				 fd;
	struct psc_journal		 pj;
	struct psc_journal_enthdr	*pje;
	struct psc_journal_hdr		 pjh;
	ssize_t				 size;
	unsigned char			*jbuf;
	int32_t			 	 slot;
	int				 count;
	uint64_t			 chksum;

	pjh.pjh_entsz = entsz;
	pjh.pjh_nents = nents;
	pjh.pjh_version = PJH_VERSION;
	pjh.pjh_options = opts;
	pjh.pjh_readahead = ra;
	pjh.pjh_unused = 0;
	pjh.pjh_start_off = PJE_OFFSET;
	pjh.pjh_magic = PJH_MAGIC;
	
	PSC_CRC64_INIT(&chksum);
	psc_crc64_add(&chksum, &pjh, offsetof(struct _psc_journal_hdr, _pjh_chksum));
	PSC_CRC64_FIN(&chksum);
	pjh.pjh_chksum = chksum;

	pj.pj_hdr = &pjh;
	jbuf = pjournal_alloc_buf(&pj);

	rc = 0;
	errno = 0;
	if ((fd = open(fn, O_WRONLY|O_CREAT|O_TRUNC, 0600)) < 0)
		psc_fatal("Could not create or truncate the log file %s", fn);

	psc_assert(PJE_OFFSET >= sizeof(pjh));
	size = pwrite(fd, &pjh, sizeof(pjh), 0);
	if (size < 0 || size != sizeof(pjh))
		psc_fatal("Failed to write header");

	for (slot = 0;  slot < pjh.pjh_nents; slot += count) {

		count = (nents - slot <= ra) ? (nents - slot) : ra;
		for (i = 0; i < count; i++) {

			pje = (struct psc_journal_enthdr *)&jbuf[PJ_PJESZ(&pj) * i];
			pje->pje_magic = PJE_MAGIC;
			pje->pje_type = PJE_FORMAT;
			pje->pje_xid = PJE_XID_NONE;
			pje->pje_sid = PJE_XID_NONE;
			pje->pje_len = 0;

			PSC_CRC64_INIT(&chksum);
			psc_crc64_add(&chksum, pje, offsetof(struct psc_journal_enthdr, pje_chksum));
			psc_crc64_add(&chksum, pje->pje_data, pje->pje_len);
			PSC_CRC64_FIN(&chksum);

			pje->pje_chksum = chksum;
		}
		size = pwrite(fd, jbuf, PJ_PJESZ(&pj) * count, 
			(off_t)(PJE_OFFSET + (slot * PJ_PJESZ(&pj))));
		/* At least on one instance, short write actually returns success on a RAM-backed file system */
		if (size < 0 || size != PJ_PJESZ(&pj) * count) {
			rc = -1;
			psc_errorx("Failed to write %d entries at slot %d", count, slot);
			break;
		}
	}
	if (close(fd) < 0) {
		psc_fatal("Failed to close journal fd");
	}
	psc_freenl(jbuf, PJ_PJESZ(&pj) * ra);
	return rc;
}

/*
 * Dump the contents of a journal file.
 */
int
pjournal_dump(const char *fn)
{
	int				 i;
	int32_t			 	 ra;
	struct psc_journal		*pj;
	struct psc_journal_hdr		*pjh;
	struct psc_journal_enthdr	*pje;
	int32_t			 	 slot;
	unsigned char			*jbuf;
	ssize_t				 size;
	int				 count;
	uint64_t			 chksum;
	int				 ntotal;
	int				 nmagic;
	int				 nchksum;
	int				 nformat;

	ntotal = 0;
	nmagic = 0;
	nchksum = 0;
	nformat = 0;

	pj = pjournal_load(fn);
	pjh = pj->pj_hdr;

	psc_info("Journal header info: "
		 "entsz=%u nents=%u vers=%u opts=%u ra=%u off=%"PRIx64" magic=%"PRIx64,
		 PJ_PJESZ(pj), pjh->pjh_nents, pjh->pjh_version, pjh->pjh_options,
		 pjh->pjh_readahead, pjh->pjh_start_off, pjh->pjh_magic);

	jbuf = pjournal_alloc_buf(pj);

	for (slot = 0, ra=pjh->pjh_readahead; slot < pjh->pjh_nents; slot += count) {

		count = (pjh->pjh_nents - slot <= ra) ? (pjh->pjh_nents - slot) : ra;
		size = pread(pj->pj_fd, jbuf, (PJ_PJESZ(pj) * count), 
			    (off_t)(PJE_OFFSET + (slot * PJ_PJESZ(pj))));

		if (size == -1 || size != (PJ_PJESZ(pj)* count))
			psc_fatal("Failed to read %d log entries at slot %d", count, slot);

		for (i = 0; i < count; i++) {
			ntotal++;
			pje = (void *)&jbuf[PJ_PJESZ(pj) * i];
			if (pje->pje_magic != PJE_MAGIC) {
				nmagic++;
				psc_warnx("Journal slot %d has a bad magic number!", (slot+i));
				continue;
			}
			if (pje->pje_magic == PJE_FORMAT) {
				nformat++;
				continue;
			}
			PSC_CRC64_INIT(&chksum);
			psc_crc64_add(&chksum, pje, offsetof(struct psc_journal_enthdr, pje_chksum));
			psc_crc64_add(&chksum, pje->pje_data, pje->pje_len);
			PSC_CRC64_FIN(&chksum);
			if (pje->pje_chksum != chksum) {
				nchksum++;
				psc_warnx("Journal slot %d has a bad checksum!", (slot+i));
				continue;
			}
			psc_info("Journal: slot=%u magic=%"PRIx64" type=%x xid=%"PRIx64" sid=%d\n",
				  (slot+i), pje->pje_magic,
				  pje->pje_type, pje->pje_xid, pje->pje_sid);
		}

	}
	if (close(pj->pj_fd) < 0)
		psc_fatal("Failed to close journal fd");

	psc_freenl(jbuf, PJ_PJESZ(pj));
	pjournal_close(pj);

	psc_info("Journal statistics: %d total, %d format, %d bad magic, %d bad checksum",
		 ntotal, nformat, nmagic, nchksum);
	return (0);
}

/*
 * pjournal_replay - traverse each open transaction in a journal and replay them.
 * @pj: the journal.
 * @pj_handler: the master journal replay function.
 * Returns: 0 on success, -1 on error.
 */
struct psc_journal *
pjournal_replay(const char * fn, psc_jhandler pj_handler)
{
	int				 i;
	int				 rc;
	struct psc_journal		*pj;
	uint64_t			 xid;
	struct psc_journal_enthdr	*pje;
	ssize_t				 size;
	int				 nents;
	int				 nerrs;
	uint64_t			 chksum;
	int				 ntrans;
	struct psc_journal_enthdr	*tmppje;
	struct psc_dynarray		 replaybufs;

	pj = pjournal_load(fn);
	if (pj == NULL)
		return NULL;

	nents = 0;
	nerrs = 0;
	ntrans = 0;
	rc = pjournal_scan_slots(pj);
	if (rc) {
		rc = 0;
		nerrs++;
	}
	while (psc_dynarray_len(&pj->pj_bufs)) {

		pje = psc_dynarray_getpos(&pj->pj_bufs, 0);
		xid = pje->pje_xid;

		psc_dynarray_init(&replaybufs);
		psc_dynarray_ensurelen(&replaybufs, 1024);

		for (i = 0; i < psc_dynarray_len(&pj->pj_bufs); i++) {
			tmppje = psc_dynarray_getpos(&pj->pj_bufs, i);
			psc_assert(tmppje->pje_len != 0);
			if (tmppje->pje_xid == xid) {
				nents++;
				psc_dynarray_add(&replaybufs, tmppje);
			}
		}

		ntrans++;
		(pj_handler)(&replaybufs, &rc);
		if (rc) {
			nerrs++;
			rc = 0;
		}

		pjournal_remove_entries(pj, xid, 1);
		psc_dynarray_free(&replaybufs);
	}
	psc_assert(!psc_dynarray_len(&pj->pj_bufs));
	psc_dynarray_free(&pj->pj_bufs);

	/* write a startup marker after replaying all the log entries */
	pje = psc_alloc(PJ_PJESZ(pj), PAF_PAGEALIGN | PAF_LOCK);

	pje->pje_magic = PJE_MAGIC;
	pje->pje_type = PJE_STRTUP;
	pj->pj_lastxid++;
	if (pj->pj_lastxid == PJE_XID_NONE)
		pj->pj_lastxid++;
	pje->pje_xid = pj->pj_lastxid;
	pje->pje_sid = PJE_XID_NONE;
	pje->pje_len = 0;	

	PSC_CRC64_INIT(&chksum);
	psc_crc64_add(&chksum, pje, offsetof(struct psc_journal_enthdr, pje_chksum));
	psc_crc64_add(&chksum, pje->pje_data, pje->pje_len);
	PSC_CRC64_FIN(&chksum);
	pje->pje_chksum = chksum;
	
	size = pwrite(pj->pj_fd, pje, PJ_PJESZ(pj),
		     (off_t)(pj->pj_hdr->pjh_start_off + 
		     (pj->pj_nextwrite * PJ_PJESZ(pj))));
	if (size < 0 || size != PJ_PJESZ(pj)) {
		psc_warnx("Fail to write a start up marker in the journal");
	}
	psc_freenl(pje, PJ_PJESZ(pj));

	pj->pj_nextwrite++;
	if (pj->pj_nextwrite == pj->pj_hdr->pjh_nents)
		pj->pj_nextwrite = 0;

	/* pre-allocate some buffers for log writes */
	for (i = 0; i < MAX_NUM_PJBUF; i++) {
		pje = psc_alloc(PJ_PJESZ(pj), PAF_PAGEALIGN|PAF_LOCK);
		psc_dynarray_add(&pj->pj_bufs, pje);
	}

	psc_warnx("Journal replay: %d log entries and %d transactions have been redone, error = %d", nents, ntrans, nerrs);
	return pj;
}
