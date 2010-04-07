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
#include "psc_util/thread.h"
#include "psc_util/atomic.h"
#include "psc_util/crc.h"
#include "psc_util/journal.h"
#include "psc_util/lock.h"
#include "psc_util/time.h"

#define MAX_LOG_TRY		3	/* # log write failure retry attempts */

__static int	pjournal_logwrite(struct psc_journal_xidhndl *, int,
			void *, size_t);
__static void	pjournal_shdw_logwrite(struct psc_journal *,
			const struct psc_journal_enthdr *, uint32_t);
__static void	pjournal_shdw_prepslot(struct psc_journal_shdw *,
			uint32_t, int32_t);

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
	INIT_PSCLIST_ENTRY(&xh->pjx_lentry);

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

int
pjournal_xadd_sngl(struct psc_journal *pj, int type, void *data, size_t size)
{
	struct psc_journal_xidhndl *xh;
	int rc;

	xh = pjournal_xnew(pj);
	xh->pjx_flags |= (PJX_XSTART | PJX_XCLOSE | PJX_XSNGL);

	rc = pjournal_logwrite(xh, type, data, size);
	return (rc);
}

/**
 * pjournal_xadd - Log changes to a piece of metadata (i.e. journal
 *	flush item).  We can't reply to our clients until after the log
 *	entry is written.
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
	ssize_t				 sz;
	struct psc_journal		*pj;
	struct psc_journal_enthdr	*pje;
	int				 ntries;
	uint64_t			 chksum;
	int				 wakeup;

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

	if (pj->pj_hdr->pjh_options & PJF_SHADOW)
		pjournal_shdw_logwrite(pj, pje, slot);

	/* commit the log entry on disk before we can return */
	ntries = MAX_LOG_TRY;
	while (ntries) {
		sz = pwrite(pj->pj_fd, pje, PJ_PJESZ(pj), (off_t)
		    (pj->pj_hdr->pjh_start_off + (slot * PJ_PJESZ(pj))));
		if (sz == -1 && errno == EAGAIN) {
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
	if (sz == -1 || (size_t)sz != PJ_PJESZ(pj)) {
		rc = -1;
		psc_errorx("Problem writing journal log entries at slot %d", slot);
	}

	PJ_LOCK(pj);
	psc_dynarray_add(&pj->pj_bufs, pje);
	wakeup = 0;
	if (pj->pj_flags & PJF_WANTBUF) {
		wakeup = 1;
		pj->pj_flags &= ~PJF_WANTBUF;
	}
	if ((pj->pj_flags & PJF_WANTSLOT) &&
	    (xh->pjx_flags & PJX_XCLOSE) &&
	    (xh->pjx_tailslot == pj->pj_nextwrite)) {
		wakeup = 1;
		pj->pj_flags &= ~PJF_WANTSLOT;
		psc_warnx("Journal %p unblocking slot %d - "
		    "owned by xid %"PRIx64, pj, slot, xh->pjx_xid);
	}
	if ((xh->pjx_flags & PJX_XCLOSE) && !(xh->pjx_flags & PJX_XSNGL)) {
		psc_dbg("Transaction %p (xid = %"PRIx64") removed from "
		    "journal %p: tail slot = %d, rc = %d",
		    xh, xh->pjx_xid, pj, xh->pjx_tailslot, rc);
		psclist_del(&xh->pjx_lentry);
		PSCFREE(xh);
	}
	if (wakeup)
		psc_waitq_wakeall(&pj->pj_waitq);
	PJ_ULOCK(pj);
	return (rc);
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
	int				 normal, rc;

	pj = xh->pjx_pj;
	tail_slot = PJX_SLOT_ANY;

 retry:
	/*
	 * Make sure that the next slot to be written does not have a
	 * pending transaction.  Since we add a new transaction at the
	 * tail of the pending transaction list, we only need to check
	 * the head of the list to find out the oldest pending transaction.
	 */
	PJ_LOCK(pj);
	slot = pj->pj_nextwrite;
	t = psclist_first_entry(&pj->pj_pndgxids, struct psc_journal_xidhndl, pjx_lentry);
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
	/*
	 * Optionally write into our shadow area only AFTER we have decided
	 * on the slot to be used.
	 *
	 * Reference the shadow tile and ensure the corresponding tile slot
	 * is available.  Note, the pjs may advance to the next tile before
	 * this operations completes.
	 */
	if (pj->pj_hdr->pjh_options & PJF_SHADOW)
		pjournal_shdw_prepslot(pj->pj_shdw, slot, 1);

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
		if (xh->pjx_flags & PJX_XSNGL) {
			psc_assert(xh->pjx_tailslot == PJX_SLOT_ANY);
			type |= PJE_XSNGL;
		} else {
			psc_assert(size == 0);
			psc_assert(xh->pjx_tailslot != PJX_SLOT_ANY);
			psc_assert(xh->pjx_tailslot != slot);
		}
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

	psc_info("Writing a log entry for transaction %"PRIx64": "
	    "transaction tail = %d, log tail = %d",
	    xh->pjx_xid, xh->pjx_tailslot, tail_slot);

	rc = pjournal_logwrite_internal(xh, slot, type, data, size);
	return (rc);
}

/**
 * pjournal_logread - Get a specified entry from a journal.
 * @pj: the journal.
 * @slot: the position in the journal of the entry to obtain.
 * @count: the number of slots to read.
 * @data: a pointer to buffer when we fill journal entries.
 * Returns: 'n' entries read on success, -1 on error.
 */
__static int
pjournal_logread(struct psc_journal *pj, int32_t slot, int32_t count,
    void *data)
{
	int		rc;
	off_t		addr;
	ssize_t		size;

	rc = 0;
	addr = pj->pj_hdr->pjh_start_off + slot * PJ_PJESZ(pj);
	size = pread(pj->pj_fd, data, PJ_PJESZ(pj) * count, addr);
	if (size == -1 || (size_t)size != PJ_PJESZ(pj) *  count) {
		psc_warn("Fail to read %zd bytes from journal %p: "
		    "rc = %d, errno = %d", size, pj, rc, errno);
		rc = -1;
	}
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
	 * We scan the log from the first entry to the last one
	 * regardless where the log really starts.  This poses a
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
				psc_info("Journal %p: found a startup "
				    "entry at slot %d!", pj, slot+i);
				if (pje->pje_xid > last_startup)
					last_startup = pje->pje_xid;
				continue;
			}
			if (pje->pje_type & PJE_XCLOSE) {
				nclose++;
				if (!(pje->pje_type & PJE_XSNGL))
					psc_assert(pje->pje_len == 0);
				nentry = pjournal_remove_entries(pj,
				    pje->pje_xid, 1);
				psc_assert(nentry <= (int)pje->pje_sid);
				if (nentry == (int)pje->pje_sid)
					continue;
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
	psc_warnx("Journal statistics: %d close, %d open, %d magic, "
	    "%d chksum, %d scan, %d total",
	    nclose, nopen, nmagic, nchksum, nscan, pj->pj_hdr->pjh_nents);
	return (rc);
}

/**
 * pjournal_load - Initialize the in-memory representation of a journal.
 */
__static struct psc_journal *
pjournal_load(const char *fn)
{
	struct psc_journal_hdr *pjh;
	struct psc_journal *pj;
	struct stat statbuf;
	ssize_t rc, pjhlen;
	uint64_t chksum;

	pj = PSCALLOC(sizeof(*pj));
	pj->pj_fd = open(fn, O_RDWR | O_SYNC | O_DIRECT);
	if (pj->pj_fd == -1)
		psc_fatal("Fail to open log file %s", fn);
	rc = fstat(pj->pj_fd, &statbuf);
	if (rc == -1)
		psc_fatal("Fail to stat log file %s", fn);

	/*
	 * O_DIRECT may impose alignment restrictions so align to page
	 * and perform I/O in multiples of file system block size.
	 */
	pjhlen = PSC_ALIGN(sizeof(*pjh), statbuf.st_blksize);
	pjh = psc_alloc(pjhlen, PAF_PAGEALIGN | PAF_LOCK);
	rc = pread(pj->pj_fd, pjh, pjhlen, 0);
	if (rc != pjhlen)
		psc_fatal("Fail to read journal header: want %zd got %zd",
		    pjhlen, rc);
	pj->pj_hdr = pjh;
	if (pjh->pjh_magic != PJH_MAGIC) {
		psc_errorx("Journal header has a bad magic number");
		goto err;
	}
	if (pjh->pjh_version != PJH_VERSION) {
		psc_errorx("Journal header has an invalid version number");
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
	INIT_PSCLIST_HEAD(&pj->pj_pndgxids);
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
 * @opts: journal operational flags.
 */
int
pjournal_format(const char *fn, uint32_t nents, uint32_t entsz,
    uint32_t ra, uint32_t opts)
{
	int32_t				 i;
	int				 rc;
	int				 fd;
	struct psc_journal		 pj;
	struct psc_journal_enthdr	*pje;
	struct psc_journal_hdr		 pjh;
	ssize_t				 size;
	unsigned char			*jbuf;
	uint32_t			 slot;
	int				 count;
	struct stat stb;

	pj.pj_hdr = &pjh;

	rc = 0;
	fd = open(fn, O_WRONLY | O_CREAT | O_TRUNC, 0600);
	if (fd == -1)
		psc_fatal("%s", fn);

	if (fstat(fd, &stb) == -1)
		psc_fatal("stat %s", fn);

	pjh.pjh_entsz = entsz;
	pjh.pjh_nents = nents;
	pjh.pjh_version = PJH_VERSION;
	pjh.pjh_options = opts;
	pjh.pjh_readahead = ra;
	pjh.pjh_iolen = PSC_ALIGN(sizeof(pjh), stb.st_blksize);
	pjh.pjh_magic = PJH_MAGIC;

	PSC_CRC64_INIT(&pjh.pjh_chksum);
	psc_crc64_add(&pjh.pjh_chksum, &pjh,
	    offsetof(struct psc_journal_hdr, pjh_chksum));
	PSC_CRC64_FIN(&pjh.pjh_chksum);

	size = pwrite(fd, &pjh, pjh.pjh_iolen, 0);
	if (size == -1 || size != (ssize_t)pjh.pjh_iolen)
		psc_fatal("Failed to write header");

	jbuf = pjournal_alloc_buf(&pj);

	for (slot = 0; slot < pjh.pjh_nents; slot += count) {
		count = (nents - slot <= ra) ? (nents - slot) : ra;
		for (i = 0; i < count; i++) {
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
		size = pwrite(fd, jbuf, PJ_PJESZ(&pj) * count,
			(off_t)(pjh.pjh_start_off + (slot * PJ_PJESZ(&pj))));
		/*
		 * At least on one instance, short write actually
		 * returns success on a RAM-backed file system.
		 */
		if (size == -1 || (size_t)size != PJ_PJESZ(&pj) * count) {
			rc = -1;
			psc_errorx("failed to write %d entries "
			    "at slot %d", count, slot);
			break;
		}
	}
	if (close(fd) == -1)
		psc_fatal("failed to close journal");
	psc_freenl(jbuf, PJ_PJESZ(&pj) * ra);
	return rc;
}

/**
 * pjournal_dump - Dump the contents of a journal file.
 * @fn: journal filename to query.
 * @verbose: whether to report stats summary or full dump.
 */
int
pjournal_dump(const char *fn, int verbose)
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
		size = pread(pj->pj_fd, jbuf, (PJ_PJESZ(pj) * count),
		    (off_t)(pjh->pjh_start_off + (slot * PJ_PJESZ(pj))));

		if (size == -1)
			psc_fatal("Failed to read %d log entries "
			    "at slot %d", count, slot);
		if ((size_t)size != (PJ_PJESZ(pj)* count))
			psc_fatalx("Short read for %d log entries "
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

/**
 * pjournal_prep_pjst - prepare a journal shadow tile for action.  The pjst
 *   must have already undergone initialization.  This function mainly serves
 *   as a sanity check on the tile.
 */
__static __inline void
pjournal_prep_pjst(struct psc_journal_shdw_tile *pjst,
		   const struct psc_journal *pj, uint32_t sjent)
{
	spinlock(&pjst->pjst_lock);
	psc_assert(pjst->pjst_base);
	memset(pjst->pjst_base, 0, (size_t)(PJ_PJESZ(pj) *
					    pj->pj_shdw->pjs_tilesize));
	psc_assert(!psc_atomic32_read(&pjst->pjst_ref));
	pjst->pjst_state = PJSHDWT_FREE;
	pjst->pjst_sjent = sjent;
	freelock(&pjst->pjst_lock);
}

/**
 * pjournal_shdw_advtile_locked - advance to the next journal shadow tile.
 * @pjs:  pointer to the journal shadow.
 * @block:  caller specifies whether he is blockable.  The shadow thread
 *   must not block.
 * Notes:  The pjs lock is used to synchronize tile activity.
 */
__static void
pjournal_shdw_advtile_locked(struct psc_journal_shdw *pjs, int block)
{
	uint32_t next_tile;

	LOCK_ENSURE(&pjs->pjs_lock);
	psc_assert(pjs->pjs_state & PJSHDW_ADVTILE);

	/* Map the current and next tiles whose values will be protected
	 *    by PJSHDW_ADVTILE.
	 */
	next_tile = pjs->pjs_curtile % (pjs->pjs_ntiles - 1);

	while (pjs->pjs_pjsts[next_tile]->pjst_state != PJSHDWT_FREE) {
		psc_assert(block);
		psc_waitq_wait(&pjs->pjs_waitq, &pjs->pjs_lock);
		spinlock(&pjs->pjs_lock);
	}

	psc_assert(pjs->pjs_pjsts[next_tile]->pjst_state == PJSHDWT_FREE);
	pjs->pjs_pjsts[next_tile]->pjst_state = PJSHDWT_INUSE;
	pjs->pjs_pjsts[pjs->pjs_curtile]->pjst_state = PJSHDWT_PROCRDY;
	pjs->pjs_curtile = next_tile;
	pjs->pjs_state &= ~PJSHDW_ADVTILE;
	psc_waitq_wakeall(&pjs->pjs_waitq);
}

/**
 * pjournal_getcur_pjst_locked - retrieve the current shadow tile in a
 *   manner which accounts for tile advancement.
 * @pjs:  the journal shadow in question
 * @slot:  requested journal slot number
 * @block:  permitted to block?
 */
__static void
pjournal_shdw_prepslot(struct psc_journal_shdw *pjs, uint32_t slot,
		      int32_t block)
{
	struct psc_journal_shdw_tile *pjst;
	struct psc_journal_enthdr *pje;

 restart:
	spinlock(&pjs->pjs_lock);
	pjst = pjs->pjs_pjsts[pjs->pjs_curtile];
	if (slot == (pjst->pjst_sjent + pjs->pjs_tilesize)) {
		if (pjs->pjs_state & PJSHDW_ADVTILE) {
			/* Another thread has already begun the tile advance
			 *    procedures.  Wait for it to complete then
			 *    retry.
			 */
			while (pjs->pjs_state & PJSHDW_ADVTILE) {
				psc_waitq_wait(&pjs->pjs_waitq, &pjs->pjs_lock);
				goto restart;
			}
		} else {
			/* Tile advancementment is our job.  Note the pjs lock
			 *    may be dropped if the next tile is still busy.
			 */
			pjs->pjs_state |= PJSHDW_ADVTILE;
			pjournal_shdw_advtile_locked(pjs, block);
		}
	}
	freelock(&pjs->pjs_lock);
	/* No other states are allowed, this must be the active tile.
	 * The big journal lock is being held so our slot # is the
	 *    largest in the system meaning that the tile could not
	 *    have been advanced ahead of us.
	 */
	psc_assert(pjst->pjst_state == PJSHDWT_INUSE);

	psc_assert(pjs->pjs_curtile < pjs->pjs_ntiles);
	psc_assert(slot >= pjst->pjst_sjent &&
		   (slot <= (pjst->pjst_sjent + pjs->pjs_tilesize)));

	psc_atomic32_inc(&pjst->pjst_ref);
	pje = (void *)((char *)pjst->pjst_base +
	    (PJ_PJESZ(pjs->pjs_journal) * (slot - pjst->pjst_sjent)));

	psc_assert(pje->pje_magic == 0);
	/* 'Reserve' the slot by writing it into the structure so it may be
	 *    checked later.
	 */
	pje->pje_shdw_slot = slot;
}

/**
 * pjournal_getbyslot_pjst - retrieve the current shadow tile in a
 *	manner which accounts for tile advancement.
 * @pjs:  the journal shadow in question
 * @slot:  requested journal slot number
 * Return:  Returns a referenced shadow tile.
 */
__static struct psc_journal_shdw_tile *
pjournal_getbyslot_pjst(struct psc_journal_shdw *pjs, uint32_t slot)
{
	struct psc_journal_shdw_tile *pjst;
	uint32_t i, found=0;

	spinlock(&pjs->pjs_lock);
	psc_assert(pjs->pjs_curtile < pjs->pjs_ntiles);

	i = pjs->pjs_curtile;

	do {
		pjst = pjs->pjs_pjsts[i];
		if (slot >= pjst->pjst_sjent &&
		    (slot <= (pjst->pjst_sjent + pjs->pjs_tilesize - 1))) {
			found = 1;
			break;

		} else
			if (++i == pjs->pjs_ntiles)
				i = 0;

	} while (i != pjs->pjs_curtile);

	if (found)
		psc_assert(psc_atomic32_read(&pjst->pjst_ref) > 0);
	else
		//pjst = NULL;
		abort();

	freelock(&pjs->pjs_lock);

	return (pjst);
}

__static void
pjournal_shdw_logwrite(struct psc_journal *pj,
		       const struct psc_journal_enthdr *pje, uint32_t slot)
{
	struct psc_journal_shdw *pjs = pj->pj_shdw;
	struct psc_journal_shdw_tile *pjst;
	struct psc_journal_enthdr *pje_shdw;

	pjst = pjournal_getbyslot_pjst(pjs, slot);

	pje_shdw = (void *)((char *)pjst->pjst_base +
			 (PJ_PJESZ(pj) * (slot - pjst->pjst_sjent)));

	psc_assert(pje_shdw->pje_magic == 0);
	psc_assert(pje_shdw->pje_shdw_slot == slot);

	memcpy(pje_shdw, pje, PJ_PJESZ(pj));
	/* Finished with the slot.  Note this pjst may no longer be
	 *   'active'.
	 */
	psc_atomic32_dec(&pjst->pjst_ref);
}

void *
pjournal_shdwthr_main(__unusedx void *arg)
{
	struct psc_thread *thr = pscthr_get();
	struct psc_journal *pj = thr->pscthr_private;
	struct psc_journal_shdw *pjs = pj->pj_shdw;
	struct psc_journal_shdw_tile *pjst;
	struct timespec ctm, rtm, mtm = PJSHDW_MAXAGE;
	uint32_t i, j;

	while (1) {
		j = (pjs->pjs_curtile + 1) < pjs->pjs_ntiles ?
			(pjs->pjs_curtile + 1) : 0;

		/* Look for full tiles and process them.
		 */
		for (i=0; i < pjs->pjs_ntiles; i++) {
			pjst = pjs->pjs_pjsts[j];
			spinlock(&pjst->pjst_lock);

			if (pjst->pjst_state & PJSHDWT_PROCRDY) {
				/* Sanity.
				 */
				psc_assert(pjst->pjst_state ==
					   PJSHDWT_PROCRDY);
				/* Only quiet pjst's may be processed.
				 */
				if (psc_atomic32_read(&pjst->pjst_ref)) {
					psc_warnx("pjst@%p has ref=%d", pjst,
					   psc_atomic32_read(&pjst->pjst_ref));
					continue;
				}
				pjst->pjst_state = PJSHDWT_PROC;
				freelock(&pjst->pjst_lock);
				//XXX Call tile entry processor here
				pjournal_prep_pjst(pjst, pj, pjst->pjst_sjent);
			}

			if (j == (pjs->pjs_ntiles - 1))
				j = 0;
		 }
		/* Check the current tile to see if it has expired.
		 */
		clock_gettime(CLOCK_REALTIME, &ctm);
		spinlock(&pjs->pjs_lock);

		timespecsub(&ctm, &pjs->pjs_lastflush, &rtm);
		if (timespeccmp(&rtm, &mtm, >)) {
			/* If a tile is being forced out due to low
			 *   system activity then the proceeding tile
			 *   MUST be ready for use.
			 */
			pjournal_shdw_advtile_locked(pjs, 0);
			freelock(&pjs->pjs_lock);
			usleep((mtm.tv_sec * 1000000) +
			       ((mtm.tv_nsec * 1000000)/1000));
			//XXX Call tile entry processor here on pjst
		} else {
			freelock(&pjs->pjs_lock);
			usleep((rtm.tv_sec * 1000000) +
			       ((rtm.tv_nsec * 1000000)/1000));
		}
	}

	return (NULL);
}

__static void
pjournal_init_shdw(struct psc_journal *pj)
{
	struct psc_thread *thr;
	int i;

	psc_assert(pj->pj_hdr->pjh_options & PJF_SHADOW);
	psc_assert(!pj->pj_shdw);

	pj->pj_shdw = PSCALLOC(sizeof(struct psc_journal_shdw));
	pj->pj_shdw->pjs_ntiles = PJSHDW_DEFTILES;
	pj->pj_shdw->pjs_tilesize = PJSHDW_TILESIZE;
	pj->pj_shdw->pjs_pjents = 0;

	clock_gettime(CLOCK_REALTIME, &pj->pj_shdw->pjs_lastflush);
	LOCK_INIT(&pj->pj_shdw->pjs_lock);
	psc_waitq_init(&pj->pj_shdw->pjs_waitq);

	for (i=0; i < PJSHDW_DEFTILES; i++) {
		pj->pj_shdw->pjs_pjsts[i] = PSCALLOC(pj->pj_hdr->pjh_entsz *
						     pj->pj_shdw->pjs_tilesize);

		pjournal_prep_pjst(pj->pj_shdw->pjs_pjsts[i], pj,
				   (uint32_t)(i * pj->pj_shdw->pjs_tilesize));
	}

	thr = pscthr_init(JRNLTHRT_SHDW, 0, pjournal_shdwthr_main, NULL, 0,
			  "pjrnlshdw");
	psc_assert(thr);

	thr->pscthr_private = pj;
	pscthr_setready(thr);
}

/**
 * pjournal_replay - Replay all open transactions in a journal.
 * @pj: the journal.
 * @pj_handler: the master journal replay function.
 * Returns: 0 on success, -1 on error.
 */
struct psc_journal *
pjournal_replay(const char *fn, psc_jhandler pj_handler)
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
	psc_crc64_add(&chksum, pje,
	    offsetof(struct psc_journal_enthdr, pje_chksum));
	psc_crc64_add(&chksum, pje->pje_data, pje->pje_len);
	PSC_CRC64_FIN(&chksum);
	pje->pje_chksum = chksum;

	size = pwrite(pj->pj_fd, pje, PJ_PJESZ(pj),
		     (off_t)(pj->pj_hdr->pjh_start_off +
		     (pj->pj_nextwrite * PJ_PJESZ(pj))));
	if (size == -1 || (size_t)size != PJ_PJESZ(pj))
		psc_warnx("Fail to write a start up marker in the journal");
	psc_freenl(pje, PJ_PJESZ(pj));

	pj->pj_nextwrite++;
	if (pj->pj_nextwrite == pj->pj_hdr->pjh_nents)
		pj->pj_nextwrite = 0;

	/* pre-allocate some buffers for log writes */
	for (i = 0; i < MAX_NUM_PJBUF; i++) {
		pje = psc_alloc(PJ_PJESZ(pj), PAF_PAGEALIGN | PAF_LOCK);
		psc_dynarray_add(&pj->pj_bufs, pje);
	}

	if (pj->pj_hdr->pjh_options & PJF_SHADOW)
		pjournal_init_shdw(pj);

	psc_info("journal replayed: %d log entries with %d transactions "
	    "have been redone, error = %d", nents, ntrans, nerrs);
	return (pj);
}
