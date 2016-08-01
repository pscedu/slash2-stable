/* $Id$ */
/*
 * %ISC_START_LICENSE%
 * ---------------------------------------------------------------------
 * Copyright 2015-2016, Google, Inc.
 * Copyright 2007-2016, Pittsburgh Supercomputing Center
 * All rights reserved.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the
 * above copyright notice and this permission notice appear in all
 * copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL
 * WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS.  IN NO EVENT SHALL THE
 * AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
 * PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 * --------------------------------------------------------------------
 * %END_LICENSE%
 */

#include <sys/types.h>
#include <sys/stat.h>

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <string.h>
#include <unistd.h>

#include "pfl/alloc.h"
#include "pfl/atomic.h"
#include "pfl/crc.h"
#include "pfl/ctl.h"
#include "pfl/ctlsvr.h"
#include "pfl/dynarray.h"
#include "pfl/fcntl.h"
#include "pfl/journal.h"
#include "pfl/lock.h"
#include "pfl/opstats.h"
#include "pfl/pool.h"
#include "pfl/str.h"
#include "pfl/sys.h"
#include "pfl/thread.h"
#include "pfl/time.h"
#include "pfl/types.h"
#include "pfl/waitq.h"

#include "zfs-fuse/zfs_slashlib.h"

static long	total_trans;
static long	total_reserve;

struct psc_journalthr {
	struct psc_journal *pjt_pj;
};

__static uint32_t	pjournal_logwrite(struct psc_journal_xidhndl *,
			    int, struct psc_journal_enthdr *, int);

__static int		pjournal_logwrite_internal(struct psc_journal *,
			    struct psc_journal_enthdr *, uint32_t);

psc_spinlock_t		pjournal_count = SPINLOCK_INIT;
psc_spinlock_t		pjournal_reserve = SPINLOCK_INIT;

struct psc_waitq	pjournal_waitq = PSC_WAITQ_INIT("distill");
psc_spinlock_t		pjournal_waitqlock = SPINLOCK_INIT;
struct psc_lockedlist	pfl_journals = PLL_INIT(&pfl_journals,
			    struct psc_journal, pj_lentry);

#define JIO_READ	0
#define JIO_WRITE	1

#define psc_journal_read(pj, p, len, off)			\
	psc_journal_io((pj), (p), (len), (off), JIO_READ)

#define psc_journal_write(pj, p, len, off)			\
	psc_journal_io((pj), (p), (len), (off), JIO_WRITE)

struct psc_poolmaster	 pfl_xidhndl_poolmaster;
struct psc_poolmgr	*pfl_xidhndl_pool;

/*
 * Perform a low-level I/O operation on the journal store.
 * @pj: the journal.
 * @p: data.
 * @len: length of I/O.
 * @off: offset into backing store.
 * @rw: read or write.
 */
__static int
psc_journal_io(struct psc_journal *pj, void *p, size_t len, off_t off,
    int rw)
{
	struct timespec ts[2], wtime = { 0, 0 }, synctime;
	ssize_t nb;
	int rc;

	if (rw == JIO_READ)
		nb = pread(pj->pj_fd, p, len, off);
	else {
		PFL_GETTIMESPEC(&ts[0]);
		nb = pwrite(pj->pj_fd, p, len, off);
		PFL_GETTIMESPEC(&ts[1]);
		timespecsub(&ts[1], &ts[0], &wtime);
	}

	if (nb == -1) {
		rc = errno;
		psclog_error("journal %s (pj=%p, len=%zd, "
		    "off=%"PSCPRIdOFFT")",
		    rw == JIO_READ ? "read" : "write", pj, len, off);
	} else if ((size_t)nb != len) {
		/*
		 * At least on one instance, short write actually
		 * returns "success" on a RAM-backed file system.
		 */
		rc = ENOSPC;
		psclog_errorx("journal %s (pj=%p, len=%zd, "
		    "off=%"PSCPRIdOFFT", nb=%zd): short I/O",
		    rw == JIO_READ ? "read" : "write", pj, len,
		    off, nb);
	} else {
		rc = 0;
		pfl_opstat_add(rw == JIO_READ ?
		    pj->pj_iostats.rd : pj->pj_iostats.wr, nb);

		if (rw == JIO_WRITE) {
			PFL_GETTIMESPEC(&ts[0]);
			if (pj->pj_flags & PJF_ISBLKDEV) {
#ifdef HAVE_SYNC_FILE_RANGE
				rc = sync_file_range(pj->pj_fd, off,
				    len, SYNC_FILE_RANGE_WRITE |
				    SYNC_FILE_RANGE_WAIT_AFTER);
#else
				rc = fdatasync(pj->pj_fd);
#endif
			} else
				rc = fsync(pj->pj_fd);

			PFL_GETTIMESPEC(&ts[1]);
			timespecsub(&ts[1], &ts[0], &synctime);

			psclog_diag("wtime="PSCPRI_TIMESPEC" "
			    "synctime="PSCPRI_TIMESPEC,
			    PFLPRI_PTIMESPEC_ARGS(&wtime),
			    PFLPRI_PTIMESPEC_ARGS(&synctime));

			if (rc)
				psclog_error("sync_file_range failed "
				    "(len=%zd, off=%"PSCPRIdOFFT")",
				    len, off);
		}
	}
	return (rc);
}

