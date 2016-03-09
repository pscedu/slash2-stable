/* $Id$ */
/*
 * %ISC_START_LICENSE%
 * ---------------------------------------------------------------------
 * Copyright 2015-2016, Google, Inc.
 * Copyright (c) 2009-2015, Pittsburgh Supercomputing Center (PSC).
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

/*
 * Get offset of a table entry, which is relative
 * to either the disk file or start of base memory-mapped address.
 */
static __inline size_t
pfl_odt_getitemoff(const struct pfl_odt *t, size_t item)
{
	const struct pfl_odt_hdr *h;

	h = t->odt_hdr;
	psc_assert(item < h->odth_nitems);
	return (item * h->odth_slotsz);
}

#define GETADDR(t, item)						\
	PSC_AGP((t)->odt_base, pfl_odt_getitemoff((t), (item)))

#define MMAPSZ(t)	((t)->odt_hdr->odth_nitems * (t)->odt_hdr->odth_slotsz)

void
pfl_odt_mmap_sync(struct pfl_odt *t, size_t item)
{
	int rc, flags;
	size_t len;
	void *p;

	flags = t->odt_hdr->odth_options & ODTBL_OPT_SYNC ?
	    MS_SYNC : MS_ASYNC;

	if (item == (size_t)-1) {
		p = t->odt_base;
		len = MMAPSZ(t);
	} else {
		p = GETADDR(t, item);
		len = t->odt_hdr->odth_slotsz;
	}

	rc = msync(p, len, flags);
	if (rc)
		PFLOG_ODT(PLL_ERROR, t, "msync: %d", errno);
}

void
pfl_odt_mmap_mapslot(struct pfl_odt *t, size_t item, void **pp,
    struct pfl_odt_slotftr **fp)
{
	void *p;

	p = GETADDR(t, item);
	if (pp)
		*pp = p;
	if (fp)
		*fp = PSC_AGP(p, t->odt_hdr->odth_slotsz -
		    sizeof(**fp));
}

void
pfl_odt_mmap_resize(__unusedx struct pfl_odt *t)
{
	psc_fatalx("not supported");
	// munmap
	// mmap
}

void
pfl_odt_mmap_create(struct pfl_odt *t, const char *fn, int overwrite)
{
	struct pfl_odt_hdr *h;

	h = t->odt_hdr;
	t->odt_fd = open(fn, O_CREAT | O_RDWR |
	    (overwrite ? O_TRUNC : O_EXCL), 0600);
	if (t->odt_fd == -1)
		psc_fatal("open %s", fn);
	if (ftruncate(t->odt_fd, h->odth_start + MMAPSZ(t)) == -1)
		psc_fatal("truncate %s", fn);
	if (pwrite(t->odt_fd, h, sizeof(*h), 0) != sizeof(*h))
		psc_fatal("write %s", fn);
	t->odt_base = mmap(NULL, MMAPSZ(t),  PROT_WRITE, MAP_SHARED,
	    t->odt_fd, h->odth_start);
	if (t->odt_base == MAP_FAILED)
		PFLOG_ODT(PLL_FATAL, t, "mmap %s; rc=%d", fn, errno);
}

void
pfl_odt_mmap_open(struct pfl_odt *t, const char *fn, int oflg)
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
	t->odt_base = mmap(NULL, MMAPSZ(t), prot, MAP_SHARED, t->odt_fd,
	    h->odth_start);
	if (t->odt_base == MAP_FAILED)
		PFLOG_ODT(PLL_FATAL, t, "mmap %s", fn);
}

void
pfl_odt_mmap_close(struct pfl_odt *t)
{
	if (t->odt_base && munmap(t->odt_base, MMAPSZ(t)) == -1)
		PFLOG_ODT(PLL_ERROR, t, "munmap: rc=%d", errno);
	if (t->odt_fd != -1 && close(t->odt_fd) == -1)
		PFLOG_ODT(PLL_ERROR, t, "close: rc=%d %d", errno, t->odt_fd);
}

struct pfl_odt_ops pfl_odtops_mmap = {
	pfl_odt_mmap_create,	/* odtop_create() */
	pfl_odt_mmap_open,	/* odtop_open() */
	pfl_odt_mmap_close,	/* odtop_close() */
	NULL,			/* odtop_read() */
	NULL,			/* odtop_write() */
	pfl_odt_mmap_mapslot,	/* odtop_mapslot() */
	pfl_odt_mmap_resize,	/* odtop_resize() */
	pfl_odt_mmap_sync	/* odtop_sync() */
};

