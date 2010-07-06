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

__static void		pjournal_logwrite(struct psc_journal_xidhndl *, int,
				struct psc_journal_enthdr *, int);

__static int		pjournal_logwrite_internal(struct psc_journal *,
				struct psc_journal_enthdr *, uint32_t);

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
		
		if (rw == JIO_WRITE) {
			rc = sync_file_range(pj->pj_fd, off, len, 0);
			if (rc)
				psc_error("sync_file_range failed (len=%zd, off=%"
					  PSCPRIdOFF")", len, off);
		}
	}
	return (rc);
}

uint64_t
pjournal_next_xid(struct psc_journal *pj)
{
	uint64_t xid;
	/*
	 * Note that even though we issue xids in increasing order here,
	 * it does not necessarily mean transactions will end up in the
	 * log in the same order.
	 */
	PJ_LOCK(pj);
	do {
		xid = ++pj->pj_lastxid;
	} while (xid == PJE_XID_NONE);
	PJ_ULOCK(pj);

	return (xid);
}

void
pjournal_next_slot(struct psc_journal_xidhndl *xh)
{
	int freed;
	uint32_t slot, tail_slot;
	struct psc_journal_xidhndl *t;
	struct psc_journal *pj;

	tail_slot = PJX_SLOT_ANY;
	pj = xh->pjx_pj;
retry:
	/*
	 * Remove completed transactions off the pending list.
	 */
	PJ_LOCK(pj);
	freed = 1;
	while (freed && (t = pll_gethdpeek(&pj->pj_pendingxids))) {
		freed = 0;
		spinlock(&t->pjx_lock);
		if ((t->pjx_txg < xh->pjx_txg) && ((t->pjx_flags & PJX_DISTILL) == 0)) {
			pll_remove(&pj->pj_pendingxids, t);
			freed = 1;
		}
		freelock(&t->pjx_lock);
	}

	/*
	 * Make sure that the next slot to be written is not used by a
	 * pending transaction.  Since we add a new transaction at the
	 * tail of the pending transaction list, we only need to check
	 * the head of the list to find out the oldest transaction.
	 */
	slot = pj->pj_nextwrite;
	t = pll_gethdpeek(&pj->pj_pendingxids);
	if (t) {
		if (t->pjx_slot == slot) {
			psc_warnx("Journal %p write is blocked on slot %d "
			  "owned by transaction %p (xid = %"PRIx64", flags = 0x%x)",
			  pj, pj->pj_nextwrite, t, t->pjx_xid, t->pjx_flags);
			pj->pj_flags |= PJF_WANTSLOT;
			psc_waitq_wait(&pj->pj_waitq, &pj->pj_lock);
			goto retry;
		}
		tail_slot = t->pjx_slot;
	}

	/* Update the next slot to be written by a next log entry */
	psc_assert(pj->pj_nextwrite < pj->pj_hdr->pjh_nents);
	if ((++pj->pj_nextwrite) == pj->pj_hdr->pjh_nents)
		pj->pj_nextwrite = 0;

	xh->pjx_slot = slot;
	pll_addtail(&pj->pj_pendingxids, xh);

	PJ_ULOCK(pj);

	psc_info("Writing a log entry xid=%"PRIx64
	    ": slot = %d, tail_slot = %d",
	    xh->pjx_xid, xh->pjx_slot, tail_slot);
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
	xh->pjx_slot = PJX_SLOT_ANY;
	INIT_PSCLIST_ENTRY(&xh->pjx_lentry1);
	INIT_PSCLIST_ENTRY(&xh->pjx_lentry2);
	xh->pjx_xid = pjournal_next_xid(pj);

	psc_info("starting a new transaction %p (xid = %"PRIx64") in "
	    "journal %p", xh, xh->pjx_xid, pj);
	return (xh);
}

/**
 * pjournal_add_entry - Add a log entry into the journal.
 */
