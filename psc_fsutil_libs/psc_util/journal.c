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

#define PSC_SUBSYS PSS_JOURNAL
#include "psc_util/subsys.h"

#include <sys/types.h>
#include <sys/stat.h>

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <string.h>
#include <unistd.h>

#include "pfl/fcntl.h"
#include "pfl/types.h"
#include "psc_ds/dynarray.h"
#include "psc_util/alloc.h"
#include "psc_util/atomic.h"
#include "psc_util/crc.h"
#include "psc_util/iostats.h"
#include "psc_util/journal.h"
#include "psc_util/lock.h"
#include "psc_util/thread.h"
#include "psc_util/time.h"
#include "psc_util/waitq.h"

struct psc_journalthr {
	struct psc_journal *pjt_pj;
};

__static int		pjournal_logwrite(struct psc_journal_xidhndl *, int,
				void *, size_t);

struct psc_waitq	pjournal_waitq = PSC_WAITQ_INIT;
psc_spinlock_t		pjournal_waitqlock = LOCK_INITIALIZER;

#define JIO_READ	0
#define JIO_WRITE	1

#define psc_journal_read(pj, p, len, off)	psc_journal_io((pj), (p), (len), (off), JIO_READ)
#define psc_journal_write(pj, p, len, off)	psc_journal_io((pj), (p), (len), (off), JIO_WRITE)

/**
 * psc_journal_io - Perform a low-level I/O operation on the journal store.
 * @pj: the owning journal.
 */
__static int
psc_journal_io(struct psc_journal *pj, void *p, size_t len, off_t off,
    int rw)
{
	ssize_t nb;
	int rc;

	if (rw == JIO_READ)
		nb = pread(pj->pj_fd, p, len, off);
	else
		nb = pwrite(pj->pj_fd, p, len, off);
	if (nb == -1) {
		rc = errno;
		psc_error("journal %s (pj=%p, len=%zd, off=%"PSCPRIdOFF")",
		    rw == JIO_READ ? "read" : "write", pj, len, off);
	} else if ((size_t)nb != len) {
		/*
		 * At least on one instance, short write actually
		 * returns "success" on a RAM-backed file system.
		 */
		rc = ENOSPC;
		psc_errorx("journal %s (pj=%p, len=%zd, off=%"PSCPRIdOFF", "
		    "nb=%zd): short I/O", rw == JIO_READ ? "read" : "write",
		    pj, len, off, nb);
	} else {
		rc = 0;
		psc_iostats_intv_add(rw == JIO_READ ?
		    &pj->pj_rdist : &pj->pj_wrist, nb);
	}
	return (rc);
}

/**
 * pjournal_xnew - Start a new transaction with a unique ID in the given
 *	journal.
 * @pj: the owning journal.
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
	INIT_PSCLIST_ENTRY(&xh->pjx_lentry1);
	INIT_PSCLIST_ENTRY(&xh->pjx_lentry2);

	/*
	 * Note that even though we issue xids in increasing order here,
	 * it does not necessarily mean transactions will end up in the
	 * log in the same order.
	 */
	PJ_LOCK(pj);
	do {
		xh->pjx_xid = ++pj->pj_lastxid;
	} while (xh->pjx_xid == PJE_XID_NONE);
	PJ_ULOCK(pj);

	psc_info("starting a new transaction %p (xid = %"PRIx64") in "
	    "journal %p", xh, xh->pjx_xid, pj);
	return (xh);
}

/**
 * pjournal_xadd_sngl - Add a single entry transaction in the system log.
 *     These transactions must be further processed by the distill process
 *     before their log space can be reclaimed.
 */
int
pjournal_xadd_sngl(struct psc_journal *pj, int type, void *data, size_t size)
{
	struct psc_journal_xidhndl *xh;
	int rc;

	xh = pjournal_xnew(pj);
	xh->pjx_flags |= PJX_XSNGL | PJX_XCLOSE;

	rc = pjournal_logwrite(xh, type, data, size);
	return (rc);
}

/**
 * pjournal_xadd - Log changes to a piece of metadata (e.g., journal flush 
 *     item).  We can't reply to our clients until after the log entry is 
 *     written.
 */
