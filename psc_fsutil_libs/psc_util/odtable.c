/* $Id$ */
/*
 * %PSC_START_COPYRIGHT%
 * -----------------------------------------------------------------------------
 * Copyright (c) 2009-2012, Pittsburgh Supercomputing Center (PSC).
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

#include <sys/stat.h>
#include <sys/types.h>
#include <sys/mman.h>

#include <fcntl.h>
#include <inttypes.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>

#include "pfl/cdefs.h"
#include "pfl/str.h"
#include "pfl/types.h"
#include "psc_ds/lockedlist.h"
#include "psc_util/alloc.h"
#include "psc_util/lock.h"
#include "psc_util/log.h"
#include "psc_util/odtable.h"

struct psc_lockedlist psc_odtables =
    PLL_INIT(&psc_odtables, struct odtable, odt_lentry);

static void
odtable_sync(struct odtable *odt, __unusedx size_t elem)
{
	int rc, flags;

	psc_assert(odt->odt_hdr);

	flags = (odt->odt_hdr->odth_options & ODTBL_OPT_SYNC) ?
		 MS_SYNC : MS_ASYNC;

	/* For now just sync the whole thing.
	 */
	rc = msync(odt->odt_base, ODTABLE_MAPSZ(odt), flags);
	if (rc)
		psclog_error("msync error on table %p", odt);
}

/**
 * odtable_putitem - Save a bmap I/O node assignment into the odtable.
 */
struct odtable_receipt *
odtable_putitem(struct odtable *odt, void *data, size_t len)
{
	struct odtable_receipt *odtr, todtr = { 0, 0 };
	struct odtable_entftr *odtf;
	uint64_t crc;
	void *p;

	psc_assert(len <= odt->odt_hdr->odth_elemsz);

	do {
		spinlock(&odt->odt_lock);
		if (psc_vbitmap_next(odt->odt_bitmap, &todtr.odtr_elem) <= 0) {
			freelock(&odt->odt_lock);
			return (NULL);
		}
		freelock(&odt->odt_lock);

		odtf = odtable_getfooter(odt, todtr.odtr_elem);
	} while (odtable_footercheck(odtf, &todtr, 0));

	/* psc_vbitmap_next() already flips the bit under odt_lock */
	odtf->odtf_inuse = ODTBL_INUSE;

	/* Setup the receipt.
	 */
	odtr = PSCALLOC(sizeof(*odtr));
	odtr->odtr_elem = todtr.odtr_elem;

	p = odtable_getitem_addr(odt, todtr.odtr_elem);
	memcpy(p, data, len);
	if (len < odt->odt_hdr->odth_elemsz)
		memset(p + len, 0, odt->odt_hdr->odth_elemsz - len);
	psc_crc64_calc(&crc, p, odt->odt_hdr->odth_elemsz);

	/*
	 * Write metadata into the mmap'd footer.  For now the key and
	 *  CRC are the same.
	 */
	odtr->odtr_key = crc;
	odtf->odtf_crc = crc;

	psclog_info("slot=%zd elemcrc=%"PSCPRIxCRC64, todtr.odtr_elem, crc);

	odtable_sync(odt, todtr.odtr_elem);

	return (odtr);
}

void *
odtable_getitem(struct odtable *odt, const struct odtable_receipt *odtr)
{
	void *data = odtable_getitem_addr(odt, odtr->odtr_elem);
	struct odtable_entftr *odtf = odtable_getfooter(odt, odtr->odtr_elem);

	if (odtable_footercheck(odtf, odtr, 1))
		return (NULL);

	if (odt->odt_hdr->odth_options & ODTBL_OPT_CRC) {
		uint64_t crc;

		psc_crc64_calc(&crc, data, odt->odt_hdr->odth_elemsz);
		if (crc != odtf->odtf_crc) {
			odtf->odtf_inuse = ODTBL_BAD;
			psclog_warnx("slot=%zd crc fail "
			    "odtfcrc=%"PSCPRIxCRC64" "
			    "elemcrc=%"PSCPRIxCRC64,
			    odtr->odtr_elem, odtf->odtf_crc, crc);
			return (NULL);
		}
	}
	return (data);
}

