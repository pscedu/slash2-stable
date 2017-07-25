/* $Id$ */
/*
 * %ISC_START_LICENSE%
 * ---------------------------------------------------------------------
 * Copyright 2015-2016, Google, Inc.
 * Copyright 2009-2016, Pittsburgh Supercomputing Center
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

/*
 * On disk table: persistent storage backend for in-memory data
 * structures.
 */

#include <sys/stat.h>
#include <sys/types.h>
#include <sys/mman.h>

#include <fcntl.h>
#include <inttypes.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>

#include "pfl/alloc.h"
#include "pfl/cdefs.h"
#include "pfl/opstats.h"
#include "pfl/lock.h"
#include "pfl/lockedlist.h"
#include "pfl/log.h"
#include "pfl/meter.h"
#include "pfl/odtable.h"
#include "pfl/str.h"
#include "pfl/types.h"

struct psc_lockedlist	 pfl_odtables =
    PLL_INIT(&pfl_odtables, struct pfl_odt, odt_lentry);

static void *pfl_odt_zerobuf;

static void
pfl_odt_zerobuf_ensurelen(size_t len)
{
	static psc_spinlock_t zerobuf_lock = SPINLOCK_INIT;
	static size_t zerobuf_len;

	if (len <= zerobuf_len)
		return;

	spinlock(&zerobuf_lock);
	if (len > zerobuf_len) {
		pfl_odt_zerobuf = psc_realloc(pfl_odt_zerobuf, len, 0);
		zerobuf_len = len;
	}
	freelock(&zerobuf_lock);
}

#define PACK_IOV(p, len)						\
	do {								\
		iov[nio].iov_base = (void *)(p);			\
		iov[nio].iov_len = (len);				\
		expect += (len);					\
		nio++;							\
	} while (0)


int
pfl_odt_new(struct pfl_odt *t, const char *fn, int overwrite)
{
	struct pfl_odt_hdr *h;

	h = t->odt_hdr;
	t->odt_fd = open(fn, O_CREAT | O_RDWR |
	    (overwrite ? O_TRUNC : O_EXCL), 0600);
	if (t->odt_fd == -1) {
		psclog_warn("open %s", fn);
		return (-1);
	}
	if (pwrite(t->odt_fd, h, sizeof(*h), 0) != sizeof(*h)) {
		psclog_warn("write %s", fn);
		return (-1);
	}
	return (0);
}

void
pfl_odt_open(struct pfl_odt *t, const char *fn, int oflg)
{
	struct pfl_odt_hdr *h;
	int prot = PROT_READ;
	ssize_t rc;

	h = t->odt_hdr;
	t->odt_fd = open(fn, oflg & ODTBL_FLG_RDONLY ?
	    O_RDONLY : O_RDWR, 0600);
	if (t->odt_fd == -1)
		PFLOG_ODT(PLL_FATAL, t, "open %s: error=%d", fn, errno);
	rc = pread(t->odt_fd, h, sizeof(*h), 0);
	if (rc != sizeof(*h))
		PFLOG_ODT(PLL_FATAL, t, "read %s: rc=%zd error=%d", fn,
		    rc, errno);
	if ((oflg & ODTBL_FLG_RDONLY) == 0)
		prot |= PROT_WRITE;
}

void
pfl_odt_close(struct pfl_odt *t)
{
	if (t->odt_fd != -1 && close(t->odt_fd) == -1)
		PFLOG_ODT(PLL_ERROR, t, "close: rc=%d %d", errno, t->odt_fd);
}

void
pfl_odt_write(struct pfl_odt *t, const void *p,
    struct pfl_odt_slotftr *f, int64_t item)
{
	ssize_t expect = 0;
	struct pfl_odt_hdr *h;
	struct iovec iov[3];
	ssize_t rc, pad;
	int nio = 0;
	off_t off;

	memset(iov, 0, sizeof(iov));

	h = t->odt_hdr;

	pad = h->odth_slotsz - h->odth_itemsz - sizeof(*f);
	psc_assert(!pad);