int
pjournal_xadd(struct psc_journal_xidhndl *xh, int type, void *data,
    size_t size)
{
	spinlock(&xh->pjx_lock);
	psc_assert(!(xh->pjx_flags & PJX_XCLOSE));
	freelock(&xh->pjx_lock);

	return (pjournal_logwrite(xh, type, data, size));
}

/**
 * pjournal_xend - Close a transaction of changes to a piece of metadata.
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

/**
 * pjournal_xrelease - Release the resources used the transaction.
 */
void
pjournal_xrelease(struct psc_journal_xidhndl *xh)
{
	int wakeup = 0;
	struct psc_journal *pj;
	struct psc_journal_enthdr *pje;

	pj = xh->pjx_pj;
	
	pje = (struct psc_journal_enthdr *) xh->pjx_data;

	PJ_LOCK(pj);
	psc_dynarray_add(&pj->pj_bufs, pje);
	wakeup = 0;
	if (pj->pj_flags & PJF_WANTBUF) {
		wakeup = 1;
		pj->pj_flags &= ~PJF_WANTBUF;
	}
	if (xh->pjx_flags & PJX_XCLOSE) {
		if ((pj->pj_flags & PJF_WANTSLOT) &&
		    (xh->pjx_tailslot == pj->pj_nextwrite)) {
			wakeup = 1;
			pj->pj_flags &= ~PJF_WANTSLOT;
			psc_warnx("Journal %p unblocking slot %d - "
				  "owned by xid %"PRIx64, pj, 
				  xh->pjx_tailslot, xh->pjx_xid);
		}
		psc_dbg("Transaction %p (xid = %"PRIx64") removed from "
		    "journal %p: tail slot = %d",
		    xh, xh->pjx_xid, pj, xh->pjx_tailslot);
		psclist_del(&xh->pjx_lentry1);
		PSCFREE(xh);
	}
	if (wakeup)
		psc_waitq_wakeall(&pj->pj_waitq);
	PJ_ULOCK(pj);
}

/**
 * pjournal_logwrite_internal - Write a new log entry for a transaction.
 * @xh: the transaction handle.
 * @slot: position location in journal to write.
 * @type: the application-specific log entry type.
 * @data: the journal entry contents to store.
 * @size: length of entry contents.
 * Returns: 0 on success, -1 on error.
 */
__static int
pjournal_logwrite_internal(struct psc_journal_xidhndl *xh, uint32_t slot,
    int type, void *data, size_t size)
{
	int				 rc;
	struct psc_journal		*pj;
	struct psc_journal_enthdr	*pje;
	int				 ntries;
	uint64_t			 chksum;

	rc = 0;
	pj = xh->pjx_pj;
	psc_assert(slot < pj->pj_hdr->pjh_nents);
	psc_assert(size + offsetof(struct psc_journal_enthdr, pje_data) <=
	    (size_t)PJ_PJESZ(pj));

	PJ_LOCK(pj);
	while (!psc_dynarray_len(&pj->pj_bufs)) {
		pj->pj_flags |= PJF_WANTBUF;
		psc_waitq_wait(&pj->pj_waitq, &pj->pj_lock);
		PJ_LOCK(pj);
	}
	pje = psc_dynarray_getpos(&pj->pj_bufs, 0);
	psc_dynarray_remove(&pj->pj_bufs, pje);
	psc_assert(pje);
	PJ_ULOCK(pj);

	xh->pjx_data = (void *)pje;

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
	ntries = PJ_MAX_TRY;
	while (ntries > 0) {
		rc = psc_journal_write(pj, pje, PJ_PJESZ(pj),
		    PJ_GETENTOFF(pj, slot));
		if (rc == EAGAIN) {
			ntries--;
			usleep(100);
			continue;
		}
		break;
	}

	/*
	 * We may want to turn off logging at this point and force
	 * write-through instead.
	 */
	if (rc) {
		rc = -1;
		psc_fatal("failed writing journal log entry at slot %d", slot);
	}
	if (xh->pjx_flags & PJX_XSNGL) {
		pll_addtail(&pj->pj_distillxids, xh);
		spinlock(&pjournal_waitqlock);
		psc_waitq_wakeall(&pjournal_waitq);
		freelock(&pjournal_waitqlock);
	} else
		pjournal_xrelease(xh);

	return (0);
}


