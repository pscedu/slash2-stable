/* $Id$ */
/*
 * %PSC_START_COPYRIGHT%
 * -----------------------------------------------------------------------------
 * Copyright (c) 2009-2013, Pittsburgh Supercomputing Center (PSC).
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
#include "pfl/iostats.h"
#include "pfl/lock.h"
#include "pfl/lockedlist.h"
#include "pfl/log.h"
#include "pfl/meter.h"
#include "pfl/odtable.h"
#include "pfl/str.h"
#include "pfl/types.h"

struct psc_lockedlist	 pfl_odtables =
    PLL_INIT(&pfl_odtables, struct pfl_odt, odt_lentry);

/**
 * odtable_getitemoff - Get offset of a table entry, which is relative
 * to either the disk file or start of base memory-mapped address.
 */
__inline size_t
pfl_odt_getitemoff(const struct pfl_odt *t, size_t elem)
{
	const struct pfl_odt_hdr *h;

	h = t->odt_hdr;
	psc_assert(elem < h->odth_nelems);
	return (elem * h->odth_slotsz);
}

#define GETADDR(t, elem)						\
	PSC_AGP((t)->odt_base, pfl_odt_getitemoff((t), (elem)))

#define MMAPSZ(t)	((t)->odt_hdr->odth_nelems * (t)->odt_hdr->odth_slotsz)

void
pfl_odt_mmap_sync(struct pfl_odt *t, size_t elem)
{
	int rc, flags;
	size_t len;
	void *p;

	flags = t->odt_hdr->odth_options & ODTBL_OPT_SYNC ?
	    MS_SYNC : MS_ASYNC;

	if (elem == (size_t)-1) {
		p = t->odt_base;
		len = MMAPSZ(t);
	} else {
		p = GETADDR(t, elem);
		len = t->odt_hdr->odth_slotsz;
	}

	rc = msync(p, len, flags);
	if (rc)
		PFLOG_ODT(PLL_ERROR, t, "msync: %d", errno);
}

void
pfl_odt_mmap_mapslot(struct pfl_odt *t, size_t elem, void **pp,
    struct pfl_odt_entftr **fp)
{
	void *p;

	p = GETADDR(t, elem);
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
	t->odt_base = mmap(NULL, MMAPSZ(t),  PROT_WRITE,
	    MAP_FILE | MAP_SHARED, t->odt_fd, h->odth_start);
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
	t->odt_base = mmap(NULL, MMAPSZ(t), prot, MAP_FILE | MAP_SHARED,
	    t->odt_fd, h->odth_start);
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
	pfl_odt_mmap_close,
	pfl_odt_mmap_create,
	pfl_odt_mmap_mapslot,
	pfl_odt_mmap_open,
	NULL,
	pfl_odt_mmap_resize,
	pfl_odt_mmap_sync,
	NULL
};

void
_pfl_odt_doput(struct pfl_odt *t, struct pfl_odt_receipt *r,
    const void *p, struct pfl_odt_entftr *f, int inuse)
{
	struct pfl_odt_hdr *h;

	h = t->odt_hdr;

	f->odtf_flags = inuse ? ODT_FTRF_INUSE : 0;
	f->odtf_slotno = r->odtr_elem;
	psc_crc64_init(&f->odtf_crc);
	if (inuse)
		psc_crc64_add(&f->odtf_crc, p, h->odth_objsz);
	psc_crc64_add(&f->odtf_crc, f, sizeof(*f) -
	    sizeof(f->odtf_crc));
	psc_crc64_fini(&f->odtf_crc);
	r->odtr_crc = f->odtf_crc;

	if (t->odt_ops.odtop_write)
		t->odt_ops.odtop_write(t, p, f, r->odtr_elem);

	psc_iostats_intv_add(&t->odt_iostats.wr, h->odth_slotsz);

	if (h->odth_options & ODTBL_OPT_SYNC)
		t->odt_ops.odtop_sync(t, r->odtr_elem);

	PFLOG_ODT(PLL_DIAG, t,
	    "r=%p slot=%zd elemcrc=%"PSCPRIxCRC64" ",
	    r, r->odtr_elem, f->odtf_crc);

	ODT_STAT_INCR(t, write);
}