void
pjournal_update_txg(struct psc_journal *pj, uint64_t txg)
{
	PJ_LOCK(pj);
	pj->pj_current_txg = txg;
	PJ_ULOCK(pj);
}

uint64_t
pjournal_next_replay(struct psc_journal *pj)
{
	uint64_t xid;

	PJ_LOCK(pj);
	xid = pj->pj_replay_xid;
	PJ_ULOCK(pj);

	return (xid);
}

uint64_t
pjournal_next_xid(struct psc_journal *pj)
{
	uint64_t xid;

	do {
		pj->pj_lastxid++;
	} while (pj->pj_lastxid == PJE_XID_NONE);
	xid = pj->pj_lastxid;

	return(xid);
}

__static void
pjournal_assign_xid(struct psc_journal *pj,
    struct psc_journal_xidhndl *xh, uint64_t txg)
{
	/*
	 * Note that even though we issue xids in increasing order here,
	 * it does not necessarily mean transactions will end up in the
	 * log in the same order.
	 */
	PJ_LOCK(pj);
	xh->pjx_xid = pjournal_next_xid(pj);

	/*
	 * Make sure that transactions appear on the distill list in
	 * strict order.  That way we can get accurate information about
	 * the lowest xid that has been distilled.
	 *
	 * If we add to the list after the transaction is written, the
	 * order may not be guaranteed, as mentioned above.
	 */
	if (xh->pjx_flags & PJX_DISTILL)
		pll_addtail(&pj->pj_distillxids, xh);

	/*
	 * Log entries written outside ZFS must be idempotent because
	 * the txg obtained this way may be one larger than the actual
	 * txg.
	 */
	if (!txg)
		xh->pjx_txg = pj->pj_current_txg;
	else
		xh->pjx_txg = txg;

	PJ_ULOCK(pj);
}

/*
 * Determine where to write the transaction's log.  Because we have
 * already reserved a slot for it, we can simply write at the next slot.
 */
__static void
pjournal_next_slot(struct psc_journal_xidhndl *xh)
{
	uint32_t slot, tail_slot;
	struct psc_journal_xidhndl *t;
	struct psc_journal *pj;

	tail_slot = PJX_SLOT_ANY;
	pj = xh->pjx_pj;

	PJ_LOCK(pj);

	slot = pj->pj_nextwrite;

	/* just a courtesy check, we rely on reservation */
	t = pll_peekhead(&pj->pj_pendingxids);
	if (t) {
		tail_slot = t->pjx_slot;
		psc_assert(tail_slot != slot);
	}

	/* Update the slot to be written by the next log entry */
	psc_assert(pj->pj_nextwrite < pj->pj_total);
	if (++pj->pj_nextwrite == pj->pj_total) {
		pj->pj_nextwrite = 0;
		pj->pj_wraparound++;
		OPSTAT_INCR("journal-wraparound");
	}
	psc_assert(pj->pj_resrv > 0);

	pj->pj_inuse++;
	xh->pjx_slot = slot;
	pll_addtail(&pj->pj_pendingxids, xh);

	psclog_info("writing a log entry xid=%#"PRIx64
	    ": slot=%d, next=%d, tail=%d",
	    xh->pjx_xid, xh->pjx_slot, pj->pj_nextwrite, tail_slot);

	PJ_ULOCK(pj);
}

/*
 * Start a new transaction with a unique ID in the given journal.
 * @pj: the owning journal.
 */
__static struct psc_journal_xidhndl *
pjournal_xnew(struct psc_journal *pj, int distill, uint64_t txg)
{
	struct psc_journal_xidhndl *xh;

	spinlock(&pjournal_count);
	psc_assert(++total_trans <= total_reserve);
	freelock(&pjournal_count);

	xh = psc_pool_get(pfl_xidhndl_pool);
	memset(xh, 0, sizeof(*xh));

	xh->pjx_pj = pj;
	INIT_SPINLOCK(&xh->pjx_lock);
	xh->pjx_flags = distill ? PJX_DISTILL : PJX_NONE;
	xh->pjx_slot = PJX_SLOT_ANY;
	INIT_PSC_LISTENTRY(&xh->pjx_lentry);
	INIT_PSC_LISTENTRY(&xh->pjx_pndg_lentry);
	INIT_PSC_LISTENTRY(&xh->pjx_dstl_lentry);
	pjournal_assign_xid(pj, xh, txg);

	psclog_info("New trans: xid=%#"PRIx64", txg=%#"PRIx64", distill=%d",
	    xh->pjx_xid, xh->pjx_txg, distill);
	return (xh);
}