void
pjournal_add_entry(struct psc_journal *pj, uint64_t txg, int type, void *buf, int size)
{
	struct psc_journal_xidhndl *xh;
	struct psc_journal_enthdr *pje;

	xh = pjournal_xnew(pj);
	xh->pjx_txg = txg;
	xh->pjx_flags = PJX_NONE;
	pje = (struct psc_journal_enthdr *)(buf - offsetof(struct psc_journal_enthdr, pje_data));
	psc_assert(pje->pje_magic == PJE_MAGIC);

	pjournal_logwrite(xh, type|PJE_NORMAL, pje, size);
}

/**
 * pjournal_add_entry_distill - Add a log entry into the journal.  The log entry needs to be
 *     distilled.
 */
void
pjournal_add_entry_distill(struct psc_journal *pj, uint64_t txg, int type, void *buf, int size)
{
	struct psc_journal_xidhndl *xh;
	struct psc_journal_enthdr *pje;

	xh = pjournal_xnew(pj);
	xh->pjx_txg = txg;
	xh->pjx_flags = PJX_DISTILL;
	pje = (struct psc_journal_enthdr *)(buf - offsetof(struct psc_journal_enthdr, pje_data));
	psc_assert(pje->pje_magic == PJE_MAGIC);

	pjournal_logwrite(xh, type|PJE_NORMAL|PJE_DISTILL, pje, size);
}

void *
pjournal_get_buf(struct psc_journal *pj, size_t size)
{
	struct psc_journal_enthdr *pje;

	psc_assert(size <= PJ_PJESZ(pj) - offsetof(struct psc_journal_enthdr, pje_data));

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
	pje->pje_magic = PJE_MAGIC;
	return ((void *)((char *)pje + offsetof(struct psc_journal_enthdr, pje_data)));
}

void
pjournal_put_buf(struct psc_journal *pj, void *buf)
{
	struct psc_journal_enthdr *pje;

	pje = (struct psc_journal_enthdr *)((char *)buf -
	    offsetof(struct psc_journal_enthdr, pje_data));
	psc_assert(pje->pje_magic == PJE_MAGIC);

	PJ_LOCK(pj);
	psc_dynarray_add(&pj->pj_bufs, pje);
	if (pj->pj_flags & PJF_WANTBUF) {
		pj->pj_flags &= ~PJF_WANTBUF;
		psc_waitq_wakeall(&pj->pj_waitq);
	}
	PJ_ULOCK(pj);
}

/**
 * pjournal_logwrite_internal - Write a new log entry for a transaction.
 * @pj: the journal.
 * @pje: the log entry to be written.
 * @slot: the slot to contain the entry.
 * Returns: 0 on success, -1 on error.
 */