void
_pfl_odt_doput(struct pfl_odt *t, struct pfl_odt_receipt *r,
    const void *p, struct pfl_odt_slotftr *f, int inuse)
{
	struct pfl_odt_hdr *h;

	h = t->odt_hdr;

	f->odtf_flags = inuse ? ODT_FTRF_INUSE : 0;
	f->odtf_slotno = r->odtr_item;
	psc_crc64_init(&f->odtf_crc);
	if (inuse)
		psc_crc64_add(&f->odtf_crc, p, h->odth_itemsz);
	psc_crc64_add(&f->odtf_crc, f, sizeof(*f) -
	    sizeof(f->odtf_crc));
	psc_crc64_fini(&f->odtf_crc);
	r->odtr_crc = f->odtf_crc;

	if (t->odt_ops.odtop_write)
		t->odt_ops.odtop_write(t, p, f, r->odtr_item);

	pfl_opstat_add(t->odt_iostats.wr, h->odth_slotsz);

	if (h->odth_options & ODTBL_OPT_SYNC)
		t->odt_ops.odtop_sync(t, r->odtr_item);

	PFLOG_ODT(PLL_DIAG, t,
	    "r=%p slot=%"PRId64" item crc=%"PSCPRIxCRC64" ",
	    r, r->odtr_item, f->odtf_crc);

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
		h->odth_nitems = psc_vbitmap_getsize(t->odt_bitmap);
		t->odt_ops.odtop_resize(t);
		PFLOG_ODT(PLL_WARN, t,
		    "odtable now has %u items (used to be %zd)",
		    h->odth_nitems, item);
		ODT_STAT_INCR(t, extend);
	}
	freelock(&t->odt_lock);
	return (item);
}

void
pfl_odt_mapslot(struct pfl_odt *t, size_t n, void *pp,
    struct pfl_odt_slotftr **fp)
{
	struct pfl_odt_hdr *h;
	void **p = (void **)pp;

	h = t->odt_hdr;
	if (t->odt_ops.odtop_mapslot)
		t->odt_ops.odtop_mapslot(t, n, p, fp);
	else {
		if (p)
			*p = PSCALLOC(h->odth_itemsz);
		if (fp)
			*fp = PSCALLOC(sizeof(**fp));
	}
}

/*
 * Store an item into an odtable.
 */
struct pfl_odt_receipt *
pfl_odt_putitemf(struct pfl_odt *t, size_t n, void *p, int inuse)
{
	struct pfl_odt_receipt *r;
	struct pfl_odt_slotftr *f;

	/* Setup and return the receipt. */
	r = PSCALLOC(sizeof(*r));
	r->odtr_item = n;

	pfl_odt_mapslot(t, n, NULL, &f);
	_pfl_odt_doput(t, r, p, f, inuse);
	pfl_odt_freebuf(t, NULL, f);
	return (r);
}

void
pfl_odt_freebuf(struct pfl_odt *t, void *p, struct pfl_odt_slotftr *f)
{
	if (t->odt_ops.odtop_mapslot == NULL) {
		PSCFREE(p);
		PSCFREE(f);
	}
}

void
pfl_odt_getslot(struct pfl_odt *t, const struct pfl_odt_receipt *r,
    void *pp, struct pfl_odt_slotftr **fp)
{
	struct pfl_odt_hdr *h;
	void **p = (void **)pp;

	h = t->odt_hdr;
	psc_assert(r->odtr_item <= h->odth_nitems - 1);

	pfl_odt_mapslot(t, r->odtr_item, p, fp);

	if (t->odt_ops.odtop_read)
		t->odt_ops.odtop_read(t, r, p ? *p : NULL,
		    fp ? *fp : NULL);

	pfl_opstat_add(t->odt_iostats.rd, h->odth_slotsz);

	ODT_STAT_INCR(t, read);
}

void
pfl_odt_replaceitem(struct pfl_odt *t, struct pfl_odt_receipt *r,
    void *p)
{
	struct pfl_odt_slotftr *f;

	pfl_odt_mapslot(t, r->odtr_item, NULL, &f);
	_pfl_odt_doput(t, r, p, f, 1);
	pfl_odt_freebuf(t, NULL, f);

	PFLOG_ODT(PLL_DIAG, t, "rcpt=%p slot=%"PRId64,
	    r, r->odtr_item);

	ODT_STAT_INCR(t, replace);
}