struct odtable_receipt *
odtable_replaceitem(struct odtable *odt, struct odtable_receipt *odtr,
    void *data, size_t len)
{
	struct odtable_entftr *odtf;
	uint64_t crc;
	void *p;

	psc_assert(len <= odt->odt_hdr->odth_elemsz);

	odtf = odtable_getfooter(odt, odtr->odtr_elem);
	if (odtable_footercheck(odtf, odtr, 1))
		return (NULL);

	p = odtable_getitem_addr(odt, odtr->odtr_elem);
	memcpy(p, data, len);
	if (len < odt->odt_hdr->odth_elemsz)
		memset(p + len, 0, odt->odt_hdr->odth_elemsz - len);
	psc_crc64_calc(&crc, p, odt->odt_hdr->odth_elemsz);
	odtr->odtr_key = crc;
	odtf->odtf_crc = crc;

	psclog_info("slot=%zd elemcrc=%"PSCPRIxCRC64, odtr->odtr_elem, crc);

	odtable_sync(odt, odtr->odtr_elem);

	return (odtr);
}

/**
 * odtable_freeitem - free the odtable slot which corresponds to the provided
 *   receipt.
 * Note: odtr is freed here.
 */
int
odtable_freeitem(struct odtable *odt, struct odtable_receipt *odtr)
{
	int rc;
	struct odtable_entftr *odtf = odtable_getfooter(odt, odtr->odtr_elem);

	if ((rc = odtable_footercheck(odtf, odtr, 1)))
		return (rc);

	odtf->odtf_inuse = ODTBL_FREE;

	psclog_info("slot=%zd", odtr->odtr_elem);

	odtable_sync(odt, odtr->odtr_elem);
	spinlock(&odt->odt_lock);
	psc_vbitmap_unset(odt->odt_bitmap, odtr->odtr_elem);
	freelock(&odt->odt_lock);

	PSCFREE(odtr);

	return (0);
}

int
odtable_create(const char *fn, size_t nelems, size_t elemsz, int overwrite)
{
	int rc = 0;
	size_t z;
	int flags = O_CREAT | O_EXCL | O_WRONLY;
	struct odtable odt;
	struct odtable_entftr odtf;
	struct odtable_hdr odth;

	odth.odth_nelems = nelems;
	odth.odth_elemsz = elemsz;
	odth.odth_slotsz = elemsz + sizeof(struct odtable_entftr);
	odth.odth_magic = ODTBL_MAGIC;
	odth.odth_version = ODTBL_VERS;
	odth.odth_options = ODTBL_OPT_CRC;
	odth.odth_start = ODTBL_START;

	odtf.odtf_crc = 0;
	odtf.odtf_inuse = ODTBL_FREE;
	odtf.odtf_slotno = 0;
	odtf.odtf_magic = ODTBL_MAGIC;

	if (overwrite)
		flags = O_CREAT | O_TRUNC | O_WRONLY;
	else
		flags = O_CREAT | O_EXCL | O_WRONLY;

	odt.odt_hdr = &odth;

	odt.odt_fd = open(fn, flags, 0600);
	if (odt.odt_fd < 0) {
		rc = -errno;
		goto out;
	}

	if (pwrite(odt.odt_fd, &odth, sizeof(odth), 0) != sizeof(odth)) {
		rc = -errno;
		goto out;
	}

	psclog_trace("odt.odt_hdr.odth_start=%"PRIx64, odt.odt_hdr->odth_start);

	/* initialize the table by writing the footers of all entries */
	for (z = 0; z < nelems; z++) {
		odtf.odtf_slotno = z;

		psclog_trace("elem=%zd offset=%zu size=%zu",
		    z, odtable_getitem_foff(&odt, z), sizeof(odtf));

		if (pwrite(odt.odt_fd, &odtf, sizeof(odtf),
		    odtable_getitem_foff(&odt, z) + elemsz) !=
		    sizeof(odtf)) {
			rc = -errno;
			goto out;
		}
	}
 out:
	close(odt.odt_fd);
	odt.odt_fd = -1;
	return (rc);
}