__static int
pjournal_logwrite_internal(struct psc_journal *pj, struct psc_journal_enthdr *pje, uint32_t slot)
{
	int				 rc;
	int				 ntries;
	uint64_t			 chksum;

	/* calculating the CRC checksum, excluding the checksum field itself */
	PSC_CRC64_INIT(&chksum);
	psc_crc64_add(&chksum, pje, offsetof(struct psc_journal_enthdr, pje_chksum));
	psc_crc64_add(&chksum, pje->pje_data, pje->pje_len);
	PSC_CRC64_FIN(&chksum);
	pje->pje_chksum = chksum;

	/* commit the log entry on disk before we can return */
	ntries = PJ_MAX_TRY;
	while (ntries > 0) {
		psc_dbg("io_start slot=%u", slot);
		rc = psc_journal_write(pj, pje, PJ_PJESZ(pj),
		    PJ_GETENTOFF(pj, slot));
		psc_dbg("io_done slot=%u (rc=%d)", slot, rc);
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
	return (0);
}


/**
 * pjournal_logwrite - store a new entry in a journal transaction.
 * @xh: the transaction to receive the log entry.
 * @type: the application-specific log entry type.
 * @data: the journal entry contents to store.
 * @size: size of the custom data
 */
__static void
pjournal_logwrite(struct psc_journal_xidhndl *xh, int type,
    struct psc_journal_enthdr *pje, int size)
{
	struct psc_journal		*pj;

	pj = xh->pjx_pj;
	xh->pjx_data = (void *)pje;

	/*
	 * Fill in the header of the log entry, its
	 * payload is already filled by our caller.
	 */
	pje->pje_type = type;
	pje->pje_len = size;
	pje->pje_xid = xh->pjx_xid;
	pje->pje_txg = xh->pjx_txg;

	pjournal_next_slot(xh);

	pjournal_logwrite_internal(pj, pje, xh->pjx_slot);

	/*
	 * If this log entry needs further processing, hand it
	 * off to the distill thread.
	 */
	if (xh->pjx_flags & PJX_DISTILL) {
		pll_addtail(&pj->pj_distillxids, xh);
		spinlock(&pjournal_waitqlock);
		psc_waitq_wakeall(&pjournal_waitq);
		freelock(&pjournal_waitqlock);
	}
}

__static void *
pjournal_alloc_buf(struct psc_journal *pj)
{
	return (psc_alloc(PJ_PJESZ(pj) * pj->pj_hdr->pjh_readahead,
	    PAF_PAGEALIGN | PAF_LOCK));
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
	return (CMP(a->pje_txg, b->pje_txg));
}

/*
 * pjournal_scan_slots - Accumulate all journal entries that need to be
 *	replayed in memory.  To reduce memory usage, we remove those
 *	entries of closed transactions as soon as we find them.
 */
__static int
pjournal_scan_slots(struct psc_journal *pj)
{
	int				 i, rc;
	struct psc_journal_enthdr	*pje;
	uint32_t			 slot;
	unsigned char			*jbuf;
	int				 nclose;
	struct psc_journal_enthdr	*tmppje;
	uint64_t			 chksum;
	int				 nchksum;
	uint64_t			 last_xid;
	int32_t				 last_slot;
	uint64_t			 last_startup;
	int				 count, nopen, nscan, nmagic;

	rc = 0;
	last_xid = PJE_XID_NONE;
	last_slot = PJX_SLOT_ANY;
	last_startup = PJE_XID_NONE;
	slot = nopen = nscan = nmagic = nclose = nchksum = 0;

	/*
	 * We scan the log from the first physical entry to the last physical
	 * one regardless where the log really starts and ends.
	 */
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
			psc_assert((pje->pje_type & PJE_FORMAT) ||
				   (pje->pje_type & PJE_NORMAL));

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
			if (((pje->pje_type & PJE_DISTILL) == 0) && 
                            (pje->pje_txg <= pj->pj_commit_txg))
				continue;

			if (((pje->pje_type & PJE_DISTILL) != 0) && 
                            (pje->pje_txg <= pj->pj_commit_txg) &&
			    (pje->pje_xid <= pj->pj_distill_xid))
				continue;

			/* Okay, we need to keep this log entry for now.  */
			tmppje = psc_alloc(PJ_PJESZ(pj), PAF_PAGEALIGN|PAF_LOCK);
			memcpy(tmppje, &jbuf[PJ_PJESZ(pj) * i], sizeof(*tmppje));
			psc_dynarray_add(&pj->pj_bufs, tmppje);
		}
		slot += count;
	}
 done:

	pj->pj_lastxid = last_xid;
	/* If last_slot is PJX_SLOT_ANY, then nextwrite will be 0 */
	pj->pj_nextwrite = (last_slot == (int)pj->pj_hdr->pjh_nents - 1) ?
	    0 : (last_slot + 1);
	qsort(pj->pj_bufs.da_items, pj->pj_bufs.da_pos,
	    sizeof(void *), pjournal_xid_cmp);
	psc_freenl(jbuf, PJ_PJESZ(pj) * pj->pj_hdr->pjh_readahead);

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
	pj->pj_fd = open(fn, O_RDWR | O_DIRECT);
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

	if (!S_ISBLK(statbuf.st_mode) &&
	    (statbuf.st_size != (off_t)(pjhlen + pjh->pjh_nents * PJ_PJESZ(pj)))) {
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
				    "txg %"PRIx64" xid %"PRIx64"\n",
				    slot + i, pje->pje_type,
				    pje->pje_xid, pje->pje_txg);
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
	char *buf;
	struct psc_journalthr *pjt = thr->pscthr_private;
	struct psc_journal *pj = pjt->pjt_pj;
	struct psc_journal_enthdr *pje;
	struct psc_journal_xidhndl *xh;

	while (pscthr_run()) {
		/*
		 * Walk the list until we find a log entry that needs processing.
		 */
		while ((xh = pll_get(&pj->pj_distillxids))) {

			psc_assert(xh->pjx_flags & PJX_DISTILL);
			pje = (struct psc_journal_enthdr *) xh->pjx_data;

			(pj->pj_distill_handler)(pje, PJ_PJESZ(pj));

			spinlock(&xh->pjx_lock);
			xh->pjx_flags &= ~PJX_DISTILL;
			freelock(&xh->pjx_lock);

			buf = (char *)pje + offsetof(struct psc_journal_enthdr, pje_data);
			pjournal_put_buf(pj, buf);
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
 * @embed_handler: the journal embed callback.
 * @replay_handler: the journal replay callback.
 * @distill_handler: the distill processor callback.
 */
struct psc_journal *
pjournal_init(const char *fn, uint64_t txg,
    int thrtype, const char *thrname,
    psc_replay_handler replay_handler,
    psc_distill_handler distill_handler)
{
	int				 i, rc;
	struct psc_journal		*pj;
	struct psc_journal_enthdr	*pje;
	int				 nerrs=0;
	int				 nentries;
	struct psc_journalthr		*pjt;
	struct psc_thread		*thr;

	psc_notify("First ZFS transaction group number is %"PRId64, txg);
	psc_notify("Journal device %s", fn);

	pj = pjournal_open(fn);
	if (pj == NULL)
		return (NULL);

	pj->pj_commit_txg = txg;
	/*
	 * Figure out the last XID whose log entry has been distilled
	 * if need be.  A naive way is to simply maintain a log named
	 * distill.log.
	 */
	pj->pj_distill_xid = 0;

	rc = pjournal_scan_slots(pj);
	if (rc) {
		rc = 0;
		nerrs++;
	}
	nentries = 0;
	while (psc_dynarray_len(&pj->pj_bufs)) {
		pje = psc_dynarray_getpos(&pj->pj_bufs, 0);
		psc_dynarray_remove(&pj->pj_bufs, pje);
		nentries++;
		replay_handler(pje, &rc);
		if (rc) {
			nerrs++;
			rc = 0;
		}
		psc_freenl(pje, PJ_PJESZ(pj));
	}
	psc_assert(!psc_dynarray_len(&pj->pj_bufs));
	psc_dynarray_free(&pj->pj_bufs);

	/* start at the first slot of the journal */
	pj->pj_nextwrite = 0;

	/* pre-allocate some buffers for log writes/distill */
	for (i = 0; i < PJ_MAX_BUF; i++) {
		pje = psc_alloc(PJ_PJESZ(pj), PAF_PAGEALIGN | PAF_LOCK);
		psc_dynarray_add(&pj->pj_bufs, pje);
	}

	if (distill_handler) {
		pj->pj_distill_handler = distill_handler;

		thr = pscthr_init(thrtype, 0, pjournal_distillthr_main,
			  NULL, sizeof(*pjt), thrname);

		pjt = thr->pscthr_private;
		pjt->pjt_pj = pj;
		pscthr_setready(thr);
	}

	psc_info("journal replayed: %d log entries "
	    "have been redone, # of errors = %d", nentries, nerrs);
	return (pj);
}