size_t
pfl_odt_allocslot(struct pfl_odt *t)
{
	struct pfl_odt_hdr *h;
	size_t elem;

	h = t->odt_hdr;
	spinlock(&t->odt_lock);
	if (psc_vbitmap_next(t->odt_bitmap, &elem) <= 0) {
		ODT_STAT_INCR(t, full);
		freelock(&t->odt_lock);
		return (-1);
	}
	if (elem >= h->odth_nelems) {
		h->odth_nelems = psc_vbitmap_getsize(t->odt_bitmap);
		t->odt_ops.odtop_resize(t);
		PFLOG_ODT(PLL_WARN, t,
		    "odtable now has %u elements (used to be %zd)",
		    h->odth_nelems, elem);
		ODT_STAT_INCR(t, extend);
	}
	freelock(&t->odt_lock);
	return (elem);
}

void
pfl_odt_mapslot(struct pfl_odt *t, size_t n, void *pp,
    struct pfl_odt_entftr **fp)
{
	struct pfl_odt_hdr *h;
	void **p = (void **)pp;

	h = t->odt_hdr;
	if (t->odt_ops.odtop_mapslot)
		t->odt_ops.odtop_mapslot(t, n, p, fp);
	else {
		if (p)
			*p = PSCALLOC(h->odth_objsz);
		if (fp)
			*fp = PSCALLOC(sizeof(**fp));
	}
}

/**
 * pfl_odtputitem - Store an item into an odtable.
 */
struct pfl_odt_receipt *
pfl_odt_putitemf(struct pfl_odt *t, size_t n, void *p, int inuse)
{
	struct pfl_odt_receipt *r;
	struct pfl_odt_entftr *f;

	/* Setup and return the receipt. */
	r = PSCALLOC(sizeof(*r));
	r->odtr_elem = n;

	pfl_odt_mapslot(t, n, NULL, &f);
	_pfl_odt_doput(t, r, p, f, inuse);
	pfl_odt_freebuf(t, NULL, f);
	return (r);
}

void
pfl_odt_freebuf(struct pfl_odt *t, void *p, struct pfl_odt_entftr *f)
{
	if (t->odt_ops.odtop_mapslot == NULL) {
		PSCFREE(p);
		PSCFREE(f);
	}
}

void
pfl_odt_getslot(struct pfl_odt *t, const struct pfl_odt_receipt *r,
    void *pp, struct pfl_odt_entftr **fp)
{
	struct pfl_odt_hdr *h;
	void **p = (void **)pp;

	h = t->odt_hdr;
	psc_assert(r->odtr_elem <= h->odth_nelems - 1);

	pfl_odt_mapslot(t, r->odtr_elem, p, fp);

	if (t->odt_ops.odtop_read)
		t->odt_ops.odtop_read(t, r, p ? *p : NULL,
		    fp ? *fp : NULL);

	psc_iostats_intv_add(&t->odt_iostats.rd, h->odth_slotsz);

	ODT_STAT_INCR(t, read);
}

void
pfl_odt_replaceitem(struct pfl_odt *t, struct pfl_odt_receipt *r,
    void *p)
{
	struct pfl_odt_entftr *f;

	pfl_odt_mapslot(t, r->odtr_elem, NULL, &f);
	_pfl_odt_doput(t, r, p, f, 1);
	pfl_odt_freebuf(t, NULL, f);

	PFLOG_ODT(PLL_DIAG, t, "rcpt=%p slot=%zd",
	    r, r->odtr_elem);

	ODT_STAT_INCR(t, replace);
}

/**
 * odtable_freeitem - Free the odtable slot which corresponds to the
 *	provided receipt.
 * Note: r is freed here.
 */
void
pfl_odt_freeitem(struct pfl_odt *t, struct pfl_odt_receipt *r)
{
	struct pfl_odt_entftr *f;

	pfl_odt_mapslot(t, r->odtr_elem, NULL, &f);
	_pfl_odt_doput(t, r, NULL, f, 0);
	pfl_odt_freebuf(t, NULL, f);

	spinlock(&t->odt_lock);
	psc_vbitmap_unset(t->odt_bitmap, r->odtr_elem);
	freelock(&t->odt_lock);

	PFLOG_ODT(PLL_DIAG, t, "freeitem r=%p slot=%zd",
	    r, r->odtr_elem);

	ODT_STAT_INCR(t, free);

	PSCFREE(r);
}

