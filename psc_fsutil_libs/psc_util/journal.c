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
#include "psc_util/crc.h"
#include "psc_util/alloc.h"
#include "psc_util/atomic.h"
#include "psc_util/journal.h"
#include "psc_util/lock.h"
#include "psc_ds/dynarray.h"

#define MAX_LOG_TRY		3	/* # of times of retry in case of a log write problem */

static int pjournal_logwrite(struct psc_journal_xidhndl *, int, void *, size_t);

/*
 * pjournal_xnew - obtain an unused journal transaction ID.
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
	atomic_set(&xh->pjx_sid, 0);
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
 * pjournal_logwrite_internal - store a new entry in a journal.
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
	ssize_t				 sz;
	struct psc_journal_enthdr	*pje;
	int				 ntries;
	uint64_t			 chksum;
	int				 wakeup;

	rc = 0;
	ntries = MAX_LOG_TRY;
	psc_assert(slot < pj->pj_hdr->pjh_nents);
	psc_assert(size + sizeof(*pje) <= PJ_PJESZ(pj));

	PJ_LOCK(pj);
	while (!dynarray_len(&pj->pj_bufs)) {
		pj->pj_flags |= PJ_WANTBUF;
		psc_waitq_wait(&pj->pj_waitq, &pj->pj_lock);
		PJ_LOCK(pj);
	}
	pje = dynarray_getpos(&pj->pj_bufs, 0);
	psc_assert(pje);
	dynarray_remove(&pj->pj_bufs, pje);
	PJ_ULOCK(pj);

	/* fill in contents for the log entry */
	pje->pje_magic = PJE_MAGIC;
	pje->pje_type = type;
	pje->pje_xid = xh->pjx_xid;
	pje->pje_len = size;
	if (!(type & PJE_XCLOSED)) {
		pje->pje_sid = atomic_inc_return(&xh->pjx_sid);
	} else {
		pje->pje_sid = atomic_read(&xh->pjx_sid);
	}
	if (data) {
		psc_assert(size);
		memcpy(pje->pje_data, data, size);
	}

	PSC_CRC_INIT(chksum);
	psc_crc_add(&chksum, pje, offsetof(struct psc_journal_enthdr, pje_chksum));
	psc_crc_add(&chksum, pje->pje_data, pje->pje_len);
	PSC_CRC_FIN(chksum);
	pje->pje_chksum = chksum;
	
#ifdef NOT_READY 

	/* commit the log on disk before we can return */
	while (ntries) {
		sz = pwrite(pj->pj_fd, pje, pj->pj_hdr->pjh_entsz, 
			   (off_t)(pj->pj_hdr->pjh_start_off + (slot * pj->pj_hdr->pjh_entsz)));
		if (sz == -1 && errno == EAGAIN) {
			ntries--;
			usleep(100);
			continue;
		}
	}
	/* we may want to turn off logging at this point and force write-through instead */
	if (sz == -1 || sz != pj->pj_hdr->pjh_entsz) {
		rc = -1;
		psc_errorx("Problem writing journal log entries at slot %d", slot);
	} else 
		rc = 0;