void
pjournal_xdestroy(struct psc_journal_xidhndl *xh)
{
	psc_assert(psclist_disjoint(&xh->pjx_pndg_lentry));
	psc_assert(psclist_disjoint(&xh->pjx_dstl_lentry));
	psc_assert(xh->pjx_data == NULL);
	psc_pool_return(pfl_xidhndl_pool, xh);
}

int
pjournal_reserve_slot(struct psc_journal *pj, int count)
{
	int nwaits = 0;
	struct psc_journal_xidhndl *t;

	spinlock(&pjournal_count);
	total_reserve += count;
	freelock(&pjournal_count);

	spinlock(&pjournal_reserve);

	psc_assert(!(pj->pj_flags & PJF_REPLAYINPROG));
	while (count) {
		PJ_LOCK(pj);
		if (pj->pj_resrv + pj->pj_inuse < pj->pj_total) {
			count--;
			pj->pj_resrv++;
			PJ_ULOCK(pj);
			continue;
		}

		pj->pj_commit_txg = zfsslash2_return_synced();

		t = pll_peekhead(&pj->pj_pendingxids);
		if (!t) {
			/* this should never happen in practice */
			psclog_warnx("journal reservation blocked "
			    "on over-reservation: resrv=%d inuse=%d",
			    pj->pj_resrv, pj->pj_inuse);

			pfl_opstat_incr(pj->pj_opst_reserves);
			pj->pj_flags |= PJF_WANTSLOT;
			psc_waitq_wait(&pj->pj_waitq, &pj->pj_lock);
			continue;
		}
		spinlock(&t->pjx_lock);
		psc_assert(t->pjx_slot < pj->pj_total);
		if (t->pjx_txg > pj->pj_commit_txg) {
			uint64_t txg;

			psclog_warnx("journal reservation blocked "
			    "by transaction %p to be committed"
			    "(xid=%#"PRIx64", txg=%"PRId64", slot=%d)",
			    t, t->pjx_xid, t->pjx_txg, t->pjx_slot);

			pfl_opstat_incr(pj->pj_opst_commits);
			txg = t->pjx_txg;
			freelock(&t->pjx_lock);
			PJ_ULOCK(pj);
			zfsslash2_wait_synced(txg);
			nwaits++;
			continue;
		}
		if (t->pjx_flags & PJX_DISTILL) {
			psclog_warnx("Journal reservation blocked "
			    "by transaction %p to be distilled"
			    "(xid=%#"PRIx64", slot=%d, flags=%#x)",
			    t, t->pjx_xid, t->pjx_slot, t->pjx_flags);

			pfl_opstat_incr(pj->pj_opst_distills);
			freelock(&t->pjx_lock);
			PJ_ULOCK(pj);
			usleep(100);
			nwaits++;
			continue;
		}

		pll_remove(&pj->pj_pendingxids, t);
		freelock(&t->pjx_lock);
		pjournal_xdestroy(t);
		pj->pj_inuse--;
		PJ_ULOCK(pj);
	}
	freelock(&pjournal_reserve);
	return nwaits;
}

void
pjournal_unreserve_slot(struct psc_journal *pj, int count)
{
	PJ_LOCK(pj);
	psc_assert(pj->pj_resrv >= (uint32_t) count);
	pj->pj_resrv -= count;
	if (pj->pj_flags & PJF_WANTSLOT) {
		pj->pj_flags &= ~PJF_WANTSLOT;
		psc_waitq_wakeone(&pj->pj_waitq);
	}
	PJ_ULOCK(pj);
}

/*
 * Add a log entry into the journal.
 */
uint32_t
pjournal_add_entry(struct psc_journal *pj, uint64_t txg,
    int type, int distill, void *buf, int size)
{
	uint32_t slot;
	struct psc_journal_xidhndl *xh;
	struct psc_journal_enthdr *pje;

	xh = pjournal_xnew(pj, distill, txg);

	pje = DATA_2_PJE(buf);
	psc_assert(pje->pje_magic == PJE_MAGIC);

	slot = pjournal_logwrite(xh, type | PJE_NORMAL, pje, size);
	return (slot);
}

void *
pjournal_get_buf(struct psc_journal *pj, size_t size)
{
	struct psc_journal_enthdr *pje;

	psc_assert(size <= PJ_PJESZ(pj) -
	    offsetof(struct psc_journal_enthdr, pje_data));

	PJ_LOCK(pj);
	while (!psc_dynarray_len(&pj->pj_bufs)) {
		pj->pj_flags |= PJF_WANTBUF;
		psc_waitq_wait(&pj->pj_waitq, &pj->pj_lock);
		PJ_LOCK(pj);
	}
	pje = psc_dynarray_getpos(&pj->pj_bufs, 0);
	psc_dynarray_remove(&pj->pj_bufs, pje);
	psc_assert(pje->pje_magic == PJE_MAGIC);
	PJ_ULOCK(pj);
	return (PJE_DATA(pje));
}