	pfl_odt_zerobuf_ensurelen(pad);

	off = item * h->odth_slotsz + h->odth_start;

	if (p)
		PACK_IOV(p, h->odth_itemsz);
	else
		off += h->odth_itemsz;

	if (p && f)
		PACK_IOV(pfl_odt_zerobuf, pad);
	else
		off += pad;

	if (f)
		PACK_IOV(f, sizeof(*f));

	rc = pwritev(t->odt_fd, iov, nio, off);
	psc_assert(rc == expect);
}

void
pfl_odt_read(struct pfl_odt *t, int64_t n,
    void *p, struct pfl_odt_slotftr *f)
{
	ssize_t expect = 0;
	struct pfl_odt_hdr *h;
	struct iovec iov[3];
	ssize_t rc, pad;
	int nio = 0;
	off_t off;

	memset(iov, 0, sizeof(iov));

	h = t->odt_hdr;
	pad = h->odth_slotsz - h->odth_itemsz - sizeof(*f);
	psc_assert(!pad);

	pfl_odt_zerobuf_ensurelen(pad);

	off = h->odth_start + n * h->odth_slotsz;

	if (p)
		PACK_IOV(p, h->odth_itemsz);
	else
		off += h->odth_itemsz;

	if (p && f)
		PACK_IOV(pfl_odt_zerobuf, pad);
	else
		off += pad;

	if (f)
		PACK_IOV(f, sizeof(*f));

	rc = preadv(t->odt_fd, iov, nio, off);
	psc_assert(rc == expect);
}

/* See also slm_odtops */
struct pfl_odt_ops pfl_odtops = {
	pfl_odt_new,		/* odtop_new() */
	pfl_odt_open,		/* odtop_open() */
	pfl_odt_read,		/* odtop_read() */
	pfl_odt_write,		/* odtop_write() */
	NULL,			/* odtop_resize() */
	pfl_odt_close		/* odtop_close() */
};

void
_pfl_odt_doput(struct pfl_odt *t, int64_t item, 
    const void *p, struct pfl_odt_slotftr *f, int inuse)
{
	struct pfl_odt_hdr *h;

	h = t->odt_hdr;

	f->odtf_flags = inuse ? ODT_FTRF_INUSE : 0;
	f->odtf_slotno = item;
	psc_crc64_init(&f->odtf_crc);
	if (inuse) {
		psc_assert(p);
		psc_crc64_add(&f->odtf_crc, p, h->odth_itemsz);
	}
	psc_crc64_add(&f->odtf_crc, f, sizeof(*f) - sizeof(f->odtf_crc));
	psc_crc64_fini(&f->odtf_crc);

	/* pfl_odt_write() and slm_odt_write() */
	t->odt_ops.odtop_write(t, p, f, item);

	pfl_opstat_add(t->odt_iostats.wr, h->odth_slotsz);

	PFLOG_ODT(PLL_DIAG, t,
	    "slot=%"PRId64" item crc=%"PSCPRIxCRC64" ",
	    item, f->odtf_crc);

	ODT_STAT_INCR(t, write);
}

size_t
pfl_odt_allocslot(struct pfl_odt *t)
{
	struct pfl_odt_hdr *h;
	size_t item; 

	h = t->odt_hdr;
	spinlock(&t->odt_lock);
	if (psc_vbitmap_next(t->odt_bitmap, &item) <= 0) {
		ODT_STAT_INCR(t, full);
		freelock(&t->odt_lock);
		return (-1);
	}
	if (item >= h->odth_nitems) {
		ODT_STAT_INCR(t, extend);
		OPSTAT_INCR("pfl.odtable-resize");
		/*
		 * psc_vbitmap_next() has enlarged the bitmap. Update
		 * the number of items accordingly and write to the
		 * disk.
		 */
		h->odth_nitems = psc_vbitmap_getsize(t->odt_bitmap);

		t->odt_ops.odtop_resize(t);	/* slm_odt_resize() */
		PFLOG_ODT(PLL_WARN, t,
		    "odtable now has %u items (used to be %zd)",
		    h->odth_nitems, item);
	}
	freelock(&t->odt_lock);
	return (item);
}