/**
 * pjournal_logwrite - store a new entry in a journal transaction.
 * @xh: the transaction to receive the log entry.
 * @type: the application-specific log entry type.
 * @data: the journal entry contents to store.
 * @size: size of the custom data
 * Returns: 0 on success, -1 on error.
 */
__static int
pjournal_logwrite(struct psc_journal_xidhndl *xh, int type, void *data,
    size_t size)
{
	struct psc_journal_xidhndl	*t;
	struct psc_journal		*pj;
	uint32_t			 slot, tail_slot;
	int				 rc;

	pj = xh->pjx_pj;
	tail_slot = PJX_SLOT_ANY;

 retry:
	/*
	 * Make sure that the next slot to be written is not used by a
	 * pending transaction.  Since we add a new transaction at the
	 * tail of the pending transaction list, we only need to check
	 * the head of the list to find out the oldest transaction.
	 */
	PJ_LOCK(pj);
	slot = pj->pj_nextwrite;
	t = pll_gethdpeek(&pj->pj_pendingxids);
	if (t) {
		if (t->pjx_tailslot == slot) {
			psc_warnx("Journal %p write is blocked on slot %d "
			  "owned by transaction %p (xid = %"PRIx64")",
			  pj, pj->pj_nextwrite, t, t->pjx_xid);
			pj->pj_flags |= PJF_WANTSLOT;
			psc_waitq_wait(&pj->pj_waitq, &pj->pj_lock);
			goto retry;
		}
		tail_slot = t->pjx_tailslot;
	}

	if (!(xh->pjx_flags & PJX_XSTART)) {
		type |= PJE_XSTART;
		xh->pjx_flags |= PJX_XSTART;
		psc_assert(size != 0);
		psc_assert(xh->pjx_tailslot == PJX_SLOT_ANY);
		xh->pjx_tailslot = slot;
		/* note we add transaction in the order of their starting point */
		pll_addtail(&pj->pj_pendingxids, xh);
	}
	if (xh->pjx_flags & PJX_XCLOSE) {
		psc_assert(xh->pjx_tailslot != PJX_SLOT_ANY);
		if (xh->pjx_flags & PJX_XSNGL)
			type |= PJE_XSNGL;
		type |= PJE_XCLOSE;
	}

	/* Update the next slot to be written by a next log entry */
	psc_assert(pj->pj_nextwrite < pj->pj_hdr->pjh_nents);
	if ((++pj->pj_nextwrite) == pj->pj_hdr->pjh_nents)
		pj->pj_nextwrite = 0;

	PJ_ULOCK(pj);

	psc_info("Writing a log entry xid=%"PRIx64": xtail = %d, ltail = %d",
	    xh->pjx_xid, xh->pjx_tailslot, tail_slot);

	rc = pjournal_logwrite_internal(xh, slot, type, data, size);

	psc_info("Completed writing log entry xid=%"PRIx64
		 ": xtail = %d, slot = %d",
		 xh->pjx_xid, xh->pjx_tailslot, slot);

	return (rc);
}

__static void *
pjournal_alloc_buf(struct psc_journal *pj)
{
	return (psc_alloc(PJ_PJESZ(pj) * pj->pj_hdr->pjh_readahead,
	    PAF_PAGEALIGN | PAF_LOCK));
}

/**
 * pjournal_remove_entries - Remove a journal entry if it either has the
 *	given xid (mode = 1) or has a xid that is less than the give xid
 *	(mode = 2).
 * @pj: in-memory journal to remove from.
 * @xid: transaction ID of entries to remove.
 * @mode: operation.
 */
__static int
pjournal_remove_entries(struct psc_journal *pj, uint64_t xid, int mode)
{
	struct psc_journal_enthdr *pje;
	int i, scan, count;

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
	return (count);
}

/**
 * pjournal_xid_cmp - Compare tranactions for use in sorting.
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
 * pjournal_scan_slots - Accumulate all journal entries that need to be
 *	replayed in memory.  To reduce memory usage, we remove those
 *	entries of closed transactions as soon as we find them.
 */