void
pjournal_put_buf(struct psc_journal *pj, void *buf)
{
	struct psc_journal_enthdr *pje;

	pje = DATA_2_PJE(buf);
	psc_assert(pje->pje_magic == PJE_MAGIC);

	PJ_LOCK(pj);
	psc_dynarray_add(&pj->pj_bufs, pje);
	if (pj->pj_flags & PJF_WANTBUF) {
		pj->pj_flags &= ~PJF_WANTBUF;
		psc_waitq_wakeall(&pj->pj_waitq);
	}
	PJ_ULOCK(pj);
}

/*
 * Write a new log entry for a transaction.
 * @pj: the journal.
 * @pje: the log entry to be written.
 * @slot: the slot to contain the entry.
 * Returns: 0 on success, -1 on error.
 */
__static int
pjournal_logwrite_internal(struct psc_journal *pj,
    struct psc_journal_enthdr *pje, uint32_t slot)
{
	uint64_t chksum;
	int rc, ntries;

	/*
	 * Calculate the CRC checksum, excluding the checksum field
	 * itself.
	 */
	psc_crc64_init(&chksum);
	psc_crc64_add(&chksum, pje, offsetof(struct psc_journal_enthdr,
	    pje_chksum));
	psc_crc64_add(&chksum, pje->pje_data, pje->pje_len);
	psc_crc64_fini(&chksum);
	pje->pje_chksum = chksum;

	/* commit the log entry on disk before we can return */
	ntries = PJ_MAX_TRY;
	while (ntries > 0) {
		psclog_vdebug("io_start slot=%u", slot);
		rc = psc_journal_write(pj, pje, PJ_PJESZ(pj),
		    PJ_GETENTOFF(pj, slot));
		psclog_vdebug("io_done slot=%u (rc=%d)", slot, rc);
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
	if (rc)
		psc_fatalx("failed writing journal log entry at "
		    "slot %d, tries=%d: %s", slot, ntries,
		    strerror(rc));
	else
		DPRINTF_PJE(PLL_DEBUG, pje, "written successfully "
		    "slot=%d", slot);
	return (0);
}

/*
 * Store a new entry in a journal transaction.
 * @xh: the transaction to receive the log entry.
 * @type: the application-specific log entry type.
 * @data: the journal entry contents to store.
 * @size: size of the custom data
 */
__static uint32_t
pjournal_logwrite(struct psc_journal_xidhndl *xh, int type,
    struct psc_journal_enthdr *pje, int size)
{
	static psc_spinlock_t writelock = SPINLOCK_INIT;
	struct psc_journal *pj;

	pj = xh->pjx_pj;

	if (xh->pjx_flags & PJX_DISTILL)
		type |= PJE_DISTILL;

	/*
	 * Fill in the header of the log entry; its
	 * payload is already filled by our caller.
	 */
	pje->pje_type = type;
	pje->pje_len = size;
	pje->pje_xid = xh->pjx_xid;
	pje->pje_txg = xh->pjx_txg;

	/* paranoid: make sure that an earlier slot is written first. */
	spinlock(&writelock);
	pjournal_next_slot(xh);
	pjournal_logwrite_internal(pj, pje, xh->pjx_slot);
	freelock(&writelock);