void
pfl_odt_create(const char *fn, size_t nelems, size_t objsz,
    int overwrite, size_t startoff, size_t pad, int tflg)
{
	struct pfl_odt_entftr *f;
	struct pfl_odt_receipt r;
	struct pfl_odt_hdr *h;
	struct pfl_odt *t;

	t = PSCALLOC(sizeof(*t));
	t->odt_ops = pfl_odtops_mmap;
	INIT_SPINLOCK(&t->odt_lock);
	snprintf(t->odt_name, sizeof(t->odt_name), "%s",
	    pfl_basename(fn));

	psc_iostats_init(&t->odt_iostats.rd, "odt-%s-rd", t->odt_name);
	psc_iostats_init(&t->odt_iostats.wr, "odt-%s-wr", t->odt_name);

	h = PSCALLOC(sizeof(*h));
	memset(h, 0, sizeof(*h));
	h->odth_nelems = nelems;
	h->odth_objsz = objsz;
	h->odth_slotsz = objsz + pad + sizeof(*f);
	h->odth_options = tflg;
	h->odth_start = startoff;
	t->odt_hdr = h;
	psc_crc64_calc(&h->odth_crc, h, sizeof(*h) -
	    sizeof(h->odth_crc));

	t->odt_ops.odtop_create(t, fn, overwrite);

	for (r.odtr_elem = 0; r.odtr_elem < nelems; r.odtr_elem++) {
		pfl_odt_mapslot(t, r.odtr_elem, NULL, &f);
		_pfl_odt_doput(t, &r, NULL, f, 0);
		pfl_odt_freebuf(t, NULL, f);
	}

	PFLOG_ODT(PLL_DIAG, t, "created");

	pfl_odt_release(t);
}

void
pfl_odt_load(struct pfl_odt **tp, struct pfl_odt_ops *odtops, int oflg,
    void (*cbf)(void *, struct pfl_odt_receipt *, void *), void *arg,
    const char *fn, const char *fmt, ...)
{
	struct psc_meter mtr;
	struct pfl_odt_receipt r = { 0, 0 };
	struct pfl_odt_entftr *f;
	struct pfl_odt_hdr *h;
	struct pfl_odt *t;
	uint64_t crc;
	va_list ap;
	void *p;

	*tp = t = PSCALLOC(sizeof(*t));
	t->odt_ops = *odtops;
	INIT_SPINLOCK(&t->odt_lock);
	INIT_PSC_LISTENTRY(&t->odt_lentry);

	va_start(ap, fmt);
	vsnprintf(t->odt_name, sizeof(t->odt_name), fmt, ap);
	va_end(ap);

	psc_iostats_init(&t->odt_iostats.rd, "odt-%s-rd", t->odt_name);
	psc_iostats_init(&t->odt_iostats.wr, "odt-%s-wr", t->odt_name);

	h = t->odt_hdr = PSCALLOC(sizeof(*h));

	odtops->odtop_open(t, fn, oflg);

	psc_crc64_calc(&crc, t->odt_hdr, sizeof(*t->odt_hdr) -
	    sizeof(t->odt_hdr->odth_crc));
	psc_assert(h->odth_crc == crc);

	t->odt_bitmap = psc_vbitmap_newf(h->odth_nelems, PVBF_AUTO);
	psc_assert(t->odt_bitmap);

	psc_meter_init(&mtr, 0, "odt-%s", t->odt_name);
	mtr.pm_max = h->odth_nelems;

#define i mtr.pm_cur
	for (i = 0; i < h->odth_nelems; i++) {
		r.odtr_elem = i;
		pfl_odt_getslot(t, &r, &p, &f);

		if (pfl_odt_footercheck(t, f, &r))
			PFLOG_ODT(PLL_FATAL, t, "footercheck");

		if (h->odth_options & ODTBL_OPT_CRC) {
			psc_crc64_init(&crc);
			if (f->odtf_flags & ODT_FTRF_INUSE)
				psc_crc64_add(&crc, p, h->odth_objsz);
			psc_crc64_add(&crc, f, sizeof(*f) -
			    sizeof(f->odtf_crc));
			psc_crc64_fini(&crc);

			if (crc != f->odtf_crc)
				PFLOG_ODT(PLL_FATAL, t,
				    "mem_crc=%"PSCPRIxCRC64" "
				    "ftr_crc=%"PSCPRIxCRC64,
				    crc, f->odtf_crc);
		}

		if (f->odtf_flags & ODT_FTRF_INUSE) {
			psc_vbitmap_set(t->odt_bitmap, i);
			if (cbf)
				cbf(p, &r, arg); // need r?
		}

		pfl_odt_freebuf(t, p, NULL);
	}
#undef i

	psc_meter_destroy(&mtr);

	PFLOG_ODT(PLL_DIAG, t, "loaded");

	pll_add(&pfl_odtables, t);
}

void
pfl_odt_release(struct pfl_odt *t)
{
	t->odt_ops.odtop_sync(t, (size_t)-1);

	if (t->odt_bitmap)
		psc_vbitmap_free(t->odt_bitmap);

	t->odt_ops.odtop_close(t);

	PSCFREE(t->odt_hdr);
	PSCFREE(t);
}