__static int
pjournal_scan_slots(struct psc_journal *pj)
{
	int				 i;
	int				 rc;
	struct psc_journal_enthdr	*pje;
	uint32_t			 slot;
	unsigned char			*jbuf;
	int				 nclose;
	struct psc_journal_enthdr	*tmppje;
	uint64_t			 chksum;
	int				 nchksum;
	uint64_t			 last_xid;
	int32_t				 last_slot;
	struct psc_dynarray		 closetrans;
	uint64_t			 last_startup;
	int				 count, nopen, nscan, nmagic, nentry;

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
	 * We scan the log from the first physical entry to the last physical
	 * one regardless where the log really starts and ends.  This poses a
	 * problem: we might see the CLOSE entry of a transaction before
	 * its other entries due to log wraparound.  As a result, we
	 * must save these CLOSE entries until we have seen all the
	 * entries of the transaction (some of them might have already
	 * been overwritten, but that is perfectly fine).
	 */
	psc_dynarray_init(&closetrans);
	psc_dynarray_ensurelen(&closetrans, pj->pj_hdr->pjh_nents / 2);

	psc_dynarray_init(&pj->pj_bufs);
	jbuf = pjournal_alloc_buf(pj);
	count = pj->pj_hdr->pjh_readahead;
	psc_assert((pj->pj_hdr->pjh_nents % count) == 0);
	while (slot < pj->pj_hdr->pjh_nents) {
		rc = psc_journal_read(pj, jbuf, PJ_PJESZ(pj) * count,
		    PJ_GETENTOFF(pj, slot));
		if (rc)
			break;
		for (i = 0; i < count; i++) {
			nscan++;
			pje = (struct psc_journal_enthdr *)
			    &jbuf[PJ_PJESZ(pj) * i];
			if (pje->pje_magic != PJE_MAGIC) {
				nmagic++;
				psc_warnx("Journal %p: slot %d has "
				    "a bad magic number!", pj, slot + i);
				rc = -1;
				continue;
			}

			PSC_CRC64_INIT(&chksum);
			psc_crc64_add(&chksum, pje, offsetof(
			    struct psc_journal_enthdr, pje_chksum));
			psc_crc64_add(&chksum, pje->pje_data, pje->pje_len);
			PSC_CRC64_FIN(&chksum);

			if (pje->pje_chksum != chksum) {
				psc_warnx("Journal %p: slot %d has "
				    "a bad checksum!", pj, slot + i);
				nchksum++;
				rc = -1;
				continue;
			}
			psc_assert((pje->pje_type & PJE_XSTART) ||
			    (pje->pje_type & PJE_XCLOSE) ||
			    (pje->pje_type & PJE_STRTUP) ||
			    (pje->pje_type & PJE_FORMAT) ||
			    (pje->pje_type & PJE_XNORML));

			/*
			 * We start from the first log entry.  If we see
			 * a formatted log entry, there should be no
			 * more real log entries after that.
			 *
			 * If the log has wrapped around, then we will
			 * never see such an entry.
			 */
			if (pje->pje_type & PJE_FORMAT) {
				psc_assert(pje->pje_len == 0);
				goto done;
			}
			/*
			 * Remember the slot with the largest XID.
			 */
			if (pje->pje_xid >= last_xid) {
				last_xid = pje->pje_xid;
				last_slot = slot + i;
			}
			if (pje->pje_type & PJE_STRTUP) {
				psc_assert(pje->pje_len == 0);
				psc_info("Journal %p: found a startup "
				    "entry at slot %d!", pj, slot+i);
				if (pje->pje_xid > last_startup)
					last_startup = pje->pje_xid;
				continue;
			}
			if (pje->pje_type & PJE_XCLOSE) {
				nclose++;
				if (!(pje->pje_type & PJE_XSNGL)) {
					psc_assert(pje->pje_len == 0);
					nentry = pjournal_remove_entries(pj, 
					    pje->pje_xid, 1);
					psc_assert(nentry <= (int)pje->pje_sid);
					if (nentry == (int)pje->pje_sid)
						continue;
				}
			}

			/* Okay, we need to keep this log entry for now.  */
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
	/*
	 * If we are dealing with a brand new log or the log has wrapped around, 
	 * we won't see a startup record because we currently only write it when 
	 * we start.
	 *
	 * The main use of the startup record is to prevent us from considering 
	 * log records from previous instance of the file system. For the current 
	 * run, we rely on PJE_XCLOSE records to reduce replay.  In other words, 
	 * inserting more startup records won't help.
	 */
	if (last_startup != PJE_XID_NONE)
		pjournal_remove_entries(pj, last_startup, 2);

	pj->pj_lastxid = last_xid;
	/* If last_slot is PJX_SLOT_ANY, then nextwrite will be 0 */
	pj->pj_nextwrite = (last_slot == (int)pj->pj_hdr->pjh_nents - 1) ?
	    0 : (last_slot + 1);
	qsort(pj->pj_bufs.da_items, pj->pj_bufs.da_pos,
	    sizeof(void *), pjournal_xid_cmp);
	psc_freenl(jbuf, PJ_PJESZ(pj) * pj->pj_hdr->pjh_readahead);

	/*
	 * We need this code because we don't start from the beginning of the log.
	 * On the other hand, I don't expect either array will be long.
	 */
	while (psc_dynarray_len(&closetrans)) {
		pje = psc_dynarray_getpos(&closetrans, 0);
		pjournal_remove_entries(pj, pje->pje_xid, 1);
		psc_dynarray_remove(&closetrans, pje);
		psc_freenl(pje, PJ_PJESZ(pj));
	}
	psc_dynarray_free(&closetrans);

	nopen = psc_dynarray_len(&pj->pj_bufs);
	psc_info("Journal statistics: %d close, %d open, %d magic, "
	    "%d chksum, %d scan, %d total",
	    nclose, nopen, nmagic, nchksum, nscan, pj->pj_hdr->pjh_nents);
	return (rc);
}

/**
 * pjournal_open - Initialize the in-memory representation of a journal.
 * @fn: path to journal on file system.
 */
__static struct psc_journal *
pjournal_open(const char *fn)
{
	struct psc_journal_hdr *pjh;
	struct psc_journal *pj;
	struct stat statbuf;
	const char *basefn;
	uint64_t chksum;
	ssize_t pjhlen;

	pj = PSCALLOC(sizeof(*pj));
	pj->pj_fd = open(fn, O_RDWR | O_SYNC | O_DIRECT);
	if (pj->pj_fd == -1)
		psc_fatal("failed to open journal %s", fn);
	if (fstat(pj->pj_fd, &statbuf) == -1)
		psc_fatal("failed to stat journal %s", fn);
	basefn = strrchr(fn, '/');
	if (basefn)
		basefn++;
	else
		basefn = fn;
	psc_iostats_init(&pj->pj_rdist, "jrnlrd-%s", basefn);
	psc_iostats_init(&pj->pj_wrist, "jrnlwr-%s", basefn);

	/*
	 * O_DIRECT may impose alignment restrictions so align the buffer
	 * and perform I/O in multiples of file system block size.
	 */
	pjhlen = PSC_ALIGN(sizeof(*pjh), statbuf.st_blksize);
	pjh = psc_alloc(pjhlen, PAF_PAGEALIGN | PAF_LOCK);
	if (psc_journal_read(pj, pjh, pjhlen, 0))
		psc_fatalx("Fail to read journal header");

	pj->pj_hdr = pjh;
	if (pjh->pjh_magic != PJH_MAGIC) {
		psc_errorx("Journal header has a bad magic number %lx", pjh->pjh_magic);
		goto err;
	}
	if (pjh->pjh_version != PJH_VERSION) {
		psc_errorx("Journal header has an invalid version number %d", pjh->pjh_version);
		goto err;
	}

	PSC_CRC64_INIT(&chksum);
	psc_crc64_add(&chksum, pjh, offsetof(struct psc_journal_hdr, pjh_chksum));
	PSC_CRC64_FIN(&chksum);

	if (pjh->pjh_chksum != chksum) {
		psc_errorx("Journal header has an invalid checksum value "
		    "%"PSCPRIxCRC64" vs %"PSCPRIxCRC64, pjh->pjh_chksum, chksum);
		goto err;
	}
	if (statbuf.st_size != (off_t)(pjhlen + pjh->pjh_nents * PJ_PJESZ(pj))) {
		psc_errorx("Size of the log file does not match specs in its header");
		goto err;
	}

	/*
	 * The remaining two fields pj_lastxid and pj_nextwrite will be
	 * filled after log replay.
	 */
	LOCK_INIT(&pj->pj_lock);
	LOCK_INIT(&pj->pj_pendinglock);
	LOCK_INIT(&pj->pj_distilllock);

	pll_init(&pj->pj_pendingxids, struct psc_journal_xidhndl,
		 pjx_lentry1, &pj->pj_pendinglock);
	pll_init(&pj->pj_distillxids, struct psc_journal_xidhndl,
		 pjx_lentry2, &pj->pj_distilllock);

	psc_waitq_init(&pj->pj_waitq);
	pj->pj_flags = PJF_NONE;
	psc_dynarray_init(&pj->pj_bufs);
	return (pj);
 err:
	psc_freenl(pjh, pjhlen);
	PSCFREE(pj);
	return (NULL);
}

/**
 * pjournal_release - Release resources associated with an in-memory
 *	journal.
 * @pj: journal to release.
 */
__static void
pjournal_release(struct psc_journal *pj)
{
	int				 n;
	struct psc_journal_enthdr	*pje;

	DYNARRAY_FOREACH(pje, n, &pj->pj_bufs)
		psc_freenl(pje, PJ_PJESZ(pj));
	psc_dynarray_free(&pj->pj_bufs);
	psc_freenl(pj->pj_hdr, pj->pj_hdr->pjh_iolen);
	PSCFREE(pj);
}

/**
 * pjournal_format - Initialize an on-disk journal.
 * @fn: file path to store journal.
 * @nents: number of entries journal may contain.
 * @entsz: size of a journal entry.
 * @ra: number of entries to operate on in one disk I/O operation.
 * Returns 0 on success, errno on error.
 */
int
pjournal_format(const char *fn, uint32_t nents, uint32_t entsz, uint32_t ra)
{
	int32_t				 i;
	int				 rc;
	int				 fd;
	struct psc_journal		 pj;
	struct stat			 stb;
	struct psc_journal_enthdr	*pje;
	struct psc_journal_hdr		 pjh;
	unsigned char			*jbuf;
	uint32_t			 slot;

	if (nents % ra) {
		printf("Number of slots should be a multiple of readahead.\n");
		return (EINVAL);
	}

	memset(&pj, 0, sizeof(struct psc_journal));
	pj.pj_hdr = &pjh;

	rc = 0;
	fd = open(fn, O_WRONLY | O_CREAT | O_TRUNC, 0600);
	if (fd == -1)
		psc_fatal("%s", fn);

	if (fstat(fd, &stb) == -1)
		psc_fatal("stat %s", fn);

	pj.pj_fd = fd;

	pjh.pjh_entsz = entsz;
	pjh.pjh_nents = nents;
	pjh.pjh_version = PJH_VERSION;
	pjh.pjh_readahead = ra;
	pjh.pjh_iolen = PSC_ALIGN(sizeof(pjh), stb.st_blksize);
	pjh.pjh_magic = PJH_MAGIC;

	PSC_CRC64_INIT(&pjh.pjh_chksum);
	psc_crc64_add(&pjh.pjh_chksum, &pjh,
	    offsetof(struct psc_journal_hdr, pjh_chksum));
	PSC_CRC64_FIN(&pjh.pjh_chksum);

	if (psc_journal_write(&pj, &pjh, pjh.pjh_iolen, 0))
		psc_fatal("failed to write journal header");

	jbuf = pjournal_alloc_buf(&pj);
	for (i = 0; i < (int)ra; i++) {
		pje = (struct psc_journal_enthdr *)
		    &jbuf[PJ_PJESZ(&pj) * i];
		pje->pje_magic = PJE_MAGIC;
		pje->pje_type = PJE_FORMAT;
		pje->pje_xid = PJE_XID_NONE;
		pje->pje_sid = PJE_XID_NONE;
		pje->pje_len = 0;

		PSC_CRC64_INIT(&pje->pje_chksum);
		psc_crc64_add(&pje->pje_chksum, pje,
		    offsetof(struct psc_journal_enthdr, pje_chksum));
		psc_crc64_add(&pje->pje_chksum, pje->pje_data,
		    pje->pje_len);
		PSC_CRC64_FIN(&pje->pje_chksum);
	}

	for (slot = 0; slot < pjh.pjh_nents; slot += ra) {
		rc = psc_journal_write(&pj, jbuf, PJ_PJESZ(&pj) * ra,
		    PJ_GETENTOFF(&pj, slot));
		if (rc)
			break;
	}
	if (close(fd) == -1)
		psc_fatal("failed to close journal");
	psc_freenl(jbuf, PJ_PJESZ(&pj) * ra);
	psc_info("journal %s formatted: %d slots, %d readahead, error = %d",
	    fn, nents, ra, rc);
	return (rc);
}

/**
 * pjournal_dump - Dump the contents of a journal file.
 * @fn: journal filename to query.
 * @verbose: whether to report stats summary or full dump.
 */
int
pjournal_dump(const char *fn, int verbose)
{
	uint32_t			 ra;
	struct psc_journal		*pj;
	struct psc_journal_hdr		*pjh;
	struct psc_journal_enthdr	*pje;
	uint32_t			 slot;
	unsigned char			*jbuf;
	uint64_t			 chksum;
	int				 i, count, ntotal, nmagic, nchksum, nformat;

	ntotal = 0;
	nmagic = 0;
	nchksum = 0;
	nformat = 0;

	pj = pjournal_open(fn);
	pjh = pj->pj_hdr;

	printf("journal header info for %s:\n"
	    "  entsize %u\n"
	    "  nents %u\n"
	    "  version %u\n"
	    "  options %u\n"
	    "  readahead %u\n"
	    "  start_offset %#"PRIx64"\n",
	    fn, PJ_PJESZ(pj), pjh->pjh_nents, pjh->pjh_version, pjh->pjh_options,
	    pjh->pjh_readahead, pjh->pjh_start_off);

	jbuf = pjournal_alloc_buf(pj);

	for (slot = 0, ra = pjh->pjh_readahead;
	    slot < pjh->pjh_nents; slot += count) {
		count = (pjh->pjh_nents - slot <= ra) ?
		    (pjh->pjh_nents - slot) : ra;
		if (psc_journal_read(pj, jbuf, PJ_PJESZ(pj) * count,
		    PJ_GETENTOFF(pj, slot)))
			psc_fatal("failed to read %d log entries "
			    "at slot %d", count, slot);

		for (i = 0; i < count; i++) {
			ntotal++;
			pje = (void *)&jbuf[PJ_PJESZ(pj) * i];
			if (pje->pje_magic != PJE_MAGIC) {
				nmagic++;
				psc_warnx("Journal slot %d has "
				    "a bad magic number", slot + i);
				continue;
			}
			if (pje->pje_magic == PJE_FORMAT) {
				nformat++;
				continue;
			}

			PSC_CRC64_INIT(&chksum);
			psc_crc64_add(&chksum, pje,
			    offsetof(struct psc_journal_enthdr, pje_chksum));
			psc_crc64_add(&chksum, pje->pje_data, pje->pje_len);
			PSC_CRC64_FIN(&chksum);

			if (pje->pje_chksum != chksum) {
				nchksum++;
				psc_warnx("Journal slot %d has "
				    "a bad checksum", slot + i);
				continue;
			}
			if (verbose)
				printf("slot %u: type %x "
				    "xid %"PRIx64" sid %d\n",
				    slot + i, pje->pje_type,
				    pje->pje_xid, pje->pje_sid);
		}

	}
	if (close(pj->pj_fd) == -1)
		psc_fatal("failed closing journal %s", fn);

	psc_freenl(jbuf, PJ_PJESZ(pj));
	pjournal_release(pj);

	printf("%d slot(s) total, %d formatted, %d bad magic, %d bad checksum(s)\n",
	    ntotal, nformat, nmagic, nchksum);
	return (0);
}

void
pjournal_distillthr_main(struct psc_thread *thr)
{
	struct psc_journalthr *pjt = thr->pscthr_private;
	struct psc_journal *pj = pjt->pjt_pj;
	struct psc_journal_enthdr *pje;
	struct psc_journal_xidhndl *xh;

	while (pscthr_run()) {
		/*
		 * Walk the list until we find a log entry that needs processing.
		 */
		while ((xh = pll_get(&pj->pj_distillxids))) {
			pje = (struct psc_journal_enthdr *) xh->pjx_data;
			(pj->pj_distill_handler)(pje, PJ_PJESZ(pj));
			pjournal_xrelease(xh);
		}
		spinlock(&pjournal_waitqlock);
		if ((xh = pll_gethdpeek(&pj->pj_distillxids))) {
			freelock(&pjournal_waitqlock);
			continue;
		}
		psc_waitq_wait(&pjournal_waitq, &pjournal_waitqlock);
	}
}

/**
 * pjournal_init - Replay all open transactions in a journal.
 * @fn: location on journal file on file system.
 * @txg: the first transaction group number to be used by ZFS.
 * @thrtype: application-specified thread type ID for distill processor.
 * @thrname: application-specified thread name for distill processor.
 * @replay_handler: the journal replay callback.
 * @distill_handler: the distill processor callback.
 */
struct psc_journal *
pjournal_init(const char *fn, uint64_t txg,
    int thrtype, const char *thrname,
    psc_replay_handler replay_handler,
    psc_distill_handler distill_handler)
{
	int				 i;
	int				 rc;
	struct psc_journal		*pj;
	uint64_t			 xid;
	struct psc_journal_enthdr	*pje;
	int				 nents;
	int				 nerrs;
	uint64_t			 chksum;
	int				 ntrans;
	struct psc_journal_enthdr	*tmppje;
	struct psc_dynarray		 replaybufs;
	struct psc_journalthr		*pjt;
	struct psc_thread		*thr;

	psc_notify("First ZFS transaction group number is %"PRId64, txg);

	pj = pjournal_open(fn);
	if (pj == NULL)
		return (NULL);

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

		/* Collect buffer belonging to the same transaction */
		for (i = 0; i < psc_dynarray_len(&pj->pj_bufs); i++) {
			tmppje = psc_dynarray_getpos(&pj->pj_bufs, i);
			psc_assert(tmppje->pje_len != 0);
			if (tmppje->pje_xid == xid) {
				nents++;
				psc_dynarray_add(&replaybufs, tmppje);
			}
			if (tmppje->pje_type & PJE_XSNGL)
				break;
		}

		ntrans++;
		replay_handler(&replaybufs, &rc);
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
	psc_crc64_add(&chksum, pje,
	    offsetof(struct psc_journal_enthdr, pje_chksum));
	psc_crc64_add(&chksum, pje->pje_data, pje->pje_len);
	PSC_CRC64_FIN(&chksum);
	pje->pje_chksum = chksum;

	if (pj->pj_nextwrite >= pj->pj_hdr->pjh_nents)
		pj->pj_nextwrite = 0;

	if (psc_journal_write(pj, pje, PJ_PJESZ(pj),
	    PJ_GETENTOFF(pj, pj->pj_nextwrite)))
		psc_fatalx("failed to write a start up marker in the journal");
	psc_freenl(pje, PJ_PJESZ(pj));

	pj->pj_nextwrite++;

	/* pre-allocate some buffers for log writes/distill */
	for (i = 0; i < PJ_MAX_BUF; i++) {
		pje = psc_alloc(PJ_PJESZ(pj), PAF_PAGEALIGN | PAF_LOCK);
		psc_dynarray_add(&pj->pj_bufs, pje);
	}
	pj->pj_distill_handler = distill_handler;

	thr = pscthr_init(thrtype, 0, pjournal_distillthr_main, NULL, sizeof(*pjt), thrname);
	pjt = thr->pscthr_private;
	pjt->pjt_pj = pj;
	pscthr_setready(thr);

	psc_info("journal replayed: %d log entries with %d transactions "
	    "have been redone, error = %d", nents, ntrans, nerrs);
	return (pj);
}