void
pfl_odt_allocitem(struct pfl_odt *t, void **pp)
{
	struct pfl_odt_hdr *h;
	void **p = (void **)pp;

	h = t->odt_hdr;
	*p = PSCALLOC(h->odth_itemsz);
}

/*
 * Store an item into an odtable.
 */
void
pfl_odt_putitem(struct pfl_odt *t, int64_t item, void *p, int inuse)
{
	struct pfl_odt_slotftr f;

	_pfl_odt_doput(t, item, p, &f, inuse);
}

void
pfl_odt_getslot(struct pfl_odt *t, int64_t n,
    void *pp, struct pfl_odt_slotftr **fp)
{
	struct pfl_odt_hdr *h;
	void **p = (void **)pp;

	h = t->odt_hdr;
	psc_assert(n <= h->odth_nitems - 1);

	if (p)
		*p = PSCALLOC(h->odth_itemsz);
	if (fp)
		*fp = PSCALLOC(sizeof(**fp));
		
	/* pfl_odt_read or slm_odt_read */
	t->odt_ops.odtop_read(t, n, p ? *p : NULL, fp ? *fp : NULL);

	pfl_opstat_add(t->odt_iostats.rd, h->odth_slotsz);

	ODT_STAT_INCR(t, read);
}

void
pfl_odt_replaceitem(struct pfl_odt *t, int64_t item,
    void *p)
{
	struct pfl_odt_slotftr f;

	_pfl_odt_doput(t, item, p, &f, 1);

	PFLOG_ODT(PLL_DIAG, t, "slot=%"PRId64, item);

	ODT_STAT_INCR(t, replace);
}

/*
 * Free the odtable slot which corresponds to the provided receipt.
 * Note: r is freed here.
 */
void
pfl_odt_freeitem(struct pfl_odt *t, int64_t item)
{
	struct pfl_odt_slotftr f;

	_pfl_odt_doput(t, item, NULL, &f, 0);

	spinlock(&t->odt_lock);
	psc_vbitmap_unset(t->odt_bitmap, item);
	freelock(&t->odt_lock);

	PFLOG_ODT(PLL_DIAG, t, "slot=%"PRId64, item);

	ODT_STAT_INCR(t, free);
}

int
pfl_odt_create(const char *fn, int64_t nitems, size_t itemsz,
    int overwrite, size_t startoff, size_t pad, int tflg)
{
	int rc;
	int64_t	item;
	struct pfl_odt_slotftr f;
	struct pfl_odt_hdr *h;
	struct pfl_odt *t;

	t = PSCALLOC(sizeof(*t));
	t->odt_ops = pfl_odtops;
	INIT_SPINLOCK(&t->odt_lock);
	snprintf(t->odt_name, sizeof(t->odt_name), "%s", pfl_basename(fn));

	t->odt_iostats.rd = pfl_opstat_init("odt-%s-rd", t->odt_name);
	t->odt_iostats.wr = pfl_opstat_init("odt-%s-wr", t->odt_name);

	h = PSCALLOC(sizeof(*h));
	memset(h, 0, sizeof(*h));
	h->odth_nitems = nitems;
	h->odth_itemsz = itemsz;
	h->odth_slotsz = itemsz + pad + sizeof(f);
	h->odth_options = tflg;
	h->odth_start = startoff;
	t->odt_hdr = h;
	psc_crc64_calc(&h->odth_crc, h, sizeof(*h) - sizeof(h->odth_crc));

	/* pfl_odt_new() and slm_odt_new() */
	rc = t->odt_ops.odtop_new(t, fn, overwrite);
	if (rc)
		return (rc);

	for (item = 0; item < nitems; item++)
		_pfl_odt_doput(t, item, NULL, &f, 0);

	PFLOG_ODT(PLL_DIAG, t, "created");

	pfl_odt_release(t);
	return (0);
}