#endif

	PJ_LOCK(pj);
	dynarray_add(&pj->pj_bufs, pje);
	wakeup = 0;
	if (pj->pj_flags & PJ_WANTBUF) {
		wakeup = 1;
		pj->pj_flags &= ~PJ_WANTBUF;
	}
	if ((pj->pj_flags & PJ_WANTSLOT) &&
	    (xh->pjx_flags & PJX_XCLOSED) && (xh->pjx_tailslot == pj->pj_nextwrite)) {
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
			pj->pj_flags |= PJ_WANTSLOT;
			psc_waitq_wait(&pj->pj_waitq, &pj->pj_lock);
			goto retry;
		}
		tail_slot = t->pjx_tailslot;
	} else {
		type |= PJE_STARTUP;
	}

	if (!(xh->pjx_flags & PJX_XSTARTED)) {
		type |= PJE_XSTARTED;
		xh->pjx_tailslot = slot;
		psclist_xadd_tail(&xh->pjx_lentry, &pj->pj_pndgxids);
		xh->pjx_flags |= PJX_XSTARTED;
	}
	if (xh->pjx_flags & PJX_XCLOSED) {
		psc_assert(xh->pjx_tailslot != slot);
		type |= PJE_XCLOSED;
	}

	if ((++pj->pj_nextwrite) == pj->pj_hdr->pjh_nents) {
		pj->pj_nextwrite = 0;
	} else
		psc_assert(pj->pj_nextwrite < pj->pj_hdr->pjh_nents);

	PJ_ULOCK(pj);

	psc_info("Writing transaction %p into journal %p: transaction tail = %d, log tail = %d",
		  xh, pj, xh->pjx_tailslot, tail_slot);

	rc = pjournal_logwrite_internal(pj, xh, slot, type, data, size);

	PJ_LOCK(pj);
	if (xh->pjx_flags & PJX_XCLOSED) {
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

static void *
pjournal_alloclog_ra(struct psc_journal *pj)
{
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
	int				 nmagic;
	int				 nclose;
	uint64_t			 chksum;
	int				 nformat;
	int				 nchksum;
	uint64_t			 last_xid;
	int				 last_slot;

	rc = 0;
	slot = 0;
	nopen = 0;
	nmagic = 0;
	nclose = 0;
	nchksum = 0;
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
			if (pje->pje_magic != PJE_MAGIC) {
				nmagic++;
				psc_warnx("journal slot %d has a bad magic number !", slot+i);
				continue;
			}

			PSC_CRC_INIT(chksum);
			psc_crc_add(&chksum, pje, offsetof(struct psc_journal_enthdr, pje_chksum));
			psc_crc_add(&chksum, pje->pje_data, pje->pje_len);
			PSC_CRC_FIN(chksum);

			if (pje->pje_chksum != chksum) {
				psc_warnx("Journal %p: found an invalid log entry at slot %d", pj, slot+i);
				nchksum++;
				rc = -1;
			}
			if (pje->pje_type & PJE_FORMAT) {
				nformat++;
				continue;
			}
			if (pje->pje_xid >= last_xid) {
				last_xid = pje->pje_xid;
				last_slot = slot + i;
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
	psc_warnx("Journal statistics: %d format, %d close, %d open, %d bad magic, %d bad checksum, %d scan, %d total", 
		   nformat, nclose, nopen, nmagic, nchksum, slot, pj->pj_hdr->pjh_nents);
	return (rc);
}

/*
 * pjournal_replay - traverse each open transaction in a journal and reply them.
 * @pj: the journal.
 * @pj_handler: the master journal replay function.
 * Returns: 0 on success, -1 on error.
 */
int
pjournal_replay(struct psc_journal *pj, psc_jhandler pj_handler)
{
	int				 i;
	int				 rc;
	uint64_t			 xid;
	struct psc_journal_enthdr	*pje;
	int				 nents;
	int				 ntrans;
	struct psc_journal_enthdr	*tmppje;
	struct dynarray			 replaybufs;

	psc_assert(pj && pj_handler);

	ntrans = 0;
	rc = pjournal_scan_slots(pj);
	while (dynarray_len(&pj->pj_bufs)) {

		pje = dynarray_getpos(&pj->pj_bufs, 0);
		xid = pje->pje_xid;

		dynarray_init(&replaybufs);
		dynarray_ensurelen(&replaybufs, 1024);

		for (i = 0; i < dynarray_len(&pj->pj_bufs); i++) {
			tmppje = dynarray_getpos(&pj->pj_bufs, i);
			if (tmppje->pje_xid == xid) {
				nents++;
				dynarray_add(&replaybufs, tmppje);
			}
		}

		ntrans++;
		(pj_handler)(&replaybufs, rc);

		pjournal_remove_entries(pj, xid);
		dynarray_free(&replaybufs);
	}
	dynarray_free(&pj->pj_bufs);

	psc_warnx("Journal replay: %d log entries and %d transactions", nents, ntrans);
	return (rc);
}

/*
 * pjournal_load - initialize the in-memory representation of a journal.
 * return: pj on success, NULL on failure
 */
struct psc_journal *
pjournal_load(const char *fn)
{
	int				 i;
	ssize_t				 rc;
	struct psc_journal		*pj;
	struct psc_journal_hdr		*pjh;
	struct psc_journal_enthdr	*pje;
	uint64_t			 chksum;

	pj = PSCALLOC(sizeof(struct psc_journal));
	pjh = psc_alloc(sizeof(struct psc_journal_hdr), PAF_PAGEALIGN | PAF_LOCK);
	/*
	 * To quote open(2), the O_DIRECT flag may impose alignment restrictions on the length 
	 * and address of userspace buffers and the file offset of I/Os. Note that we are using
	 * 512 byte log entries.
	 */
	pj->pj_fd = open(fn, O_RDWR | O_SYNC | O_DIRECT);
	if (pj->pj_fd == -1)
		psc_fatal("open %s", fn);

	rc = pread(pj->pj_fd, pjh, sizeof(*pjh), 0);
	if (rc != sizeof(*pjh))
		psc_fatal("read journal header: want %zu got %zd",
		    sizeof(*pjh), rc);

	pj->pj_hdr = pjh;
	if (pjh->pjh_magic != PJH_MAGIC) {
		PSCFREE(pj);
		psc_freenl(pjh, sizeof(struct psc_journal_hdr));
		pj = NULL;
		psc_errorx("Journal header has a bad magic number!");
		goto done; 
	}

	PSC_CRC_INIT(chksum);
	i = offsetof(struct _psc_journal_hdr, _pjh_chksum);
	psc_crc_add(&chksum, pjh, offsetof(struct _psc_journal_hdr, _pjh_chksum));
	PSC_CRC_FIN(chksum);

	if (pjh->pjh_chksum != chksum) {
		errx(1, "journal header has an invalid checksum value "
		    PRI_PSC_CRC" vs "PRI_PSC_CRC, pjh->pjh_chksum, chksum);
		PSCFREE(pj);
		psc_freenl(pjh, sizeof(struct psc_journal_hdr));
		pj = NULL;
		goto done; 
	}

	LOCK_INIT(&pj->pj_lock);
	INIT_PSCLIST_HEAD(&pj->pj_pndgxids);
	psc_waitq_init(&pj->pj_waitq);
	pj->pj_flags = PJ_NONE;

	dynarray_init(&pj->pj_bufs);
	dynarray_ensurelen(&pj->pj_bufs, MAX_NUM_PJBUF);
	for (i = 0; i < MAX_NUM_PJBUF; i++) {
		pje = psc_alloc(PJ_PJESZ(pj), PAF_PAGEALIGN | PAF_LOCK);
		dynarray_add(&pj->pj_bufs, pje);
	}
done:
	return (pj);
}

/*
 * Initialize a new journal file.
 */
void
pjournal_format(const char *fn, uint32_t nents, uint32_t entsz, uint32_t ra,
		uint32_t opts)
{
	int32_t			 	 i;
	int				 fd;
	struct psc_journal		 pj;
	struct psc_journal_enthdr	*pje;
	struct psc_journal_hdr		 pjh;
	ssize_t				 size;
	unsigned char			*jbuf;
	uint32_t			 slot;
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
	
	PSC_CRC_INIT(chksum);
	psc_crc_add(&chksum, &pjh, offsetof(struct _psc_journal_hdr, _pjh_chksum));
	PSC_CRC_FIN(chksum);
	pjh.pjh_chksum = chksum;

	pj.pj_hdr = &pjh;
	jbuf = pjournal_alloclog_ra(&pj);

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
			pje = (struct psc_journal_enthdr *)&jbuf[pjh.pjh_entsz * i];
			pje->pje_magic = PJE_MAGIC;
			pje->pje_type = PJE_FORMAT;
			pje->pje_xid = PJE_XID_NONE;
			pje->pje_sid = PJE_XID_NONE;
			pje->pje_len = 0;

			PSC_CRC_INIT(chksum);
			psc_crc_add(&chksum, pje, offsetof(struct psc_journal_enthdr, pje_chksum));
			psc_crc_add(&chksum, pje->pje_data, pje->pje_len);
			PSC_CRC_FIN(chksum);

			pje->pje_chksum = chksum;
		}
		size = pwrite(fd, jbuf, pjh.pjh_entsz * count, 
			(off_t)(PJE_OFFSET + (slot * pjh.pjh_entsz)));
		if (size < 0 || size != pjh.pjh_entsz * count) {
			psc_fatal("Failed to write %d entries at slot %d", count, slot);
		}
	}
	if (close(fd) < 0) {
		psc_fatal("Failed to close journal fd");
	}
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
	int				 i;
	uint32_t			 ra;
	struct psc_journal		*pj;
	struct psc_journal_hdr		*pjh;
	struct psc_journal_enthdr	*pje;
	uint32_t			 slot;
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
		 pjh->pjh_entsz, pjh->pjh_nents, pjh->pjh_version, pjh->pjh_options,
		 pjh->pjh_readahead, pjh->pjh_start_off, pjh->pjh_magic);

	jbuf = pjournal_alloclog_ra(pj);

	for (slot = 0, ra=pjh->pjh_readahead; slot < pjh->pjh_nents; slot += count) {

		count = (pjh->pjh_nents - slot <= ra) ? (pjh->pjh_nents - slot) : ra;
		size = pread(pj->pj_fd, jbuf, (pjh->pjh_entsz * count), 
			    (off_t)(PJE_OFFSET + (slot * pjh->pjh_entsz)));

		if (size == -1 || size != (pjh->pjh_entsz * count))
			psc_fatal("Failed to read entries");

		for (i = 0; i < count; i++) {
			ntotal++;
			pje = (void *)&jbuf[pjh->pjh_entsz * i];
			if (pje->pje_magic != PJE_MAGIC) {
				nmagic++;
				psc_warnx("journal slot %d has bad magic!", (slot+i));
				continue;
			}
			if (pje->pje_magic == PJE_FORMAT) {
				nformat++;
				continue;
			}
			PSC_CRC_INIT(chksum);
			psc_crc_add(&chksum, pje, offsetof(struct psc_journal_enthdr, pje_chksum));
			psc_crc_add(&chksum, pje->pje_data, pje->pje_len);
			PSC_CRC_FIN(chksum);
			if (pje->pje_chksum != chksum) {
				nchksum++;
				psc_warnx("journal slot %d has bad checksum!", (slot+i));
				continue;
			}
			psc_info("slot=%u magic=%"PRIx64
				" type=%x xid=%"PRIx64" sid=%d\n",
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