	/*
	 * If this log entry needs further processing, hand it
	 * off to the distill thread.
	 */
	spinlock(&xh->pjx_lock);
	if (xh->pjx_flags & PJX_DISTILL) {
		xh->pjx_data = pje;
		xh->pjx_flags |= PJX_WRITTEN;
		freelock(&xh->pjx_lock);

		spinlock(&pjournal_waitqlock);
		psc_waitq_wakeall(&pjournal_waitq);
		freelock(&pjournal_waitqlock);
	} else {
		psc_assert(!xh->pjx_data);
		freelock(&xh->pjx_lock);
	}
	return (xh->pjx_slot);
}

__static void *
pjournal_alloc_buf(struct psc_journal *pj)
{
	return (psc_alloc(PJ_PJESZ(pj) * pj->pj_hdr->pjh_readsize,
	    PAF_PAGEALIGN | PAF_LOCK));
}

/*
 * Compare tranactions for use in sorting.
 */
__static int
pjournal_xid_cmp(const void *x, const void *y)
{
	const struct psc_journal_enthdr	* const *pa = x, *a = *pa;
	const struct psc_journal_enthdr	* const *pb = y, *b = *pb;
	int rc;

	rc = CMP(a->pje_xid, b->pje_xid);
	if (rc)
		return (rc);
	return (CMP(a->pje_txg, b->pje_txg));
}

/*
 * Accumulate all journal entries that need to be replayed in memory.
 * To reduce memory usage, we remove entries of closed transactions as
 * soon as we find them.
 */
__static int
pjournal_scan_slots(struct psc_journal *pj)
{
	int i, rc, count, nopen, nscan, nmagic, nchksum, nclose;
	struct psc_journal_enthdr *pje, *tmppje;
	uint64_t chksum, last_xid;
	unsigned char *jbuf;
	int32_t last_slot;
	uint32_t slot;

	rc = 0;
	last_xid = PJE_XID_NONE;
	last_slot = PJX_SLOT_ANY;
	slot = nopen = nscan = nmagic = nclose = nchksum = 0;

	/*
	 * We scan the log from the first physical entry to the last
	 * physical one regardless where the log really starts and ends.
	 */
	jbuf = pjournal_alloc_buf(pj);
	count = pj->pj_hdr->pjh_readsize;
	psc_assert((pj->pj_total % count) == 0);
	while (slot < pj->pj_total) {
		rc = psc_journal_read(pj, jbuf, PJ_PJESZ(pj) * count,
		    PJ_GETENTOFF(pj, slot));
		if (rc)
			break;
		for (i = 0; i < count; i++) {
			nscan++;
			pje = PSC_AGP(jbuf, PJ_PJESZ(pj) * i);
			if (pje->pje_magic != PJE_MAGIC) {
				nmagic++;
				psclog_warnx("journal %p: slot %d has "
				    "a bad magic number!", pj, slot + i);
				rc = -1;
				continue;
			}

			psc_crc64_init(&chksum);
			psc_crc64_add(&chksum, pje, offsetof(
			    struct psc_journal_enthdr, pje_chksum));
			psc_crc64_add(&chksum, pje->pje_data, pje->pje_len);
			psc_crc64_fini(&chksum);

			if (pje->pje_chksum != chksum) {
				psclog_warnx("journal %p: slot %d has "
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
			 * Well, this is only true if we write slots
			 * sequentially.
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
			if (pje->pje_xid <= pj->pj_replay_xid)
				continue;

			if (((pje->pje_type & PJE_DISTILL) == 0) &&
			    (pje->pje_txg <= pj->pj_commit_txg))
				continue;

			if (((pje->pje_type & PJE_DISTILL) != 0) &&
			    (pje->pje_txg <= pj->pj_commit_txg) &&
			    (pje->pje_xid <= pj->pj_distill_xid))
				continue;

			/* Okay, we need to keep this log entry for now. */
			tmppje = psc_alloc(PJ_PJESZ(pj), PAF_PAGEALIGN |
			    PAF_LOCK);
			memcpy(tmppje, pje, pje->pje_len +
			    offsetof(struct psc_journal_enthdr, pje_data));
			psc_dynarray_add(&pj->pj_bufs, tmppje);
			psclog_debug("tmppje=%p type=%hu xid=%"PRId64" "
			    "txg=%"PRId64,
			    tmppje, tmppje->pje_type, tmppje->pje_xid,
			    tmppje->pje_txg);
		}
		slot += count;
	}
 done:
	/*
	 * Our cursor file lives within ZFS while the system journal
	 * lives outside ZFS.  This is a hack for debugging convenience.
	 *
	 * 10/17/2012: Hit this after we create a new journal.
	 */
	if (last_xid < pj->pj_distill_xid) {
		psclog_warnx("system journal and cursor file mismatch!");
		last_xid = pj->pj_distill_xid;
	}

	pj->pj_lastxid = last_xid;
	psc_dynarray_sort(&pj->pj_bufs, qsort, pjournal_xid_cmp);
	psc_free(jbuf, PAF_LOCK | PAF_PAGEALIGN, PJ_PJESZ(pj) *
	    pj->pj_hdr->pjh_readsize);

	nopen = psc_dynarray_len(&pj->pj_bufs);
	psclog_info("journal scan statistics: closed=%d open=%d magic=%d "
	    "chksum=%d scan=%d last=%d total=%d",
	    nclose, nopen, nmagic, nchksum, nscan, last_slot,
	    pj->pj_total);
	psclog_info("last journal transaction ID found is %"PRId64,
	    pj->pj_lastxid);
	return (rc);
}

/*
 * Initialize the in-memory representation of a journal.
 * @fn: path to journal on file system.
 */
struct psc_journal *
pjournal_open(const char *name, const char *fn)
{
	struct psc_journal_hdr *pjh;
	struct psc_journal *pj;
	struct stat statbuf;
	const char *basefn;
	uint64_t chksum;
	ssize_t pjhlen;
	char type[LINE_MAX];
	int flags = 0;

	pj = PSCALLOC(sizeof(*pj));

	strlcpy(pj->pj_name, name, sizeof(pj->pj_name));
	strlcpy(pj->pj_devname, fn, sizeof(pj->pj_devname));

	pfl_getfstype(fn, type, sizeof(type));
	if (strcmp(type, "tmpfs"))
		flags |= O_DIRECT;

	pj->pj_fd = open(fn, O_RDWR | flags);
	if (pj->pj_fd == -1)
		psc_fatal("failed to open journal %s", fn);
	if (fstat(pj->pj_fd, &statbuf) == -1)
		psc_fatal("failed to stat journal %s", fn);
	basefn = strrchr(fn, '/');
	if (basefn)
		basefn++;
	else
		basefn = fn;
	pj->pj_iostats.rd = pfl_opstat_init("jrnlrd-%s", basefn);
	pj->pj_iostats.wr = pfl_opstat_init("jrnlwr-%s", basefn);

	pj->pj_opst_commits = pfl_opstat_init("jrnl.%s.commits",
	    basefn);
	pj->pj_opst_reserves = pfl_opstat_init("jrnl.%s.reserves",
	    basefn);
	pj->pj_opst_distills = pfl_opstat_init("jrnl.%s.distills",
	    basefn);

	/*
	 * O_DIRECT may impose alignment restrictions so align the
	 * buffer and perform I/O in multiples of file system block
	 * size.
	 */
	pjhlen = PSC_ALIGN(sizeof(*pjh), statbuf.st_blksize);
	pjh = psc_alloc(pjhlen, PAF_PAGEALIGN);
	if (psc_journal_read(pj, pjh, pjhlen, 0))
		psc_fatal("failed to read journal header");

	pj->pj_hdr = pjh;
	if (pjh->pjh_magic != PJH_MAGIC) {
		psclog_errorx("journal header has a bad magic number "
		    "%#"PRIx64, pjh->pjh_magic);
		goto error;
	}
	if (pjh->pjh_version != PJH_VERSION) {
		psclog_errorx("journal header has an invalid version "
		    "number %d", pjh->pjh_version);
		goto error;
	}

	psc_crc64_init(&chksum);
	psc_crc64_add(&chksum, pjh, offsetof(struct psc_journal_hdr,
	    pjh_chksum));
	psc_crc64_fini(&chksum);

	if (pjh->pjh_chksum != chksum) {
		psclog_errorx("journal header has an invalid checksum "
		    "disk:%"PSCPRIxCRC64" vs mem:%"PSCPRIxCRC64,
		    pjh->pjh_chksum, chksum);
		goto error;
	}

	if (S_ISBLK(statbuf.st_mode))
		pj->pj_flags |= PJF_ISBLKDEV;

	else if (statbuf.st_size !=
	    (off_t)(pjhlen + pjh->pjh_nents * PJ_PJESZ(pj))) {
		psclog_errorx("size of the journal log %"PSCPRIdOFFT" "
		    "does not match specs in its header",
		    statbuf.st_size);
		goto error;
	}

	/*
	 * The remaining two fields pj_lastxid and pj_nextwrite will be
	 * filled after log replay.
	 */
	INIT_SPINLOCK(&pj->pj_lock);
	INIT_LISTENTRY(&pj->pj_lentry);

	pj->pj_inuse = 0;
	pj->pj_resrv = 0;
	pj->pj_total = pj->pj_hdr->pjh_nents;

	pll_init(&pj->pj_pendingxids, struct psc_journal_xidhndl,
	    pjx_pndg_lentry, &pj->pj_lock);
	pll_init(&pj->pj_distillxids, struct psc_journal_xidhndl,
	    pjx_dstl_lentry, NULL);

	psc_waitq_init(&pj->pj_waitq, "journal");
	psc_dynarray_init(&pj->pj_bufs);

	pll_add(&pfl_journals, pj);

	psc_poolmaster_init(&pfl_xidhndl_poolmaster,
	    struct psc_journal_xidhndl, pjx_lentry, PPMF_AUTO, 4096,
	    4096, 0, NULL, "xidhndl");
	pfl_xidhndl_pool = psc_poolmaster_getmgr(
	    &pfl_xidhndl_poolmaster);

	return (pj);

 error:
	psc_free(pjh, PAF_LOCK | PAF_PAGEALIGN, pjhlen);
	PSCFREE(pj);
	return (NULL);
}

/*
 * Release resources associated with an in-memory journal.
 * @pj: journal to release.
 */
__static void
pjournal_release(struct psc_journal *pj)
{
	struct psc_journal_enthdr *pje;
	int n;

	DYNARRAY_FOREACH(pje, n, &pj->pj_bufs)
		psc_free(pje, PAF_LOCK | PAF_PAGEALIGN, PJ_PJESZ(pj));
	psc_dynarray_free(&pj->pj_bufs);
	psc_free(pj->pj_hdr, PAF_LOCK | PAF_PAGEALIGN,
	    pj->pj_hdr->pjh_iolen);
	PSCFREE(pj);
}

void
pjournal_thr_main(struct psc_thread *thr)
{
	static int once = 0;
	struct psc_journal_enthdr *pje;
	struct psc_journal_xidhndl *xh;
	struct psc_journalthr *pjt;
	struct psc_journal *pj;
	uint64_t xid, txg;
	int last_reclaimed = 1, reclaimed = 0;

	pjt = thr->pscthr_private;
	pj = pjt->pjt_pj;
	xid = pj->pj_distill_xid;
	while (pscthr_run(thr)) {
		/*
		 * Walk the list until we find a log entry that needs
		 * processing.  We also make sure that we distill in
		 * order of transaction IDs.
		 */
		while ((xh = pll_peekhead(&pj->pj_distillxids))) {
			psclog_debug("xh=%p xh->pjx_flags=%d", xh,
			    xh->pjx_flags);
			spinlock(&xh->pjx_lock);
			psc_assert(xh->pjx_flags & PJX_DISTILL);

			if (!(xh->pjx_flags & PJX_WRITTEN)) {
				freelock(&xh->pjx_lock);
				break;
			}
			pje = xh->pjx_data;
			xh->pjx_data = NULL;
			xh->pjx_flags &= ~PJX_WRITTEN;
			pll_remove(&pj->pj_distillxids, xh);
			freelock(&xh->pjx_lock);

			pj->pj_distill_handler(pje, 0, pj->pj_npeers, 0);

			PJ_LOCK(pj);
			txg = pj->pj_current_txg + 1;
			PJ_ULOCK(pj);

			/*
			 * Once we clear the distill flag, xh can be
			 * freed after its associated txg has been
			 * committed by the ZFS.
			 */
			spinlock(&xh->pjx_lock);
			psc_assert(xid < xh->pjx_xid);
			xid = xh->pjx_xid;

			if (txg > xh->pjx_txg)
				xh->pjx_txg = txg;

			xh->pjx_flags &= ~PJX_DISTILL;
			freelock(&xh->pjx_lock);

			pjournal_put_buf(pj, PJE_DATA(pje));

			/*
			 * The logic only works when the system is
			 * otherwise quiet.
			 */
			last_reclaimed = 1;
		}

		/*
		 * Free committed pending transactions to avoid hogging
		 * memory.
		 */
		PJ_LOCK(pj);

		if (pj->pj_distill_xid < xid)
			pj->pj_distill_xid = xid;

		txg = zfsslash2_return_synced();

		/*
		 * This is only possible if we receive a snapshot that
		 * overwrites the existing contents of a file system.
		 * And this should only happen once.
		 */
		if (pj->pj_commit_txg > txg) {
			once++;
			psc_assert(once <= 1);
			psclog_warnx("Journal txg goes backwards: "
			    "%"PRId64" -> %"PRId64,
			    pj->pj_commit_txg, txg);
		}
		pj->pj_commit_txg = txg;

		txg = 0;
		while ((xh = pll_peekhead(&pj->pj_pendingxids)) != NULL) {
			spinlock(&xh->pjx_lock);
			if (xh->pjx_txg > pj->pj_commit_txg) {
				freelock(&xh->pjx_lock);
				txg = xh->pjx_txg;
				break;
			}
			if (xh->pjx_flags & PJX_DISTILL) {
				freelock(&xh->pjx_lock);
				break;
			}
			reclaimed++;
			pll_remove(&pj->pj_pendingxids, xh);
			freelock(&xh->pjx_lock);
			pjournal_xdestroy(xh);
			psc_assert(pj->pj_inuse > 0);
			pj->pj_inuse--;
		}
		PJ_ULOCK(pj);
		/*
		 * Some of the log entries are written after
		 * the data they protect has been written to
		 * ZFS.  So their txg can be off by plus one.
		 * This code makes sure inuse drops to zero.
		 */
		if (txg && !last_reclaimed && !reclaimed)
			zfsslash2_wait_synced(txg);

		spinlock(&pjournal_waitqlock);
		if (pll_empty(&pj->pj_distillxids)) {
			last_reclaimed = reclaimed;
			reclaimed = 0;
			/* 30 seconds is the ZFS txg sync interval */
			psc_waitq_waitrel_s(&pjournal_waitq,
			    &pjournal_waitqlock, 30);
		} else
			freelock(&pjournal_waitqlock);
	}
}

/*
 * Replay all open transactions in a journal.
 * @pj: journal.
 * @thrtype: application-specified thread type ID for distill processor.
 * @thrname: application-specified thread name for distill processor.
 * @replay_handler: the journal replay callback.
 * @distill_handler: the distill processor callback.
 */
struct psc_thread *
pjournal_replay(struct psc_journal *pj, int thrtype,
    const char *thrname, psc_replay_handler_t replay_handler,
    psc_distill_handler_t distill_handler)
{
	int i, rc, len, nerrs = 0, nentries;
	struct psc_journal_enthdr *pje;
	struct psc_journalthr *pjt;
	struct psc_thread *thr;

	pj->pj_flags |= PJF_REPLAYINPROG;

	rc = pjournal_scan_slots(pj);
	if (rc) {
		rc = 0;
		nerrs++;
	}
	nentries = 0;
	len = psc_dynarray_len(&pj->pj_bufs);
	psclog_info("The total number of entries to be replayed is %d",
	    len);

	for (i = 0; i < len; i++) {
		pje = psc_dynarray_getpos(&pj->pj_bufs, i);

		nentries++;

		if (pje->pje_xid > pj->pj_distill_xid) {
			rc = distill_handler(pje, 0, pj->pj_npeers, 1);
			if (rc)
				nerrs++;
		}

		if (pje->pje_txg > pj->pj_commit_txg) {
			rc = replay_handler(pje);
			if (rc)
				nerrs++;
		}

		PJ_LOCK(pj);
		pj->pj_replay_xid = pje->pje_xid;
		PJ_ULOCK(pj);

		psc_free(pje, PAF_LOCK | PAF_PAGEALIGN, PJ_PJESZ(pj));
	}
	psc_dynarray_free(&pj->pj_bufs);

	/*
	 * Make the current replay in effect.  Otherwise, a crash may
	 * lose previous work.
	 */
	zfsslash2_wait_synced(0);

	psclog_info("journal replay statistics: replayed=%d errors=%d",
	    nentries, nerrs);

	/* always start at the first slot of the journal */
	pj->pj_nextwrite = 0;
	pj->pj_wraparound = 0;

	/* pre-allocate some buffers for log writes/distill */
	psc_dynarray_ensurelen(&pj->pj_bufs, PJ_MAX_BUF);

	for (i = 0; i < PJ_MAX_BUF; i++) {
		pje = psc_alloc(PJ_PJESZ(pj), PAF_PAGEALIGN | PAF_LOCK);
		pje->pje_magic = PJE_MAGIC;
		psc_dynarray_add(&pj->pj_bufs, pje);
	}

	pj->pj_distill_handler = distill_handler;

	thr = pscthr_init(thrtype, pjournal_thr_main, sizeof(*pjt), thrname);
	pjt = thr->pscthr_private;
	pjt->pjt_pj = pj;
	pscthr_setready(thr);

	pj->pj_flags &= ~PJF_REPLAYINPROG;
	return (thr);
}

/*
 * Respond to a "GETJOURNAL" control inquiry.
 * @fd: client socket descriptor.
 * @mh: already filled-in control message header.
 * @m: control message to be filled in and sent out.
 */
int
psc_ctlrep_getjournal(int fd, struct psc_ctlmsghdr *mh, void *m)
{
	struct psc_ctlmsg_journal *pcj = m;
	struct psc_journal *j;
	int rc = 1;

	PLL_LOCK(&pfl_journals);
	PLL_FOREACH(j, &pfl_journals) {
		PJ_LOCK(j);
		strlcpy(pcj->pcj_name, j->pj_name,
		    sizeof(pcj->pcj_name));
		pcj->pcj_flags		= j->pj_flags;
		pcj->pcj_inuse		= j->pj_inuse;
		pcj->pcj_total		= j->pj_total;
		pcj->pcj_resrv		= j->pj_resrv;
		pcj->pcj_lastxid	= j->pj_lastxid;
		pcj->pcj_commit_txg	= j->pj_commit_txg;
		pcj->pcj_replay_xid	= j->pj_replay_xid;
		pcj->pcj_dstl_xid	= j->pj_distill_xid;
		pcj->pcj_pndg_xids_cnt	= pll_nitems(&j->pj_pendingxids);
		pcj->pcj_dstl_xids_cnt	= pll_nitems(&j->pj_distillxids);
		pcj->pcj_bufs_cnt	= psc_dynarray_len(&j->pj_bufs);
		pcj->pcj_nwaiters	= psc_waitq_nwaiters(&j->pj_waitq);
		pcj->pcj_nextwrite	= j->pj_nextwrite;
		pcj->pcj_wraparound	= j->pj_wraparound;
		PJ_ULOCK(j);

		rc = psc_ctlmsg_sendv(fd, mh, pcj, NULL);
		if (!rc)
			break;
	}
	PLL_ULOCK(&pfl_journals);
	return (rc);
}

void
pfl_journal_register_ctlops(struct psc_ctlop *ops)
{
	struct psc_ctlop *op;

	op = &ops[PCMT_GETJOURNAL];
	op->pc_op = psc_ctlrep_getjournal;
	op->pc_siz = sizeof(struct psc_ctlmsg_journal);
}