void
pfl_odt_load(struct pfl_odt **tp, struct pfl_odt_ops *odtops, int oflg,
    const char *fn, const char *fmt, ...)
{
	struct pfl_odt_hdr *h;
	struct pfl_odt *t;
	uint64_t crc;
	va_list ap;

	*tp = t = PSCALLOC(sizeof(*t));
	t->odt_ops = *odtops;
	INIT_SPINLOCK(&t->odt_lock);
	INIT_PSC_LISTENTRY(&t->odt_lentry);

	va_start(ap, fmt);
	vsnprintf(t->odt_name, sizeof(t->odt_name), fmt, ap);
	va_end(ap);

	t->odt_iostats.rd = pfl_opstat_init("odt-%s-rd", t->odt_name);
	t->odt_iostats.wr = pfl_opstat_init("odt-%s-wr", t->odt_name);

	h = t->odt_hdr = PSCALLOC(sizeof(*h));

	/* pfl_odt_open() and slm_odt_open() */
	odtops->odtop_open(t, fn, oflg);

	psc_crc64_calc(&crc, t->odt_hdr, sizeof(*t->odt_hdr) -
	    sizeof(t->odt_hdr->odth_crc));
	psc_assert(h->odth_crc == crc);

	t->odt_bitmap = psc_vbitmap_newf(h->odth_nitems, PVBF_AUTO);
	psc_assert(t->odt_bitmap);
	/*
 	 * Skip the first slot, so that we can detect whether we have
 	 * assigned a lease easily.
 	 */
	psc_vbitmap_set(t->odt_bitmap, 0);

	PFLOG_ODT(PLL_DIAG, t, "loaded");

	pll_add(&pfl_odtables, t);
}

void
pfl_odt_check(struct pfl_odt *t,
    void (*cbf)(void *, int64_t, void *), void *arg)
{
	struct pfl_odt_slotftr *f;
	struct pfl_odt_hdr *h;
	struct pfl_meter mtr;
	uint64_t crc;
	void *p;

	h = t->odt_hdr;

	pfl_meter_init(&mtr, 0, "odt-%s", t->odt_name);
	mtr.pm_max = h->odth_nitems;

#define i mtr.pm_cur
	for (i = 0; i < h->odth_nitems; i++) {
		pfl_odt_getslot(t, i, &p, &f);

		if (f->odtf_slotno != i)
			PFLOG_ODT(PLL_FATAL, t, "footercheck: i = %lu", i);

		if (h->odth_options & ODTBL_OPT_CRC) {
			psc_crc64_init(&crc);
			if (f->odtf_flags & ODT_FTRF_INUSE)
				psc_crc64_add(&crc, p, h->odth_itemsz);
			psc_crc64_add(&crc, f, sizeof(*f) -
			    sizeof(f->odtf_crc));
			psc_crc64_fini(&crc);

			if (crc != f->odtf_crc)
				PFLOG_ODT(PLL_FATAL, t,
				    "CRC failed; slot=%"PRId64" "
				    "mem_crc=%"PSCPRIxCRC64" "
				    "ftr_crc=%"PSCPRIxCRC64,
				    i, crc, f->odtf_crc);
		}

		if (f->odtf_flags & ODT_FTRF_INUSE) {
			psc_vbitmap_set(t->odt_bitmap, i);
			if (cbf)
				cbf(p, i, arg);
		}
		PSCFREE(p);
		PSCFREE(f);
	}
#undef i

	pfl_meter_destroy(&mtr);
}

void
pfl_odt_release(struct pfl_odt *t)
{
	if (t->odt_bitmap)
		psc_vbitmap_free(t->odt_bitmap);

	t->odt_ops.odtop_close(t);

	pfl_opstat_destroy(t->odt_iostats.rd);
	pfl_opstat_destroy(t->odt_iostats.wr);

	PSCFREE(t->odt_hdr);
	PSCFREE(t);
}