int
odtable_load(struct odtable **t, const char *fn, const char *fmt, ...)
{
	struct odtable *odt = PSCALLOC(sizeof(struct odtable));
	struct odtable_receipt todtr = {0, 0};
	struct odtable_entftr *odtf;
	struct odtable_hdr *odth;
	int rc = 0, frc;
	va_list ap;
	size_t z;
	void *p;

	psc_assert(t);
	*t = NULL;

	INIT_SPINLOCK(&odt->odt_lock);

	odt->odt_fd = open(fn, O_RDWR, 0600);
	if (odt->odt_fd == -1)
		PFL_GOTOERR(out, rc = errno);

	odth = odt->odt_hdr = PSCALLOC(sizeof(*odth));

	if (pread(odt->odt_fd, odth, sizeof(*odth), 0) != sizeof(*odth))
		PFL_GOTOERR(out, rc = errno);

	if (odth->odth_magic != ODTBL_MAGIC ||
	    odth->odth_version != ODTBL_VERS)
		PFL_GOTOERR(out, rc = EINVAL);

	rc = odtable_createmmap(odt);
	if (rc)
		PFL_GOTOERR(out, rc);

	odt->odt_bitmap = psc_vbitmap_new(odt->odt_hdr->odth_nelems);
	if (!odt->odt_bitmap)
		PFL_GOTOERR(out, rc = ENOMEM);

	for (z = 0; z < odt->odt_hdr->odth_nelems; z++) {
		todtr.odtr_elem = z;
		p = odtable_getitem_addr(odt, z);
		odtf = odtable_getfooter(odt, z);

		frc = odtable_footercheck(odtf, &todtr, -1);
		/* Sanity checks for debugging. */
		psc_assert(frc != ODTBL_MAGIC_ERR);
		psc_assert(frc != ODTBL_SLOT_ERR);

		if (odtf->odtf_inuse == ODTBL_FREE)
			psc_vbitmap_unset(odt->odt_bitmap, z);

		else if (odtf->odtf_inuse == ODTBL_INUSE) {
			psc_vbitmap_set(odt->odt_bitmap, z);

			if (odth->odth_options & ODTBL_OPT_CRC) {
				uint64_t crc;

				psc_crc64_calc(&crc, p,
				    odt->odt_hdr->odth_elemsz);
				if (crc != odtf->odtf_crc) {
					odtf->odtf_inuse = ODTBL_BAD;
					psclog_warnx("slot=%zd crc fail "
					    "odtfcrc=%"PSCPRIxCRC64" "
					    "elemcrc=%"PSCPRIxCRC64,
					    z, odtf->odtf_crc, crc);
				}
			}
		} else {
			psc_vbitmap_set(odt->odt_bitmap, z);
			psclog_warnx("slot=%zd ignoring, bad inuse value"
			    "inuse=0x%"PRIx64,
			    z, odtf->odtf_inuse);
		}
	}

	psclog_notice("odtable=%p base=%p has %d/%zd slots available "
	    "elemsz=%zd magic=%"PRIx64,
	    odt, odt->odt_base, psc_vbitmap_nfree(odt->odt_bitmap),
	    odth->odth_nelems, odth->odth_elemsz, odth->odth_magic);

	INIT_PSC_LISTENTRY(&odt->odt_lentry);

	va_start(ap, fmt);
	vsnprintf(odt->odt_name, sizeof(odt->odt_name), fmt, ap);
	va_end(ap);

	*t = odt;
	pll_add(&psc_odtables, odt);

 out:
	if (odt->odt_fd != -1) {
		close(odt->odt_fd);
		odt->odt_fd = -1;
	}
	if (rc) {
		/* XXX odtable_release()? */
		if (odt->odt_base &&
		    odt->odt_base != MAP_FAILED)
			odtable_freemap(odt);
		if (odt->odt_bitmap)
			psc_vbitmap_free(odt->odt_bitmap);
		PSCFREE(odt->odt_hdr);
		PSCFREE(odt);
	}
	return (rc);
}

int
odtable_release(struct odtable *odt)
{
	int rc = 0;

	psc_vbitmap_free(odt->odt_bitmap);
	odt->odt_bitmap = NULL;

	if (odtable_freemap(odt))
		psc_fatal("odtable_freemap() failed on %p", odt);

	PSCFREE(odt->odt_hdr);
	if (odt->odt_fd != -1) {
		rc = close(odt->odt_fd);
		odt->odt_fd = -1;
	}
	PSCFREE(odt);
	return (rc);
}

void
odtable_scan(struct odtable *odt,
    void (*odt_handler)(void *, struct odtable_receipt *))
{
	int rc;
	struct odtable_receipt *odtr, todtr = {0, 0};
	struct odtable_entftr *odtf;

	psc_assert(odt_handler != NULL);

	for (todtr.odtr_elem = 0;
	    todtr.odtr_elem < odt->odt_hdr->odth_nelems;
	     todtr.odtr_elem++) {
		if (!psc_vbitmap_get(odt->odt_bitmap, todtr.odtr_elem))
			continue;

		odtf = odtable_getfooter(odt, todtr.odtr_elem);

		rc = odtable_footercheck(odtf, &todtr, 2);
		psc_assert(rc != ODTBL_FREE_ERR);
		if (rc) {
			psclog_warnx("slot=%zd marked bad, skipping",
			    todtr.odtr_elem);
			continue;
		}

		odtr = PSCALLOC(sizeof(*odtr));
		odtr->odtr_key  = odtf->odtf_key;
		odtr->odtr_elem = todtr.odtr_elem;

		psclog_debug("handing back key=%"PRIx64" slot=%zd odtr=%p",
		    odtr->odtr_key, odtr->odtr_elem, odtr);

		odt_handler(odtable_getitem_addr(odt, todtr.odtr_elem), odtr);
	}
}