/*
 * Free the odtable slot which corresponds to the provided receipt.
 * Note: r is freed here.
 */
void
pfl_odt_freeitem(struct pfl_odt *t, struct pfl_odt_receipt *r)
{
	struct pfl_odt_slotftr *f;

	pfl_odt_mapslot(t, r->odtr_item, NULL, &f);
	_pfl_odt_doput(t, r, NULL, f, 0);
	pfl_odt_freebuf(t, NULL, f);

	spinlock(&t->odt_lock);
	psc_vbitmap_unset(t->odt_bitmap, r->odtr_item);
	freelock(&t->odt_lock);

	PFLOG_ODT(PLL_DIAG, t, "freeitem r=%p slot=%"PRId64,
	    r, r->odtr_item);

	ODT_STAT_INCR(t, free);

	PSCFREE(r);
}

void
pfl_odt_create(const char *fn, size_t nitems, size_t itemsz,
    int overwrite, size_t startoff, size_t pad, int tflg)
{
	struct pfl_odt_slotftr *f;
	struct pfl_odt_receipt r;
	struct pfl_odt_hdr *h;
	struct pfl_odt *t;

	t = PSCALLOC(sizeof(*t));
	t->odt_ops = pfl_odtops_mmap;
	INIT_SPINLOCK(&t->odt_lock);
	snprintf(t->odt_name, sizeof(t->odt_name), "%s",
	    pfl_basename(fn));

	t->odt_iostats.rd = pfl_opstat_init("odt-%s-rd", t->odt_name);
	t->odt_iostats.wr = pfl_opstat_init("odt-%s-wr", t->odt_name);

	h = PSCALLOC(sizeof(*h));
	memset(h, 0, sizeof(*h));
	h->odth_nitems = nitems;
	h->odth_itemsz = itemsz;
	h->odth_slotsz = itemsz + pad + sizeof(*f);
	h->odth_options = tflg;
	h->odth_start = startoff;
	t->odt_hdr = h;
	psc_crc64_calc(&h->odth_crc, h, sizeof(*h) -
	    sizeof(h->odth_crc));

	t->odt_ops.odtop_create(t, fn, overwrite);

	for (r.odtr_item = 0; r.odtr_item < nitems; r.odtr_item++) {
		pfl_odt_mapslot(t, r.odtr_item, NULL, &f);
		_pfl_odt_doput(t, &r, NULL, f, 0);
		pfl_odt_freebuf(t, NULL, f);
	}

	PFLOG_ODT(PLL_DIAG, t, "created");

	pfl_odt_release(t);
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

	odtops->odtop_open(t, fn, oflg);

	psc_crc64_calc(&crc, t->odt_hdr, sizeof(*t->odt_hdr) -
	    sizeof(t->odt_hdr->odth_crc));
	psc_assert(h->odth_crc == crc);

	t->odt_bitmap = psc_vbitmap_newf(h->odth_nitems, PVBF_AUTO);
	psc_assert(t->odt_bitmap);

	PFLOG_ODT(PLL_DIAG, t, "loaded");

	pll_add(&pfl_odtables, t);
}

void
pfl_odt_check(struct pfl_odt *t,
    void (*cbf)(void *, struct pfl_odt_receipt *, void *), void *arg)
{
	struct pfl_odt_receipt r = { 0, 0 };
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
		r.odtr_item = i;
		pfl_odt_getslot(t, &r, &p, &f);
		r.odtr_crc = f->odtf_crc;

		if (pfl_odt_footercheck(t, f, &r))
			PFLOG_ODT(PLL_FATAL, t, "footercheck");

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
				cbf(p, &r, arg); // need r?
		}

		pfl_odt_freebuf(t, p, f);
	}
#undef i

	pfl_meter_destroy(&mtr);
}

void
pfl_odt_release(struct pfl_odt *t)
{
	t->odt_ops.odtop_sync(t, (size_t)-1);

	if (t->odt_bitmap)
		psc_vbitmap_free(t->odt_bitmap);

	t->odt_ops.odtop_close(t);

	PSCFREE(t->odt_hdr);

	pfl_opstat_destroy(t->odt_iostats.rd);
	pfl_opstat_destroy(t->odt_iostats.wr);

	PSCFREE(t);
}
